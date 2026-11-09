static QuestMap::TerrainRaster terrain;
static zCTexture* terrainTexture=nullptr;
static zCTexture* solidTexture=nullptr;
static bool solidUnavailable=false;
static zCTexture* rimTexture=nullptr;
static float rimRadius=0;
static bool rimUnavailable=false;
static size_t terrainCursor=0;
static int terrainFinishRow=0;
static bool terrainUnavailable=false;
static void ClearTerrain() {
 if(terrainTexture)terrainTexture->Release();terrainTexture=nullptr;
 if(solidTexture)solidTexture->Release();solidTexture=nullptr;solidUnavailable=false;
 if(rimTexture)rimTexture->Release();rimTexture=nullptr;rimRadius=0;rimUnavailable=false;
 terrain=QuestMap::TerrainRaster{};terrainCursor=0;terrainFinishRow=0;terrainUnavailable=false;
}
// The original DirectX 7 renderer of Gothic II keeps only 16-bit textures
// (ARGB 4444, ARGB 1555, RGB 565) and DXT. zCTex_D3D::XTEX_BuildSurfaces turns
// any other format into an RGB 565 surface, so a 32-bit image never fits its
// buffer (the reason 0.4.0 always fell back to the parchment). Images are
// therefore converted here: RGB 565 when opaque, ARGB 4444 when they need alpha.
// "blend" also marks the texture as transparent for the renderer, so vertex
// colours with alpha (shadows) are blended even on an opaque image.
static uint16_t To565(uint32_t c){return (uint16_t)(((c>>8)&0xF800)|((c>>5)&0x07E0)|((c>>3)&0x001F));}
static uint16_t To4444(uint32_t c){return (uint16_t)(((c>>16)&0xF000)|((c>>12)&0x0F00)|((c>>8)&0x00F0)|((c>>4)&0x000F));}
static zCTexture* MakeTexture(const uint32_t* pixels,int width,int height,const char* what,bool blend=false) {
 const bool alpha=blend||std::any_of(pixels,pixels+(size_t)width*height,[](uint32_t c){return (c>>24)!=255;});
 auto* texture=zrenderer->CreateTexture();if(!texture){Log("QuestNav %s texture %s: no texture object",kVersion,what);return nullptr;}
 struct Owner{zCTexture* p;~Owner(){if(p)p->Release();}} owner{texture};
 zCTextureInfo info;info.texFormat=alpha?zRND_TEX_FORMAT_ARGB_4444:zRND_TEX_FORMAT_RGB_565;
 info.sizeX=info.refSizeX=width;info.sizeY=info.refSizeY=height;info.numMipMap=1;
 info.averageColor=zCOLOR(90,110,80,255);
 if(!texture->SetTextureInfo(info)){Log("QuestNav %s texture %s: format refused",kVersion,what);return nullptr;}
 // A generated texture has no file resource to CacheIn. Its data is supplied
 // here, retained by this plugin, and released at the explicit load boundary.
 texture->cacheState=zRES_CACHED_IN;texture->cacheOutLock=1;texture->changingRealtime=1;
 texture->hasAlpha=alpha;
 if(!texture->Lock(zTEX_LOCK_WRITE)){Log("QuestNav %s texture %s: lock failed",kVersion,what);return nullptr;}
 struct Unlock{zCTexture* p;~Unlock(){if(p)p->Unlock();}} unlock{texture};
 void* buffer=nullptr;int pitch=0;
 if(!texture->GetTextureBuffer(0,buffer,pitch)||!buffer||pitch<width*2){Log("QuestNav %s texture %s: no buffer (pitch %d)",kVersion,what,pitch);return nullptr;}
 for(int row=0;row<height;row++) {
  uint16_t* out=(uint16_t*)((char*)buffer+(size_t)row*pitch);const uint32_t* in=pixels+(size_t)row*width;
  if(alpha)for(int x=0;x<width;x++)out[x]=To4444(in[x]);else for(int x=0;x<width;x++)out[x]=To565(in[x]);
 }
 unlock.p=nullptr;
 if(!texture->Unlock()){Log("QuestNav %s texture %s: unlock failed",kVersion,what);return nullptr;}
 Log("QuestNav %s texture %s: %dx%d %s",kVersion,what,width,height,alpha?"ARGB 4444":"RGB 565");
 owner.p=nullptr;return texture;
}
static bool UploadTerrain() {
 terrainTexture=MakeTexture(terrain.color.data(),terrain.size,terrain.size,"terrain");
 if(!terrainTexture)return false;
 std::vector<float>().swap(terrain.height);std::vector<uint32_t>().swap(terrain.color);
 return true;
}
static zCTexture* SolidTexture() {
 if(!solidTexture&&!solidUnavailable) {
  unsigned long error=0;
  if(!GT::SehInvoke([](void*){const uint32_t white[16]={0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF};solidTexture=MakeTexture(white,4,4,"fill",true);},nullptr,&error)||!solidTexture)solidUnavailable=true;
 }
 return solidTexture;
}
static zCTexture* RimTexture(float radius) {
 if(rimTexture&&fabsf(radius-rimRadius)>.5f){rimTexture->Release();rimTexture=nullptr;}
 if(!rimTexture&&!rimUnavailable) {
  unsigned long error=0;
  if(!GT::SehInvoke([](void* p){float r=*(float*)p;auto image=QuestMap::DialFrame(512,r);rimTexture=MakeTexture(image.data(),512,512,"frame");rimRadius=r;},&radius,&error)||!rimTexture)rimUnavailable=true;
 }
 return rimTexture;
}
static void TerrainTick(void*) {
 if(terrainTexture||terrainUnavailable||maps.empty())return;
 auto& bsp=ogame->GetGameWorld()->bspTree;
 auto* mesh=bsp.mesh;auto** list=mesh&&mesh->polyList?mesh->polyList:bsp.treePolyList;
 int count=mesh&&mesh->polyList?mesh->numPoly:bsp.numPolys;
 if(!list||count<=0||count>4000000){terrainUnavailable=true;return;}
 if(!terrain.size) {
  int cap=zrenderer->GetMaxTextureSize();int resolution=2048;
  if(cap>0)while(resolution>cap&&resolution>256)resolution/=2;
  terrain.Init(maps.front().def,resolution);
 }
 LARGE_INTEGER frequency,start,now;QueryPerformanceFrequency(&frequency);QueryPerformanceCounter(&start);
 const double budget=.002; // Two milliseconds per frame; no whole-world blocking pass.
 int work=0;
 while(terrainCursor<(size_t)count && work++<4096) {
  auto* poly=list[terrainCursor++];
  if(poly&&poly->vertex&&poly->material&&poly->polyNumVert>=3&&poly->polyNumVert<=32
      &&!poly->flags.portalPoly&&!poly->flags.ghostOccluder&&!poly->flags.portalIndoorOutdoor&&poly->polyPlane.normal.n[1]>.015f) {
   auto* mat=poly->material;
   std::string name=mat->texture?mat->texture->GetObjectName().ToChar():mat->GetObjectName().ToChar();
   auto surface=QuestMap::Classify(name,(int)mat->matGroup);
   auto normal=poly->polyPlane.normal;
   auto ink=QuestMap::SurfaceColor(surface,normal.n[0],normal.n[1],normal.n[2]);
   auto point=[](zCVertex* p){auto v=p->position;return QuestMap::WorldVertex{v.n[0],v.n[1],v.n[2]};};
   if(!QuestMap::Decorative(name)&&poly->vertex[0])for(int i=2;i<poly->polyNumVert;i++)if(poly->vertex[i-1]&&poly->vertex[i])terrain.Triangle(point(poly->vertex[0]),point(poly->vertex[i-1]),point(poly->vertex[i]),ink);
  }
  QueryPerformanceCounter(&now);if((double)(now.QuadPart-start.QuadPart)/frequency.QuadPart>budget)return;
 }
 if(terrainCursor<(size_t)count)return;
 // Relief processing is also split into frames.
 if(terrainFinishRow<terrain.size){terrain.FinishRows(terrainFinishRow,terrainFinishRow+32);terrainFinishRow+=32;return;}
 if(!terrain.triangles||!UploadTerrain())terrainUnavailable=true;
 auto path=GT::Paths::DataDir()+"\\GothicQuestNav.log";FILE* f=fopen(path.c_str(),"a");
 if(f){fprintf(f,"QuestNav %s terrain: %u triangles, %dx%d, %s\n",kVersion,(unsigned)terrain.triangles,terrain.size,terrain.size,terrainTexture?"ready":"parchment fallback");fclose(f);}
}
static void UpdateTerrain() {
 unsigned long error=0;
 if(!GT::SehInvoke(&TerrainTick,nullptr,&error)) {
  terrainUnavailable=true;
  auto path=GT::Paths::DataDir()+"\\GothicQuestNav.log";FILE* f=fopen(path.c_str(),"a");
  if(f){fprintf(f,"Terrain fallback after exception 0x%08lX\n",error);fclose(f);}
 }
}
