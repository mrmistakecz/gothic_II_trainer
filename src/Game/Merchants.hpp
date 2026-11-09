// Traders and teachers of the world. Traders are the characters with a trade
// dialog (C_INFO.trade); their goods are what they carry. Teachers are the
// characters whose dialogs call one of the game's B_Teach* functions; the quest
// analyzer finds them together with the text the game shows for each lesson.
// Game thread only; the UI reads the results.

namespace GOTHIC_NAMESPACE {
namespace Trainer {

	struct TradeGood {
		int instance = -1;
		int amount = 0;
		int value = 0;          // the price of one piece
		std::string name;       // UTF-8
		std::string folded;
	};

	struct TraderEntry {
		int npc = -1;             // NPC instance
		int info = -1;            // the trade dialog
		std::string name;         // UTF-8
		std::string folded;
		std::string line;         // the line that opens trading
		std::string world;        // world key from the startup scripts
		bool here = false;        // in this world
		bool dead = false;
		bool tradesNow = false;   // the trade dialog is offered now
		int place = kNpcUnknown;
		float metres = -1.0f;      // -1 when the place is not known
		zVEC3 pos;
		std::string area;
		int gold = 0;
		std::vector<TradeGood> goods;   // by name
	};

	enum LessonKind : int {
		kLessonAttribute = 0, kLessonFight, kLessonThief, kLessonMagic, kLessonRunes, kLessonAlchemy,
		kLessonSmith, kLessonTrophy, kLessonLanguage, kLessonWisp, kLessonOther, kLessonKinds
	};

	struct LessonEntry {
		int kind = kLessonOther;
		int what = 0;
		std::string title;      // "Luk", "Esence hojivé síly"
		std::string folded;
		int max = 0;            // the teacher's maximum (attributes, fighting)
		int now = -1;           // the hero's value now (attributes, fighting), -1 not a number
		bool learned = false;   // the hero cannot learn more of it here
	};

	struct TeacherEntry {
		int npc = -1;
		std::string name;
		std::string folded;
		std::string world;
		bool here = false;
		bool dead = false;
		int place = kNpcUnknown;
		float metres = -1.0f;      // -1 when the place is not known
		zVEC3 pos;
		std::string area;
		int infoNow = -1;            // a lesson dialog offered now, -1 none
		int infoAny = -1;            // the first lesson dialog
		std::string line;            // its line
		std::vector<LessonEntry> lessons;
	};

	struct MerchantBoard {
		std::vector<TraderEntry> traders;   // nearest first, the ones in other worlds last
		std::vector<TeacherEntry> teachers;
		int learnPoints = 0;
		int heroGold = 0;
		bool ready = false;
		bool pending = false;
		double nextRefresh = 0.0;
	};

	static MerchantBoard g_merchants;

	// The most of each item a trader was seen with in this session, for "Doplnit zboží".
	static std::unordered_map<oCNpc*, std::unordered_map<int, int> > g_traderStock;

	static const char* LessonKindName(int kind) {
		static const char* const kNames[kLessonKinds] = {
			u8"Vlastnosti", u8"Boj", u8"Zlodějské dovednosti", u8"Magie", u8"Výroba run", u8"Alchymie",
			u8"Kovářství", u8"Lov a trofeje", u8"Jazyky stavitelů", u8"Bludička", u8"Ostatní"
		};
		return kind >= 0 && kind < kLessonKinds ? kNames[kind] : kNames[kLessonOther];
	}

	static int LessonKindOf(const std::string& func) {
		if (func == "B_TEACHATTRIBUTEPOINTS") return kLessonAttribute;
		if (func == "B_TEACHFIGHTTALENTPERCENT") return kLessonFight;
		if (func == "B_TEACHTHIEFTALENT") return kLessonThief;
		if (func == "B_TEACHMAGICCIRCLE") return kLessonMagic;
		if (func == "B_TEACHPLAYERTALENTRUNES") return kLessonRunes;
		if (func == "B_TEACHPLAYERTALENTALCHEMY") return kLessonAlchemy;
		if (func == "B_TEACHPLAYERTALENTSMITH") return kLessonSmith;
		if (func == "B_TEACHPLAYERTALENTTAKEANIMALTROPHY") return kLessonTrophy;
		if (func == "B_TEACHPLAYERTALENTFOREIGNLANGUAGE") return kLessonLanguage;
		if (func == "B_TEACHPLAYERTALENTWISPDETECTOR") return kLessonWisp;
		return kLessonOther;
	}

	// "Síla + 5" -> "Síla"
	static std::string WithoutPlus(std::string label) {
		const size_t plus = label.rfind(" + ");
		if (plus != std::string::npos && plus + 3 < label.size() && isdigit((unsigned char)label[plus + 3]))
			label.erase(plus);
		while (!label.empty() && label.back() == ' ')
			label.pop_back();
		return label;
	}

	static std::string ScriptItemName(const std::string& instance) {
		const int sym = ScriptSymbolIndex(instance.c_str());
		const int member = ScriptSymbolIndex("C_ITEM.NAME");
		return sym >= 0 && member >= 0 ? ScriptInstanceString(sym, member) : std::string();
	}

	static std::string LessonTitle(const QS::Teach& t, int kind, const QS::DialogFacts& f) {
		switch (kind) {
			case kLessonAttribute:
				switch (t.what) {
					case 1: return u8"Životní energie";
					case 3: return u8"Mana";
					case 4: return u8"Síla";
					case 5: return u8"Obratnost";
				}
				break;
			case kLessonFight:
				switch (t.what) {
					case 1: return u8"Jednoruční zbraně";
					case 2: return u8"Obouruční zbraně";
					case 3: return u8"Luk";
					case 4: return u8"Kuše";
				}
				break;
			case kLessonThief:
				switch (t.what) {
					case 5: return u8"Páčení zámků";
					case 8: return u8"Plížení";
					case 11: return u8"Akrobacie";
					case 12: return u8"Kapsářství";
				}
				break;
			case kLessonMagic:
				return ::GT::Text::Format(u8"%d. kruh magie", t.what);
			default:
				break;
		}
		std::string label;
		if (t.label >= 0 && parser)
			if (zCPar_Symbol* sym = parser->GetSymbol(t.label)) {
				zSTRING value;
				sym->GetValue(value, 0);
				label = WithoutPlus(FromZ(value));
			}
		if (!label.empty())
			return label;
		if (kind == kLessonRunes && t.whatSym >= 0) {
			// SPL_FIREBOLT -> the rune ITRU_FIREBOLT
			zCPar_Symbol* sym = parser->GetSymbol(t.whatSym);
			std::string code = sym ? std::string(sym->name.ToChar()) : std::string();
			if (code.compare(0, 4, "SPL_") == 0) {
				const std::string name = ScriptItemName("ITRU_" + code.substr(4));
				if (!name.empty())
					return name;
			}
		}
		const std::string line = ::GT::Text::FromGame(f.dialog.line.c_str());
		if (!line.empty())
			return line;
		zCPar_Symbol* sym = t.whatSym >= 0 && parser ? parser->GetSymbol(t.whatSym) : nullptr;
		return sym ? std::string(sym->name.ToChar()) : ::GT::Text::Format("%d", t.what);
	}

	static int ScriptArrayValue(const char* name, int index) {
		const int sym = ScriptSymbolIndex(name);
		zCPar_Symbol* s = sym >= 0 ? parser->GetSymbol(sym) : nullptr;
		if (!s || s->type != zPAR_TYPE_INT || index < 0 || index >= (int)s->ele)
			return 0;
		int value = 0;
		s->GetValue(value, index);
		return value;
	}

	// What the hero knows of a lesson now.
	static void LessonState(LessonEntry& l) {
		switch (l.kind) {
			case kLessonAttribute:
				if (l.what >= 0 && l.what < NPC_ATR_MAX) {
					l.now = player->attribute[l.what];
					l.learned = l.max > 0 && l.now >= l.max;
				}
				break;
			case kLessonFight:
				if (l.what >= 0 && l.what < NPC_HITCHANCE_MAX) {
					l.now = player->hitChance[l.what];
					l.learned = l.max > 0 && l.now >= l.max;
				}
				break;
			case kLessonThief:
				l.learned = player->GetTalentSkill(l.what) > 0;
				break;
			case kLessonMagic:
				l.learned = player->GetTalentSkill(oCNpcTalent::NPC_TAL_MAGE) >= l.what;
				break;
			case kLessonRunes: l.learned = ScriptArrayValue("PLAYER_TALENT_RUNES", l.what) != 0; break;
			case kLessonAlchemy: l.learned = ScriptArrayValue("PLAYER_TALENT_ALCHEMY", l.what) != 0; break;
			case kLessonSmith: l.learned = ScriptArrayValue("PLAYER_TALENT_SMITH", l.what) != 0; break;
			case kLessonTrophy: l.learned = ScriptArrayValue("PLAYER_TALENT_TAKEANIMALTROPHY", l.what) != 0; break;
			case kLessonLanguage: l.learned = ScriptArrayValue("PLAYER_TALENT_FOREIGNLANGUAGE", l.what) != 0; break;
			case kLessonWisp: l.learned = ScriptArrayValue("PLAYER_TALENT_WISPDETECTOR", l.what) != 0; break;
			default: break;
		}
	}

	static bool IsDebugNpc(int sym) {
		zCPar_Symbol* s = parser && sym >= 0 ? parser->GetSymbol(sym) : nullptr;
		if (!s)
			return true;
		const std::string name = s->name.ToChar();
		return name == "CH" || name == "SH" || name == "SH_OLDWORLD" || name == "PC_ROCKEFELLER" || name == "PC_LEVELINSPEKTOR";
	}

	// Where a trader or teacher is, for the lists.
	template<typename Entry>
	static void PlaceEntry(Entry& e, QuestScanNow& s) {
		std::unordered_map<int, oCNpc*>::const_iterator it = s.npcs.find(e.npc);
		e.here = it != s.npcs.end();
		std::unordered_map<int, std::string>::const_iterator w = g_questDb.worldOf.find(e.npc);
		if (w != g_questDb.worldOf.end())
			e.world = w->second;
		if (!e.here)
			return;
		oCNpc* npc = it->second;
		e.dead = npc->IsDead() != 0;
		const NpcPlace& p = PlaceOf(s, npc);
		e.place = p.state;
		if (p.Usable()) {
			e.metres = sqrtf(Distance2(s.hero, p.pos)) / 100.0f;
			e.pos = p.pos;
			e.area = AreaOf(p.pos);
		}
		const std::string live = NpcName(npc);
		if (!live.empty())
			e.name = live;
	}

	// Game thread, a few milliseconds while the tab is open.
	static void RefreshMerchants() {
		struct Finish { ~Finish() { g_merchants.pending = false; } } finish;
		const QuestDatabase& db = g_questDb;
		if (!db.ready || !InGame())
			return;
		QuestScanNow s;
		StartQuestScan(s);
		MerchantBoard board;
		board.nextRefresh = g_merchants.nextRefresh;
		board.learnPoints = (int)player->learn_points;
		board.heroGold = db.goldItem >= 0 ? player->inventory2.GetAmount(db.goldItem) : 0;

		// --- traders: everyone with a trade dialog
		std::unordered_map<int, int> traderOf;   // NPC instance -> board.traders
		oCInfoManager* infos = ogame ? ogame->GetInfoManager() : nullptr;
		for (auto* node = infos ? infos->infoList.next : nullptr; node; node = node->next) {
			oCInfo* info = node->data;
			if (!info || !info->pd.trade || IsDebugNpc(info->pd.npc))
				continue;
			std::unordered_map<int, int>::iterator known = traderOf.find(info->pd.npc);
			TraderEntry* t = nullptr;
			if (known == traderOf.end()) {
				traderOf[info->pd.npc] = (int)board.traders.size();
				board.traders.push_back(TraderEntry());
				t = &board.traders.back();
				t->npc = info->pd.npc;
				t->info = info->instance;
				t->name = QuestName(info->pd.npc);
				t->line = FromZ(info->pd.description);
			}
			else {
				t = &board.traders[known->second];
			}
			std::unordered_map<int, oCNpc*>::const_iterator it = s.npcs.find(info->pd.npc);
			if (!t->tradesNow && it != s.npcs.end() && !it->second->IsDead() && (!info->told || info->pd.permanent)
				&& OfferedNow(s, info, it->second)) {
				t->tradesNow = true;
				t->info = info->instance;
				t->line = FromZ(info->pd.description);
			}
		}
		for (TraderEntry& t : board.traders) {
			PlaceEntry(t, s);
			t.folded = ::GT::Text::Fold(t.name);
			if (!t.here)
				continue;
			oCNpc* npc = s.npcs[t.npc];
			std::unordered_map<int, int>& seen = g_traderStock[npc];
			{
				// what the game keeps packed is unpacked like the trade screen does
				ScriptInstanceScope scope;
				npc->inventory2.UnpackCategory();
			}
			std::unordered_map<int, int> byInstance;   // instance -> goods index
			for (auto* n = npc->inventory2.inventory.next; n; n = n->next) {
				oCItem* item = n->data;
				if (!item || item->HasFlag(ITM_FLAG_ACTIVE))
					continue;   // what he wears is not for sale
				const int instance = item->GetInstance();
				const int amount = item->amount > 0 ? item->amount : 1;
				if (instance == db.goldItem) {
					t.gold += amount;
					continue;
				}
				std::unordered_map<int, int>::iterator g = byInstance.find(instance);
				if (g != byInstance.end()) {
					t.goods[g->second].amount += amount;
					continue;
				}
				TradeGood good;
				good.instance = instance;
				good.amount = amount;
				good.value = item->value;
				good.name = FromZ(item->name);
				if (good.name.empty())
					good.name = FromZ(item->GetInstanceName());
				good.folded = ::GT::Text::Fold(good.name);
				byInstance[instance] = (int)t.goods.size();
				t.goods.push_back(good);
			}
			for (const TradeGood& g : t.goods)
				if (g.amount > seen[g.instance])
					seen[g.instance] = g.amount;
			std::sort(t.goods.begin(), t.goods.end(), [](const TradeGood& a, const TradeGood& b) { return a.folded < b.folded; });
		}
		std::stable_sort(board.traders.begin(), board.traders.end(), [](const TraderEntry& a, const TraderEntry& b) {
			const bool ah = a.metres >= 0.0f, bh = b.metres >= 0.0f;
			if (ah != bh)
				return ah;
			return ah ? a.metres < b.metres : a.folded < b.folded;
		});

		// --- teachers: every dialog that teaches something
		std::unordered_map<int, int> teacherOf;
		for (const QS::DialogFacts& f : db.model.dialogs) {
			if (f.teaches.empty() || IsDebugNpc(f.dialog.npc))
				continue;
			std::unordered_map<int, int>::iterator known = teacherOf.find(f.dialog.npc);
			TeacherEntry* t = nullptr;
			if (known == teacherOf.end()) {
				teacherOf[f.dialog.npc] = (int)board.teachers.size();
				board.teachers.push_back(TeacherEntry());
				t = &board.teachers.back();
				t->npc = f.dialog.npc;
				t->name = QuestName(f.dialog.npc);
			}
			else {
				t = &board.teachers[known->second];
			}
			oCInfo* info = InfoIn(s.infos, f.dialog.info);
			if (!info)
				continue;
			if (t->infoAny < 0) {
				t->infoAny = f.dialog.info;
				t->line = ::GT::Text::FromGame(f.dialog.line.c_str());
			}
			std::unordered_map<int, oCNpc*>::const_iterator it = s.npcs.find(f.dialog.npc);
			if (t->infoNow < 0 && it != s.npcs.end() && !it->second->IsDead() && (!info->told || f.dialog.permanent)
				&& OfferedNow(s, info, it->second)) {
				t->infoNow = f.dialog.info;
				t->line = ::GT::Text::FromGame(f.dialog.line.c_str());
			}
			for (const QS::Teach& teach : f.teaches) {
				zCPar_Symbol* func = parser->GetSymbol(teach.func);
				const int kind = LessonKindOf(func ? std::string(func->name.ToChar()) : std::string());
				LessonEntry* l = nullptr;
				for (LessonEntry& e : t->lessons)
					if (e.kind == kind && e.what == teach.what)
						l = &e;
				if (!l) {
					t->lessons.push_back(LessonEntry());
					l = &t->lessons.back();
					l->kind = kind;
					l->what = teach.what;
					l->title = LessonTitle(teach, kind, f);
					l->folded = ::GT::Text::Fold(l->title);
				}
				if (teach.max > l->max)
					l->max = teach.max;
			}
		}
		for (TeacherEntry& t : board.teachers) {
			PlaceEntry(t, s);
			t.folded = ::GT::Text::Fold(t.name);
			for (LessonEntry& l : t.lessons)
				LessonState(l);
			std::stable_sort(t.lessons.begin(), t.lessons.end(), [](const LessonEntry& a, const LessonEntry& b) {
				return a.kind != b.kind ? a.kind < b.kind : a.folded < b.folded;
			});
		}
		std::stable_sort(board.teachers.begin(), board.teachers.end(), [](const TeacherEntry& a, const TeacherEntry& b) {
			const bool ah = a.metres >= 0.0f, bh = b.metres >= 0.0f;
			if (ah != bh)
				return ah;
			return ah ? a.metres < b.metres : a.folded < b.folded;
		});
		board.ready = true;
		g_merchants = std::move(board);
	}

	// ------------------------------------------------------------------ actions

	static void TraderGiveGold(int npcSym, int amount, std::string name) {
		Enqueue([npcSym, amount, name]() {
			if (!InGame() || g_questDb.goldItem < 0)
				return;
			oCNpc* npc = FindNpcByInstance(npcSym);
			if (!npc || npc->IsDead()) {
				::GT::Overlay::Toast(name + u8" tu není");
				return;
			}
			npc->CreateItems(g_questDb.goldItem, amount);
			::GT::Overlay::Toast(::GT::Text::Format(u8"%s má o %d zlatých víc – může od tebe víc koupit", name.c_str(), amount));
			g_merchants.nextRefresh = 0.0;
		});
	}

	// Puts back what was bought from the trader since the trainer first saw his goods.
	static void TraderRestock(int npcSym, std::string name) {
		Enqueue([npcSym, name]() {
			if (!InGame())
				return;
			oCNpc* npc = FindNpcByInstance(npcSym);
			if (!npc || npc->IsDead()) {
				::GT::Overlay::Toast(name + u8" tu není");
				return;
			}
			std::unordered_map<oCNpc*, std::unordered_map<int, int> >::const_iterator seen = g_traderStock.find(npc);
			if (seen == g_traderStock.end()) {
				::GT::Overlay::Toast(u8"Zboží tohohle obchodníka trainer ještě neviděl");
				return;
			}
			int kinds = 0;
			{
				ScriptInstanceScope scope;
				npc->inventory2.UnpackCategory();
				for (const std::pair<const int, int>& want : seen->second) {
					const int have = npc->inventory2.GetAmount(want.first);
					if (have < want.second) {
						npc->CreateItems(want.first, want.second - have);
						kinds++;
					}
				}
			}
			::GT::Overlay::Toast(kinds ? ::GT::Text::Format(u8"%s má zase plné zásoby (%d druhů doplněno)", name.c_str(), kinds)
				: name + u8" má všechno, co měl");
			g_merchants.nextRefresh = 0.0;
		});
	}

} // namespace Trainer
} // namespace GOTHIC_NAMESPACE
