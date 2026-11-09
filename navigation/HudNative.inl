static void Polygon(zCTexture* texture,const std::vector<QuestMap::TexVertex>& points,zCOLOR color) {
 if(points.size()<3)return;
 std::vector<zTRndSimpleVertex> vertices(points.size());
 for(size_t i=0;i<points.size();i++){vertices[i].pos=zVEC2(points[i].pos.x,points[i].pos.y);vertices[i].uv=zVEC2(points[i].uv.x,points[i].uv.y);vertices[i].z=1.1f;vertices[i].color=color;}
 zrenderer->DrawPolySimple(texture,vertices.data(),(int)vertices.size());
}
static void Flat(const std::vector<QuestMap::Point>& points,zCOLOR color) {
 if(auto* solid=SolidTexture()){std::vector<QuestMap::TexVertex> vertices;for(auto p:points)vertices.push_back({p,{.5f,.5f}});Polygon(solid,vertices,color);return;}
 // The original DX7 DrawPolySimple ignores null textures. If procedural texture
 // allocation is unavailable, a bounded scanline fallback keeps the HUD visible.
 if(points.size()<3)return;float top=points[0].y,bottom=top;
 for(auto p:points){top=(std::min)(top,p.y);bottom=(std::max)(bottom,p.y);}
 for(int y=(int)ceilf(top);y<=(int)floorf(bottom);y++) {
  std::vector<float> intersections;
  for(size_t i=0;i<points.size();i++){auto a=points[i],b=points[(i+1)%points.size()];if((a.y<=y&&b.y>y)||(b.y<=y&&a.y>y))intersections.push_back(a.x+(y-a.y)*(b.x-a.x)/(b.y-a.y));}
  std::sort(intersections.begin(),intersections.end());for(size_t i=1;i<intersections.size();i+=2)Line(intersections[i-1],(float)y,intersections[i],(float)y,color);
 }
}
static void Disc(QuestMap::Point c,float radius,zCOLOR color,int segments=40) {
 for(int i=0;i<segments;i++) {
  float a=i*6.283185307f/segments,b=(i+1)*6.283185307f/segments;
  Flat({c,{c.x+cosf(a)*radius,c.y+sinf(a)*radius},{c.x+cosf(b)*radius,c.y+sinf(b)*radius}},color);
 }
}
static void Ring(QuestMap::Point c,float radius,zCOLOR light,zCOLOR shade,int segments=96) {
 for(int i=0;i<segments;i++) {
  float a=i*6.283185307f/segments,b=(i+1)*6.283185307f/segments;
  Line(c.x+cosf(a)*radius,c.y+sinf(a)*radius,c.x+cosf(b)*radius,c.y+sinf(b)*radius,sinf(a)<-.15f?light:shade);
 }
}
static void Stroke(QuestMap::Point a,QuestMap::Point b,float width,zCOLOR color) {
 float dx=b.x-a.x,dy=b.y-a.y,len=sqrtf(dx*dx+dy*dy);if(len<.001f)return;
 dx=dx/len*width*.5f;dy=dy/len*width*.5f;
 Flat({{a.x-dy,a.y+dx},{a.x+dy,a.y-dx},{b.x+dy,b.y-dx},{b.x-dy,b.y+dx}},color);
}
static QuestMap::Rect TextureView(const QuestMap::Definition& from,const QuestMap::Definition& to,QuestMap::Rect view) {
 auto a=to.UV(from.left+view.l*(from.right-from.left),from.top+view.t*(from.bottom-from.top));
 auto b=to.UV(from.left+view.r*(from.right-from.left),from.top+view.b*(from.bottom-from.top));
 return {a.x,a.y,b.x,b.y};
}
static void MapImage(zCTexture* texture,QuestMap::Rect pixels,QuestMap::Rect view,bool circular) {
 zCOLOR white(255,255,255,255);
 if(circular)for(const auto& t:QuestMap::CircleMesh(pixels,view))Polygon(texture,{t[0],t[1],t[2]},white);
 else {QuestMap::Rect uv,out;if(QuestMap::TextureRect(view,pixels,uv,out))zrenderer->DrawTile(texture,zVEC2(out.l,out.t),zVEC2(out.r,out.b),1.1f,zVEC2(uv.l,uv.t),zVEC2(uv.r,uv.b),white);}
 zrenderer->FlushPolys();
}
static void Draw() {
 if(!(GetAsyncKeyState('M')&0x8000))mapHeld=false;
 if(!(GetAsyncKeyState(VK_F6)&0x8000))questHeld=false;
 if(!(GetAsyncKeyState(VK_F7)&0x8000))styleHeld=false;
 if(!Ready()||!screen||!zrenderer){if(fullMap)CloseMap();return;}
 DisplaySettings();
 if(!mapInput){mapInput=new MapInput();mapInput->SetEnableHandleEvent(1);}
 if(!zCMenu::GetActive())mapInput->SetHandleEventTop();
 if(!fullMap&&!CanOpenMap())return;
 if(fullMap&&zCMenu::GetActive()){CloseMap();return;}
 // Update() runs once a second before the frame (LogicThunk): it calls dialog checks.
 if(!visible&&!fullMap)return;
 ReadMaps();if(terrainMode)UpdateTerrain();
 float sw=(float)screen->psizex,sh=(float)screen->psizey;if(sw<320||sh<240)return;
 MapRenderState restore;
 zCOLOR dark(19,25,22,255),shadow(6,9,8,190),edge(120,103,67,255),brass(210,186,125,255),gold(255,205,75,255),white(250,248,235,255),ink(17,21,21,255);
 auto pos=player->GetPositionWorld();
 float size=QuestMap::Clamp(sh*.30f,180,300),margin=QuestMap::Clamp(sw*.023f,20,42);
 if(size>sh*.53f)size=sh*.53f;
 QuestMap::Rect pixels{sw-size-margin,sh-size-48,sw-margin,sh-48},view;
 MapAsset* map=maps.empty()?nullptr:&maps[fullMap?mapIndex:0];
 // The city parchment has four times the useful local resolution. Select it
 // only when the entire local view fits, so the world edge never flickers.
 if(!fullMap&&!(terrainMode&&terrainTexture))for(auto& candidate:maps) {
  auto crop=QuestMap::Around(candidate.def,pos.n[0],pos.n[2],miniDiameter);
  if(crop.l>=0&&crop.t>=0&&crop.r<=1&&crop.b<=1&&candidate.def.right-candidate.def.left<map->def.right-map->def.left)map=&candidate;
 }
 bool detailed=terrainMode&&terrainTexture;
 if(fullMap) {
  float w=sw*.88f,h=sh*.73f;
  if(map){float aspect=detailed?(map->def.right-map->def.left)/(map->def.top-map->def.bottom):(float)map->width/map->height;if(w/h>aspect)w=h*aspect;else h=w/aspect;}
  pixels={(sw-w)*.5f,sh*.06f,(sw+w)*.5f,sh*.06f+h};
  float half=.5f/fullZoom;view={mapCenter.x-half,mapCenter.y-half,mapCenter.x+half,mapCenter.y+half};
  Flat({{pixels.l,pixels.t},{pixels.r,pixels.t},{pixels.r,pixels.b},{pixels.l,pixels.b}},dark);
 } else {
  if(map)view=QuestMap::Around(map->def,pos.n[0],pos.n[2],miniDiameter);
 }
 QuestMap::Point center{(pixels.l+pixels.r)*.5f,(pixels.t+pixels.b)*.5f};float radius=(pixels.r-pixels.l)*.5f;
 auto* rim=fullMap?nullptr:RimTexture(radius);
 if(!fullMap){if(!rim){Disc({center.x+2,center.y+4},radius+6,shadow);Disc(center,radius+3,ink);}Disc(center,radius,dark);}
 if(map) {
  auto* texture=detailed?terrainTexture:map->texture;
  auto imageView=detailed?TextureView(map->def,terrain.bounds,view):view;
  if(!detailed)map->texture->CacheIn(-1.0f);
  MapImage(texture,pixels,imageView,!fullMap);
  auto project=[&](const zVEC3& p){return QuestMap::Project(map->def.UV(p.n[0],p.n[2]),view,pixels);};
  auto clip=[&](QuestMap::Point& a,QuestMap::Point& b){return fullMap?QuestMap::Clip(a.x,a.y,b.x,b.y,{pixels.l+4,pixels.t+4,pixels.r-4,pixels.b-4}):QuestMap::ClipCircle(a.x,a.y,b.x,b.y,center,radius-6);};
  // A dark casing keeps the route legible on both parchment and detailed terrain.
  for(int pass=0;pass<2;pass++)for(size_t i=1;i<route.size();i++) {
   auto a=project(route[i-1]),b=project(route[i]);if(clip(a,b))Stroke(a,b,pass?2.5f:5.5f,pass?gold:ink);
  }
  // the other objectives of the quest: small marks, coloured by what to do there
  for(size_t i=1;i<objectives.size();i++) {
   auto p=project(objectives[i].pos);
   bool inside=fullMap?p.x>=pixels.l+6&&p.x<=pixels.r-6&&p.y>=pixels.t+6&&p.y<=pixels.b-6:std::hypot(p.x-center.x,p.y-center.y)<radius-8;
   if(!inside)continue;
   int kind=objectives[i].kind;
   zCOLOR mark=kind==kGoalKill?zCOLOR(235,92,70,255):(kind==kGoalFind||kind==kGoalChest||kind==kGoalCarrier||kind==kGoalSource)?zCOLOR(140,220,118,255):zCOLOR(125,192,255,255);
   Disc(p,fullMap?6.5f:5.0f,ink,14);Disc(p,fullMap?4.5f:3.5f,mark,14);
  }
  if(haveTarget) {
   auto original=project(target),p=original;
   if(fullMap){p.x=QuestMap::Clamp(p.x,pixels.l+12,pixels.r-12);p.y=QuestMap::Clamp(p.y,pixels.t+12,pixels.b-12);}
   else p=QuestMap::CircleClamp(p,center,radius-16);
   bool outside=fabsf(p.x-original.x)>.1f||fabsf(p.y-original.y)>.1f;
   Disc(p,9,ink,20);
   if(outside) {
    float dx=original.x-center.x,dy=original.y-center.y,d=sqrtf(dx*dx+dy*dy);if(d>.01f){dx/=d;dy/=d;Flat({{p.x+dx*7,p.y+dy*7},{p.x-dx*5-dy*5,p.y-dy*5+dx*5},{p.x-dx*2,p.y-dy*2},{p.x-dx*5+dy*5,p.y-dy*5-dx*5}},gold);}
   } else {
    Flat({{p.x,p.y-6},{p.x+6,p.y},{p.x,p.y+6},{p.x-6,p.y}},gold);
    Disc(p,2,white,10);
   }
  }
  auto p=project(pos);
  bool inside=fullMap?p.x>=pixels.l+12&&p.x<=pixels.r-12&&p.y>=pixels.t+12&&p.y<=pixels.b-12:std::hypot(p.x-center.x,p.y-center.y)<radius-12;
  if(inside) {
   auto f=player->GetAtVectorWorld();auto q=project(pos+f*100.0f);float dx=q.x-p.x,dy=q.y-p.y,d=sqrtf(dx*dx+dy*dy);if(d<.001f){dx=0;dy=-1;d=1;}dx/=d;dy/=d;
   Disc(p,10,shadow,24);
   Flat({{p.x+dx*13,p.y+dy*13},{p.x-dx*8-dy*7,p.y-dy*8+dx*7},{p.x-dx*4,p.y-dy*4},{p.x-dx*8+dy*7,p.y-dy*8-dx*7}},ink);
   Flat({{p.x+dx*10,p.y+dy*10},{p.x-dx*5-dy*4.5f,p.y-dy*5+dx*4.5f},{p.x-dx*2,p.y-dy*2},{p.x-dx*5+dy*4.5f,p.y-dy*5-dx*4.5f}},white);
  }
 }
 std::string info=status;
 if(haveTarget){char text[48];float dx=target.n[0]-pos.n[0],dz=target.n[2]-pos.n[2];snprintf(text,sizeof(text)," | %.0f m",sqrtf(dx*dx+dz*dz)/100);info+=text;if(route.size()<2)info+=u8" | jen směr";}
 if(fullMap) {
  Line(pixels.l,pixels.t,pixels.r,pixels.t,brass);Line(pixels.r,pixels.t,pixels.r,pixels.b,edge);Line(pixels.r,pixels.b,pixels.l,pixels.b,edge);Line(pixels.l,pixels.b,pixels.l,pixels.t,brass);
  std::vector<std::string> lines{selected.empty()?std::string(u8"Mapa světa"):selected,info};
  // the next objectives with their distance, the main one is already above
  for(size_t i=1;i<objectives.size()&&i<4;i++){char metres[32];snprintf(metres,sizeof(metres)," | %.0f m",objectives[i].metres);lines.push_back("- "+objectives[i].text+metres);}
  lines.push_back(u8"M / Esc zavřít | F6 další úkol | F7 terén / pergamen | +/- přiblížení");
  lines.push_back(u8"Šipky posun | Home hráč | Tab oblast / město");
  TextAt(sw*.06f,pixels.b+7,sw*.88f,lines);
 } else {
  if(rim)MapImage(rim,{pixels.l-10,pixels.t-10,pixels.r+10,pixels.b+10},{0,0,1,1},false);
  else {Ring(center,radius+3,brass,edge);Ring(center,radius+2,brass,edge);Ring(center,radius-1,ink,ink);}
  for(int i=0;i<16;i++){float a=i*6.283185307f/16;float inner=radius-(i%4==0?9.0f:5.0f);Line(center.x+cosf(a)*inner,center.y+sinf(a)*inner,center.x+cosf(a)*(radius-2),center.y+sinf(a)*(radius-2),i%4==0?brass:edge);}
  Disc({center.x,pixels.t+18},10,shadow,20);TextAt(center.x-7,pixels.t+9,30,{"N"});
  float metres=miniDiameter<=9000?20:miniDiameter<=18000?50:100;
  float width=size*(metres*100/miniDiameter),y=pixels.b-38;
  Stroke({center.x-width*.5f,y},{center.x+width*.5f,y},3,ink);Stroke({center.x-width*.5f,y},{center.x+width*.5f,y},1,brass);
  Line(center.x-width*.5f,y-3,center.x-width*.5f,y+3,brass);Line(center.x+width*.5f,y-3,center.x+width*.5f,y+3,brass);
  char scale[32];snprintf(scale,sizeof(scale),"%.0f m",metres);TextAt(center.x-17,pixels.b-33,70,{scale});
  std::string title=selected.empty()?std::string(u8"M mapa | F7 vzhled"):selected;
  if(!map)info=u8"Svět nemá kalibrovanou mapu";
  else if(terrainMode&&!terrainTexture&&!terrainUnavailable&&!haveTarget)info=u8"Připravuji detailní terén...";
  else if(selected.empty())info=detailed?u8"Detailní terén | +/- měřítko":u8"Pergamen | +/- měřítko";
  // Compact quest information sits above the dial, leaving the bottom HUD free.
  TextAt(pixels.l-4,pixels.t-48,size+8,{title,info});
 }
}
