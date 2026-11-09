// Tab "Obchod a učení" – who sells what and for how much, who teaches what and
// up to which level, with teleports and one click to the trade screen or the
// lesson menu of the character.

namespace GOTHIC_NAMESPACE {
namespace Trainer {
namespace Ui {

	struct MerchantsUiState {
		char tradeSearch[96] = {};
		bool tradingNow = false;
		int trader = -1;              // NPC instance of the selected trader
		bool allGoods = false;
		char teachSearch[96] = {};
		bool notLearned = false;
		int teachView = 0;            // 0 by skill, 1 by teacher
		int lessonKind = -1;          // selected lesson (kind, what)
		int lessonWhat = 0;
		int teacher = -1;             // NPC instance of the selected teacher
		bool openTeachers = false;    // the search asks for the teachers' sub-tab
	};

	static MerchantsUiState g_merchantsUi;

	static void RequestMerchants() {
		const double now = ImGui::GetTime();
		if (g_merchants.pending && now > g_merchants.nextRefresh + 5.0)
			g_merchants.pending = false;
		if (!g_merchants.pending && now >= g_merchants.nextRefresh) {
			g_merchants.pending = true;
			g_merchants.nextRefresh = now + 2.0;
			Enqueue([]() { RefreshMerchants(); });
		}
	}

	template<typename Entry>
	static std::string EntryWhere(const Entry& e) {
		if (!e.here)
			return e.world.empty() ? std::string(u8"v jiném světě") : WorldDisplayName(e.world);
		if (e.dead)
			return u8"mrtvý";
		if (e.place == kNpcParked)
			return u8"mimo hru";
		std::string where = MetresText(e.metres);
		if (!e.area.empty())
			where += (where.empty() ? "" : u8" · ") + e.area;
		return where.empty() ? std::string(u8"neznámo kde") : where;
	}

	template<typename Entry>
	static bool EntryReachable(const Entry& e) {
		return e.here && !e.dead && e.place != kNpcParked && e.metres >= 0.0f;
	}

	static bool EnsureMerchantsReady() {
		if (!InGame()) {
			NotInGame();
			return false;
		}
		if (g_questDb.ready && (g_questDb.parser != parser || g_questDb.symbols != parser->symtab.table.GetNumInList()))
			g_questDb.ready = false;
		if (!g_questDb.ready) {
			if (!g_questDb.building) {
				g_questDb.building = true;
				Enqueue([]() { BuildQuestDatabase(); });
			}
			ImGui::TextColored(kGold, u8"Čtu obchodníky a učitele ze skriptů hry…");
			return false;
		}
		RequestMerchants();
		if (!g_merchants.ready) {
			ImGui::TextColored(kGold, u8"Zjišťuji, kdo co prodává a učí…");
			return false;
		}
		return true;
	}

	// ------------------------------------------------------------------ traders

	static const TraderEntry* FindTrader(int npc) {
		for (const TraderEntry& t : g_merchants.traders)
			if (t.npc == npc)
				return &t;
		return nullptr;
	}

	static void TraderButtons(const TraderEntry& t) {
		ButtonRow b;
		const bool reach = EntryReachable(t);
		if (!reach || !t.tradesNow)
			ImGui::BeginDisabled();
		if (b.Colored(u8"Obchodovat", kGold))
			TalkAndPickInfo(t.info);
		if (!reach || !t.tradesNow)
			ImGui::EndDisabled();
		Tooltip(t.tradesNow ? u8"Přenese tě k obchodníkovi, začne rozhovor a vybere obchodování – otevře se obrazovka obchodu."
			: u8"Teď s tebou neobchoduje – často až po splnění úkolu nebo když se přidáš k jeho straně.");
		if (!reach)
			ImGui::BeginDisabled();
		if (b.Button(u8"Teleport"))
			QuestTeleportToNpc(t.npc, t.name);
		if (b.Button(u8"Doplnit zboží"))
			TraderRestock(t.npc, t.name);
		Tooltip(u8"Vrátí obchodníkovi zboží, které od té doby, co ho trainer poprvé uviděl (v této hře od načtení), ubylo.");
		if (b.Button(u8"Dát mu 500 zlatých"))
			TraderGiveGold(t.npc, 500, t.name);
		Tooltip(u8"Obchodník platí za tvé věci jen tím zlatem, které má. Takhle od tebe koupí víc.");
		if (!reach)
			ImGui::EndDisabled();
	}

	static void TraderDetail(const TraderEntry& t, const std::string& needle) {
		MerchantsUiState& s = g_merchantsUi;
		ImGui::PushFont(::GT::Overlay::TitleFont());
		WrappedText(t.name, kGold);
		ImGui::PopFont();
		ImGui::TextDisabled("%s", EntryWhere(t).c_str());
		if (t.here && !t.dead) {
			ImGui::SameLine();
			ImGui::TextColored(t.tradesNow ? kGood : kWarn, "%s", t.tradesNow ? u8"· obchoduje s tebou" : u8"· zatím s tebou neobchoduje");
		}
		if (!t.line.empty())
			WrappedText(u8"„" + t.line + u8"“", kMuted);
		TraderButtons(t);
		if (!t.here) {
			ImGui::Spacing();
			WrappedText(u8"Zboží uvidíš, až budeš v jeho světě.", kMuted);
			return;
		}
		ImGui::SeparatorText(::GT::Text::Format(u8"Zboží (%d) · má %s", (int)t.goods.size(), GoldText(t.gold).c_str()).c_str());
		const bool filtering = !needle.empty() && !::GT::Text::Contains(t.folded, needle);
		if (filtering)
			ImGui::Checkbox(u8"Ukázat všechno zboží", &s.allGoods);
		const ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_ScrollY;
		const float tableHeight = ImGui::GetContentRegionAvail().y;
		if (ImGui::BeginTable("##goods", 3, flags, ImVec2(0.0f, tableHeight > Em(6.0f) ? tableHeight : Em(6.0f)))) {
			ImGui::TableSetupScrollFreeze(0, 1);
			ImGui::TableSetupColumn(u8"Předmět", ImGuiTableColumnFlags_WidthStretch, 3.0f);
			ImGui::TableSetupColumn(u8"Ks", ImGuiTableColumnFlags_WidthFixed, Em(3.0f));
			ImGui::TableSetupColumn(u8"Cena", ImGuiTableColumnFlags_WidthFixed, Em(5.5f));
			ImGui::TableHeadersRow();
			int shown = 0;
			for (const TradeGood& g : t.goods) {
				const bool match = !needle.empty() && ::GT::Text::Contains(g.folded, needle);
				if (filtering && !s.allGoods && !match)
					continue;
				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				if (match)
					ImGui::TextColored(kGold, "%s", g.name.c_str());
				else
					ImGui::TextUnformatted(g.name.c_str());
				ImGui::TableNextColumn();
				ImGui::TextDisabled("%d", g.amount);
				ImGui::TableNextColumn();
				const bool affordable = g.value <= g_merchants.heroGold;
				ImGui::TextColored(affordable ? ImGui::GetStyleColorVec4(ImGuiCol_Text) : kMuted, "%d", g.value);
				shown++;
			}
			ImGui::EndTable();
			if (shown == 0 && t.goods.empty())
				ImGui::TextDisabled(u8"Teď nemá nic na prodej.");
		}
	}

	struct GoodHit {
		const TraderEntry* trader;
		const TradeGood* good;
	};

	static void TradersView() {
		MerchantsUiState& s = g_merchantsUi;
		ImGui::SetNextItemWidth(Em(18.0f));
		ImGui::InputTextWithHint("##tradesearch", u8"Hledat zboží nebo obchodníka (luk, lektvar, Bosper…)", s.tradeSearch, sizeof(s.tradeSearch));
		ImGui::SameLine();
		ImGui::Checkbox(u8"Jen kdo obchoduje teď", &s.tradingNow);
		ImGui::SameLine();
		ImGui::TextDisabled(u8"· máš %s", GoldText(g_merchants.heroGold).c_str());
		const std::string needle = ::GT::Text::Fold(s.tradeSearch);

		// goods matching the search, cheapest first
		std::vector<GoodHit> hits;
		if (!needle.empty())
			for (const TraderEntry& t : g_merchants.traders) {
				if (s.tradingNow && !t.tradesNow)
					continue;
				for (const TradeGood& g : t.goods)
					if (::GT::Text::Contains(g.folded, needle))
						hits.push_back({ &t, &g });
			}
		std::stable_sort(hits.begin(), hits.end(), [](const GoodHit& a, const GoodHit& b) {
			return a.good->value != b.good->value ? a.good->value < b.good->value : a.trader->metres < b.trader->metres;
		});

		const TraderEntry* selected = FindTrader(s.trader);
		if (ImGui::BeginTable("##tradersLayout", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_Resizable, ImGui::GetContentRegionAvail())) {
			ImGui::TableSetupColumn("list", ImGuiTableColumnFlags_WidthStretch, 1.1f);
			ImGui::TableSetupColumn("detail", ImGuiTableColumnFlags_WidthStretch, 1.5f);
			ImGui::TableNextColumn();
			if (ImGui::BeginChild("##traderlist", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders)) {
				if (!hits.empty()) {
					ImGui::PushStyleColor(ImGuiCol_Text, kGold);
					ImGui::SeparatorText(::GT::Text::Format(u8"Kde koupit (%d)", (int)hits.size()).c_str());
					ImGui::PopStyleColor();
					for (size_t i = 0; i < hits.size() && i < 200; i++) {
						const GoodHit& h = hits[i];
						ImGui::PushID((int)i);
						const bool sel = selected == h.trader;
						if (ImGui::Selectable("##hit", sel, ImGuiSelectableFlags_AllowOverlap))
							s.trader = h.trader->npc;
						ImGui::SameLine(Em(0.3f));
						ImGui::TextColored(kGold, "%d", h.good->value);
						ImGui::SameLine(Em(3.6f));
						ImGui::TextUnformatted(h.good->name.c_str());
						ImGui::SameLine();
						ImGui::TextDisabled(u8"· %s · %s", h.trader->name.c_str(), EntryWhere(*h.trader).c_str());
						ImGui::PopID();
					}
				}
				int listed = 0;
				for (const TraderEntry& t : g_merchants.traders) {
					if (s.tradingNow && !t.tradesNow)
						continue;
					if (!needle.empty() && !::GT::Text::Contains(t.folded, needle))
						continue;
					if (listed++ == 0) {
						ImGui::PushStyleColor(ImGuiCol_Text, kGold);
						ImGui::SeparatorText(u8"Obchodníci");
						ImGui::PopStyleColor();
					}
					ImGui::PushID(t.npc);
					const bool sel = selected == &t;
					if (ImGui::Selectable("##trader", sel, ImGuiSelectableFlags_AllowOverlap))
						s.trader = t.npc;
					ImGui::SameLine(Em(0.3f));
					ImGui::TextColored(t.tradesNow ? kGood : kMuted, "%s", t.tradesNow ? u8"•" : u8"·");
					ImGui::SameLine();
					ImGui::TextUnformatted(t.name.c_str());
					ImGui::SameLine();
					ImGui::TextDisabled("%s", EntryWhere(t).c_str());
					ImGui::PopID();
				}
				if (hits.empty() && listed == 0)
					ImGui::TextDisabled(u8"Nic nenalezeno.");
			}
			ImGui::EndChild();
			ImGui::TableNextColumn();
			if (ImGui::BeginChild("##traderdetail", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders)) {
				if (selected)
					TraderDetail(*selected, needle);
				else
					WrappedText(u8"Vyber obchodníka vlevo, nebo napiš, co chceš koupit – uvidíš, kdo to prodává nejlevněji a kde je. "
						u8"• = obchoduje s tebou teď, · = zatím ne (často až po úkolu nebo když se přidáš k jeho straně).", kMuted);
			}
			ImGui::EndChild();
			ImGui::EndTable();
		}
	}

	// ------------------------------------------------------------------ teachers

	static const TeacherEntry* FindTeacher(int npc) {
		for (const TeacherEntry& t : g_merchants.teachers)
			if (t.npc == npc)
				return &t;
		return nullptr;
	}

	static std::string LessonText(const LessonEntry& l) {
		std::string text = l.title;
		if (l.max > 0)
			text += l.kind == kLessonFight ? ::GT::Text::Format(u8" (až %d %%)", l.max) : ::GT::Text::Format(u8" (až %d)", l.max);
		return text;
	}

	static void LessonLine(const LessonEntry& l) {
		const ImVec4 color = l.learned ? kGood : ImGui::GetStyleColorVec4(ImGuiCol_Text);
		ImGui::TextColored(l.learned ? kGood : kMuted, "%s", l.learned ? u8"✓" : u8"•");
		ImGui::SameLine();
		ImGui::TextColored(color, "%s", LessonText(l).c_str());
		if (l.now >= 0) {
			ImGui::SameLine();
			ImGui::TextDisabled(l.kind == kLessonFight ? u8"· máš %d %%" : u8"· máš %d", l.now);
		}
	}

	static void TeacherButtons(const TeacherEntry& t) {
		ButtonRow b;
		const bool reach = EntryReachable(t);
		const bool now = t.infoNow >= 0;
		if (!reach || !now)
			ImGui::BeginDisabled();
		if (b.Colored(u8"Učit se", kGold))
			TalkAndPickInfo(t.infoNow);
		if (!reach || !now)
			ImGui::EndDisabled();
		Tooltip(now ? u8"Přenese tě k učiteli, začne rozhovor a otevře nabídku lekcí. Co se naučíš, vybereš sám – stojí to body učení (a někdy zlato) jako ve hře."
			: u8"Teď tě neučí – často až když mu pomůžeš, zaplatíš nebo se přidáš k jeho straně.");
		if (!reach)
			ImGui::BeginDisabled();
		if (b.Button(u8"Teleport"))
			QuestTeleportToNpc(t.npc, t.name);
		if (b.Button(u8"Promluvit"))
			QuestTalkTo(t.npc, t.name);
		if (!reach)
			ImGui::EndDisabled();
	}

	static void TeacherDetail(const TeacherEntry& t) {
		ImGui::PushFont(::GT::Overlay::TitleFont());
		WrappedText(t.name, kGold);
		ImGui::PopFont();
		ImGui::TextDisabled("%s", EntryWhere(t).c_str());
		if (t.here && !t.dead) {
			ImGui::SameLine();
			ImGui::TextColored(t.infoNow >= 0 ? kGood : kWarn, "%s", t.infoNow >= 0 ? u8"· učí tě" : u8"· zatím tě neučí");
		}
		if (!t.line.empty())
			WrappedText(u8"„" + t.line + u8"“", kMuted);
		TeacherButtons(t);
		int kind = -1;
		for (const LessonEntry& l : t.lessons) {
			if (l.kind != kind) {
				kind = l.kind;
				ImGui::SeparatorText(LessonKindName(kind));
			}
			LessonLine(l);
		}
	}

	struct LessonKey {
		int kind;
		int what;
		std::string title;
		int bestMax;
		bool learnedEverywhere;
		int teachers;
	};

	static void TeachersView() {
		MerchantsUiState& s = g_merchantsUi;
		ImGui::SetNextItemWidth(Em(18.0f));
		ImGui::InputTextWithHint("##teachsearch", u8"Hledat dovednost nebo učitele (luk, síla, alchymie…)", s.teachSearch, sizeof(s.teachSearch));
		ImGui::SameLine();
		ImGui::Checkbox(u8"Jen co ještě neumím", &s.notLearned);
		ImGui::SameLine();
		ImGui::RadioButton(u8"Podle dovednosti", &s.teachView, 0);
		ImGui::SameLine();
		ImGui::RadioButton(u8"Podle učitele", &s.teachView, 1);
		ImGui::SameLine();
		ImGui::TextDisabled(u8"· body učení: %d", g_merchants.learnPoints);
		const std::string needle = ::GT::Text::Fold(s.teachSearch);

		if (ImGui::BeginTable("##teachLayout", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_Resizable, ImGui::GetContentRegionAvail())) {
			ImGui::TableSetupColumn("list", ImGuiTableColumnFlags_WidthStretch, 1.1f);
			ImGui::TableSetupColumn("detail", ImGuiTableColumnFlags_WidthStretch, 1.5f);
			ImGui::TableNextColumn();
			if (s.teachView == 0) {
				// every lesson once, with the best teacher's maximum
				std::vector<LessonKey> keys;
				for (const TeacherEntry& t : g_merchants.teachers)
					for (const LessonEntry& l : t.lessons) {
						size_t k = 0;
						while (k < keys.size() && !(keys[k].kind == l.kind && keys[k].what == l.what))
							k++;
						if (k == keys.size())
							keys.push_back({ l.kind, l.what, l.title, 0, true, 0 });
						keys[k].bestMax = l.max > keys[k].bestMax ? l.max : keys[k].bestMax;
						keys[k].learnedEverywhere = keys[k].learnedEverywhere && l.learned;
						keys[k].teachers++;
					}
				std::stable_sort(keys.begin(), keys.end(), [](const LessonKey& a, const LessonKey& b) {
					return a.kind != b.kind ? a.kind < b.kind : ::GT::Text::Fold(a.title) < ::GT::Text::Fold(b.title);
				});
				if (ImGui::BeginChild("##lessonlist", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders)) {
					int kind = -1, shown = 0;
					for (const LessonKey& k : keys) {
						if (s.notLearned && k.learnedEverywhere)
							continue;
						if (!needle.empty() && !::GT::Text::Contains(::GT::Text::Fold(k.title), needle)
							&& !::GT::Text::Contains(::GT::Text::Fold(LessonKindName(k.kind)), needle))
							continue;
						if (k.kind != kind) {
							kind = k.kind;
							ImGui::PushStyleColor(ImGuiCol_Text, kGold);
							ImGui::SeparatorText(LessonKindName(kind));
							ImGui::PopStyleColor();
						}
						ImGui::PushID(k.kind * 100000 + k.what);
						const bool sel = s.lessonKind == k.kind && s.lessonWhat == k.what;
						if (ImGui::Selectable("##lesson", sel, ImGuiSelectableFlags_AllowOverlap)) {
							s.lessonKind = k.kind;
							s.lessonWhat = k.what;
						}
						ImGui::SameLine(Em(0.3f));
						ImGui::TextColored(k.learnedEverywhere ? kGood : kMuted, "%s", k.learnedEverywhere ? u8"✓" : u8"•");
						ImGui::SameLine();
						ImGui::TextUnformatted(k.title.c_str());
						ImGui::SameLine();
						if (k.bestMax > 0)
							ImGui::TextDisabled(k.kind == kLessonFight ? u8"· až %d %% · učitelů: %d" : u8"· až %d · učitelů: %d", k.bestMax, k.teachers);
						else
							ImGui::TextDisabled(u8"· učitelů: %d", k.teachers);
						ImGui::PopID();
						shown++;
					}
					if (shown == 0)
						ImGui::TextDisabled(u8"Nic nenalezeno.");
				}
				ImGui::EndChild();
				ImGui::TableNextColumn();
				if (ImGui::BeginChild("##lessondetail", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders)) {
					struct Who { const TeacherEntry* teacher; const LessonEntry* lesson; };
					std::vector<Who> who;
					for (const TeacherEntry& t : g_merchants.teachers)
						for (const LessonEntry& l : t.lessons)
							if (l.kind == s.lessonKind && l.what == s.lessonWhat)
								who.push_back({ &t, &l });
					if (who.empty()) {
						WrappedText(u8"Vyber dovednost vlevo – uvidíš všechny, kdo ji učí, do jaké úrovně a kde jsou. ✓ = už umíš, nebo víc, než tě kdo z nich naučí.", kMuted);
					}
					else {
						std::stable_sort(who.begin(), who.end(), [](const Who& a, const Who& b) {
							const bool an = a.teacher->infoNow >= 0, bn = b.teacher->infoNow >= 0;
							if (an != bn)
								return an;
							if (a.lesson->max != b.lesson->max)
								return a.lesson->max > b.lesson->max;
							const bool ah = a.teacher->metres >= 0.0f, bh = b.teacher->metres >= 0.0f;
							if (ah != bh)
								return ah;
							return a.teacher->metres < b.teacher->metres;
						});
						ImGui::PushFont(::GT::Overlay::TitleFont());
						WrappedText(who[0].lesson->title, kGold);
						ImGui::PopFont();
						ImGui::TextDisabled("%s", LessonKindName(who[0].lesson->kind));
						if (who[0].lesson->now >= 0)
							ImGui::TextDisabled(who[0].lesson->kind == kLessonFight ? u8"Máš teď %d %%." : u8"Máš teď %d.", who[0].lesson->now);
						for (size_t i = 0; i < who.size(); i++) {
							const TeacherEntry& t = *who[i].teacher;
							const LessonEntry& l = *who[i].lesson;
							ImGui::PushID((int)i);
							ImGui::Separator();
							ImGui::TextColored(t.infoNow >= 0 ? kGood : kMuted, "%s", t.infoNow >= 0 ? u8"•" : u8"·");
							ImGui::SameLine();
							ImGui::TextColored(kGold, "%s", t.name.c_str());
							if (l.max > 0) {
								ImGui::SameLine();
								ImGui::TextUnformatted(l.kind == kLessonFight ? ::GT::Text::Format(u8"až %d %%", l.max).c_str() : ::GT::Text::Format(u8"až %d", l.max).c_str());
							}
							if (l.learned) {
								ImGui::SameLine();
								ImGui::TextColored(kGood, u8"✓ víc tě nenaučí");
							}
							ImGui::TextDisabled("%s%s", EntryWhere(t).c_str(), t.infoNow >= 0 ? u8" · učí tě" : (t.here && !t.dead ? u8" · zatím tě neučí" : ""));
							TeacherButtons(t);
							ImGui::PopID();
						}
					}
				}
				ImGui::EndChild();
			}
			else {
				const TeacherEntry* selected = FindTeacher(s.teacher);
				if (ImGui::BeginChild("##teacherlist", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders)) {
					int shown = 0;
					for (const TeacherEntry& t : g_merchants.teachers) {
						bool match = needle.empty() || ::GT::Text::Contains(t.folded, needle);
						int open = 0;
						for (const LessonEntry& l : t.lessons) {
							if (!l.learned)
								open++;
							if (!match && (::GT::Text::Contains(l.folded, needle) || ::GT::Text::Contains(::GT::Text::Fold(LessonKindName(l.kind)), needle)))
								match = true;
						}
						if (!match || (s.notLearned && open == 0))
							continue;
						ImGui::PushID(t.npc);
						if (ImGui::Selectable("##teacher", selected == &t, ImGuiSelectableFlags_AllowOverlap))
							s.teacher = t.npc;
						ImGui::SameLine(Em(0.3f));
						ImGui::TextColored(t.infoNow >= 0 ? kGood : kMuted, "%s", t.infoNow >= 0 ? u8"•" : u8"·");
						ImGui::SameLine();
						ImGui::TextUnformatted(t.name.c_str());
						ImGui::SameLine();
						ImGui::TextDisabled(u8"%s · lekcí: %d", EntryWhere(t).c_str(), (int)t.lessons.size());
						ImGui::PopID();
						shown++;
					}
					if (shown == 0)
						ImGui::TextDisabled(u8"Nic nenalezeno.");
				}
				ImGui::EndChild();
				ImGui::TableNextColumn();
				if (ImGui::BeginChild("##teacherdetail", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders)) {
					if (selected)
						TeacherDetail(*selected);
					else
						WrappedText(u8"Vyber učitele vlevo – uvidíš, co všechno učí a do jaké úrovně. • = učí tě teď, · = zatím ne.", kMuted);
				}
				ImGui::EndChild();
			}
			ImGui::EndTable();
		}
	}

	// ------------------------------------------------------------------ tab

	static void TabMerchants() {
		if (!EnsureMerchantsReady())
			return;
		if (ImGui::BeginTabBar("##merchantTabs")) {
			if (ImGui::BeginTabItem(::GT::Text::Format(u8"Obchodníci (%d)###traders", (int)g_merchants.traders.size()).c_str())) {
				TradersView();
				ImGui::EndTabItem();
			}
			const ImGuiTabItemFlags teachFlags = g_merchantsUi.openTeachers ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None;
			g_merchantsUi.openTeachers = false;
			if (ImGui::BeginTabItem(::GT::Text::Format(u8"Učitelé (%d)###teachers", (int)g_merchants.teachers.size()).c_str(), nullptr, teachFlags)) {
				TeachersView();
				ImGui::EndTabItem();
			}
			ImGui::EndTabBar();
		}
	}

} // namespace Ui
} // namespace Trainer
} // namespace GOTHIC_NAMESPACE
