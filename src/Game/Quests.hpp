// Engine side of the quest tab: feeds the analyzer (Core/QuestScan.h) with the
// parser and the game's dialogs, keeps a snapshot of the diary and the live
// state of the selected quest, and performs the quest actions. Everything that
// touches the engine runs on the game thread (queued actions).

namespace GOTHIC_NAMESPACE {
namespace Trainer {

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
			if (!s || s->type != zPAR_TYPE_INT || (s->flags & zPAR_FLAG_CLASSVAR) || s->ele < 1)
				return false;
			s->GetValue(out, 0);
			return true;
		}

		bool StringValue(int i, std::string& out) const override {
			zCPar_Symbol* s = Sym(i);
			if (!s || s->type != zPAR_TYPE_STRING || (s->flags & zPAR_FLAG_CLASSVAR) || s->ele < 1 || !s->stringdata)
				return false;
			zSTRING value;
			s->GetValue(value, 0);
			out = value.ToChar();
			return true;
		}

		std::string Name(int i) const override {
			zCPar_Symbol* s = Sym(i);
			return s ? std::string(s->name.ToChar()) : std::string();
		}

		int Find(const char* upperName) const override { return p_ ? p_->GetIndex(zSTRING(upperName)) : -1; }
		int BaseClass(int i) const override { return (p_ && i >= 0 && i < count_) ? p_->GetBaseClass(i) : -1; }
		const unsigned char* Code() const override { return p_ ? p_->stack.stack : nullptr; }
		int CodeSize() const override { return p_ ? p_->stack.stacksize : 0; }

	private:
		zCPar_Symbol* Sym(int i) const { return (p_ && i >= 0 && i < count_) ? p_->GetSymbol(i) : nullptr; }
		zCParser* p_;
		int count_;
	};

	// ------------------------------------------------------------------ database

	struct QuestText {
		std::string title;      // UTF-8
		std::string folded;
		std::vector<std::string> texts;
	};

	struct DialogText {
		std::string npcName;    // UTF-8
		std::string line;       // UTF-8, empty for dialogs the NPC starts himself
		std::string world;      // world key from the startup scripts, may be empty
	};

	struct QuestDatabase {
		QS::Model model;
		std::vector<QuestText> quests;                  // parallel to model.quests
		std::vector<DialogText> dialogs;                // parallel to model.dialogs
		std::unordered_map<int, std::string> names;     // NPC and item instances -> UTF-8 name
		std::unordered_map<std::string, int> byRaw;     // raw title -> quest
		std::unordered_map<int, std::string> worldOf;   // NPC instance -> world key
		std::unordered_map<int, int> dialogOfInfo;      // C_INFO instance -> model.dialogs index
		std::unordered_map<int, std::vector<int> > givers;   // item instance -> dialogs that hand it out
		int itemClass = -1;
		int goldItem = -1;                              // ITMI_GOLD
		zCParser* parser = nullptr;
		int symbols = 0;
		bool ready = false;
		bool building = false;
	};

	static QuestDatabase g_questDb;

	static std::string QuestName(int sym) {
		std::unordered_map<int, std::string>::const_iterator it = g_questDb.names.find(sym);
		return it != g_questDb.names.end() ? it->second : std::string(u8"různé postavy");
	}

	static bool IsDebugDialog(const QS::Dialog& d) {
		if (!parser)
			return true;
		zCPar_Symbol* npc = d.npc >= 0 ? parser->GetSymbol(d.npc) : nullptr;
		zCPar_Symbol* info = d.info >= 0 ? parser->GetSymbol(d.info) : nullptr;
		if (!npc || !info)
			return true;
		const char* debugNpcs[] = { "SH", "SH_OLDWORLD", "PC_ROCKEFELLER", "PC_LEVELINSPEKTOR" };
		std::string npcName = npc->name.ToChar();
		for (const char* n : debugNpcs)
			if (npcName == n)
				return true;
		return std::string(info->name.ToChar()).compare(0, 11, "STORYHELPER") == 0;
	}

	// Game thread, takes a few dozen milliseconds once per game start.
	static void BuildQuestDatabase() {
		struct FinishBuild { ~FinishBuild() { g_questDb.building = false; } } finish;
		QuestDatabase& db = g_questDb;
		db.ready = false;
		db.quests.clear();
		db.dialogs.clear();
		db.names.clear();
		db.byRaw.clear();
		db.worldOf.clear();
		db.dialogOfInfo.clear();
		db.givers.clear();
		if (!parser || !ogame)
			return;
		oCInfoManager* infos = ogame->GetInfoManager();
		if (!infos)
			return;
		const DWORD started = GetTickCount();

		std::vector<QS::Dialog> dialogs;
		for (auto* node = infos->infoList.next; node; node = node->next) {
			oCInfo* info = node->data;
			if (!info)
				continue;
			QS::Dialog d;
			d.info = info->instance;
			d.npc = info->pd.npc;
			d.conditions = info->pd.conditions;
			d.information = info->pd.information;
			d.important = info->pd.important != 0;
			d.permanent = info->pd.permanent != 0;
			d.line = info->pd.description.ToChar();
			if (!IsDebugDialog(d))
				dialogs.push_back(d);
		}

		std::vector<std::string> worlds;
		worlds.push_back("NEWWORLD");
		worlds.push_back("OLDWORLD");
		worlds.push_back("ADDONWORLD");
		worlds.push_back("DRAGONISLAND");
		std::string current = WorldKeyFromPath(CurrentWorldKey());
		if (!current.empty() && std::find(worlds.begin(), worlds.end(), current) == worlds.end())
			worlds.push_back(current);

		ParserScripts scripts(parser);
		db.model = QS::Build(scripts, dialogs, worlds);

		// readable texts
		const int npcName = parser->GetIndex(zSTRING("C_NPC.NAME"));
		const int itemName = parser->GetIndex(zSTRING("C_ITEM.NAME"));
		db.itemClass = parser->GetIndex(zSTRING("C_ITEM"));
		db.goldItem = parser->GetIndex(zSTRING("ITMI_GOLD"));
		auto nameOf = [&](int sym, bool item) {
			if (sym < 0 || db.names.count(sym))
				return;
			std::string name = ScriptInstanceString(sym, item ? itemName : npcName);
			if (name.empty()) {
				zCPar_Symbol* s = parser->GetSymbol(sym);
				std::string code = s ? std::string(s->name.ToChar()) : std::string("?");
				name = item ? code : NpcLabel(code, LooksLikePerson(code));
			}
			db.names[sym] = name;
		};
		db.quests.reserve(db.model.quests.size());
		for (size_t i = 0; i < db.model.quests.size(); i++) {
			const QS::Quest& q = db.model.quests[i];
			QuestText t;
			t.title = ::GT::Text::FromGame(q.raw.c_str());
			if (t.title.empty())
				t.title = scripts.Name(q.topic);
			t.folded = ::GT::Text::Fold(t.title);
			for (const std::string& text : q.texts)
				t.texts.push_back(::GT::Text::FromGame(text.c_str()));
			db.quests.push_back(t);
			if (!q.raw.empty() && !db.byRaw.count(q.raw))
				db.byRaw[q.raw] = (int)i;
		}
		for (const std::pair<int, std::string>& w : db.model.npcWorlds)
			db.worldOf[w.first] = w.second;
		db.dialogs.reserve(db.model.dialogs.size());
		for (size_t i = 0; i < db.model.dialogs.size(); i++) {
			const QS::DialogFacts& f = db.model.dialogs[i];
			db.dialogOfInfo[f.dialog.info] = (int)i;
			for (const QS::Amount& a : f.rewards)
				db.givers[a.item].push_back((int)i);
			for (const QS::Amount& a : f.choiceRewards)
				db.givers[a.item].push_back((int)i);
		}
		for (const QS::DialogFacts& f : db.model.dialogs) {
			nameOf(f.dialog.npc, false);
			for (int npc : f.mustBeDead)
				nameOf(npc, false);
			for (const QS::Amount& a : f.needs)
				nameOf(a.item, true);
			for (const QS::Amount& a : f.rewards)
				nameOf(a.item, true);
			for (const QS::Amount& a : f.handIn)
				nameOf(a.item, true);
			for (const QS::Amount& a : f.choiceRewards)
				nameOf(a.item, true);
			DialogText t;
			t.npcName = QuestName(f.dialog.npc);
			t.line = ::GT::Text::FromGame(f.dialog.line.c_str());
			std::unordered_map<int, std::string>::const_iterator w = db.worldOf.find(f.dialog.npc);
			if (w != db.worldOf.end())
				t.world = w->second;
			db.dialogs.push_back(t);
		}

		db.parser = parser;
		db.symbols = parser->symtab.table.GetNumInList();
		db.ready = true;
		::GT::Log::Write("Quest database: %d topics, %d dialogs, %d functions walked, %u ms",
			(int)db.model.quests.size(), (int)db.model.dialogs.size(), db.model.analyzedFunctions, (unsigned)(GetTickCount() - started));
	}

	// ------------------------------------------------------------------ diary

	struct DiaryTopic {
		std::string raw;        // game encoding, matches QS::Quest::raw
		std::string title;      // UTF-8
		int status = 0;         // oCLogTopic::zELogTopicStatus
		int section = 0;
		std::vector<std::string> entries;
	};

	struct DiaryState {
		std::vector<DiaryTopic> topics;
		std::unordered_map<std::string, int> byRaw;
		bool ready = false;
		bool pending = false;
		double nextRefresh = 0.0;
	};

	static DiaryState g_diary;

	static void RefreshDiary() {
		struct Finish { ~Finish() { g_diary.pending = false; } } finish;
		std::vector<DiaryTopic> topics;
		std::unordered_map<std::string, int> byRaw;
		oCLogManager& log = oCLogManager::GetLogManager();
		for (auto* node = log.m_lstTopics.next; node; node = node->next) {
			oCLogTopic* t = node->data;
			if (!t)
				continue;
			DiaryTopic d;
			d.raw = t->m_strDescription.ToChar();
			d.title = FromZ(t->m_strDescription);
			d.status = (int)t->m_enuStatus;
			d.section = (int)t->m_enuSection;
			for (auto* e = t->m_lstEntries.next; e; e = e->next)
				if (e->data)
					d.entries.push_back(FromZ(e->data->m_strDescription));
			if (!byRaw.count(d.raw))
				byRaw[d.raw] = (int)topics.size();
			topics.push_back(std::move(d));
		}
		g_diary.topics.swap(topics);
		g_diary.byRaw.swap(byRaw);
		g_diary.ready = true;
	}

	static const DiaryTopic* DiaryFor(const std::string& raw) {
		std::unordered_map<std::string, int>::const_iterator it = g_diary.byRaw.find(raw);
		return it != g_diary.byRaw.end() ? &g_diary.topics[it->second] : nullptr;
	}

	// Game thread: status of a topic in the diary, -1 when it is not there.
	static int DiaryStatusNow(const std::string& raw) {
		oCLogManager& log = oCLogManager::GetLogManager();
		for (auto* node = log.m_lstTopics.next; node; node = node->next) {
			oCLogTopic* t = node->data;
			if (t && raw == t->m_strDescription.ToChar())
				return (int)t->m_enuStatus;
		}
		return -1;
	}

	// Game thread.
	static bool SetDiaryStatus(const std::string& raw, int status) {
		oCLogManager& log = oCLogManager::GetLogManager();
		for (auto* node = log.m_lstTopics.next; node; node = node->next) {
			oCLogTopic* t = node->data;
			if (t && raw == t->m_strDescription.ToChar()) {
				t->m_enuStatus = (oCLogTopic::zELogTopicStatus)status;
				return true;
			}
		}
		return false;
	}

	// ------------------------------------------------------------------ live state of one quest

	struct QuestStepLive {
		bool known = false;      // the game has this dialog
		bool told = false;       // already talked through
		bool available = false;  // the hero could pick it right now
		bool here = false;       // the NPC is in this world
		bool dead = false;
		float distance = 0.0f;
		int place = kNpcHere;    // NpcPlaceState
	};

	struct QuestLiveState {
		int quest = -1;
		std::vector<QuestStepLive> steps;
		std::vector<int> values;                       // of the mission variables
		std::unordered_map<int, int> carried;          // item -> pieces the hero has
		std::unordered_map<int, int> npcState;         // NPC -> 0 alive here, 1 dead here, -1 not here
		std::unordered_map<int, int> dialogTold;       // C_INFO instance -> 1 told, 0 not yet
		int chapter = 0;
		int status = -1;                               // in the diary now, -1 not there
		bool pending = false;
		double nextRefresh = 0.0;
	};

	static QuestLiveState g_questLive;

	// Instance -> NPC in this world (the living one if there are more).
	static std::unordered_map<int, oCNpc*> WorldNpcsByInstance() {
		std::unordered_map<int, oCNpc*> map;
		ForEachWorldNpc([&](oCNpc* npc) {
			int instance = npc->GetInstance();
			std::unordered_map<int, oCNpc*>::iterator it = map.find(instance);
			if (it == map.end())
				map[instance] = npc;
			else if (it->second->IsDead() && !npc->IsDead())
				it->second = npc;
		});
		return map;
	}

	static oCNpc* FindNpcByInstance(int instance) {
		std::unordered_map<int, oCNpc*> map = WorldNpcsByInstance();
		std::unordered_map<int, oCNpc*>::iterator it = map.find(instance);
		return it != map.end() ? it->second : nullptr;
	}

	// C_INFO instance -> the game's dialog object.
	static std::unordered_map<int, oCInfo*> InfosByInstance() {
		std::unordered_map<int, oCInfo*> map;
		oCInfoManager* infos = ogame ? ogame->GetInfoManager() : nullptr;
		if (!infos)
			return map;
		for (auto* node = infos->infoList.next; node; node = node->next)
			if (node->data)
				map[node->data->instance] = node->data;
		return map;
	}

	static oCInfo* InfoIn(const std::unordered_map<int, oCInfo*>& infos, int instance) {
		std::unordered_map<int, oCInfo*>::const_iterator it = infos.find(instance);
		return it != infos.end() ? it->second : nullptr;
	}

	static bool IsItemInstance(int sym) {
		return parser && sym >= 0 && g_questDb.itemClass >= 0 && sym < parser->symtab.table.GetNumInList()
			&& parser->GetBaseClass(sym) == g_questDb.itemClass;
	}

	static void RefreshQuestLive(int questIndex) {
		struct Finish { ~Finish() { g_questLive.pending = false; } } finish;
		const QuestDatabase& db = g_questDb;
		if (!db.ready || !InGame() || questIndex < 0 || questIndex >= (int)db.model.quests.size())
			return;
		const QS::Quest& quest = db.model.quests[questIndex];
		QuestLiveState live;
		live.quest = questIndex;
		live.nextRefresh = g_questLive.nextRefresh;
		std::unordered_map<int, oCNpc*> npcs = WorldNpcsByInstance();
		std::unordered_map<int, oCInfo*> infos = InfosByInstance();
		const int selfIndex = ScriptSymbolIndex("SELF");
		const int otherIndex = ScriptSymbolIndex("OTHER");
		const zVEC3 here = player->GetPositionWorld();
		for (const QS::Step& step : quest.steps) {
			const QS::DialogFacts& f = db.model.dialogs[step.dialog];
			QuestStepLive s;
			oCInfo* info = InfoIn(infos, f.dialog.info);
			s.known = info != nullptr;
			s.told = info && info->told;
			std::unordered_map<int, oCNpc*>::iterator it = npcs.find(f.dialog.npc);
			oCNpc* npc = it != npcs.end() ? it->second : nullptr;
			s.here = npc != nullptr;
			if (npc) {
				s.dead = npc->IsDead() != 0;
				const NpcPlace place = WhereIsNpc(npc);
				s.place = place.state;
				s.distance = place.Usable() ? DistanceBetween(here, place.pos) : -1.0f;
			}
			if (info && npc && !s.dead && (!s.told || f.dialog.permanent) && selfIndex >= 0 && otherIndex >= 0) {
				// the same check the game makes before it offers the dialog
				ScriptInstanceScope scope;
				parser->SetInstance(selfIndex, npc);
				parser->SetInstance(otherIndex, player);
				s.available = info->InfoConditions() == 1;
			}
			live.steps.push_back(s);

			for (const QS::Amount& a : f.needs)
				if (IsItemInstance(a.item))
					live.carried[a.item] = player->inventory2.GetAmount(a.item);
			for (const QS::Amount& a : f.handIn)
				if (IsItemInstance(a.item))
					live.carried[a.item] = player->inventory2.GetAmount(a.item);
			for (int target : f.mustBeDead) {
				std::unordered_map<int, oCNpc*>::iterator t = npcs.find(target);
				live.npcState[target] = t == npcs.end() ? -1 : (t->second->IsDead() ? 1 : 0);
			}
			for (const std::vector<int>* list : { &f.needsDialogs, &f.anyDialogs })
				for (int sym : *list) {
					oCInfo* need = InfoIn(infos, sym);
					if (need)
						live.dialogTold[sym] = need->told ? 1 : 0;
				}
		}
		live.status = DiaryStatusNow(quest.raw);
		for (int var : quest.missionVars) {
			int value = 0;
			zCPar_Symbol* sym = parser->GetSymbol(var);
			if (sym)
				sym->GetValue(value, 0);
			live.values.push_back(value);
		}
		if (db.model.chapterVar >= 0) {
			zCPar_Symbol* sym = parser->GetSymbol(db.model.chapterVar);
			if (sym)
				sym->GetValue(live.chapter, 0);
		}
		g_questLive = std::move(live);
	}

	// ------------------------------------------------------------------ actions

	static zVEC3 InFrontOf(oCNpc* npc) {
		return MeetingSpot(npc, WhereIsNpc(npc));
	}

	static std::string NotHereText(int npcSym, const std::string& name) {
		std::unordered_map<int, std::string>::const_iterator w = g_questDb.worldOf.find(npcSym);
		if (w != g_questDb.worldOf.end() && w->second != WorldKeyFromPath(CurrentWorldKey()))
			return name + u8" tu není (nejspíš " + WorldDisplayName(w->second) + u8")";
		return name + u8" teď v tomto světě není";
	}

	// Game thread: gives the hero what is missing up to the wanted amounts.
	static int GiveMissingItems(const std::vector<QS::Amount>& items) {
		int given = 0;
		for (const QS::Amount& a : items) {
			if (!IsItemInstance(a.item) || a.amount <= 0)
				continue;
			int have = player->inventory2.GetAmount(a.item);
			if (have < a.amount) {
				player->CreateItems(a.item, a.amount - have);
				given++;
			}
		}
		return given;
	}

	// Everything a completing dialog asks for: required items, one of each
	// "any of" group and what the hero hands over in the dialog.
	static std::vector<QS::Amount> ItemsFor(const QS::DialogFacts& f) {
		std::vector<QS::Amount> out;
		std::vector<int> groups;
		auto add = [&](const QS::Amount& a) {
			for (QS::Amount& o : out)
				if (o.item == a.item) {
					if (a.amount > o.amount)
						o.amount = a.amount;
					return;
				}
			out.push_back(a);
		};
		for (const QS::Amount& a : f.needs) {
			if (a.group > 0) {
				if (std::find(groups.begin(), groups.end(), a.group) != groups.end())
					continue;
				groups.push_back(a.group);
			}
			add(a);
		}
		for (const QS::Amount& a : f.handIn)
			add(a);
		return out;
	}

	static void QuestGiveItems(std::vector<QS::Amount> items) {
		Enqueue([items]() {
			if (!InGame())
				return;
			int given = GiveMissingItems(items);
			::GT::Overlay::Toast(given ? ::GT::Text::Format(u8"Předměty pro úkol doplněny (%d druhů)", given) : std::string(u8"Všechny potřebné předměty už máš"));
		});
	}

	static void QuestTeleportToNpc(int npcSym, std::string name) {
		Enqueue([npcSym, name]() {
			if (!InGame())
				return;
			oCNpc* npc = FindNpcByInstance(npcSym);
			if (!npc) {
				::GT::Overlay::Toast(NotHereText(npcSym, name));
				return;
			}
			const NpcPlace place = WhereIsNpc(npc);
			if (!place.Usable()) {
				::GT::Overlay::Toast(NpcNotReachableText(place, name));
				return;
			}
			RememberHeroSpot();
			MoveHero(MeetingSpot(npc, place));
			::GT::Overlay::Toast(u8"Teleport k: " + name);
		});
	}

	// Stands the hero in front of the NPC and starts talking, exactly like
	// pressing the action key: the NPC's own rules for talking still apply.
	// A sleeping character is woken by the game when the hero arrives; the
	// talk then starts as soon as it stands there (ProcessPendingTalk).
	static bool TalkToNpcNow(oCNpc* npc) {
		const NpcPlace place = WhereIsNpc(npc);
		if (!place.Usable()) {
			::GT::Overlay::Toast(NpcNotReachableText(place, NpcName(npc)));
			return false;
		}
		RememberHeroSpot();
		MoveHero(MeetingSpot(npc, place));
		::GT::Overlay::SetVisible(false);
		PendingTalk t;
		t.npc = npc;
		t.notBefore = GetTickCount() + (place.state == kNpcAsleep ? 400 : 0);
		t.giveUpAt = GetTickCount() + 10000;
		t.name = NpcName(npc);
		g_pendingTalk = t;
		return true;
	}

	static void QuestTalkTo(int npcSym, std::string name) {
		Enqueue([npcSym, name]() {
			if (!InGame())
				return;
			if (!oCInformationManager::GetInformationManager().HasFinished()) {
				::GT::Overlay::Toast(u8"Právě probíhá jiný rozhovor");
				return;
			}
			oCNpc* npc = FindNpcByInstance(npcSym);
			if (!npc) {
				::GT::Overlay::Toast(NotHereText(npcSym, name));
				return;
			}
			if (npc->IsDead()) {
				::GT::Overlay::Toast(name + u8" je mrtvý");
				return;
			}
			TalkToNpcNow(npc);
		});
	}

	// The completing dialog the hero can reach best: its NPC alive in this
	// world first, then one that has not been played yet, then a plain success
	// before one that can also end the quest badly ("too late", "you killed her").
	static int BestSuccessStep(const QS::Quest& q, const std::unordered_map<int, oCNpc*>& npcs,
		const std::unordered_map<int, oCInfo*>* infos = nullptr) {
		int best = -1;
		int bestScore = -1;
		for (size_t i = 0; i < q.steps.size(); i++) {
			if (!(q.steps[i].roles & QS::kRoleSuccess))
				continue;
			const QS::DialogFacts& f = g_questDb.model.dialogs[q.steps[i].dialog];
			std::unordered_map<int, oCNpc*>::const_iterator it = npcs.find(f.dialog.npc);
			const bool here = it != npcs.end() && !it->second->IsDead();
			bool open = true;
			if (infos) {
				oCInfo* info = InfoIn(*infos, f.dialog.info);
				open = info && (!info->told || f.dialog.permanent);
			}
			const bool clean = !(q.steps[i].roles & QS::kRoleFail);
			const int score = (here ? 4 : 0) + (open ? 2 : 0) + (clean ? 1 : 0);
			if (score > bestScore) {
				bestScore = score;
				best = (int)i;
			}
		}
		return best;
	}

	// The way to a completing dialog: the dialogs with the same NPC that may
	// have to come first (what the completing dialog needs, the quest's
	// opening, deliveries of the quest's items) and every item the way takes.
	struct CompletionPlan {
		std::vector<oCInfo*> helpers;
		std::vector<QS::Amount> items;
	};

	static CompletionPlan PlanCompletion(const QS::Quest& q, int step, oCInfo* target, const std::unordered_map<int, oCInfo*>& infos) {
		const QuestDatabase& db = g_questDb;
		const QS::DialogFacts& f = db.model.dialogs[q.steps[step].dialog];
		CompletionPlan plan;
		plan.items = ItemsFor(f);
		if (!target)
			return plan;
		auto addHelper = [&](int sym, bool delivery) -> bool {
			oCInfo* h = InfoIn(infos, sym);
			if (!h || h == target || h->pd.npc != target->pd.npc || h->pd.important)
				return false;
			if (h->pd.permanent ? !delivery : h->told)
				return false;
			if (std::find(plan.helpers.begin(), plan.helpers.end(), h) != plan.helpers.end())
				return false;
			plan.helpers.push_back(h);
			return true;
		};
		std::vector<int> wanted(f.needsDialogs.begin(), f.needsDialogs.end());
		wanted.insert(wanted.end(), f.anyDialogs.begin(), f.anyDialogs.end());
		for (const QS::Step& s : q.steps)
			if (s.roles & QS::kRoleStart)
				wanted.push_back(db.model.dialogs[s.dialog].dialog.info);
		for (size_t k = 0; k < wanted.size() && k < 24; k++) {
			addHelper(wanted[k], false);
			std::unordered_map<int, int>::const_iterator d = db.dialogOfInfo.find(wanted[k]);
			if (d == db.dialogOfInfo.end())
				continue;
			const QS::DialogFacts& next = db.model.dialogs[d->second];
			for (const std::vector<int>* list : { &next.needsDialogs, &next.anyDialogs })
				for (int need : *list)
					if (std::find(wanted.begin(), wanted.end(), need) == wanted.end())
						wanted.push_back(need);
		}
		for (size_t i = 0; i < q.steps.size(); i++) {
			const QS::DialogFacts& d = db.model.dialogs[q.steps[i].dialog];
			if ((int)i == step || (q.steps[i].roles & (QS::kRoleSuccess | QS::kRoleFail)) || d.handIn.empty())
				continue;
			if (!addHelper(d.dialog.info, true))
				continue;
			// the delivery takes its items away, so they come on top
			for (const QS::Amount& a : ItemsFor(d)) {
				bool merged = false;
				for (QS::Amount& o : plan.items)
					if (o.item == a.item) {
						o.amount += a.amount;
						merged = true;
						break;
					}
				if (!merged)
					plan.items.push_back(a);
			}
		}
		return plan;
	}

	// Gives the hero everything the completing dialog wants and opens it.
	static void QuestPrepareCompletion(int questIndex) {
		Enqueue([questIndex]() {
			const QuestDatabase& db = g_questDb;
			if (!InGame() || !db.ready || questIndex < 0 || questIndex >= (int)db.model.quests.size())
				return;
			const QS::Quest& q = db.model.quests[questIndex];
			std::unordered_map<int, oCNpc*> npcs = WorldNpcsByInstance();
			std::unordered_map<int, oCInfo*> infos = InfosByInstance();
			int step = BestSuccessStep(q, npcs, &infos);
			if (step < 0) {
				::GT::Overlay::Toast(u8"Tenhle úkol se nedokončuje rozhovorem – zbývá Jen označit jako splněný");
				return;
			}
			const QS::DialogFacts& f = db.model.dialogs[q.steps[step].dialog];
			GiveMissingItems(PlanCompletion(q, step, InfoIn(infos, f.dialog.info), infos).items);
			const std::string& name = db.dialogs[q.steps[step].dialog].npcName;
			std::unordered_map<int, oCNpc*>::iterator it = npcs.find(f.dialog.npc);
			if (it == npcs.end() || it->second->IsDead()) {
				::GT::Overlay::Toast(u8"Předměty máš. " + NotHereText(f.dialog.npc, name));
				return;
			}
			if (!oCInformationManager::GetInformationManager().HasFinished()) {
				::GT::Overlay::Toast(u8"Předměty máš. Právě ale probíhá jiný rozhovor.");
				return;
			}
			TalkToNpcNow(it->second);
		});
	}

	// ------------------------------------------------------------------ completing through the dialog

	// "Dokončit úkol": the hero talks to the NPC and the trainer picks the
	// completing line in the dialog menu itself, the same way the player does:
	// it highlights the line and confirms it like the Enter key. The game then
	// plays the dialog exactly as if the player had chosen it - the NPC takes
	// the items, pays the reward, changes its routine, the next quest starts.
	// When the line is not offered yet, the dialogs on the way come first.
	struct QuestAutoState {
		bool active = false;
		int quest = -1;
		int step = -1;
		oCNpc* npc = nullptr;
		oCInfo* target = nullptr;
		bool permanent = false;
		bool choices = false;
		std::vector<oCInfo*> helpers;
		bool talking = false;
		bool targetRan = false;
		int restarts = 0;
		int picks = 0;
		int tries = 0;
		DWORD restartAt = 0;
		DWORD giveUpAt = 0;
		bool finishing = true;    // the completing dialog; false: any dialog of the quest
		std::string npcName;
		std::string line;
	};

	static QuestAutoState g_questAuto;

	static void StopQuestAuto(const std::string& message) {
		g_questAuto = QuestAutoState();
		if (!message.empty())
			::GT::Overlay::Toast(message);
	}

	static std::string Quoted(const std::string& text) {
		return u8"„" + text + u8"“";
	}

	// Why the completing line is not in the menu, as far as the trainer can tell.
	static std::string QuestStuckText(const QuestAutoState& a) {
		const QuestDatabase& db = g_questDb;
		if (!db.ready || a.quest < 0 || a.quest >= (int)db.model.quests.size() || a.step < 0)
			return std::string();
		const QS::Quest& q = db.model.quests[a.quest];
		if (a.finishing && DiaryStatusNow(q.raw) < 0)
			for (const QS::Step& s : q.steps)
				if ((s.roles & QS::kRoleStart) && db.model.dialogs[s.dialog].dialog.npc != a.target->pd.npc)
					return u8"Úkol ještě nezačal – nejdřív si ho vezmi: " + db.dialogs[s.dialog].npcName;
		const QS::DialogFacts& f = db.model.dialogs[q.steps[a.step].dialog];
		{
			// a dialog with someone else has to come first
			std::unordered_map<int, oCInfo*> infos = InfosByInstance();
			auto toldAny = [&](const std::vector<int>& list) {
				for (int sym : list) {
					oCInfo* info = InfoIn(infos, sym);
					if (info && info->told)
						return true;
				}
				return false;
			};
			auto elsewhere = [&](int sym) -> std::string {
				std::unordered_map<int, int>::const_iterator d = db.dialogOfInfo.find(sym);
				oCInfo* info = InfoIn(infos, sym);
				if (d == db.dialogOfInfo.end() || !info || info->told || db.model.dialogs[d->second].dialog.npc == a.target->pd.npc)
					return std::string();
				return u8"Nejdřív si promluv: " + db.dialogs[d->second].npcName;
			};
			for (int sym : f.needsDialogs) {
				const std::string text = elsewhere(sym);
				if (!text.empty())
					return text;
			}
			if (!toldAny(f.anyDialogs))
				for (int sym : f.anyDialogs) {
					const std::string text = elsewhere(sym);
					if (!text.empty())
						return text;
				}
		}
		if (f.minChapter > 0 && db.model.chapterVar >= 0) {
			int chapter = 0;
			zCPar_Symbol* sym = parser->GetSymbol(db.model.chapterVar);
			if (sym)
				sym->GetValue(chapter, 0);
			if (chapter < f.minChapter)
				return ::GT::Text::Format(u8"Závěrečný rozhovor přijde až v %d. kapitole", f.minChapter);
		}
		if (!f.mustBeDead.empty()) {
			std::unordered_map<int, oCNpc*> npcs = WorldNpcsByInstance();
			for (int target : f.mustBeDead) {
				std::unordered_map<int, oCNpc*>::iterator it = npcs.find(target);
				if (it != npcs.end() && !it->second->IsDead())
					return u8"Nejdřív musí padnout: " + QuestName(target);
			}
		}
		return a.finishing ? a.npcName + u8" teď závěrečnou větu nenabízí – v Úkolech uvidíš, co ještě chybí"
			: a.npcName + u8" teď tenhle rozhovor nenabízí";
	}

	// The dialog menu waits for the player to pick a line.
	static bool DialogMenuWaiting(const oCInformationManager& im) {
		return !im.IsDone && im.Mode == oCInformationManager::INFO_MGR_MODE_INFO && im.IsWaitingForSelection
			&& !im.IsWaitingForScript && !im.IsWaitingForClose && !im.IsWaitingForOpen && !im.MustOpen
			&& !im.IsWaitingForEnd && im.DlgChoice && im.DlgChoice->IsActivated && !im.DlgChoice->IsDone;
	}

	// Picks a line of the dialog menu like the player does: highlights it and
	// confirms it the way the Enter key does. 1 = picked, 0 = the menu is still
	// moving (try again next frame), -1 = it cannot be done.
	static int PickDialogLine(oCInformationManager& im, int index) {
		zCViewDialogChoice* dlg = im.DlgChoice;
		zCInputCallback* input = static_cast<zCInputCallback*>(dlg);
		if ((char*)input - (char*)dlg != 0xEC) {
			// the menu is not laid out the way this engine version keeps it
			::GT::Log::Write("Quest: unexpected dialog menu layout (%d)", (int)((char*)input - (char*)dlg));
			return -1;
		}
		dlg->Select(index);
		if (dlg->ChoiceSelected != index)
			return -1;
		input->HandleEvent(KEY_RETURN);
		return dlg->IsDone ? 1 : 0;
	}

	// Game frame.
	static void ProcessQuestAuto() {
		QuestAutoState& a = g_questAuto;
		if (!a.active)
			return;
		if (!InGame() || !NpcStillExists(a.npc) || !ogame) {
			StopQuestAuto(std::string());
			return;
		}
		if (a.npc->IsDead()) {
			StopQuestAuto(a.npcName + u8" je mrtvý – rozhovorem to nepůjde");
			return;
		}
		const DWORD now = GetTickCount();
		if (now > a.giveUpAt) {
			if (a.finishing)
				StopQuestAuto(a.talking ? std::string(u8"Dokončování úkolu přerušeno – trvalo příliš dlouho")
					: a.npcName + u8" se do rozhovoru nedal – zkus Připravit a promluv sám");
			else
				StopQuestAuto(a.talking ? std::string(u8"Rozhovor přerušen – trvalo příliš dlouho")
					: a.npcName + u8" se do rozhovoru nedal – zkus to znovu, nebo ho osloví sám");
			return;
		}
		oCInformationManager& im = oCInformationManager::GetInformationManager();
		if (im.IsDone) {
			if (a.talking) {
				// the dialog ended before the completing line came up, e.g. after a greeting
				a.talking = false;
				if (a.restarts >= 2) {
					StopQuestAuto(a.npcName + u8" rozhovor ukončil dřív – zkus to znovu");
					return;
				}
				a.restarts++;
				a.restartAt = now + 800;
				a.giveUpAt = now + 15000;
			}
			if (a.restartAt && now >= a.restartAt) {
				a.restartAt = 0;
				a.npc->AssessTalk_S(player);
			}
			return;
		}
		if (im.Npc != a.npc)
			return;
		a.talking = true;
		a.giveUpAt = now + 180000;
		if (a.target->pd.important && im.Mode == oCInformationManager::INFO_MGR_MODE_IMPORTANT && im.Info == a.target)
			a.targetRan = true;   // the NPC starts the completing dialog himself
		if ((a.target->told && !a.permanent) || a.targetRan) {
			// the NPC brought it up himself
			StopQuestAuto(a.finishing ? u8"Úkol se dokončuje v rozhovoru" : u8"Rozhovor běží");
			return;
		}
		if (!DialogMenuWaiting(im))
			return;   // greetings, speech or answers to choose are still running
		if (a.target->pd.important) {
			// the NPC would have started it himself by now
			const std::string why = QuestStuckText(a);
			StopQuestAuto(why.empty() ? a.npcName + u8" závěrečný rozhovor nezačal" : why);
			return;
		}

		oCInfoManager* man = ogame->GetInfoManager();
		zCViewDialogChoice* dlg = im.DlgChoice;
		std::vector<oCInfo*> offered;
		if (man) {
			// the same list the game shows in the menu, in the same order
			ScriptInstanceScope scope;
			const int count = man->GetInfoCountUnimportant(im.Player, im.Npc);
			for (int i = 0; i < count; i++)
				offered.push_back(man->GetInfoUnimportant(im.Player, im.Npc, i));
		}
		if ((int)offered.size() != dlg->Choices) {
			// the menu does not match what the game would offer now; give it a moment
			if (++a.tries > 30)
				StopQuestAuto(u8"Vyber v rozhovoru sám: " + Quoted(a.line));
			return;
		}
		auto indexOf = [&](oCInfo* info) -> int {
			for (size_t i = 0; i < offered.size(); i++)
				if (offered[i] == info)
					return (int)i;
			return -1;
		};
		auto asksBack = [](oCInfo* info) -> bool {
			std::unordered_map<int, int>::const_iterator d = g_questDb.dialogOfInfo.find(info->instance);
			return d != g_questDb.dialogOfInfo.end() && g_questDb.model.dialogs[d->second].choices;
		};
		oCInfo* pick = a.target;
		int index = indexOf(a.target);
		// on the way: plain dialogs first, the ones that ask the player something last
		for (int pass = 0; pass < 2 && index < 0 && a.picks < 6; pass++)
			for (oCInfo* h : a.helpers) {
				if ((h->told && !h->pd.permanent) || asksBack(h) != (pass == 1))
					continue;
				index = indexOf(h);
				if (index >= 0) {
					pick = h;
					break;
				}
			}
		if (index < 0) {
			const std::string why = QuestStuckText(a);
			StopQuestAuto(why.empty() ? a.npcName + u8" teď závěrečnou větu nenabízí" : why);
			return;
		}
		const int result = PickDialogLine(im, index);
		if (result == 0) {
			if (++a.tries > 60)
				StopQuestAuto(u8"Vyber v rozhovoru sám: " + Quoted(a.line));
			return;
		}
		if (result < 0) {
			StopQuestAuto(u8"Vyber v rozhovoru sám: " + Quoted(a.line));
			return;
		}
		a.tries = 0;
		::GT::Log::Write("Quest: picked %s%s", pick->name.ToChar(), pick == a.target ? "" : " (on the way)");
		if (pick == a.target) {
			if (a.choices)
				StopQuestAuto(u8"Vybráno: " + Quoted(a.line) + u8" – odpověď vyber sám");
			else
				StopQuestAuto(u8"Vybráno: " + Quoted(a.line) + (a.finishing ? u8" – rozhovor úkol dokončí" : ""));
			return;
		}
		a.picks++;
		::GT::Overlay::Toast(u8"Cestou k úkolu: " + Quoted(FromZ(pick->pd.description)) + (asksBack(pick) ? u8" – odpověď vyber sám" : ""));
	}

	// Talks to the NPC of a quest step and picks its line in the dialog menu.
	// finishing: the completing dialog (stepIndex < 0 = the one the hero can
	// reach best); the items it wants are given and the dialogs on the way are
	// played first. Otherwise any dialog of the quest that is offered now.
	static void StartDialogStep(int questIndex, int stepIndex, bool finishing) {
		Enqueue([questIndex, stepIndex, finishing]() {
			const QuestDatabase& db = g_questDb;
			if (!InGame() || !db.ready || questIndex < 0 || questIndex >= (int)db.model.quests.size())
				return;
			if (g_questAuto.active) {
				::GT::Overlay::Toast(u8"Jeden rozhovor se už vede – počkej, nebo ho v Úkolech zruš");
				return;
			}
			const QS::Quest& q = db.model.quests[questIndex];
			std::unordered_map<int, oCNpc*> npcs = WorldNpcsByInstance();
			std::unordered_map<int, oCInfo*> infos = InfosByInstance();
			int step = stepIndex;
			if (finishing && (step < 0 || step >= (int)q.steps.size() || !(q.steps[step].roles & QS::kRoleSuccess)))
				step = BestSuccessStep(q, npcs, &infos);
			if (step < 0 || step >= (int)q.steps.size()) {
				::GT::Overlay::Toast(finishing ? u8"Tenhle úkol se rozhovorem nedokončuje – zbývá Jen označit jako splněný"
					: u8"Tenhle rozhovor ve hře není");
				return;
			}
			const QS::DialogFacts& f = db.model.dialogs[q.steps[step].dialog];
			const std::string& name = db.dialogs[q.steps[step].dialog].npcName;
			std::unordered_map<int, oCNpc*>::iterator it = npcs.find(f.dialog.npc);
			if (it == npcs.end() || it->second->IsDead()) {
				::GT::Overlay::Toast(it == npcs.end() ? NotHereText(f.dialog.npc, name) : name + u8" je mrtvý");
				return;
			}
			oCInfo* info = InfoIn(infos, f.dialog.info);
			if (!info) {
				::GT::Overlay::Toast(finishing ? u8"Závěrečný rozhovor ve hře chybí" : u8"Tenhle rozhovor ve hře chybí");
				return;
			}
			if (info->told && !f.dialog.permanent) {
				::GT::Overlay::Toast(finishing ? u8"Závěrečný rozhovor už proběhl" : u8"Tenhle rozhovor už proběhl");
				return;
			}
			if (!oCInformationManager::GetInformationManager().HasFinished()) {
				::GT::Overlay::Toast(u8"Právě probíhá jiný rozhovor");
				return;
			}
			QuestAutoState a;
			if (finishing) {
				CompletionPlan plan = PlanCompletion(q, step, info, infos);
				GiveMissingItems(plan.items);
				a.helpers = std::move(plan.helpers);
			}
			a.active = true;
			a.finishing = finishing;
			a.quest = questIndex;
			a.step = step;
			a.npc = it->second;
			a.target = info;
			a.permanent = f.dialog.permanent;
			a.choices = f.choices;
			a.giveUpAt = GetTickCount() + 15000;
			a.npcName = name;
			a.line = db.dialogs[q.steps[step].dialog].line;
			::GT::Log::Write("Quest: %s %s through %s (%d dialogs on the way)", finishing ? "completing" : "talking", q.raw.c_str(),
				info->name.ToChar(), (int)a.helpers.size());
			g_questAuto = std::move(a);
			if (!TalkToNpcNow(it->second)) {
				g_questAuto = QuestAutoState();
				return;
			}
			::GT::Overlay::Toast((finishing ? u8"Dokončuji úkol v rozhovoru: " : u8"Mluvím s: ") + name);
		});
	}

	// stepIndex < 0: the completing dialog the hero can reach best.
	static void QuestCompleteByDialog(int questIndex, int stepIndex = -1) {
		StartDialogStep(questIndex, stepIndex, true);
	}

	// Any dialog of the quest that is offered now (taking the quest, moving it on).
	static void QuestTalkStep(int questIndex, int stepIndex) {
		StartDialogStep(questIndex, stepIndex, false);
	}

	// Talks to the NPC of any dialog and picks its line in the menu: trading,
	// a lesson. What follows (the trade screen, the lessons to choose) is the
	// player's.
	static void TalkAndPickInfo(int infoSym) {
		Enqueue([infoSym]() {
			if (!InGame())
				return;
			if (g_questAuto.active) {
				::GT::Overlay::Toast(u8"Jeden rozhovor se už vede – počkej, nebo ho v Úkolech zruš");
				return;
			}
			std::unordered_map<int, oCInfo*> infos = InfosByInstance();
			oCInfo* info = InfoIn(infos, infoSym);
			if (!info) {
				::GT::Overlay::Toast(u8"Tenhle rozhovor ve hře chybí");
				return;
			}
			oCNpc* npc = FindNpcByInstance(info->pd.npc);
			const std::string name = npc ? NpcName(npc) : QuestName(info->pd.npc);
			if (!npc) {
				::GT::Overlay::Toast(NotHereText(info->pd.npc, name));
				return;
			}
			if (npc->IsDead()) {
				::GT::Overlay::Toast(name + u8" je mrtvý");
				return;
			}
			if (info->told && !info->pd.permanent) {
				::GT::Overlay::Toast(u8"Tenhle rozhovor už proběhl");
				return;
			}
			if (!oCInformationManager::GetInformationManager().HasFinished()) {
				::GT::Overlay::Toast(u8"Právě probíhá jiný rozhovor");
				return;
			}
			QuestAutoState a;
			a.active = true;
			a.finishing = false;
			a.npc = npc;
			a.target = info;
			a.permanent = info->pd.permanent != 0;
			std::unordered_map<int, int>::const_iterator d = g_questDb.dialogOfInfo.find(infoSym);
			a.choices = d != g_questDb.dialogOfInfo.end() && g_questDb.model.dialogs[d->second].choices;
			a.giveUpAt = GetTickCount() + 15000;
			a.npcName = name;
			a.line = FromZ(info->pd.description);
			g_questAuto = std::move(a);
			if (!TalkToNpcNow(npc)) {
				g_questAuto = QuestAutoState();
				return;
			}
			::GT::Overlay::Toast(u8"Mluvím s: " + name);
		});
	}

	// Marks the quest as done without playing the dialog: mission variables,
	// diary, the completing dialogs count as told, and the reward of the first
	// one (experience and items) unless the hero already got it.
	static void QuestForceComplete(int questIndex) {
		Enqueue([questIndex]() {
			const QuestDatabase& db = g_questDb;
			if (!InGame() || !db.ready || questIndex < 0 || questIndex >= (int)db.model.quests.size())
				return;
			const QS::Quest& q = db.model.quests[questIndex];
			const bool alreadyDone = DiaryStatusNow(q.raw) == (int)oCLogTopic::zELogTopicStatus_Success;
			for (int var : q.missionVars) {
				zCPar_Symbol* sym = parser->GetSymbol(var);
				if (sym && sym->type == zPAR_TYPE_INT && !(sym->flags & zPAR_FLAG_CONST))
					sym->SetValue(db.model.logSuccess, 0);
			}
			const bool inDiary = SetDiaryStatus(q.raw, oCLogTopic::zELogTopicStatus_Success);

			std::unordered_map<int, oCInfo*> infos = InfosByInstance();
			const QS::DialogFacts* reward = nullptr;
			for (const QS::Step& step : q.steps) {
				if (!(step.roles & QS::kRoleSuccess))
					continue;
				const QS::DialogFacts& f = db.model.dialogs[step.dialog];
				oCInfo* info = InfoIn(infos, f.dialog.info);
				if (!info)
					continue;
				if (info->told) {
					reward = nullptr;     // finished in the game already: no second reward
					break;
				}
				if (!reward)
					reward = &f;
			}
			for (const QS::Step& step : q.steps) {
				if (!(step.roles & QS::kRoleSuccess))
					continue;
				oCInfo* info = InfoIn(infos, db.model.dialogs[step.dialog].dialog.info);
				if (info)
					info->SetTold(TRUE);
			}
			int xp = 0;
			if (reward && !alreadyDone) {
				xp = reward->xp > 0 ? reward->xp : reward->choiceXp;
				for (const QS::Amount& a : reward->rewards)
					if (IsItemInstance(a.item) && a.amount > 0)
						player->CreateItems(a.item, a.amount);
				if (xp > 0)
					CallScriptInt("B_GIVEPLAYERXP", xp);
			}
			::GT::Log::Write("Quest force-completed: %s (diary %d, xp %d)", q.raw.c_str(), inDiary ? 1 : 0, xp);
			::GT::Overlay::Toast(u8"Úkol splněn: " + db.quests[questIndex].title + (inDiary ? std::string() : std::string(u8" (v deníku zatím není)")));
			g_diary.nextRefresh = 0.0;
			g_questLive.nextRefresh = 0.0;
		});
	}

	static void QuestSetDiaryStatus(std::string raw, int status) {
		Enqueue([raw, status]() {
			if (!InGame())
				return;
			if (SetDiaryStatus(raw, status))
				::GT::Overlay::Toast(u8"Stav v deníku změněn");
			else
				::GT::Overlay::Toast(u8"Úkol v deníku ještě není – nejdřív ho musíš dostat");
			g_diary.nextRefresh = 0.0;
		});
	}

	static void QuestSetVariable(int var, int value) {
		Enqueue([var, value]() {
			if (!InGame() || !parser)
				return;
			zCPar_Symbol* sym = parser->GetSymbol(var);
			if (sym && sym->type == zPAR_TYPE_INT && !(sym->flags & zPAR_FLAG_CONST))
				sym->SetValue(value, 0);
			g_questLive.nextRefresh = 0.0;
		});
	}

	// The game's own B_CheckLog: moves topics to "done" or "failed" according
	// to their mission variables.
	static void QuestSyncDiary() {
		Enqueue([]() {
			if (!InGame() || !parser)
				return;
			int index = ScriptSymbolIndex("B_CHECKLOG");
			zCPar_Symbol* sym = index >= 0 ? parser->GetSymbol(index) : nullptr;
			if (sym && sym->type == zPAR_TYPE_FUNC && sym->ele == 0) {
				ScriptInstanceScope scope;
				parser->CallFunc(index);
				::GT::Overlay::Toast(u8"Deník srovnán podle stavu úkolů");
			}
			g_diary.nextRefresh = 0.0;
		});
	}

} // namespace Trainer
} // namespace GOTHIC_NAMESPACE
