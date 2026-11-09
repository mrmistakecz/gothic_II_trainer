// Included inside the QuestNav namespace; all rendering uses the game's renderer.
struct MapAsset { QuestMap::Definition def; zCTexture* texture=nullptr; int width=1,height=1; };
static std::vector<MapAsset> maps;
static bool mapsRead=false, fullMap=false, pausedByMap=false;
static bool mapHeld=false,questHeld=false;
static size_t mapIndex=0;
static float fullZoom=1;
static QuestMap::Point mapCenter{0.5f,0.5f};
static bool terrainMode=true,displaySettingsRead=false;
static bool styleHeld=false;
static float miniDiameter=18000;
#include "TerrainNative.inl"
static void DisplaySettings() {
 if(displaySettingsRead)return;displaySettingsRead=true;
 terrainMode=Read("Display","Terrain")!="0";
 int range=atoi(Read("Display","Range").c_str());
 if(range==9000||range==18000||range==30000)miniDiameter=(float)range;
}
static void ChangeRange(bool closer) {
 if(closer)miniDiameter=miniDiameter>18000?18000:9000;
 else miniDiameter=miniDiameter<18000?18000:30000;
 Write("Display","Range",std::to_string((int)miniDiameter));
}
static void CloseMap() {
 fullMap=false;
 if(pausedByMap && ogame)ogame->Unpause();
 pausedByMap=false;
 if(zinput)zinput->ClearKeyBuffer();
}
static void ResetMaps() {
 if(fullMap||pausedByMap)CloseMap();
 ClearTerrain();
 for(auto& m:maps)if(m.texture)m.texture->Release();
 maps.clear();mapsRead=false;mapIndex=0;fullZoom=1;
}
static std::string Basename(std::string s) {
 auto i=s.find_last_of("\\/");if(i!=std::string::npos)s=s.substr(i+1);
 for(auto& c:s)c=(char)toupper((unsigned char)c);return s;
}
static void ReadMaps() {
 if(mapsRead||!parser||!Ready())return;
 mapsRead=true;
 const char* functions[]={"USE_MAP_NEWWORLD","USE_MAP_NEWWORLD_CITY","USE_MAP_OLDWORLD","USE_MAP_ADDONWORLD"};
 auto symbol=[](int id)->zCPar_Symbol* {return id>=0&&id<parser->symtab.GetNumInList()?parser->GetSymbol(id):nullptr;};
 auto value=[&](int id) {
  auto* s=symbol(id);if(!s||!(s->flags&1)||!s->ele)return QuestMap::Value{};
  if(s->type==zPAR_TYPE_INT){int n=0;s->GetValue(n,0);return QuestMap::Value::Int(n);}
  if(s->type==zPAR_TYPE_STRING){zSTRING text;s->GetValue(text,0);return QuestMap::Value::Str(text.ToChar());}
  return QuestMap::Value{};
 };
 auto name=[&](int id){auto* s=symbol(id);return s?std::string(s->name.ToChar()):std::string();};
 int size=parser->stack.GetStatSize();
 if(size<=0||size>parser->stack.stacksize)return;
 for(auto function:functions) {
  auto* s=symbol(parser->GetIndex(zSTRING(function)));if(!s||s->type!=zPAR_TYPE_FUNC||s->flags&8)continue;
  int start=-1;s->GetStackPos(start,0);if(start<0)continue;
  MapAsset asset;
  if(!QuestMap::Extract(parser->stack.stack,(size_t)size,(size_t)start,value,name,asset.def))continue;
  if(Basename(asset.def.world)!=World())continue;
  auto texture=zSTRING(asset.def.texture.c_str());asset.texture=zCTexture::Load(texture,0);
  if(!asset.texture)continue;
  asset.texture->CacheIn(-1.0f);asset.texture->GetPixelSize(asset.width,asset.height);
  if(asset.width<=0||asset.height<=0){asset.texture->Release();continue;}
  maps.push_back(asset);
 }
 auto logPath=GT::Paths::DataDir()+"\\GothicQuestNav.log";
 FILE* f=fopen(logPath.c_str(),"a");
 if(f){fprintf(f,"QuestNav %s: %s, %u calibrated maps\n",kVersion,World().c_str(),(unsigned)maps.size());for(const auto& m:maps)fprintf(f,"  %s: %.0f %.0f %.0f %.0f\n",m.def.texture.c_str(),m.def.left,m.def.top,m.def.right,m.def.bottom);fclose(f);}
}
static bool CanOpenMap() {
 return Ready()&&!ogame->IsOnPause()&&!zCMenu::GetActive()&&!player->IsDead()
     &&!player->GetTalkingWith()&&!player->IsInGlobalCutscene()&&!player->openinventory;
}
static void CycleQuest() {
 RefreshQuests();
 for(size_t i=0;i<quests.size();i++)if(quests[i].title==selected){selected=quests[(i+1)%quests.size()].title;break;}
 lastUpdate=0;
}
class MapInput:public zCInputCallback {
public:
 int HandleEvent(int key) override {
  struct Request{MapInput* self;int key;int result=0;} request{this,key};
  unsigned long error=0;
  if(!GT::SehInvoke([](void* p){auto* r=static_cast<Request*>(p);r->result=r->self->OnKey(r->key);},&request,&error)){Disable(error);return 0;}
  return request.result;
 }
 int OnKey(int key) {
  if(failed||!Ready())return 0;
  if(key==0x32){if(mapHeld)return 1;mapHeld=true;}
  if(key==0x40){if(questHeld)return 1;questHeld=true;}
  if((fullMap||CanOpenMap())&&key==0x41) {if(styleHeld)return 1;styleHeld=true;terrainMode=!terrainMode;Write("Display","Terrain",terrainMode?"1":"0");return 1;}
  if(fullMap) {
   if(key==0x32||key==0x01||key==0x44){CloseMap();return key==0x44?0:1;} // M, Esc, trainer F10
   if(key==0x0F && maps.size()>1){mapIndex=(mapIndex+1)%maps.size();fullZoom=1;mapCenter={0.5f,0.5f};}
   if(key==0x0D||key==0x4E)fullZoom=(std::min)(8.0f,fullZoom*1.5f);
   if(key==0x0C||key==0x4A){fullZoom=(std::max)(1.0f,fullZoom/1.5f);if(fullZoom==1)mapCenter={0.5f,0.5f};}
   if(key==0xC7 && !maps.empty()){auto p=player->GetPositionWorld();mapCenter=maps[mapIndex].def.UV(p.n[0],p.n[2]);}
   float delta=0.08f/fullZoom;
   if(key==0xCB)mapCenter.x-=delta;if(key==0xCD)mapCenter.x+=delta;
   if(key==0xC8)mapCenter.y-=delta;if(key==0xD0)mapCenter.y+=delta;
   mapCenter.x=QuestMap::Clamp(mapCenter.x,0,1);mapCenter.y=QuestMap::Clamp(mapCenter.y,0,1);
   if(key==0x40)CycleQuest();
   return 1;
  }
  if(!CanOpenMap())return 0;
  if(key==0x0D||key==0x4E){ChangeRange(true);return 1;}
  if(key==0x0C||key==0x4A){ChangeRange(false);return 1;}
  if(key==0x32) {
   ReadMaps();fullMap=true;fullZoom=1;mapCenter={0.5f,0.5f};mapIndex=0;
   pausedByMap=true;ogame->Pause(1);if(zinput)zinput->ClearKeyBuffer();return 1;
  }
  if(key==0x40) {
   if(zinput&&(zinput->KeyPressed(0x1D)||zinput->KeyPressed(0x9D))) {
    auto* focus=player->GetFocusNpc();if(focus&&!selected.empty())Register(selected,World(),"NPC",Utf(focus->GetInstanceName()));
   } else if(zinput&&(zinput->KeyPressed(0x2A)||zinput->KeyPressed(0x36)))CycleQuest();
   else visible=!visible;
   return 1;
  }
  return 0;
 }
};
static MapInput* mapInput=nullptr;
struct MapRenderState {
 int fog,write,wrap,bilerp;zTRnd_ZBufferCmp compare;zTRnd_AlphaBlendFunc blend;
 zTRnd_AlphaBlendSource source;float alpha;
 MapRenderState() {
  zrenderer->FlushPolys();fog=zrenderer->GetFog();write=zrenderer->GetZBufferWriteEnabled();wrap=zrenderer->GetTextureWrapEnabled();
  bilerp=zrenderer->GetBilerpFilterEnabled();compare=zrenderer->GetZBufferCompare();blend=zrenderer->GetAlphaBlendFunc();
  source=zrenderer->GetAlphaBlendSource();alpha=zrenderer->GetAlphaBlendFactor();
  zrenderer->SetFog(0);zrenderer->SetZBufferWriteEnabled(0);zrenderer->SetZBufferCompare(zRND_ZBUFFER_CMP_ALWAYS);
  zrenderer->SetTextureWrapEnabled(0);zrenderer->SetBilerpFilterEnabled(1);zrenderer->SetAlphaBlendFunc(zRND_ALPHA_FUNC_BLEND);
  zrenderer->SetAlphaBlendSource(zRND_ALPHA_SOURCE_MATERIAL);zrenderer->SetAlphaBlendFactor(1.0f);
 }
 ~MapRenderState() {
  zrenderer->FlushPolys();zrenderer->SetFog(fog);zrenderer->SetZBufferWriteEnabled(write);zrenderer->SetZBufferCompare(compare);
  zrenderer->SetTextureWrapEnabled(wrap);zrenderer->SetBilerpFilterEnabled(bilerp);zrenderer->SetAlphaBlendFunc(blend);
  zrenderer->SetAlphaBlendSource(source);zrenderer->SetAlphaBlendFactor(alpha);
 }
};
static void TextAt(float x,float y,float width,const std::vector<std::string>& lines) {
 if(!label)label=new zCView();
 float sw=(float)screen->psizex,sh=(float)screen->psizey;
 label->SetPos((int)(x/sw*8192),(int)(y/sh*8192));label->SetSize((int)(width/sw*8192),1600);
 label->SetFont(zSTRING("FONT_OLD_10_WHITE.TGA"));label->ClrPrintwin();int row=0;
 for(auto text:lines) {
  auto game=Game(text);
  // shorten by whole UTF-8 letters, a cut "č" would turn into garbage
  while(text.size()>3 && label->FontSize(game)>8190){
   while(!text.empty()&&((unsigned char)text.back()&0xC0)==0x80)text.pop_back();
   if(!text.empty())text.pop_back();
   game=Game(text+"...");
  }
  label->Print(0,row,game);row+=label->FontY();
 }
 label->BlitText();
}
#include "HudNative.inl"
