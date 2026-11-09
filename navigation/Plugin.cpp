#pragma warning(disable:4005)
#include <algorithm>
#include <climits>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
#include <Union/Hook.h>
#include <ZenGin/zGothicAPI.h>
#include "Core/Paths.h"
#include "Core/Text.h"
#include "Core/Seh.h"
#include "Core/QuestScan.h"
#include "MapData.h"
#include "Terrain.h"

#ifdef __G2A
HOOKSPACE(Gothic_II_Addon, GetGameVersion() == Engine_G2A);
namespace Gothic_II_Addon { namespace QuestNav {
static const char* kVersion = "0.5.1";
struct Quest { std::string title, raw, stage; };
static std::vector<Quest> quests;
static std::vector<zVEC3> route;
static std::string selected, worldName, status;
static bool visible = true, haveTarget = false, failed = false, settingsRead = false;
static int loading = 0;
static DWORD lastUpdate = 0;
static zVEC3 target;
static zCView* label = nullptr;
static std::string requested;   // raw diary title the trainer asked to navigate to
static void ResetMaps();
static void CloseMap();
static void ResetObjectives();

static void Log(const char* format, ...) {
 std::string path=GT::Paths::DataDir()+"\\GothicQuestNav.log";
 FILE* f=fopen(path.c_str(),"a");if(!f)return;
 va_list args;va_start(args,format);vfprintf(f,format,args);va_end(args);
 fputc('\n',f);fclose(f);
}
static void Disable(unsigned long error) {
 failed=true;
 unsigned long ignored=0;
 GT::SehInvoke([](void*){CloseMap();},nullptr,&ignored);
 Log("QuestNav %s disabled after exception 0x%08lX",kVersion,error);
}

static std::string Utf(const zSTRING& s) { return GT::Text::FromGame(s.ToChar()); }
static zSTRING Game(const std::string& s) { return zSTRING(GT::Text::ToGame(s).c_str()); }
static std::string Ini() { return GT::Paths::DataDir() + "\\GothicQuestNav.ini"; }
static unsigned Hash(const std::string& s) { unsigned h=2166136261u; for (unsigned char c:s) {h^=c;h*=16777619u;} return h; }
static std::string Key(const std::string& s) { char b[24]; snprintf(b,sizeof(b),"Quest_%08X",Hash(s)); return b; }
static std::string Read(const std::string& section,const char* key) { char b[2048]={}; GetPrivateProfileStringA(section.c_str(),key,"",b,sizeof(b),Ini().c_str()); return b; }
static void Write(const std::string& section,const char* key,const std::string& value) { WritePrivateProfileStringA(section.c_str(),key,value.c_str(),Ini().c_str()); }
// The trainer's text encoding (Nastavení → Kódování), so both show the same letters.
static void ReadSettings() {
 if(settingsRead)return;settingsRead=true;
 int cp=(int)GetPrivateProfileIntA("GothicTrainer","CodePage",0,(GT::Paths::DataDir()+"\\GothicTrainer.ini").c_str());
 if(cp>0&&IsValidCodePage((UINT)cp))GT::Text::SetCodePage((unsigned)cp);
}
static bool Ready() { return !loading && ogame && player && !ogame->inLoadSaveGame && !ogame->inLevelChange && !ogame->inScriptStartup && ogame->GetGameWorld() && player->GetHomeWorld()==ogame->GetGameWorld(); }
static void Reset() { ResetMaps();ResetObjectives();route.clear(); quests.clear(); haveTarget=false; lastUpdate=0; }
struct LoadGuard { LoadGuard(){++loading;Reset();} ~LoadGuard(){--loading;} };
static std::string World() { std::string w=ogame->GetGameWorld()->GetWorldFilename().ToChar(); size_t i=w.find_last_of("\\/"); if(i!=std::string::npos)w=w.substr(i+1); for(char& c:w)c=(char)toupper((unsigned char)c); return w; }
static void RefreshQuests() {
 auto previous=quests;
 quests.clear();
 for(auto* n=oCLogManager::GetLogManager().m_lstTopics.next;n;n=n->next) {
  auto* q=n->data;
  if(!q || q->m_enuSection!=oCLogTopic::zELogSection_Missions || q->m_enuStatus!=oCLogTopic::zELogTopicStatus_Running)continue;
  Quest item; item.title=Utf(q->m_strDescription); item.raw=q->m_strDescription.ToChar();
  // Hash every journal entry, rather than relying on insertion order.
  std::string stage;
  for(auto* e=q->m_lstEntries.next;e;e=e->next)if(e->data)stage+=Utf(e->data->m_strDescription)+"\n";
  item.stage=Key(stage); quests.push_back(item);
 }
 // the trainer's "Navigovat" wins, then a newly started quest, then the previous choice
 if(!requested.empty()){for(const auto& q:quests)if(q.raw==requested){selected=q.title;requested.clear();break;}}
 if(!previous.empty())for(const auto& q:quests)if(std::none_of(previous.begin(),previous.end(),[&](const Quest& old){return old.title==q.title;})){selected=q.title;break;}
 if(!quests.empty() && std::none_of(quests.begin(),quests.end(),[](const Quest& q){return q.title==selected;}))selected=quests.front().title;
 if(quests.empty()) selected.clear();
}
#include "QuestBrain.inl"
static bool ResolveNpc(const std::string& code,zVEC3& pos) {
 auto* list=ogame->GetGameWorld()->voblist_npcs;
 for(auto* n=list?list->next:nullptr;n;n=n->next)if(n->data && !n->data->IsDead() && Utf(n->data->GetInstanceName())==code)return Where(n->data,pos);
 return false;
}
// A target registered by hand (Ctrl+F6) or by a script (GQN_SetTarget) for the
// current diary stage of the quest. Returns false when there is none.
static bool ManualTarget(const std::string& w) {
 std::string section=Key(selected);
 if(Read(section,"Topic")!=selected)return false;
 auto q=std::find_if(quests.begin(),quests.end(),[](const Quest& q){return q.title==selected;});
 if(q==quests.end() || Read(section,"Stage")!=q->stage)return false;   // a newer stage: back to automatic
 if(Read(section,"World")!=w){status=u8"Ruční cíl je v jiném světě";return true;}
 std::string kind=Read(section,"Kind"), code=Read(section,"Target");
 auto* net=ogame->GetGameWorld()->wayNet;
 if(kind=="NPC")haveTarget=ResolveNpc(code,target);
 else if(kind=="WP" && net)for(auto* n=net->wplist.next;n;n=n->next)if(n->data && code==n->data->name.ToChar()){target=n->data->pos;haveTarget=true;break;}
 status=haveTarget?u8"Ruční cíl":u8"Ruční cíl teď v tomto světě není";
 return true;
}
static void Update() {
 if(!Ready()){Reset();return;}
 std::string w=World(); if(w!=worldName){Reset();worldName=w;}
 RefreshQuests(); haveTarget=false;route.clear();objectives.clear();objectiveNote.clear();status.clear();
 if(selected.empty()){status=u8"Žádné probíhající úkoly";return;}
 if(!ManualTarget(w)) {
  auto q=std::find_if(quests.begin(),quests.end(),[](const Quest& q){return q.title==selected;});
  if(q!=quests.end())ResolveObjectives(q->raw);
  if(!objectives.empty()){target=objectives.front().pos;haveTarget=true;status=objectives.front().text;}
  else status=objectiveNote.empty()?std::string(u8"Cíl neznám, Ctrl+F6 přiřadí zaměřenou postavu"):objectiveNote;
 }
 if(!haveTarget)return;
 auto* net=ogame->GetGameWorld()->wayNet;
 if(net) {
  zCRoute* path=net->FindRoute(player->GetPositionWorld(),target,player);
  if(path){route.push_back(player->GetPositionWorld()); for(int i=0;i<4096;i++){auto* p=path->GetNextWP();if(!p)break;route.push_back(p->pos);}delete path;}
 }
}
static void Register(const std::string& topic,const std::string& world,const std::string& kind,const std::string& code) {
 if(!Ready()||topic.empty()||code.empty())return;
 RefreshQuests();auto q=std::find_if(quests.begin(),quests.end(),[&](const Quest& q){return q.title==topic;});if(q==quests.end())return;
 auto s=Key(topic);Write(s,"Topic",topic);Write(s,"World",world);Write(s,"Kind",kind);Write(s,"Target",code);Write(s,"Stage",q->stage);lastUpdate=0;
}
// Optional Daedalus bridge: called AFTER Log_AddEntry for the new objective.
static int __cdecl SetTarget() { zSTRING waypoint,world,topic;parser->GetParameter(waypoint);parser->GetParameter(world);parser->GetParameter(topic);Register(Utf(topic),Utf(world),"WP",Utf(waypoint));return 0; }
static void Line(float x,float y,float a,float b,zCOLOR c) {zrenderer->DrawLine(x,y,a,b,c);}
#include "MapOverlay.inl"
static void Thunk(void*){Draw();}
// Game logic once a second, before the frame is rendered: the dialog checks
// run Daedalus, so they stay out of the renderer's EndFrame.
static void LogicThunk(void*) {
 ReadSettings();
 if(!Ready())return;
 if(fullMap||(visible&&CanOpenMap())) {
  DWORD now=GetTickCount();
  if(!lastUpdate||now-lastUpdate>1000){Update();lastUpdate=now;}
 }
}
} // QuestNav
void __fastcall Nav_Render(oCGame* self,void* v);
auto Nav_RenderHook=Union::CreateHook((void*)0x006C86A0,&Nav_Render,Union::HookType::Hook_Detours);
static int Nav_RenderDepth=0;
void __fastcall Nav_Render(oCGame* self,void* v){
 struct Scope{Scope(){++Nav_RenderDepth;}~Scope(){--Nav_RenderDepth;}} scope;
 if(!QuestNav::failed){unsigned long error=0;if(!GT::SehInvoke(&QuestNav::LogicThunk,nullptr,&error))QuestNav::Disable(error);}
 Nav_RenderHook(self,v);
}
// oCGame::Render ends the Direct3D scene before returning. Native DrawTile/DrawLine
// must run BEFORE EndFrame, unlike the trainer's separate DirectX overlay.
void __fastcall Nav_EndFrame(zCRnd_D3D* self,void* v);
auto Nav_EndFrameHook=Union::CreateHook((void*)0x0064DF20,&Nav_EndFrame,Union::HookType::Hook_Detours);
void __fastcall Nav_EndFrame(zCRnd_D3D* self,void* v){
 if(Nav_RenderDepth>0&&!QuestNav::failed){unsigned long error=0;if(!GT::SehInvoke(&QuestNav::Thunk,nullptr,&error))QuestNav::Disable(error);}
 Nav_EndFrameHook(self,v);
}
void __fastcall Nav_Load(oCGame* self,void* v,int a,int b);
auto Nav_LoadHook=Union::CreateHook((void*)0x006C67D0,&Nav_Load,Union::HookType::Hook_Detours);
void __fastcall Nav_Load(oCGame* self,void* v,int a,int b){QuestNav::LoadGuard guard;Nav_LoadHook(self,v,a,b);}
void __fastcall Nav_World(oCGame* self,void* v,int a,const zSTRING& b);
auto Nav_WorldHook=Union::CreateHook((void*)0x006C90B0,&Nav_World,Union::HookType::Hook_Detours);
void __fastcall Nav_World(oCGame* self,void* v,int a,const zSTRING& b){QuestNav::LoadGuard guard;Nav_WorldHook(self,v,a,b);}
void __fastcall Nav_Externals(oCGame* self,void* v,zCParser* p);
auto Nav_ExternalHook=Union::CreateHook((void*)0x006D4780,&Nav_Externals,Union::HookType::Hook_Detours);
void __fastcall Nav_Externals(oCGame* self,void* v,zCParser* p){Nav_ExternalHook(self,v,p);p->DefineExternal(zSTRING("GQN_SetTarget"),&QuestNav::SetTarget,zPAR_TYPE_VOID,zPAR_TYPE_STRING,zPAR_TYPE_STRING,zPAR_TYPE_STRING,zPAR_TYPE_VOID);}
}
#endif
HOOKSPACE(Global,true);

// For the trainer ("Navigovat" in its quest tab): track this diary topic, given
// as its title in the game's encoding. Called on the game thread, like the
// trainer's own UI. Returns 1 when it is a running quest the minimap now leads to.
extern "C" __declspec(dllexport) int __cdecl GothicQuestNav_Track(const char* rawTitle) {
#ifdef __G2A
 namespace G = Gothic_II_Addon;
 if(!rawTitle||!*rawTitle||G::QuestNav::failed||!G::ogame)return 0;
 bool running=false;
 for(auto* n=G::oCLogManager::GetLogManager().m_lstTopics.next;n&&!running;n=n->next) {
  auto* q=n->data;
  running=q&&q->m_enuSection==G::oCLogTopic::zELogSection_Missions&&q->m_enuStatus==G::oCLogTopic::zELogTopicStatus_Running
   &&std::strcmp(q->m_strDescription.ToChar(),rawTitle)==0;
 }
 if(!running)return 0;
 G::QuestNav::requested=rawTitle;
 G::QuestNav::visible=true;
 G::QuestNav::lastUpdate=0;
 return 1;
#else
 (void)rawTitle;
 return 0;
#endif
}
