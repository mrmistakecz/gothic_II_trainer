#pragma once
#include "MapData.h"

namespace QuestMap {
struct WorldVertex { float x=0,y=0,z=0; };
enum class Surface { Ground,Grass,Road,Rock,Roof,Water,Snow,Sand,Stone };
inline bool Decorative(std::string name) {
    for(char& c:name)c=(char)std::toupper((unsigned char)c);
    for(const char* part:{"TREE","LEAF","LEAVES","BUSH","FERN","FARN","PLANT","BLATT"})if(name.find(part)!=std::string::npos)return true;
    return false;
}
inline Surface Classify(std::string name,int group) {
    for(char& c:name)c=(char)std::toupper((unsigned char)c);
    auto has=[&](const char* s){return name.find(s)!=std::string::npos;};
    if(group==5||has("WATER")||has("WASSER"))return Surface::Water;
    if(has("ROOF")||has("DACH")||has("SHINGLE"))return Surface::Roof;
    if(has("PATH")||has("ROAD")||has("WAY")||has("WEG")||has("PAVEMENT")||has("COBBLE"))return Surface::Road;
    if(group==6||has("SNOW"))return Surface::Snow;
    if(has("SAND")||has("BEACH"))return Surface::Sand;
    if(has("GRASS")||has("MEADOW")||has("MOSS")||has("FOREST")||has("DARKW"))return Surface::Grass;
    if(has("ROCK")||has("CLIFF"))return Surface::Rock;
    if(group==2||has("STONE")||has("FLOOR"))return Surface::Stone;
    return Surface::Ground;
}
inline uint32_t SurfaceColor(Surface surface,float nx,float ny,float nz) {
    const unsigned colors[]={0x686349,0x596F4A,0xC2B18A,0x7A7F78,0xA36D50,0x3B7587,0xC9D1CE,0xBBA77A,0x92938A};
    unsigned rgb=colors[(int)surface];
    float shade=Clamp(.70f+.24f*ny-.18f*nx+.16f*nz,.48f,1.12f);
    unsigned r=(unsigned)Clamp(((rgb>>16)&255)*shade,0,255),g=(unsigned)Clamp(((rgb>>8)&255)*shade,0,255),b=(unsigned)Clamp((rgb&255)*shade,0,255);
    return 0xFF000000u|(r<<16)|(g<<8)|b;
}
// Orthographic cartography from the actual static world mesh. The highest
// upward-facing surface wins; texture names only choose the cartographic ink.
// All geometry, shorelines and building footprints come from world vertices.
class TerrainRaster {
public:
    Definition bounds;
    int size=0;
    size_t triangles=0;
    std::vector<uint32_t> color;
    std::vector<float> height;
    void Init(Definition def,int resolution) {
        bounds=std::move(def);size=resolution;triangles=0;
        color.assign((size_t)size*size,0xFF202A25);
        height.assign(color.size(),-(std::numeric_limits<float>::max)());
    }
    void Triangle(WorldVertex a,WorldVertex b,WorldVertex c,uint32_t ink) {
        if(size<=0)return;
        for(auto v:{a,b,c})if(!std::isfinite(v.x)||!std::isfinite(v.y)||!std::isfinite(v.z))return;
        auto uvA=bounds.UV(a.x,a.z),uvB=bounds.UV(b.x,b.z),uvC=bounds.UV(c.x,c.z);
        Point p{uvA.x*size,uvA.y*size},q{uvB.x*size,uvB.y*size},r{uvC.x*size,uvC.y*size};
        auto edge=[](Point a,Point b,Point p){return (p.x-a.x)*(b.y-a.y)-(p.y-a.y)*(b.x-a.x);};
        float area=edge(p,q,r);if(std::fabs(area)<.00001f)return;
        float minX=(std::min)({p.x,q.x,r.x}),maxX=(std::max)({p.x,q.x,r.x});
        float minY=(std::min)({p.y,q.y,r.y}),maxY=(std::max)({p.y,q.y,r.y});
        if(maxX<0||minX>=size||maxY<0||minY>=size)return;
        int x0=(int)Clamp(std::floor(minX),0,(float)size-1),x1=(int)Clamp(std::ceil(maxX),0,(float)size-1);
        int y0=(int)Clamp(std::floor(minY),0,(float)size-1),y1=(int)Clamp(std::ceil(maxY),0,(float)size-1);
        float inv=1/area;
        for(int y=y0;y<=y1;y++)for(int x=x0;x<=x1;x++) {
            Point sample{x+.5f,y+.5f};float u=edge(q,r,sample)*inv,v=edge(r,p,sample)*inv,w=1-u-v;
            if(u<-.00001f||v<-.00001f||w<-.00001f)continue;
            float z=u*a.y+v*b.y+w*c.y;size_t index=(size_t)y*size+x;
            if(z>=height[index]){height[index]=z;color[index]=ink;}
        }
        ++triangles;
    }
    // Subtle relief and silhouette shading, without a noisy triangle wireframe.
    void FinishRows(int first,int last) {
        for(int y=(std::max)(1,first);y<(std::min)(last,size-1);y++)for(int x=1;x<size-1;x++) {
            size_t i=(size_t)y*size+x;float h=height[i],west=height[i-1],north=height[i-size];
            if(h < -1e20f)continue;
            bool edge=(west>-1e20f&&h-west>180)||(north>-1e20f&&h-north>180);
            if(edge){unsigned c=color[i];color[i]=0xFF000000|((((c>>16)&255)*76/100)<<16)|((((c>>8)&255)*76/100)<<8)|((c&255)*76/100);}
        }
    }
};
}
