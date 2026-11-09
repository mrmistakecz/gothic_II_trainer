// Tab "Hledat" – one search over everything the trainer knows: quests,
// characters in the world, items (and who sells them), lessons and teachers,
// places and shrines. Every hit has its action right there.

namespace GOTHIC_NAMESPACE {
namespace Trainer {
namespace Ui {

	struct SearchNpcHit {
		oCNpc* npc = nullptr;     // only used on the game thread after NpcStillExists
		int instance = -1;
		std::string name;
		std::string area;
		float metres = -1.0f;
		int place = kNpcUnknown;
		bool dead = false;
	};

	struct SearchState {
		char text[96] = {};
		bool focus = true;
		std::string npcsFor;          // the folded query the NPC hits belong to
		std::vector<SearchNpcHit> npcs;
		bool npcsPending = false;
		double npcsAt = 0.0;
	};

	static SearchState g_search;

	// Game thread: characters in the world whose name or code matches.
	static void SearchWorldNpcs(const std::string& needle) {
		Enqueue([needle]() {
			struct Finish { ~Finish() { g_search.npcsPending = false; } } finish;
			if (!InGame())
				return;
			std::vector<SearchNpcHit> hits;
			const zVEC3 here = player->GetPositionWorld();
			ForEachWorldNpc([&](oCNpc* npc) {
				const std::string name = NpcName(npc);
				if (!::GT::Text::Contains(::GT::Text::Fold(name + " " + FromZ(npc->GetInstanceName())), needle))
					return;
				SearchNpcHit h;
				h.npc = npc;
				h.instance = npc->GetInstance();
				h.name = name;
				h.dead = npc->IsDead() != 0;
				const NpcPlace place = WhereIsNpc(npc);
				h.place = place.state;
				if (place.Usable())
					h.metres = DistanceBetween(here, place.pos) / 100.0f;
				hits.push_back(h);
			});
			std::stable_sort(hits.begin(), hits.end(), [](const SearchNpcHit& a, const SearchNpcHit& b) {
				if (a.dead != b.dead)
					return !a.dead;
				const bool ah = a.metres >= 0.0f, bh = b.metres >= 0.0f;
				if (ah != bh)
					return ah;
				return a.metres < b.metres;
			});
			if (hits.size() > 40)
				hits.resize(40);
			oCWorld* world = GameWorld();
			for (size_t i = 0; i < hits.size() && i < 12; i++)
				if (hits[i].metres >= 0.0f && world && world->wayNet) {
					const NpcPlace place = WhereIsNpc(hits[i].npc);
					hits[i].area = AreaOf(place.pos);
				}
			g_search.npcs.swap(hits);
			g_search.npcsFor = needle;
		});
	}

	static bool SearchSection(const char* title, int count) {
		if (count <= 0)
			return false;
		ImGui::PushStyleColor(ImGuiCol_Text, kGold);
		ImGui::SeparatorText(::GT::Text::Format("%s (%d)", title, count).c_str());
		ImGui::PopStyleColor();
		return true;
	}

	static void MoreHits(int total, int shown) {
		if (total > shown)
			ImGui::TextDisabled(u8"…a dalších %d – upřesni hledání", total - shown);
	}

	static int QuestFilterFor(int status) {
		switch (status) {
			case 1: return kShowRunning;
			case 2: return kShowDone;
			case 3:
			case 4: return kShowFailed;
			default: return kShowNotStarted;
		}
	}

	static void TabSearch() {
		if (!InGame()) {
			NotInGame();
			return;
		}
		SearchState& s = g_search;
		// what the search reads
		if (!g_items.ready && !g_items.building) {
			g_items.building = true;
			Enqueue([]() { BuildItemDatabase(); });
		}
		RequestQuestBoard();
		if (g_questDb.ready)
			RequestMerchants();
		const double now = ImGui::GetTime();
		if (g_diary.pending && now > g_diary.nextRefresh + 5.0)
			g_diary.pending = false;
		if (g_questDb.ready && !g_diary.pending && now >= g_diary.nextRefresh) {
			g_diary.pending = true;
			g_diary.nextRefresh = now + 1.0;
			Enqueue([]() { RefreshDiary(); });
		}

		if (s.focus) {
			ImGui::SetKeyboardFocusHere();
			s.focus = false;
		}
		ImGui::SetNextItemWidth(-FLT_MIN);
		ImGui::InputTextWithHint("##globalsearch", u8"Hledat všude: úkol, postavu, předmět, dovednost, místo… (luk, Lobart, síla, kovárna)", s.text, sizeof(s.text));
		const std::string needle = ::GT::Text::Fold(s.text);
		if (needle.size() < 2) {
			ImGui::Spacing();
			WrappedText(u8"Napiš aspoň dvě písmena. Hledá se najednou v úkolech, postavách ve světě, předmětech (i kdo je prodává), "
				u8"v tom, co kdo učí, a v místech na teleport – u každého výsledku je hned tlačítko, co s ním udělat.", kMuted);
			return;
		}
		if (s.npcsFor != needle || now >= s.npcsAt) {
			if (!s.npcsPending) {
				s.npcsPending = true;
				s.npcsAt = now + 2.0;
				SearchWorldNpcs(needle);
			}
		}
		if (!ImGui::BeginChild("##searchresults", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None)) {
			ImGui::EndChild();
			return;
		}
		int sections = 0;

		// --- quests
		if (g_questDb.ready) {
			std::vector<int> hits;
			for (size_t i = 0; i < g_questDb.quests.size(); i++)
				if (!g_questDb.model.quests[i].raw.empty() && g_questDb.model.quests[i].section != g_questDb.model.logNote
					&& ::GT::Text::Contains(g_questDb.quests[i].folded, needle))
					hits.push_back((int)i);
			if (SearchSection(u8"Úkoly", (int)hits.size())) {
				sections++;
				const int shown = hits.size() < 8 ? (int)hits.size() : 8;
				for (int k = 0; k < shown; k++) {
					const int q = hits[k];
					const DiaryTopic* d = DiaryFor(g_questDb.model.quests[q].raw);
					const int status = d ? d->status : 0;
					ImGui::PushID(q);
					ImGui::TextColored(QuestStatusColor(status), "%s", QuestStatusGlyph(status));
					ImGui::SameLine();
					ImGui::TextUnformatted(g_questDb.quests[q].title.c_str());
					ImGui::SameLine();
					std::unordered_map<int, QuestBrief>::const_iterator b = g_questBoard.brief.find(q);
					if (status == 1 && b != g_questBoard.brief.end() && !b->second.text.empty())
						ImGui::TextColored(TierColor(b->second.tier), "%s", b->second.text.c_str());
					else
						ImGui::TextDisabled("%s", QuestStatusName(status));
					SameLineIfFits(SmallButtonWidth(u8"Otevřít"));
					if (ImGui::SmallButton(u8"Otevřít")) {
						SelectQuest(q, QuestFilterFor(status));
						RequestMainTab(kTabQuests);
					}
					ImGui::PopID();
				}
				MoreHits((int)hits.size(), shown);
			}
		}

		// --- characters in the world
		if (s.npcsFor == needle && SearchSection(u8"Postavy ve světě", (int)s.npcs.size())) {
			sections++;
			const int shown = s.npcs.size() < 10 ? (int)s.npcs.size() : 10;
			for (int k = 0; k < shown; k++) {
				const SearchNpcHit& h = s.npcs[k];
				ImGui::PushID(1000 + k);
				ImGui::TextColored(h.dead ? kMuted : kGold, "%s", h.name.c_str());
				ImGui::SameLine();
				std::string where = h.dead ? std::string(u8"mrtvý") : h.metres >= 0.0f ? MetresText(h.metres) : NpcStateText(h.place);
				if (!h.area.empty())
					where += u8" · " + h.area;
				ImGui::TextDisabled("%s", where.c_str());
				const bool usable = h.metres >= 0.0f;
				if (!usable)
					ImGui::BeginDisabled();
				SameLineIfFits(SmallButtonWidth(u8"Teleport"));
				if (ImGui::SmallButton(u8"Teleport"))
					TeleportHeroToNpc(h.npc, h.name);
				ImGui::SameLine();
				if (ImGui::SmallButton(u8"Přivolat"))
					SummonNpcToHero(h.npc);
				if (!h.dead) {
					ImGui::SameLine();
					if (ImGui::SmallButton(u8"Promluvit"))
						QuestTalkTo(h.instance, h.name);
					Tooltip(kTipTalk);
				}
				if (!usable)
					ImGui::EndDisabled();
				ImGui::PopID();
			}
			MoreHits((int)s.npcs.size(), shown);
		}

		// --- items and who sells them
		if (g_items.ready) {
			std::vector<const ItemEntry*> hits;
			for (const ItemEntry& e : g_items.items)
				if (::GT::Text::Contains(e.searchKey, needle))
					hits.push_back(&e);
			if (SearchSection(u8"Předměty", (int)hits.size())) {
				sections++;
				const int shown = hits.size() < 10 ? (int)hits.size() : 10;
				for (int k = 0; k < shown; k++) {
					const ItemEntry& e = *hits[k];
					ImGui::PushID(2000 + k);
					ImGui::TextUnformatted(e.name.c_str());
					ImGui::SameLine();
					ImGui::TextDisabled(u8"%s · cena %d", CategoryName(e.category), e.value);
					SameLineIfFits(SmallButtonWidth(u8"Dát mi ho"));
					if (ImGui::SmallButton(u8"Dát mi ho"))
						GiveItem(e.index, 1, e.name);
					if (g_merchants.ready) {
						std::vector<std::string> sellers;
						for (const TraderEntry& t : g_merchants.traders)
							for (const TradeGood& g : t.goods)
								if (g.instance == e.index)
									sellers.push_back(t.name + ::GT::Text::Format(" (%d)", g.value));
						if (!sellers.empty()) {
							ImGui::Indent();
							WrappedText(u8"Prodává: " + JoinTexts(sellers, ", ", ", "), kMuted);
							ImGui::Unindent();
						}
					}
					ImGui::PopID();
				}
				MoreHits((int)hits.size(), shown);
			}
		}

		// --- lessons
		if (g_merchants.ready) {
			struct LessonHit { int kind; int what; std::string title; std::vector<std::string> who; };
			std::vector<LessonHit> hits;
			for (const TeacherEntry& t : g_merchants.teachers)
				for (const LessonEntry& l : t.lessons) {
					if (!::GT::Text::Contains(l.folded, needle) && !::GT::Text::Contains(t.folded, needle)
						&& !::GT::Text::Contains(::GT::Text::Fold(LessonKindName(l.kind)), needle))
						continue;
					size_t k = 0;
					while (k < hits.size() && !(hits[k].kind == l.kind && hits[k].what == l.what))
						k++;
					if (k == hits.size())
						hits.push_back({ l.kind, l.what, l.title, {} });
					std::string who = t.name;
					if (l.max > 0)
						who += l.kind == kLessonFight ? ::GT::Text::Format(u8" (až %d %%)", l.max) : ::GT::Text::Format(u8" (až %d)", l.max);
					AddUnique(hits[k].who, who);
				}
			if (SearchSection(u8"Co se dá naučit", (int)hits.size())) {
				sections++;
				const int shown = hits.size() < 8 ? (int)hits.size() : 8;
				for (int k = 0; k < shown; k++) {
					const LessonHit& h = hits[k];
					ImGui::PushID(3000 + k);
					ImGui::TextUnformatted(h.title.c_str());
					ImGui::SameLine();
					ImGui::TextDisabled("%s", LessonKindName(h.kind));
					SameLineIfFits(SmallButtonWidth(u8"Ukázat učitele"));
					if (ImGui::SmallButton(u8"Ukázat učitele")) {
						g_merchantsUi.teachView = 0;
						g_merchantsUi.lessonKind = h.kind;
						g_merchantsUi.lessonWhat = h.what;
						g_merchantsUi.openTeachers = true;
						RequestMainTab(kTabMerchants);
					}
					ImGui::Indent();
					WrappedText(u8"Učí: " + JoinTexts(h.who, ", ", ", "), kMuted);
					ImGui::Unindent();
					ImGui::PopID();
				}
				MoreHits((int)hits.size(), shown);
			}
		}

		// --- places
		{
			struct PlaceHit { std::string label; std::string waypoint; int shrine; };
			std::vector<PlaceHit> hits;
			const std::string worldKey = WorldKeyFromPath(CurrentWorldKey());
			for (const KnownPlace& p : kKnownPlaces) {
				if (worldKey != p.world)
					continue;
				if (::GT::Text::Contains(::GT::Text::Fold(std::string(p.label) + " " + p.group), needle))
					hits.push_back({ std::string(p.label) + u8" · " + p.group, p.waypoint, -1 });
			}
			const std::vector<ShrineEntry>& shrines = Shrines();
			static const char* const kShrineNames[] = { u8"Innosova svatyně", u8"Innosova socha", u8"Beliarova socha" };
			for (size_t i = 0; i < shrines.size(); i++) {
				const std::string label = std::string(kShrineNames[shrines[i].kind]) + (shrines[i].where.empty() ? std::string() : u8" – " + shrines[i].where);
				if (::GT::Text::Contains(::GT::Text::Fold(label + u8" svatyne socha oltar"), needle))
					hits.push_back({ label, std::string(), (int)i });
			}
			if (SearchSection(u8"Místa", (int)hits.size())) {
				sections++;
				const int shown = hits.size() < 10 ? (int)hits.size() : 10;
				for (int k = 0; k < shown; k++) {
					const PlaceHit& h = hits[k];
					ImGui::PushID(4000 + k);
					ImGui::TextUnformatted(h.label.c_str());
					SameLineIfFits(SmallButtonWidth(u8"Teleport"));
					if (ImGui::SmallButton(u8"Teleport")) {
						if (h.shrine >= 0)
							TeleportToShrine(h.shrine);
						else
							TeleportHeroToWaypoint(h.waypoint, h.label);
					}
					ImGui::PopID();
				}
				MoreHits((int)hits.size(), shown);
			}
		}

		if (sections == 0)
			ImGui::TextDisabled("%s", s.npcsPending || !g_items.ready ? u8"Hledám…" : u8"Nic nenalezeno.");
		ImGui::EndChild();
	}

} // namespace Ui
} // namespace Trainer
} // namespace GOTHIC_NAMESPACE
