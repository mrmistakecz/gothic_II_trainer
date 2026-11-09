#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

namespace QuestMap {
struct Point { float x=0, y=0; };
struct Rect { float l=0,t=0,r=1,b=1; };
struct Definition {
    std::string texture, world;
    float left=0, top=0, right=0, bottom=0;
    bool Valid() const {
        return !texture.empty() && !world.empty() && std::isfinite(left) && std::isfinite(top)
            && std::isfinite(right) && std::isfinite(bottom) && right>left && top>bottom;
    }
    Point UV(float x,float z) const { return {(x-left)/(right-left),(z-top)/(bottom-top)}; }
};
inline float Clamp(float v,float a,float b) { return (std::max)(a,(std::min)(v,b)); }
inline Point Project(Point uv,Rect view,Rect pixels) {
    return {pixels.l+(uv.x-view.l)/(view.r-view.l)*(pixels.r-pixels.l),
            pixels.t+(uv.y-view.t)/(view.b-view.t)*(pixels.b-pixels.t)};
}
inline Rect Around(const Definition& m,float x,float z,float diameter) {
    auto p=m.UV(x,z);float dx=diameter/(m.right-m.left)*0.5f,dz=diameter/(m.top-m.bottom)*0.5f;
    return {p.x-dx,p.y-dz,p.x+dx,p.y+dz};
}
// Clip the texture itself: outside the map stays blank, never repeats or stretches.
inline bool TextureRect(Rect view,Rect pixels,Rect& uv,Rect& out) {
    uv={Clamp(view.l,0,1),Clamp(view.t,0,1),Clamp(view.r,0,1),Clamp(view.b,0,1)};
    if(uv.r<=uv.l || uv.b<=uv.t)return false;
    auto a=Project({uv.l,uv.t},view,pixels), b=Project({uv.r,uv.b},view,pixels);
    out={a.x,a.y,b.x,b.y};return true;
}
inline bool Clip(float& x,float& y,float& a,float& b,Rect r) {
    float dx=a-x,dy=b-y,lo=0,hi=1;
    float p[]={-dx,dx,-dy,dy},q[]={x-r.l,r.r-x,y-r.t,r.b-y};
    for(int i=0;i<4;i++) {
        if(std::fabs(p[i])<0.00001f){if(q[i]<0)return false;}
        else {float u=q[i]/p[i];if(p[i]<0){if(u>hi)return false;if(u>lo)lo=u;}else{if(u<lo)return false;if(u<hi)hi=u;}}
    }
    a=x+hi*dx;b=y+hi*dy;x+=lo*dx;y+=lo*dy;return true;
}
inline Point CircleClamp(Point p,Point center,float radius) {
    float x=p.x-center.x,y=p.y-center.y,d=std::sqrt(x*x+y*y);
    if(d>radius&&d>0){p.x=center.x+x*radius/d;p.y=center.y+y*radius/d;}return p;
}
inline bool ClipCircle(float& x,float& y,float& a,float& b,Point center,float radius) {
    float dx=a-x,dy=b-y,px=x-center.x,py=y-center.y,A=dx*dx+dy*dy;
    if(A<.00001f)return px*px+py*py<=radius*radius;
    float B=2*(px*dx+py*dy),C=px*px+py*py-radius*radius,D=B*B-4*A*C;
    if(D<0)return false;
    float root=std::sqrt(D),lo=(std::max)(0.0f,(-B-root)/(2*A)),hi=(std::min)(1.0f,(-B+root)/(2*A));
    if(lo>hi)return false;
    a=x+dx*hi;b=y+dy*hi;x+=dx*lo;y+=dy*lo;return true;
}
struct TexVertex {Point pos,uv;};
using TexTriangle=std::array<TexVertex,3>;
// Genuine circular texture geometry: corners are never drawn over the game.
inline std::vector<TexTriangle> CircleMesh(Rect pixels,Rect view,int segments=80) {
    std::vector<TexTriangle> result;
    Point center{(pixels.l+pixels.r)*.5f,(pixels.t+pixels.b)*.5f};float radius=(pixels.r-pixels.l)*.5f;
    auto vertex=[&](Point p){return TexVertex{p,{view.l+(p.x-pixels.l)/(pixels.r-pixels.l)*(view.r-view.l),view.t+(p.y-pixels.t)/(pixels.b-pixels.t)*(view.b-view.t)}};};
    for(int i=0;i<segments;i++) {
        float a=i*6.283185307f/segments,b=(i+1)*6.283185307f/segments;
        std::vector<TexVertex> poly{vertex(center),vertex({center.x+std::cos(a)*radius,center.y+std::sin(a)*radius}),vertex({center.x+std::cos(b)*radius,center.y+std::sin(b)*radius})};
        for(int side=0;side<4&&!poly.empty();side++) {
            std::vector<TexVertex> clipped;
            auto distance=[&](TexVertex v){return side==0?v.uv.x:side==1?1-v.uv.x:side==2?v.uv.y:1-v.uv.y;};
            auto previous=poly.back();float old=distance(previous);
            for(auto current:poly) {
                float d=distance(current);
                if((d>=0)!=(old>=0)) {
                    float t=old/(old-d);
                    clipped.push_back({{previous.pos.x+(current.pos.x-previous.pos.x)*t,previous.pos.y+(current.pos.y-previous.pos.y)*t},{previous.uv.x+(current.uv.x-previous.uv.x)*t,previous.uv.y+(current.uv.y-previous.uv.y)*t}});
                }
                if(d>=0)clipped.push_back(current);previous=current;old=d;
            }
            poly=std::move(clipped);
        }
        for(size_t j=2;j<poly.size();j++)result.push_back({poly[0],poly[j-1],poly[j]});
    }
    return result;
}
inline std::vector<uint32_t> DialFrame(int resolution,float radius) {
    std::vector<uint32_t> pixels((size_t)resolution*resolution);
    float scale=(radius+10)*2/resolution;
    for(int y=0;y<resolution;y++)for(int x=0;x<resolution;x++) {
        float dx=(x+.5f-resolution*.5f)*scale,dy=(y+.5f-resolution*.5f)*scale;
        float d=std::hypot(dx,dy)-radius;
        float sd=std::hypot(dx-2,dy-3)-radius;
        float alpha=d>=-2&&sd>0?72*Clamp(1-sd/10,0,1)*Clamp(1-sd/10,0,1):0;
        float red=0,green=0,blue=0;
        if(d>=-2.5f&&d<=3.5f) {
            float coverage=Clamp(d+2.5f,0,1)*Clamp(3.5f-d,0,1);
            float shade=Clamp(.86f-.22f*dy/(radius+1),.55f,1.08f);
            if(d<-1||d>2)shade*=.32f;
            red=205*shade;green=177*shade;blue=112*shade;alpha=(std::max)(alpha,coverage*255);
        }
        pixels[(size_t)y*resolution+x]=((uint32_t)alpha<<24)|((uint32_t)red<<16)|((uint32_t)green<<8)|(uint32_t)blue;
    }
    return pixels;
}
struct Value {
    enum Kind {Unknown, Integer, String} kind=Unknown;
    int number=0;
    std::string text;
    static Value Int(int n){Value v;v.kind=Integer;v.number=n;return v;}
    static Value Str(std::string s){Value v;v.kind=String;v.text=std::move(s);return v;}
};
// Read only the literal argument sequences of the game's map-use function.
// Never run Daedalus, change its PC/stack, create a document, or grant a map item.
// A mod with dynamic map arguments is rejected instead of using guessed bounds.
template<class SymbolValue,class SymbolName>
bool Extract(const unsigned char* code,size_t size,size_t start,SymbolValue value,SymbolName name,Definition& result) {
    if(!code || start>=size)return false;
    Definition map;bool coords=false,page=false,level=false,returned=false;
    std::vector<Value> args;
    size_t pos=start;
    for(int count=0;count<512 && pos<size && pos-start<4096;count++) {
        unsigned op=code[pos++];int n=0;
        bool operand=op==61||op==62||op==64||op==65||op==67||op==75||op==76||op==80||op==245;
        if(operand){if(size-pos<4)return false;std::memcpy(&n,code+pos,4);pos+=4;}
        if(op==245){if(pos>=size)return false;++pos;}
        if(op==60){returned=true;break;}
        if(op==64)args.push_back(Value::Int(n));
        else if(op==65)args.push_back(value(n));
        else if(op==31 && !args.empty() && args.back().kind==Value::Integer) {
            if(args.back().number==(std::numeric_limits<int>::min)())return false;
            args.back().number=-args.back().number;
        }
        else if(op==62) {
            auto call=name(n);
            if(call=="DOC_SETPAGE" && args.size()==4 && args[1].kind==Value::Integer && args[1].number==0 && args[2].kind==Value::String) {
                if(page)return false;map.texture=args[2].text;page=true;
            }
            if(call=="DOC_SETLEVEL" && args.size()==2 && args[1].kind==Value::String) {
                if(level)return false;map.world=args[1].text;level=true;
            }
            if(call=="DOC_SETLEVELCOORDS" && args.size()==5) {
                if(coords)return false;
                for(int i=1;i<5;i++)if(args[i].kind!=Value::Integer)return false;
                map.left=(float)args[1].number;map.top=(float)args[2].number;
                map.right=(float)args[3].number;map.bottom=(float)args[4].number;coords=true;
            }
            args.clear();
        }
        else args.clear();
        if(args.size()>32)return false;
    }
    if(!returned||!page||!level||!coords||!map.Valid())return false;
    result=map;return true;
}
}
