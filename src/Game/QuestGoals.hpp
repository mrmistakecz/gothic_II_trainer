// Next steps of the quests. For a running quest the trainer works out what to
// do now, the same way the minimap (GothicQuestNav) does it: a dialog that
// completes the quest or moves it on, the items the completing dialog still
// wants (lying in the world, in a chest, carried by someone, handed out in a
// dialog), the characters it wants dead, the dialogs that have to come first
// and finally who closes the quest. For quests the hero has not taken yet: who
// offers them right now. Game thread only; the UI reads the results.

namespace GOTHIC_NAMESPACE {
namespace Trainer {

	enum QuestGoalKind : int {
		kGoalFinish = 0,   // the completing dialog is offered now
		kGoalTalk,         // a dialog of the quest is offered now
		kGoalTake,         // the dialog that gives the quest is offered now
		kGoalFind,         // a wanted item lies in the world
		kGoalChest,        // a wanted item lies in a chest
		kGoalCarrier,      // someone carries a wanted item
		kGoalSource,       // a dialog hands a wanted item out
		kGoalKill,         // the quest wants someone dead
		kGoalFirst,        // a dialog with someone has to come first
		kGoalFinisher,     // who closes the quest
		kGoalGiver         // who gives the quest (not offered now)
	};

	struct QuestGoal {
		int kind = kGoalTalk;
		int tier = 0;           // 0 can be completed now, 1 a dialog now, 2 what is still missing, 3 who closes or gives it
		std::string text;       // UTF-8, "Odevzdej: Lobart"
		std::string who;        // the character of the goal, UTF-8, may be empty
		std::string line;       // the line of the dialog, UTF-8, may be empty
		std::string area;       // "Město · tržiště"
		zVEC3 pos;              // the goal itself
		zVEC3 dest;             // where the hero should stand for an item or a chest
		float metres = 0.0f;
		int npc = -1;           // NPC instance of a dialog, carrier or enemy
		int place = kNpcHere;   // NpcPlaceState of that NPC
		int step = -1;          // the quest step of a dialog goal
		int dialog = -1;        // model.dialogs index of a dialog goal
		int item = -1;          // item goals: the item
		int amount = 0;         // and how many the quest wants
	};

	struct QuestNext {
		std::vector<QuestGoal> goals;   // the best first
		std::string note;               // what is not a place: gold to have, another world
		std::string elsewhere;          // world key of a goal in another world
		bool finisherDead = false;      // everyone who completes the quest is dead
		std::string deadName;
		int failNow = -1;               // a quest step offered now that fails the quest
	};

	// ------------------------------------------------------------------ one look at the world

	struct QuestItemSpots {
		bool ground = false;
		bool chest = false;
		zVEC3 groundPos;
		zVEC3 chestPos;
		zVEC3 chestDest;
		oCNpc* carrier = nullptr;
	};

	struct QuestScanNow {
		std::unordered_map<int, oCNpc*> npcs;          // instance -> NPC, the living one first
		std::unordered_map<int, oCInfo*> infos;        // C_INFO instance -> dialog
		std::unordered_map<oCNpc*, NpcPlace> places;
		std::unordered_map<int, int> offered;          // C_INFO instance -> 1 offered now
		std::unordered_map<int, QuestItemSpots> items;
		zVEC3 hero;
		std::string world;
		int self = -1;
		int other = -1;
	};

	static void StartQuestScan(QuestScanNow& s) {
		s.npcs = WorldNpcsByInstance();
		s.infos = InfosByInstance();
		s.hero = player->GetPositionWorld();
		s.world = WorldKeyFromPath(CurrentWorldKey());
		s.self = ScriptSymbolIndex("SELF");
		s.other = ScriptSymbolIndex("OTHER");
	}

	static const NpcPlace& PlaceOf(QuestScanNow& s, oCNpc* npc) {
		std::unordered_map<oCNpc*, NpcPlace>::iterator it = s.places.find(npc);
		if (it != s.places.end())
			return it->second;
		return s.places[npc] = WhereIsNpc(npc);
	}

	static float Distance2(const zVEC3& a, const zVEC3& b) {
		const float dx = a.n[0] - b.n[0], dy = a.n[1] - b.n[1], dz = a.n[2] - b.n[2];
		return dx * dx + dy * dy + dz * dz;
	}

	// The same check the game makes before it offers the dialog.
	static bool OfferedNow(QuestScanNow& s, oCInfo* info, oCNpc* npc) {
		if (!info || !npc || s.self < 0 || s.other < 0)
			return false;
		std::unordered_map<int, int>::iterator it = s.offered.find(info->instance);
		if (it != s.offered.end())
			return it->second != 0;
		bool yes = false;
		{
			ScriptInstanceScope scope;
			parser->SetInstance(s.self, npc);
			parser->SetInstance(s.other, player);
			yes = info->InfoConditions() == 1;
		}
		s.offered[info->instance] = yes ? 1 : 0;
		return yes;
	}

	// Chests of the world, read once per world (their content is read every time).
	struct QuestChests {
		oCWorld* world = nullptr;
		std::vector<oCMobContainer*> list;
	};

	static QuestChests g_questChests;

	static const std::vector<oCMobContainer*>& WorldChests() {
		oCWorld* world = GameWorld();
		if (world == g_questChests.world)
			return g_questChests.list;
		g_questChests.world = world;
		g_questChests.list.clear();
		if (!world)
			return g_questChests.list;
		zCArray<zCVob*> found;
		world->SearchVobListByBaseClass(oCMobContainer::classDef, found, &world->globalVobTree);
		for (int i = 0; i < found.GetNumInList(); i++)
			if (oCMobContainer* chest = zDYNAMIC_CAST<oCMobContainer>(found[i]))
				g_questChests.list.push_back(chest);
		::GT::Log::Write("Quest: %d chests in this world", (int)g_questChests.list.size());
		return g_questChests.list;
	}

	// Where the hero can get an item: the nearest piece lying around, the
	// nearest chest with it and the nearest character carrying it.
	static const QuestItemSpots& ItemSpotsFor(QuestScanNow& s, int sym) {
		std::unordered_map<int, QuestItemSpots>::iterator known = s.items.find(sym);
		if (known != s.items.end())
			return known->second;
		QuestItemSpots spots;
		oCWorld* world = GameWorld();
		float best = 1.0e30f;
		if (world && world->voblist_items)
			for (auto* node = world->voblist_items->next; node; node = node->next) {
				oCItem* item = node->data;
				if (!item || item->GetInstance() != sym)
					continue;
				zCTree<zCVob>* tree = item->globalVobTreeNode;
				if (!tree || (tree->parent && tree->parent->data && zDYNAMIC_CAST<oCNpc>(tree->parent->data)))
					continue;   // not placed, or held by someone
				const zVEC3 p = item->GetPositionWorld();
				const float d = Distance2(p, s.hero);
				if (d < best) {
					best = d;
					spots.ground = true;
					spots.groundPos = p;
				}
			}
		best = 1.0e30f;
		for (oCMobContainer* chest : WorldChests()) {
			bool has = false;
			for (auto* n = chest->containList.next; n && !has; n = n->next)
				has = n->data && n->data->GetInstance() == sym;
			if (!has)
				continue;
			const zVEC3 p = chest->GetPositionWorld();
			const float d = Distance2(p, s.hero);
			if (d < best) {
				best = d;
				spots.chest = true;
				spots.chestPos = p;
				// stand on the side the hero comes from, a wall in the way shortens it
				zVEC3 from = p;
				from.n[1] += 90.0f;
				zVEC3 toward = PlaceVec(s.hero.n[0] - p.n[0], 0.0f, s.hero.n[2] - p.n[2]);
				spots.chestDest = FreeSpotInFront(from, toward, 110.0f, chest);
			}
		}
		best = 1.0e30f;
		ForEachWorldNpc([&](oCNpc* npc) {
			if (npc->inventory2.GetAmount(sym) <= 0)
				return;
			const NpcPlace& p = PlaceOf(s, npc);
			if (!p.Usable())
				return;
			const float d = Distance2(p.pos, s.hero);
			if (d < best) {
				best = d;
				spots.carrier = npc;
			}
		});
		return s.items[sym] = spots;
	}

	// ------------------------------------------------------------------ where a goal is

	static bool GenericPlaceWord(const char* word) {
		static const char* const kWords[] = { "cesta", u8"výskyt", "vchod", "spojka", "pohyb", u8"místo", "oblast", "strana", u8"střed",
			"stezka", u8"→", "uvnitř", "venku", "nahoře", "dole", "vlevo", "vpravo", "vpředu", "vzadu", "malý", "hlavní" };
		for (const char* w : kWords)
			if (std::strcmp(word, w) == 0)
				return true;
		return false;
	}

	// The area a waypoint belongs to, from its first telling word:
	// "NW_CITY_MERCHANT_SHOP01_IN_02" -> "Město · tržiště",
	// "NW_FARM1_PATH_SPAWN_07" -> "Lobartova farma", "OW_PATH_161" -> "".
	static std::string AreaOfWaypoint(const std::string& raw) {
		std::vector<std::string> tokens;
		std::string cur;
		for (char ch : raw) {
			if (ch == '_' || ch == ' ' || ch == '-') {
				if (!cur.empty())
					tokens.push_back(cur);
				cur.clear();
			}
			else
				cur += (char)toupper((unsigned char)ch);
		}
		if (!cur.empty())
			tokens.push_back(cur);
		size_t i = tokens.size() > 1 && IsWorldPrefix(tokens[0]) ? 1 : 0;
		auto wordAt = [&](size_t k, size_t& used) -> const char* {
			used = 1;
			if (k + 1 < tokens.size())
				for (const TokenPhrase& p : kWaypointPhrases)
					if (tokens[k] == p.a && tokens[k + 1] == p.b) {
						used = 2;
						return p.czech;
					}
			if (const char* w = WaypointWord(tokens[k]))
				return w;
			std::string base = tokens[k];   // "TAVERN01" -> "TAVERN"
			while (!base.empty() && isdigit((unsigned char)base.back()))
				base.pop_back();
			return base.empty() || base == tokens[k] ? nullptr : WaypointWord(base);
		};
		for (; i < tokens.size(); i++) {
			size_t used = 1;
			const char* word = wordAt(i, used);
			if (!word || GenericPlaceWord(word)) {
				i += used - 1;
				continue;
			}
			std::string area = CapitalizeFirst(word);
			if (tokens[i] == "CITY") {
				static const char* const kDistricts[] = { u8"přístav", u8"tržiště", u8"horní čtvrť", u8"nádvoří horní čtvrti", u8"kasárna",
					u8"kanály", u8"radnice", u8"kovárna", u8"hospoda", u8"chrám", u8"tábor paladinů", u8"hradby", u8"loděnice", u8"maják",
					u8"chudinská čtvrť", u8"Rudá lucerna", u8"hlavní ulice" };
				for (size_t k = i + 1; k < tokens.size() && k < i + 4; k++) {
					size_t skip = 1;
					const char* district = wordAt(k, skip);
					if (!district)
						continue;
					for (const char* d : kDistricts)
						if (std::strcmp(district, d) == 0)
							return area + u8" · " + d;
				}
			}
			return area;
		}
		return std::string();
	}

	static std::string AreaOf(const zVEC3& pos) {
		oCWorld* world = GameWorld();
		if (!world || !world->wayNet)
			return std::string();
		zCWaypoint* wp = world->wayNet->GetNearestWaypoint(pos);
		return wp ? AreaOfWaypoint(wp->name.ToChar()) : std::string();
	}

	// ------------------------------------------------------------------ the next step of a quest

	static std::string CountText(int have, int need) {
		if (need <= 1)
			return std::string();
		return ::GT::Text::Format(" (%d/%d)", have, need);
	}

	static std::string ItemTitle(int sym) {
		std::unordered_map<int, std::string>::const_iterator it = g_questDb.names.find(sym);
		if (it != g_questDb.names.end())
			return it->second;
		zCPar_Symbol* s = parser ? parser->GetSymbol(sym) : nullptr;
		return s ? std::string(s->name.ToChar()) : std::string("?");
	}

	static std::string LiveNpcName(oCNpc* npc, int sym) {
		std::string name = NpcName(npc);
		return name.empty() ? QuestName(sym) : name;
	}

	// status: of the topic in the diary, -1 when it is not there yet.
	// offersOnly: for a quest not taken yet only who offers it right now.
	static QuestNext ResolveQuestGoals(QuestScanNow& s, int questIndex, int status, size_t limit, bool offersOnly = false) {
		QuestNext out;
		const QuestDatabase& db = g_questDb;
		if (!db.ready || questIndex < 0 || questIndex >= (int)db.model.quests.size())
			return out;
		const QS::Quest& q = db.model.quests[questIndex];

		auto alive = [&](int sym) -> oCNpc* {
			std::unordered_map<int, oCNpc*>::const_iterator it = s.npcs.find(sym);
			return it != s.npcs.end() && !it->second->IsDead() ? it->second : nullptr;
		};
		auto noteWorld = [&](int sym) {
			if (!out.elsewhere.empty() || s.npcs.count(sym))
				return;
			std::unordered_map<int, std::string>::const_iterator w = db.worldOf.find(sym);
			if (w != db.worldOf.end() && w->second != s.world)
				out.elsewhere = w->second;
		};
		auto open = [&](int sym, bool permanent) {
			oCInfo* i = InfoIn(s.infos, sym);
			return i && (!i->told || permanent);
		};
		auto add = [&](QuestGoal& g) {
			for (const QuestGoal& o : out.goals)
				if (o.text == g.text && Distance2(o.pos, g.pos) < 1.0f)
					return;
			g.metres = sqrtf(Distance2(s.hero, g.pos)) / 100.0f;
			out.goals.push_back(g);
		};
		auto addNpc = [&](int kind, int tier, oCNpc* npc, int sym, const std::string& prefix, int step, int dialog) {
			const NpcPlace& p = PlaceOf(s, npc);
			if (!p.Usable())
				return;   // moved out of the story for now
			QuestGoal g;
			g.kind = kind;
			g.tier = tier;
			g.who = LiveNpcName(npc, sym);
			g.text = prefix + g.who;
			g.pos = p.pos;
			g.dest = p.pos;
			g.npc = sym;
			g.place = p.state;
			g.step = step;
			g.dialog = dialog;
			if (dialog >= 0)
				g.line = db.dialogs[dialog].line;
			add(g);
		};

		if (status < 0) {
			// not taken yet: who offers it now, otherwise who gives it
			for (size_t i = 0; i < q.steps.size(); i++) {
				if (!(q.steps[i].roles & QS::kRoleStart))
					continue;
				const QS::DialogFacts& f = db.model.dialogs[q.steps[i].dialog];
				if (!open(f.dialog.info, f.dialog.permanent))
					continue;
				oCNpc* npc = alive(f.dialog.npc);
				if (!npc) {
					noteWorld(f.dialog.npc);
					continue;
				}
				if (OfferedNow(s, InfoIn(s.infos, f.dialog.info), npc))
					addNpc(kGoalTake, 1, npc, f.dialog.npc, u8"Vezmi si úkol: ", (int)i, q.steps[i].dialog);
			}
			if (out.goals.empty() && !offersOnly)
				for (size_t i = 0; i < q.steps.size(); i++) {
					if (!(q.steps[i].roles & QS::kRoleStart))
						continue;
					const QS::DialogFacts& f = db.model.dialogs[q.steps[i].dialog];
					if (!open(f.dialog.info, f.dialog.permanent))
						continue;
					if (oCNpc* npc = alive(f.dialog.npc))
						addNpc(kGoalGiver, 3, npc, f.dialog.npc, u8"Zadává: ", (int)i, q.steps[i].dialog);
				}
		}
		else if (status == (int)oCLogTopic::zELogTopicStatus_Running) {
			// 1. what the characters of this quest offer right now
			for (size_t i = 0; i < q.steps.size(); i++) {
				const QS::Step& st = q.steps[i];
				const QS::DialogFacts& f = db.model.dialogs[st.dialog];
				if (!open(f.dialog.info, f.dialog.permanent))
					continue;
				oCNpc* npc = alive(f.dialog.npc);
				if (!npc) {
					noteWorld(f.dialog.npc);
					continue;
				}
				if (!OfferedNow(s, InfoIn(s.infos, f.dialog.info), npc))
					continue;
				if ((st.roles & QS::kRoleFail) && !(st.roles & QS::kRoleSuccess)) {
					if (out.failNow < 0)
						out.failNow = (int)i;   // never lead into failing it
					continue;
				}
				const bool finish = (st.roles & QS::kRoleSuccess) != 0;
				const bool clean = !(st.roles & QS::kRoleFail);
				addNpc(finish ? kGoalFinish : kGoalTalk, finish && clean ? 0 : 1, npc, f.dialog.npc,
					finish ? u8"Odevzdej: " : u8"Promluv si: ", (int)i, st.dialog);
			}

			// 2. what the completing dialog (or, without one, a diary step) still needs
			if (out.goals.empty()) {
				std::vector<int> wanting;   // quest steps
				const int best = BestSuccessStep(q, s.npcs, &s.infos);
				if (best >= 0)
					wanting.push_back(best);
				else
					for (size_t i = 0; i < q.steps.size(); i++) {
						const QS::DialogFacts& f = db.model.dialogs[q.steps[i].dialog];
						if (!(q.steps[i].roles & QS::kRoleFail) && !f.needs.empty() && open(f.dialog.info, f.dialog.permanent))
							wanting.push_back((int)i);
					}
				for (int stepIndex : wanting) {
					const int dialog = q.steps[stepIndex].dialog;
					const QS::DialogFacts& f = db.model.dialogs[dialog];
					// items, an "any of" group counts as one need
					std::vector<int> doneGroups;
					for (const QS::Amount& a : f.needs)
						if (a.group > 0 && player->inventory2.GetAmount(a.item) >= a.amount)
							doneGroups.push_back(a.group);
					for (const QS::Amount& a : f.needs) {
						if (!IsItemInstance(a.item))
							continue;
						const int have = player->inventory2.GetAmount(a.item);
						if (have >= a.amount || (a.group > 0 && std::find(doneGroups.begin(), doneGroups.end(), a.group) != doneGroups.end()))
							continue;
						if (a.item == db.goldItem) {
							if (out.note.empty())
								out.note = ::GT::Text::Format(u8"Potřebuješ %d zlatých (máš %d)", a.amount, have);
							continue;
						}
						const std::string name = ItemTitle(a.item) + CountText(have, a.amount);
						const QuestItemSpots& spots = ItemSpotsFor(s, a.item);
						bool shown = false;
						QuestGoal g;
						g.item = a.item;
						g.amount = a.amount;
						g.tier = 2;
						if (spots.ground) {
							g.kind = kGoalFind;
							g.text = u8"Najdi: " + name;
							g.pos = spots.groundPos;
							g.dest = spots.groundPos;
							g.dest.n[1] += 100.0f;
							add(g);
							shown = true;
						}
						if (spots.chest) {
							g.kind = kGoalChest;
							g.text = u8"V truhle: " + name;
							g.pos = spots.chestPos;
							g.dest = spots.chestDest;
							add(g);
							shown = true;
						}
						if (spots.carrier) {
							const NpcPlace& p = PlaceOf(s, spots.carrier);
							const int sym = spots.carrier->GetInstance();
							g.kind = kGoalCarrier;
							g.who = LiveNpcName(spots.carrier, sym);
							g.text = g.who + u8" má: " + name;
							g.pos = p.pos;
							// this very character: there may be more of the same kind
							g.dest = MeetingSpot(spots.carrier, p);
							g.npc = -1;
							g.place = p.state;
							add(g);
							shown = true;
						}
						std::unordered_map<int, std::vector<int> >::const_iterator givers = db.givers.find(a.item);
						if (givers != db.givers.end())
							for (int giver : givers->second) {
								const QS::DialogFacts& source = db.model.dialogs[giver];
								if (!open(source.dialog.info, source.dialog.permanent))
									continue;
								oCNpc* npc = alive(source.dialog.npc);
								if (!npc) {
									noteWorld(source.dialog.npc);
									continue;
								}
								const bool now = OfferedNow(s, InfoIn(s.infos, source.dialog.info), npc);
								const size_t before = out.goals.size();
								addNpc(kGoalSource, now ? 1 : 2, npc, source.dialog.npc, now ? u8"Promluv si: " : u8"Dá ti to: ", -1, giver);
								if (out.goals.size() > before) {
									out.goals.back().item = a.item;
									out.goals.back().amount = a.amount;
									shown = true;
								}
							}
						if (!shown && out.note.empty())
							out.note = u8"Potřebuješ: " + name;
					}
					// who must die
					for (int target : f.mustBeDead) {
						if (oCNpc* npc = alive(target))
							addNpc(kGoalKill, 2, npc, target, u8"Zabij: ", -1, -1);
						else
							noteWorld(target);
					}
					// dialogs that have to come first
					auto talkFirst = [&](int sym) {
						std::unordered_map<int, int>::const_iterator d = db.dialogOfInfo.find(sym);
						oCInfo* i = InfoIn(s.infos, sym);
						if (d == db.dialogOfInfo.end() || !i || i->told)
							return;
						const int npcSym = db.model.dialogs[d->second].dialog.npc;
						if (oCNpc* npc = alive(npcSym))
							addNpc(kGoalFirst, 2, npc, npcSym, u8"Promluv si: ", -1, d->second);
						else
							noteWorld(npcSym);
					};
					for (int sym : f.needsDialogs)
						talkFirst(sym);
					bool anyTold = false;
					for (int sym : f.anyDialogs) {
						oCInfo* i = InfoIn(s.infos, sym);
						anyTold = anyTold || (i && i->told);
					}
					if (!anyTold)
						for (int sym : f.anyDialogs)
							talkFirst(sym);
					// and who closes it
					if (oCNpc* npc = alive(f.dialog.npc))
						addNpc(kGoalFinisher, 3, npc, f.dialog.npc, u8"Úkol uzavírá: ", stepIndex, dialog);
					else
						noteWorld(f.dialog.npc);
				}
			}

			// nobody left who could complete it
			int finishers = 0, dead = 0;
			for (const QS::Step& st : q.steps) {
				if (!(st.roles & QS::kRoleSuccess))
					continue;
				finishers++;
				const int sym = db.model.dialogs[st.dialog].dialog.npc;
				std::unordered_map<int, oCNpc*>::const_iterator it = s.npcs.find(sym);
				if (it != s.npcs.end() && it->second->IsDead()) {
					dead++;
					if (out.deadName.empty())
						out.deadName = LiveNpcName(it->second, sym);
				}
			}
			out.finisherDead = finishers > 0 && dead == finishers;
		}

		std::stable_sort(out.goals.begin(), out.goals.end(), [](const QuestGoal& a, const QuestGoal& b) {
			return a.tier != b.tier ? a.tier < b.tier : a.metres < b.metres;
		});
		if (out.goals.size() > limit)
			out.goals.resize(limit);
		for (QuestGoal& g : out.goals)
			g.area = AreaOf(g.pos);
		if (out.goals.empty() && !out.elsewhere.empty()) {
			const std::string world = u8"Cíl je v jiném světě: " + WorldDisplayName(out.elsewhere);
			out.note = out.note.empty() ? world : world + u8" · " + out.note;
		}
		return out;
	}

	// ------------------------------------------------------------------ the selected quest

	struct QuestDetailNext {
		int quest = -1;
		QuestNext next;
	};

	static QuestDetailNext g_questNext;

	// Game thread: the live state of the selected quest and its next steps.
	static void RefreshQuestDetail(int questIndex) {
		RefreshQuestLive(questIndex);
		const QuestDatabase& db = g_questDb;
		if (!db.ready || !InGame() || questIndex < 0 || questIndex >= (int)db.model.quests.size())
			return;
		QuestScanNow s;
		StartQuestScan(s);
		QuestDetailNext next;
		next.quest = questIndex;
		next.next = ResolveQuestGoals(s, questIndex, DiaryStatusNow(db.model.quests[questIndex].raw), 16);
		g_questNext = std::move(next);
	}

	// ------------------------------------------------------------------ all quests at once

	struct QuestBrief {
		int tier = 9;              // of the best goal, 9 = no goal found
		int kind = -1;
		int step = -1;             // quest step of the best goal
		float metres = -1.0f;
		zVEC3 pos;                 // of the best goal, when there is one (tier < 9)
		std::string text;          // the best goal, or the note
		std::string area;
		std::string elsewhere;     // world key
		bool finisherDead = false;
		std::string deadName;
		bool failNow = false;
	};

	// A quest the hero can take right now.
	struct QuestOffer {
		int quest = -1;
		int step = -1;
		int npc = -1;
		int place = kNpcHere;
		float metres = 0.0f;
		std::string npcName;
		std::string line;
		std::string area;
	};

	struct QuestBoard {
		std::unordered_map<int, QuestBrief> brief;   // quest index -> running quest
		std::unordered_map<int, int> offerOf;        // quest index -> offers index
		std::vector<QuestOffer> offers;              // nearest first
		bool ready = false;
		bool pending = false;
		double nextRefresh = 0.0;
		unsigned ms = 0;
	};

	static QuestBoard g_questBoard;

	// Game thread: the next step of every running quest and the quests on
	// offer, a few milliseconds while the quest tab is open.
	static void RefreshQuestBoard() {
		struct Finish { ~Finish() { g_questBoard.pending = false; } } finish;
		const QuestDatabase& db = g_questDb;
		if (!db.ready || !InGame())
			return;
		const DWORD started = GetTickCount();
		std::unordered_map<std::string, int> diary;   // raw title -> status
		oCLogManager& log = oCLogManager::GetLogManager();
		for (auto* node = log.m_lstTopics.next; node; node = node->next)
			if (oCLogTopic* t = node->data)
				diary[t->m_strDescription.ToChar()] = (int)t->m_enuStatus;

		QuestScanNow s;
		StartQuestScan(s);
		QuestBoard board;
		board.nextRefresh = g_questBoard.nextRefresh;
		for (size_t i = 0; i < db.model.quests.size(); i++) {
			const QS::Quest& q = db.model.quests[i];
			if (q.raw.empty() || q.section == db.model.logNote)
				continue;
			std::unordered_map<std::string, int>::const_iterator d = diary.find(q.raw);
			if (d == diary.end()) {
				QuestNext next = ResolveQuestGoals(s, (int)i, -1, 1, true);
				if (next.goals.empty() || next.goals[0].kind != kGoalTake)
					continue;
				const QuestGoal& g = next.goals[0];
				QuestOffer o;
				o.quest = (int)i;
				o.step = g.step;
				o.npc = g.npc;
				o.place = g.place;
				o.metres = g.metres;
				o.npcName = g.who;
				o.line = g.line;
				o.area = g.area;
				board.offers.push_back(o);
				continue;
			}
			if (d->second != (int)oCLogTopic::zELogTopicStatus_Running)
				continue;
			QuestNext next = ResolveQuestGoals(s, (int)i, d->second, 1);
			QuestBrief b;
			if (!next.goals.empty()) {
				const QuestGoal& g = next.goals[0];
				b.tier = g.tier;
				b.kind = g.kind;
				b.step = g.step;
				b.metres = g.metres;
				b.pos = g.pos;
				b.text = g.text;
				b.area = g.area;
			}
			else {
				b.text = next.note;
			}
			b.elsewhere = next.elsewhere;
			b.finisherDead = next.finisherDead;
			b.deadName = next.deadName;
			b.failNow = next.failNow >= 0;
			board.brief[(int)i] = b;
		}
		std::stable_sort(board.offers.begin(), board.offers.end(), [](const QuestOffer& a, const QuestOffer& b) {
			return a.metres < b.metres;
		});
		for (size_t k = 0; k < board.offers.size(); k++)
			board.offerOf[board.offers[k].quest] = (int)k;
		board.ready = true;
		board.ms = (unsigned)(GetTickCount() - started);
		if (!g_questBoard.ready || board.ms > 40)
			::GT::Log::Write("Quest board: %d running, %d on offer, %u ms", (int)board.brief.size(), (int)board.offers.size(), board.ms);
		g_questBoard = std::move(board);
	}

	// ------------------------------------------------------------------ actions on goals

	static void QuestTeleportToSpot(float x, float y, float z, std::string name) {
		Enqueue([x, y, z, name]() {
			if (!InGame())
				return;
			RememberHeroSpot();
			MoveHero(PlaceVec(x, y, z));
			::GT::Overlay::Toast(u8"Teleport k: " + name);
		});
	}

	static void QuestKillNpc(int npcSym, std::string name) {
		Enqueue([npcSym, name]() {
			if (!InGame())
				return;
			oCNpc* npc = FindNpcByInstance(npcSym);
			if (!npc) {
				::GT::Overlay::Toast(NotHereText(npcSym, name));
				return;
			}
			if (npc->IsDead()) {
				::GT::Overlay::Toast(name + u8" už je mrtvý");
				return;
			}
			if (!NpcActive(npc)) {
				// a sleeping character would die outside the world
				::GT::Overlay::Toast(name + u8" je daleko – teleportuj se k cíli a zkus to znovu");
				return;
			}
			npc->attribute[NPC_ATR_HITPOINTS] = 0;
			npc->DoDie(player);
			::GT::Overlay::Toast(u8"Zabito: " + name);
			g_questBoard.nextRefresh = 0.0;
			g_questLive.nextRefresh = 0.0;
		});
	}

} // namespace Trainer
} // namespace GOTHIC_NAMESPACE
