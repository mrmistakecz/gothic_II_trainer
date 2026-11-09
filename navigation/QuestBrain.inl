// Included inside the QuestNav namespace.
//
// Objectives for every quest of the game. The scripts are analysed the same
// way as in the trainer's quest tab (src/Core/QuestScan.h): which dialogs belong
// to a diary topic, who completes it, which items it wants and who must die.
// The live game then decides what to do next for the tracked topic: a dialog
// the NPC offers right now, an item lying in the world, in a chest or carried
// by someone, a dialog that hands the item out, an enemy to kill, and finally
// the NPC who closes the quest.

namespace QS = ::GT::QuestScan;

// The parser seen through the analyzer's interface.
class ParserScripts : public QS::Scripts {
public:
 explicit ParserScripts(zCParser* p) : p_(p), count_(p ? p->symtab.table.GetNumInList() : 0) {}
 int Count() const override { return count_; }
 int Type(int i) const override { zCPar_Symbol* s = Sym(i); return s ? (int)s->type : -1; }
 unsigned Flags(int i) const override { zCPar_Symbol* s = Sym(i); return s ? (unsigned)s->flags : 0u; }
 int Elements(int i) const override { zCPar_Symbol* s = Sym(i); return s ? (int)s->ele : 0; }
 int Content(int i) const override { zCPar_Symbol* s = Sym(i); return s ? s->single_intdata : 0; }
 bool IntValue(int i, int& out) const override {
  zCPar_Symbol* s = Sym(i);
  if (!s || s->type != zPAR_TYPE_INT || (s->flags & zPAR_FLAG_CLASSVAR) || s->ele < 1) return false;
  s->GetValue(out, 0);
  return true;
 }
 bool StringValue(int i, std::string& out) const override {
  zCPar_Symbol* s = Sym(i);
  if (!s || s->type != zPAR_TYPE_STRING || (s->flags & zPAR_FLAG_CLASSVAR) || s->ele < 1 || !s->stringdata) return false;
  zSTRING value;
  s->GetValue(value, 0);
  out = value.ToChar();
  return true;
 }
 std::string Name(int i) const override { zCPar_Symbol* s = Sym(i); return s ? std::string(s->name.ToChar()) : std::string(); }
 int Find(const char* upperName) const override { return p_ ? p_->GetIndex(zSTRING(upperName)) : -1; }
 int BaseClass(int i) const override { return (p_ && i >= 0 && i < count_) ? p_->GetBaseClass(i) : -1; }
 const unsigned char* Code() const override { return p_ ? p_->stack.stack : nullptr; }
 int CodeSize() const override { return p_ ? p_->stack.stacksize : 0; }
private:
 zCPar_Symbol* Sym(int i) const { return (p_ && i >= 0 && i < count_) ? p_->GetSymbol(i) : nullptr; }
 zCParser* p_;
 int count_;
};

// Dialog conditions change the parser's current instances; put them back.
struct InstanceScope {
 zCParser* saved = parser;
 zCPar_Symbol* symbol = zCPar_Symbol::instance_sym;
 void* address = zCPar_Symbol::instance_adr;
 int instance = saved ? saved->instance : 0;
 int helper = saved ? saved->instance_help : 0;
 int indices[4] = { -1, -1, -1, -1 };
 void* addresses[4] = {};
 InstanceScope() {
  if (!saved) return;
  const char* names[] = { "SELF", "OTHER", "VICTIM", "ITEM" };
  for (int i = 0; i < 4; i++) {
   indices[i] = saved->GetIndex(zSTRING(names[i]));
   zCPar_Symbol* value = indices[i] >= 0 ? saved->GetSymbol(indices[i]) : nullptr;
   if (value && value->type == zPAR_TYPE_INSTANCE) addresses[i] = value->GetInstanceAdr();
   else indices[i] = -1;
  }
 }
 ~InstanceScope() {
  if (!saved || parser != saved) return;
  for (int i = 0; i < 4; i++) if (indices[i] >= 0) saved->SetInstance(indices[i], addresses[i]);
  saved->instance = instance;
  saved->instance_help = helper;
  zCPar_Symbol::instance_sym = symbol;
  zCPar_Symbol::instance_adr = address;
 }
};

// "instance X (C_Item) { name = "..."; }" compiles to PUSHVAR <string> PUSHVAR C_ITEM.NAME
// ASSIGNSTR: the name the game will show, without creating the item.
static bool AssignedString(int address, int member, std::string& out) {
 if (!parser || !parser->stack.stack || member < 0) return false;
 const unsigned char* code = parser->stack.stack;
 const int size = parser->stack.stacksize, symbols = parser->symtab.table.GetNumInList();
 if (address <= 0 || address >= size) return false;
 int pc = address, last = -1, prev = -1;
 for (int steps = 0; steps < 8192 && pc >= 0 && pc < size; steps++) {
  const unsigned char op = code[pc];
  int arg = 0;
  if (op == 61 || op == 62 || op == 64 || op == 65 || op == 67 || op == 75 || op == 76 || op == 80) {
   if (pc + 5 > size) return false;
   std::memcpy(&arg, code + pc + 1, 4);
   prev = last; last = op == 65 ? arg : -1; pc += 5;
  }
  else if (op == 245) {
   if (pc + 6 > size) return false;
   std::memcpy(&arg, code + pc + 1, 4);
   prev = last; last = code[pc + 5] == 0 ? arg : -1; pc += 6;
  }
  else if (op == 70) {
   if (last == member && prev >= 0 && prev < symbols) {
    zCPar_Symbol* value = parser->GetSymbol(prev);
    if (value && value->type == zPAR_TYPE_STRING && !(value->flags & zPAR_FLAG_CLASSVAR) && value->ele >= 1 && value->stringdata) {
     zSTRING raw;
     value->GetValue(raw, 0);
     std::string text = Utf(raw);
     if (!text.empty()) { out = text; return true; }
    }
   }
   prev = last = -1; pc += 1;
  }
  else if (op == 60) return false;
  else { prev = last = -1; pc += 1; }
 }
 return false;
}

static std::string StaticName(int instance, int member) {
 std::string name;
 zCPar_Symbol* sym = parser && instance >= 0 ? parser->GetSymbol(instance) : nullptr;
 if (!sym || sym->type != zPAR_TYPE_INSTANCE || member < 0) return name;
 if (AssignedString(sym->single_intdata, member, name)) return name;
 zCPar_Symbol* proto = sym->parent;
 for (int depth = 0; proto && depth < 8 && proto->type == zPAR_TYPE_PROTOTYPE; depth++) {
  if (AssignedString(proto->single_intdata, member, name)) return name;
  proto = proto->parent;
 }
 return std::string();
}

struct Brain {
 bool tried = false, ready = false;
 zCParser* owner = nullptr;
 int symbols = 0;
 QS::Model model;
 std::unordered_map<std::string, int> byRaw;          // diary title (game encoding) -> quest
 std::unordered_map<int, int> dialogOfInfo;           // C_INFO instance -> model.dialogs
 std::unordered_map<int, std::vector<int> > givers;   // item instance -> dialogs that hand it out
 std::unordered_map<int, std::string> worldOf;        // NPC instance -> world key of the startup script
 std::unordered_map<int, std::string> names;
 int npcName = -1, itemName = -1, gold = -1;
};
static Brain brain;

static std::string WorldKey() {
 std::string w = World();
 size_t dot = w.rfind('.');
 return dot == std::string::npos ? w : w.substr(0, dot);
}

static std::string WorldTitle(const std::string& key) {
 if (key == "NEWWORLD") return u8"Khorinis";
 if (key == "OLDWORLD") return u8"Hornické údolí";
 if (key == "ADDONWORLD") return u8"Jharkendar";
 if (key == "DRAGONISLAND") return u8"Dračí ostrov";
 return key;
}

static bool DebugDialog(const QS::Dialog& d) {
 zCPar_Symbol* npc = d.npc >= 0 ? parser->GetSymbol(d.npc) : nullptr;
 zCPar_Symbol* info = d.info >= 0 ? parser->GetSymbol(d.info) : nullptr;
 if (!npc || !info) return true;
 std::string name = npc->name.ToChar();
 for (const char* debug : { "SH", "SH_OLDWORLD", "PC_ROCKEFELLER", "PC_LEVELINSPEKTOR" })
  if (name == debug) return true;
 return std::string(info->name.ToChar()).compare(0, 11, "STORYHELPER") == 0;
}

// Game thread, a few dozen milliseconds once per game start.
static void BuildBrain() {
 if (brain.tried && parser && brain.owner == parser && brain.symbols == parser->symtab.table.GetNumInList()) return;
 brain = Brain();
 brain.tried = true;
 brain.owner = parser;
 if (!parser || !ogame) return;
 brain.symbols = parser->symtab.table.GetNumInList();
 oCInfoManager* infos = ogame->GetInfoManager();
 if (!infos) return;
 const DWORD started = GetTickCount();
 std::vector<QS::Dialog> dialogs;
 for (auto* node = infos->infoList.next; node; node = node->next) {
  oCInfo* info = node->data;
  if (!info) continue;
  QS::Dialog d;
  d.info = info->instance;
  d.npc = info->pd.npc;
  d.conditions = info->pd.conditions;
  d.information = info->pd.information;
  d.important = info->pd.important != 0;
  d.permanent = info->pd.permanent != 0;
  d.line = info->pd.description.ToChar();
  if (!DebugDialog(d)) dialogs.push_back(d);
 }
 std::vector<std::string> worlds = { "NEWWORLD", "OLDWORLD", "ADDONWORLD", "DRAGONISLAND" };
 const std::string current = WorldKey();
 if (!current.empty() && std::find(worlds.begin(), worlds.end(), current) == worlds.end()) worlds.push_back(current);
 ParserScripts scripts(parser);
 brain.model = QS::Build(scripts, dialogs, worlds);
 for (size_t i = 0; i < brain.model.quests.size(); i++)
  if (!brain.model.quests[i].raw.empty() && !brain.byRaw.count(brain.model.quests[i].raw)) brain.byRaw[brain.model.quests[i].raw] = (int)i;
 for (size_t i = 0; i < brain.model.dialogs.size(); i++) {
  const QS::DialogFacts& f = brain.model.dialogs[i];
  brain.dialogOfInfo[f.dialog.info] = (int)i;
  for (const QS::Amount& a : f.rewards) brain.givers[a.item].push_back((int)i);
  for (const QS::Amount& a : f.choiceRewards) brain.givers[a.item].push_back((int)i);
 }
 for (const std::pair<int, std::string>& w : brain.model.npcWorlds) brain.worldOf[w.first] = w.second;
 brain.npcName = parser->GetIndex(zSTRING("C_NPC.NAME"));
 brain.itemName = parser->GetIndex(zSTRING("C_ITEM.NAME"));
 brain.gold = parser->GetIndex(zSTRING("ITMI_GOLD"));
 brain.ready = true;
 Log("QuestNav %s: %u quests, %u dialogs, %u ms", kVersion, (unsigned)brain.model.quests.size(), (unsigned)brain.model.dialogs.size(), (unsigned)(GetTickCount() - started));
}

static std::string ItemName(int sym) {
 std::unordered_map<int, std::string>::const_iterator it = brain.names.find(sym);
 if (it != brain.names.end()) return it->second;
 std::string name = StaticName(sym, brain.itemName);
 if (name.empty()) { zCPar_Symbol* s = parser->GetSymbol(sym); name = s ? s->name.ToChar() : "?"; }
 brain.names[sym] = name;
 return name;
}

static std::string NpcName(oCNpc* npc) {
 std::string name = Utf(npc->GetName(0));
 return name.empty() ? Utf(npc->GetInstanceName()) : name;
}

// ------------------------------------------------------------------ objectives

enum : int { kGoalFinish, kGoalTalk, kGoalFind, kGoalChest, kGoalCarrier, kGoalSource, kGoalKill, kGoalFinisher, kGoalManual };

struct Objective {
 int kind = kGoalTalk;
 int tier = 0;        // 0 complete now, 1 talk now, 2 get what is missing, 3 who closes it
 zVEC3 pos;
 std::string text;    // UTF-8
 float metres = 0;
};
static std::vector<Objective> objectives;
static std::string objectiveNote;   // what cannot be shown on the map (another world, gold to have)
static std::vector<oCMobContainer*> chests;
static bool chestsRead = false;

static void ResetObjectives() {
 objectives.clear();
 objectiveNote.clear();
 chests.clear();
 chestsRead = false;
}

static float Distance2(const zVEC3& a, const zVEC3& b) {
 float dx = a.n[0] - b.n[0], dy = a.n[1] - b.n[1], dz = a.n[2] - b.n[2];
 return dx * dx + dy * dy + dz * dz;
}

// Where a character really is. Characters far from the hero sleep outside
// the world tree with a stale position (often the world origin, in Khorinis
// the harbour); the game wakes them at the place of their daily routine.
// Characters parked on the waypoint TOT are out of the story.
static bool NearOrigin(const zVEC3& p) { return fabsf(p.n[0]) < 1.0f && fabsf(p.n[1]) < 1.0f && fabsf(p.n[2]) < 1.0f; }
static bool NamedSpot(const zSTRING& name, zVEC3& pos) {
 oCWorld* world = ogame->GetGameWorld();
 if (!world || name.IsEmpty()) return false;
 if (world->wayNet) if (zCWaypoint* wp = world->wayNet->GetWaypoint(name)) { pos = wp->pos; return true; }
 if (zCVob* vob = world->zCWorld::SearchVobByName(name)) { pos = vob->GetPositionWorld(); return true; }
 return false;
}
static bool Where(oCNpc* npc, zVEC3& pos) {
 zSTRING wp = npc->wpname;
 wp.Upper();
 if (wp == "TOT") return false;
 if (npc->GetHomeWorld()) { pos = npc->GetPositionWorld(); return !NearOrigin(pos); }
 pos = rtnMan ? rtnMan->GetRoutinePos(npc) : zVEC3(0.0f, 0.0f, 0.0f);
 if (NearOrigin(pos) && !NamedSpot(wp, pos)) { zSTRING spawn = npc->spawnPoint; spawn.Upper(); NamedSpot(spawn, pos); }
 return !NearOrigin(pos);
}

static std::unordered_map<int, oCNpc*> NpcsHere() {
 std::unordered_map<int, oCNpc*> map;
 auto* list = ogame->GetGameWorld()->voblist_npcs;
 for (auto* n = list ? list->next : nullptr; n; n = n->next) {
  oCNpc* npc = n->data;
  if (!npc || npc == player) continue;
  const int instance = npc->GetInstance();
  std::unordered_map<int, oCNpc*>::iterator it = map.find(instance);
  if (it == map.end()) map[instance] = npc;
  else if (it->second->IsDead() && !npc->IsDead()) it->second = npc;
 }
 return map;
}

static std::unordered_map<int, oCInfo*> InfosNow() {
 std::unordered_map<int, oCInfo*> map;
 oCInfoManager* infos = ogame->GetInfoManager();
 for (auto* node = infos ? infos->infoList.next : nullptr; node; node = node->next)
  if (node->data) map[node->data->instance] = node->data;
 return map;
}

// The same check the game makes before it offers the dialog.
static bool Offered(oCInfo* info, oCNpc* npc) {
 const int self = parser->GetIndex(zSTRING("SELF")), other = parser->GetIndex(zSTRING("OTHER"));
 if (self < 0 || other < 0) return false;
 InstanceScope scope;
 parser->SetInstance(self, npc);
 parser->SetInstance(other, player);
 return info->InfoConditions() == 1;
}

static void ReadChests() {
 if (chestsRead) return;
 chestsRead = true;
 auto* list = ogame->GetGameWorld()->voblist;
 for (auto* n = list ? list->next : nullptr; n; n = n->next)
  if (oCMobContainer* chest = zDYNAMIC_CAST<oCMobContainer>(n->data)) chests.push_back(chest);
}

static bool NearestWorldItem(int sym, const zVEC3& hero, zVEC3& pos) {
 auto* list = ogame->GetGameWorld()->voblist_items;
 float best = 1.0e30f;
 bool found = false;
 for (auto* n = list ? list->next : nullptr; n; n = n->next) {
  oCItem* item = n->data;
  if (!item || item->GetInstance() != sym) continue;
  zVEC3 p = item->GetPositionWorld();
  float d = Distance2(p, hero);
  if (d < best) { best = d; pos = p; found = true; }
 }
 return found;
}

static bool NearestChest(int sym, const zVEC3& hero, zVEC3& pos) {
 ReadChests();
 float best = 1.0e30f;
 bool found = false;
 for (oCMobContainer* chest : chests) {
  bool has = false;
  for (auto* n = chest->containList.next; n && !has; n = n->next) has = n->data && n->data->GetInstance() == sym;
  if (!has) continue;
  zVEC3 p = chest->GetPositionWorld();
  float d = Distance2(p, hero);
  if (d < best) { best = d; pos = p; found = true; }
 }
 return found;
}

static oCNpc* NearestCarrier(int sym, const zVEC3& hero) {
 auto* list = ogame->GetGameWorld()->voblist_npcs;
 oCNpc* best = nullptr;
 float bestDistance = 1.0e30f;
 for (auto* n = list ? list->next : nullptr; n; n = n->next) {
  oCNpc* npc = n->data;
  zVEC3 at;
  if (!npc || npc == player || npc->inventory2.GetAmount(sym) <= 0 || !Where(npc, at)) continue;
  float d = Distance2(at, hero);
  if (d < bestDistance) { bestDistance = d; best = npc; }
 }
 return best;
}

// The completing dialog that fits best: its NPC alive here, not played yet,
// a plain success before one that can also end the quest badly.
static int BestStep(const QS::Quest& q, const std::unordered_map<int, oCNpc*>& npcs, const std::unordered_map<int, oCInfo*>& infos) {
 int best = -1, bestScore = -1;
 for (size_t i = 0; i < q.steps.size(); i++) {
  if (!(q.steps[i].roles & QS::kRoleSuccess)) continue;
  const QS::DialogFacts& f = brain.model.dialogs[q.steps[i].dialog];
  std::unordered_map<int, oCNpc*>::const_iterator npc = npcs.find(f.dialog.npc);
  std::unordered_map<int, oCInfo*>::const_iterator info = infos.find(f.dialog.info);
  const bool here = npc != npcs.end() && !npc->second->IsDead();
  const bool open = info != infos.end() && (!info->second->told || f.dialog.permanent);
  const bool clean = !(q.steps[i].roles & QS::kRoleFail);
  const int score = (here ? 4 : 0) + (open ? 2 : 0) + (clean ? 1 : 0);
  if (score > bestScore) { bestScore = score; best = (int)i; }
 }
 return best;
}

static std::string Count(int have, int need) {
 if (need <= 1) return std::string();
 char text[32];
 snprintf(text, sizeof(text), " (%d/%d)", have, need);
 return text;
}

static void ResolveObjectives(const std::string& raw) {
 objectives.clear();
 objectiveNote.clear();
 BuildBrain();
 if (!brain.ready) return;
 std::unordered_map<std::string, int>::const_iterator found = brain.byRaw.find(raw);
 if (found == brain.byRaw.end()) return;
 const QS::Quest& q = brain.model.quests[found->second];
 const std::unordered_map<int, oCNpc*> npcs = NpcsHere();
 const std::unordered_map<int, oCInfo*> infos = InfosNow();
 const zVEC3 hero = player->GetPositionWorld();
 const std::string here = WorldKey();
 std::string elsewhere;
 auto add = [&](int kind, int tier, const zVEC3& pos, const std::string& text) {
  for (const Objective& o : objectives) if (o.text == text && Distance2(o.pos, pos) < 1.0f) return;
  Objective o;
  o.kind = kind; o.tier = tier; o.pos = pos; o.text = text;
  o.metres = sqrtf(Distance2(hero, pos)) / 100.0f;
  objectives.push_back(o);
 };
 auto alive = [&](int sym) -> oCNpc* {
  std::unordered_map<int, oCNpc*>::const_iterator it = npcs.find(sym);
  return it != npcs.end() && !it->second->IsDead() ? it->second : nullptr;
 };
 auto noteWorld = [&](int sym) {
  if (!elsewhere.empty() || npcs.count(sym)) return;
  std::unordered_map<int, std::string>::const_iterator w = brain.worldOf.find(sym);
  if (w != brain.worldOf.end() && w->second != here) elsewhere = w->second;
 };
 auto info = [&](int sym) -> oCInfo* {
  std::unordered_map<int, oCInfo*>::const_iterator it = infos.find(sym);
  return it != infos.end() ? it->second : nullptr;
 };
 auto open = [&](int sym, bool permanent) { oCInfo* i = info(sym); return i && (!i->told || permanent); };

 // 1. what the NPCs of this quest offer right now
 for (const QS::Step& s : q.steps) {
  if ((s.roles & QS::kRoleFail) && !(s.roles & QS::kRoleSuccess)) continue;   // never lead into failing it
  const QS::DialogFacts& f = brain.model.dialogs[s.dialog];
  if (!open(f.dialog.info, f.dialog.permanent)) continue;
  oCNpc* npc = alive(f.dialog.npc);
  if (!npc) { noteWorld(f.dialog.npc); continue; }
  if (!Offered(info(f.dialog.info), npc)) continue;
  const bool finish = (s.roles & QS::kRoleSuccess) != 0;
  const bool clean = !(s.roles & QS::kRoleFail);
  zVEC3 at;
  if (Where(npc, at)) add(finish ? kGoalFinish : kGoalTalk, finish && clean ? 0 : 1, at, (finish ? u8"Odevzdej: " : u8"Promluv si: ") + NpcName(npc));
 }

 // 2. what the completing dialog (or, without one, a diary step) still needs
 if (objectives.empty()) {
  std::vector<int> wanting;
  const int step = BestStep(q, npcs, infos);
  if (step >= 0) wanting.push_back(q.steps[step].dialog);
  else
   for (const QS::Step& s : q.steps) {
    const QS::DialogFacts& f = brain.model.dialogs[s.dialog];
    if (!(s.roles & QS::kRoleFail) && !f.needs.empty() && open(f.dialog.info, f.dialog.permanent)) wanting.push_back(s.dialog);
   }
  for (int dialog : wanting) {
   const QS::DialogFacts& f = brain.model.dialogs[dialog];
   // items, an "any of" group counts as one need
   std::vector<int> doneGroups;
   for (const QS::Amount& a : f.needs)
    if (a.group > 0 && player->inventory2.GetAmount(a.item) >= a.amount) doneGroups.push_back(a.group);
   for (const QS::Amount& a : f.needs) {
    const int have = player->inventory2.GetAmount(a.item);
    if (have >= a.amount || (a.group > 0 && std::find(doneGroups.begin(), doneGroups.end(), a.group) != doneGroups.end())) continue;
    const std::string name = ItemName(a.item) + Count(have, a.amount);
    zVEC3 pos;
    if (a.item == brain.gold) {
     char text[96];
     snprintf(text, sizeof(text), u8"Potřebuješ %d zlatých (máš %d)", a.amount, have);
     if (objectiveNote.empty()) objectiveNote = text;
     continue;
    }
    bool shown = false;
    if (NearestWorldItem(a.item, hero, pos)) { add(kGoalFind, 2, pos, u8"Najdi: " + name); shown = true; }
    if (NearestChest(a.item, hero, pos)) { add(kGoalChest, 2, pos, u8"V truhle: " + name); shown = true; }
    if (oCNpc* carrier = NearestCarrier(a.item, hero)) {
     zVEC3 at;
     if (Where(carrier, at)) { add(kGoalCarrier, 2, at, NpcName(carrier) + u8" má: " + name); shown = true; }
    }
    std::unordered_map<int, std::vector<int> >::const_iterator g = brain.givers.find(a.item);
    if (g != brain.givers.end())
     for (int giver : g->second) {
      const QS::DialogFacts& source = brain.model.dialogs[giver];
      if (!open(source.dialog.info, source.dialog.permanent)) continue;
      oCNpc* npc = alive(source.dialog.npc);
      if (!npc) { noteWorld(source.dialog.npc); continue; }
      const bool now = Offered(info(source.dialog.info), npc);
      zVEC3 at;
      if (!Where(npc, at)) continue;
      add(kGoalSource, now ? 1 : 2, at, (now ? u8"Promluv si: " : u8"Dá ti to: ") + NpcName(npc));
      shown = true;
     }
    if (!shown && objectiveNote.empty()) objectiveNote = u8"Potřebuješ: " + name;
   }
   // who must die
   for (int target : f.mustBeDead) {
    zVEC3 at;
    if (oCNpc* npc = alive(target)) { if (Where(npc, at)) add(kGoalKill, 2, at, u8"Zabij: " + NpcName(npc)); }
    else noteWorld(target);
   }
   // dialogs that have to come first
   auto talkFirst = [&](int sym) {
    std::unordered_map<int, int>::const_iterator d = brain.dialogOfInfo.find(sym);
    oCInfo* i = info(sym);
    if (d == brain.dialogOfInfo.end() || !i || i->told) return;
    const int npcSym = brain.model.dialogs[d->second].dialog.npc;
    zVEC3 at;
    if (oCNpc* npc = alive(npcSym)) { if (Where(npc, at)) add(kGoalTalk, 2, at, u8"Promluv si: " + NpcName(npc)); }
    else noteWorld(npcSym);
   };
   for (int sym : f.needsDialogs) talkFirst(sym);
   bool anyTold = false;
   for (int sym : f.anyDialogs) { oCInfo* i = info(sym); anyTold = anyTold || (i && i->told); }
   if (!anyTold) for (int sym : f.anyDialogs) talkFirst(sym);
   // and who closes it
   zVEC3 at;
   if (oCNpc* npc = alive(f.dialog.npc)) { if (Where(npc, at)) add(kGoalFinisher, 3, at, u8"Úkol uzavírá: " + NpcName(npc)); }
   else noteWorld(f.dialog.npc);
  }
 }
 std::stable_sort(objectives.begin(), objectives.end(), [](const Objective& a, const Objective& b) {
  return a.tier != b.tier ? a.tier < b.tier : a.metres < b.metres;
 });
 if (objectives.size() > 16) objectives.resize(16);
 if (objectives.empty() && !elsewhere.empty()) objectiveNote = u8"Cíl je v jiném světě: " + WorldTitle(elsewhere);
}
