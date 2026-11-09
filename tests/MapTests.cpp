#include "../navigation/MapData.h"
#include "../navigation/Terrain.h"
#include <cstdio>
#include <fstream>
#include <iterator>
#include <stdexcept>

using namespace QuestMap;
static int checks=0;
static void Check(bool pass,const char* text) {if(!pass)throw std::runtime_error(text);++checks;}
static bool Near(float a,float b){return std::fabs(a-b)<0.001f;}
struct Symbol {std::string name;Value value;int type=0,flags=0,start=0;};
struct Dat {
 std::vector<unsigned char> bytes,code;size_t p=0;std::vector<Symbol> symbols;
 unsigned Read(){if(bytes.size()-p<4)throw std::runtime_error("Truncated DAT");unsigned n=0;std::memcpy(&n,bytes.data()+p,4);p+=4;return n;}
 std::string Line(){size_t begin=p;while(p<bytes.size()&&bytes[p]!=10)++p;if(p==bytes.size())throw std::runtime_error("Truncated string");return std::string((char*)bytes.data()+begin,p++-begin);}
 explicit Dat(const char* path) {
  std::ifstream f(path,std::ios::binary);if(!f)throw std::runtime_error("Cannot open Gothic.dat");
  bytes.assign(std::istreambuf_iterator<char>(f),{});if(bytes.empty())throw std::runtime_error("Empty DAT");p=1;
  unsigned count=Read();if(count>1000000)throw std::runtime_error("Invalid symbol count");p+=count*4;
  for(unsigned i=0;i<count;i++) {
   Symbol s;if(Read())s.name=Line();Read();unsigned props=Read();unsigned n=props&4095;s.type=(props>>12)&15;s.flags=(props>>16)&63;
   for(int j=0;j<5;j++)Read();
   if(!(s.flags&4)) {
    if(s.type==1||s.type==2)for(unsigned j=0;j<n;j++){int v=(int)Read();if(j==0&&s.type==2)s.value=Value::Int(v);}
    if(s.type==3)for(unsigned j=0;j<n;j++){auto v=Line();if(j==0)s.value=Value::Str(v);}
    if(s.type>=4&&s.type<=7)s.start=(int)Read();
   }
   Read();symbols.push_back(s);
  }
  unsigned size=Read();if(size>bytes.size()-p)throw std::runtime_error("Truncated bytecode");code.assign(bytes.begin()+p,bytes.begin()+p+size);
 }
 bool Map(const char* function,Definition& map) {
  auto value=[&](int id){return id>=0&&(size_t)id<symbols.size()&&(symbols[id].flags&1)?symbols[id].value:Value{};};
  auto name=[&](int id){return id>=0&&(size_t)id<symbols.size()?symbols[id].name:std::string();};
  for(const auto& s:symbols)if(s.name==function&&s.type==5)return Extract(code.data(),code.size(),s.start,value,name,map);
  return false;
 }
};
static void Geometry() {
 Definition m{"map.tga","world.zen",-28000,50500,95500,-42500};
 auto a=m.UV(-28000,50500),b=m.UV(95500,-42500),c=m.UV(33750,4000);
 Check(Near(a.x,0)&&Near(a.y,0)&&Near(b.x,1)&&Near(b.y,1),"Calibration corners must match native map");
 Check(Near(c.x,.5f)&&Near(c.y,.5f),"Map center and north/south orientation");
 auto view=Around(m,33750,4000,24000);auto hero=Project(c,view,{100,100,340,340});
 Check(Near(hero.x,220)&&Near(hero.y,220),"Minimap follows player");
 auto east=Project(m.UV(34750,4000),view,{100,100,340,340});auto north=Project(m.UV(33750,5000),view,{100,100,340,340});
 Check(Near(east.x,230)&&Near(north.y,210),"Ten metres east/right and north/up use equal minimap scale");
 Rect uv,out;Check(TextureRect({-.5f,-.5f,.5f,.5f},{0,0,200,200},uv,out)&&Near(out.l,100)&&Near(out.t,100)&&Near(uv.r,.5f),"World edge clips image without stretching");
 Check(!TextureRect({2,2,3,3},{0,0,200,200},uv,out),"Outside map does not wrap texture");
 float x=-20,y=50,a1=120,b1=50;Check(Clip(x,y,a1,b1,{0,0,100,100})&&Near(x,0)&&Near(a1,100),"Route crosses map boundary with both ends outside");
 x=-20;y=-20;a1=-10;b1=80;Check(!Clip(x,y,a1,b1,{0,0,100,100}),"Offscreen route cannot paint over game");
 m.right=m.left;Check(!m.Valid(),"Degenerate calibration rejected");
 unsigned char truncated[]={64,1};Definition untouched{"keep","world",0,1,1,0};
 auto v=[](int){return Value{};};auto n=[](int){return std::string();};
 Check(!Extract(truncated,sizeof(truncated),0,v,n,untouched)&&untouched.texture=="keep","Truncated bytecode fails without replacing valid data");
 unsigned char missing[]={60};Check(!Extract(missing,1,0,v,n,untouched),"Missing map calls never use guessed calibration");
 auto circle=CircleMesh({0,0,200,200},{0,0,1,1});double area=0;bool allInside=true;
 for(const auto& t:circle){for(auto v:t)allInside=allInside&&std::hypot(v.pos.x-100,v.pos.y-100)<=100.01f;area+=std::fabs((t[1].pos.x-t[0].pos.x)*(t[2].pos.y-t[0].pos.y)-(t[1].pos.y-t[0].pos.y)*(t[2].pos.x-t[0].pos.x))*.5;}
 Check(allInside,"Circular image never paints square corners");
 Check(std::fabs(area-31415.9265)<40,"Circular fan covers the entire disc without gaps");
 bool valid=true;for(const auto& t:CircleMesh({0,0,200,200},{-.5f,-.5f,.5f,.5f}))for(auto v:t)valid=valid&&v.uv.x>=-.00001f&&v.uv.x<=1.00001f&&v.uv.y>=-.00001f&&v.uv.y<=1.00001f;
 Check(valid,"Circle near world edge clips UVs rather than repeating terrain");
 x=-100;y=50;a1=200;b1=50;Check(ClipCircle(x,y,a1,b1,{50,50},40)&&Near(x,10)&&Near(a1,90),"Route entering and exiting the circular map is clipped correctly");
 x=0;y=0;a1=100;b1=0;Check(!ClipCircle(x,y,a1,b1,{50,50},40),"Route outside the disc cannot enter its square corners");
 auto marker=CircleClamp({200,200},{100,100},80);Check(Near(std::hypot(marker.x-100,marker.y-100),80),"Far objective follows the round rim");
 auto frame=DialFrame(128,100);
 Check((frame[0]>>24)==0&&(frame[64*128+64]>>24)==0,"Dial corners and center stay transparent over game and terrain");
 bool softened=false;for(auto c:frame)if((c>>24)>0&&(c>>24)<255)softened=true;
 Check(softened,"Dial rim includes smooth coverage and a soft shadow");
 Check(Classify("NW_NATURE_PATH_01.TGA",4)==Surface::Road&&Classify("RIVER.TGA",5)==Surface::Water,"Actual material metadata distinguishes paths and water");
 Check(Decorative("NW_NATURE_TREE_LEAF_01.TGA")&&!Decorative("NW_CITY_ROOF_01.TGA"),"Transparent tree billboards do not obscure streets as solid triangles");
 TerrainRaster raster;raster.Init({"test","world",0,10,10,0},32);
 raster.Triangle({0,0,10},{10,0,10},{0,0,0},0xFF112233);
 raster.Triangle({0,5,10},{10,5,10},{0,5,0},0xFF445566);
 Check(raster.color[4*32+4]==0xFF445566&&Near(raster.height[4*32+4],5),"Roof occludes lower terrain at the actual world height");
 raster.Triangle({0,-5,10},{10,-5,10},{0,-5,0},0xFF778899);
 Check(raster.color[4*32+4]==0xFF445566,"A later lower polygon cannot overwrite the building");
 auto previous=raster.color;raster.Triangle({0,1,0},{0,2,0},{0,3,0},0xFFFFFFFF);
 Check(raster.color==previous,"Vertical and degenerate geometry cannot streak across the map");
 raster.Triangle({100,0,100},{101,0,100},{100,0,101},0xFFFFFFFF);Check(raster.color==previous,"Off-map triangles remain outside the atlas");
}
static void RenderFixture(const char* path,const char* output) {
 std::ifstream f(path,std::ios::binary);if(!f)throw std::runtime_error("Missing external world fixture");
 auto read=[&](void* p,size_t size){if(!f.read((char*)p,size))throw std::runtime_error("Truncated terrain fixture");};
 char magic[4];read(magic,4);Check(std::string(magic,4)=="GQNT","World fixture format");
 float bounds[4];unsigned count=0;read(bounds,sizeof(bounds));read(&count,4);Check(count>1000&&count<4000000,"Actual world fixture contains substantial geometry");
 TerrainRaster raster;raster.Init({"atlas","NEWWORLD.ZEN",bounds[0],bounds[1],bounds[2],bounds[3]},2048);
 for(unsigned i=0;i<count;i++) {
  WorldVertex vertices[3];float normal[3];unsigned char group;unsigned short length;
  read(vertices,sizeof(vertices));read(normal,sizeof(normal));read(&group,1);read(&length,2);std::string texture(length,'\0');read(texture.data(),length);
  if(!Decorative(texture))raster.Triangle(vertices[0],vertices[1],vertices[2],SurfaceColor(Classify(texture,group),normal[0],normal[1],normal[2]));
 }
 raster.FinishRows(0,raster.size);
 Check(raster.triangles>1000,"Actual installed-world surfaces rasterized");
 std::ofstream out(output,std::ios::binary);unsigned char h[18]={};h[2]=2;h[12]=raster.size&255;h[13]=raster.size>>8;h[14]=h[12];h[15]=h[13];h[16]=32;h[17]=0x28;
 out.write((char*)h,18);out.write((char*)raster.color.data(),raster.color.size()*4);if(!out)throw std::runtime_error("Cannot write terrain render");
 std::printf("Rendered %zu installed-world triangles to %s\n",raster.triangles,output);
}
int main(int argc,char** argv) {
 try {
  Geometry();
  if(argc==4&&std::string(argv[1])=="--terrain-fixture")RenderFixture(argv[2],argv[3]);
  if(argc==3&&std::string(argv[1])=="--game-dat") {
   Dat dat(argv[2]);
   struct Expected{const char* function;const char* texture;float l,t,r,b;};
   const Expected expected[]={
    {"USE_MAP_NEWWORLD","Map_NewWorld.tga",-28000,50500,95500,-42500},
    {"USE_MAP_NEWWORLD_CITY","Map_NewWorld_City.tga",-6900,11800,21600,-9400},
    {"USE_MAP_OLDWORLD","Map_OldWorld.tga",-78500,47500,54000,-53000},
    {"USE_MAP_ADDONWORLD","Map_AddonWorld.tga",-47783,36300,43949,-32300}};
   for(const auto& e:expected) {
    Definition m;Check(dat.Map(e.function,m),e.function);
    Check(m.texture==e.texture&&m.left==e.l&&m.top==e.t&&m.right==e.r&&m.bottom==e.b,"Installed game calibration mismatch");
    std::printf("Verified installed %s: %s [%.0f %.0f %.0f %.0f]\n",e.function,m.texture.c_str(),m.left,m.top,m.right,m.bottom);
   }
  }
  std::printf("All %d map checks passed.\n",checks);return 0;
 } catch(const std::exception& e){std::fprintf(stderr,"FAIL: %s\n",e.what());return 1;}
}
