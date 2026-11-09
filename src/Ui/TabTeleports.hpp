// Tab "Teleporty" – named places, any NPC in the world, every waypoint,
// saved positions, other worlds and a way back.

namespace GOTHIC_NAMESPACE {
namespace Trainer {
namespace Ui {

	struct WorldNpcRow {
		oCNpc* npc = nullptr;     // only dereferenced on the game thread after NpcStillExists
		std::string name;
		std::string code;
		std::string folded;
		float distance = 0.0f;
		int place = kNpcHere;     // NpcPlaceState
		bool dead = false;
		bool person = false;
	};

	struct TeleportUiState {
		// known places
		char placeSearch[96] = {};
		std::vector<int> places;          // indices into kKnownPlaces present in this world
		oCWorld* placesWorld = nullptr;
		size_t placesWaypointCount = 0;

		// NPCs of the whole world
		char npcSearch[96] = {};
		bool onlyPeople = false;
		bool hideDead = true;
		std::vector<WorldNpcRow> npcs;
		double npcRefreshAt = -1.0;

		// waypoints
		char wpSearch[96] = {};
		int selectedWp = -1;
		std::vector<int> wpFiltered;
		std::string wpLastSearch;
		size_t wpLastSize = (size_t)-1;

		// saved positions and coordinates
		char posName[64] = {};
		float coords[3] = {};

		// world transitions
		int confirmTransition = -1;
		double confirmUntil = 0.0;
	};

	static TeleportUiState g_teleportUi;

	// Called from ResetSession: nothing may point into the previous world.
	static void ResetTeleportUi() {
		TeleportUiState& s = g_teleportUi;
		s.places.clear();
		s.placesWorld = nullptr;
		s.placesWaypointCount = 0;
		s.npcs.clear();
		s.npcRefreshAt = -1.0;
		s.selectedWp = -1;
		s.wpFiltered.clear();
		s.wpLastSize = (size_t)-1;
		s.confirmTransition = -1;
	}

	static void EnsureWaypoints() {
		oCWorld* world = GameWorld();
		if (!g_waypoints.ready || g_waypoints.world != world)
			BuildWaypointDatabase();
	}

	static bool SameNameIgnoreCase(const std::string& a, const char* b) {
		size_t n = std::strlen(b);
		if (a.size() != n)
			return false;
		for (size_t i = 0; i < n; i++)
			if (toupper((unsigned char)a[i]) != toupper((unsigned char)b[i]))
				return false;
		return true;
	}

	static const char* WaypointDisplay(const WaypointEntry& wp) {
		return wp.label.empty() ? wp.name.c_str() : wp.label.c_str();
	}

	// ------------------------------------------------------------------ header

	static void TeleportHeader() {
		std::string worldKey = WorldKeyFromPath(CurrentWorldKey());
		zVEC3 here = player->GetPositionWorld();
		ImGui::AlignTextToFramePadding();
		ImGui::TextColored(kGold, "%s", WorldDisplayName(worldKey).c_str());
		ImGui::SameLine();
		ImGui::TextDisabled("X %.0f  Y %.0f  Z %.0f", here.n[0], here.n[1], here.n[2]);

		const float right = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x;
		ImGui::SameLine();
		if (ImGui::GetCursorPosX() < right - Em(21.5f))
			ImGui::SetCursorPosX(right - Em(21.5f));   // buttons on the right, never over the text
		const bool canGoBack = !g_teleportBack.empty();
		if (!canGoBack)
			ImGui::BeginDisabled();
		std::string back = ::GT::Text::Format(u8"← Zpět (%d)", (int)g_teleportBack.size());
		if (ImGui::Button(back.c_str()))
			TeleportHeroBack();
		if (!canGoBack)
			ImGui::EndDisabled();
		Tooltip(u8"Vrátí hrdinu na místo před posledním teleportem. Pamatuje si 30 míst v tomto světě.");
		ImGui::SameLine();
		if (ImGui::Button(u8"Vpřed 5 m"))
			NudgeHero(500.0f, 0.0f);
		Tooltip(u8"Posune hrdinu o 5 metrů dopředu – projde zdí nebo mříží.");
		ImGui::SameLine();
		if (ImGui::Button(u8"Nahoru 3 m"))
			NudgeHero(0.0f, 300.0f);
		Tooltip(u8"Vyzvedne hrdinu o 3 metry – když se zasekne v terénu.");
	}

	// ------------------------------------------------------------------ known places

	static void RebuildPlaces() {
		TeleportUiState& s = g_teleportUi;
		s.places.clear();
		s.placesWorld = g_waypoints.world;
		s.placesWaypointCount = g_waypoints.list.size();
		std::string worldKey = WorldKeyFromPath(CurrentWorldKey());
		const int count = (int)(sizeof(kKnownPlaces) / sizeof(kKnownPlaces[0]));
		for (int i = 0; i < count; i++) {
			const KnownPlace& p = kKnownPlaces[i];
			if (worldKey != p.world)
				continue;
			for (const WaypointEntry& wp : g_waypoints.list) {
				if (SameNameIgnoreCase(wp.raw, p.waypoint)) {
					s.places.push_back(i);
					break;
				}
			}
		}
	}

	static void TeleportPlacesTab() {
		TeleportUiState& s = g_teleportUi;
		EnsureWaypoints();
		if (s.placesWorld != g_waypoints.world || s.placesWaypointCount != g_waypoints.list.size())
			RebuildPlaces();

		ImGui::SetNextItemWidth(-FLT_MIN);
		ImGui::InputTextWithHint("##placesearch", u8"Hledat místo nebo postavu (vatras, kovárna, drak…)", s.placeSearch, sizeof(s.placeSearch));
		if (s.places.empty()) {
			ImGui::Spacing();
			ImGui::TextWrapped(u8"Pro tento svět nemám seznam známých míst. Použij Postavy ve světě nebo Waypointy – fungují v každém světě i v módech.");
			return;
		}

		const std::string needle = ::GT::Text::Fold(s.placeSearch);
		if (ImGui::BeginChild("##places", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders)) {
			const char* group = nullptr;
			int shown = 0;
			for (int index : s.places) {
				const KnownPlace& p = kKnownPlaces[index];
				std::string key = ::GT::Text::Fold(std::string(p.label) + " " + p.group + " " + p.waypoint);
				if (!::GT::Text::Contains(key, needle))
					continue;
				if (!group || std::strcmp(group, p.group) != 0) {
					group = p.group;
					ImGui::SeparatorText(group);
				}
				shown++;
				ImGui::PushID(index);
				if (ImGui::SmallButton(u8"Teleport"))
					TeleportHeroToWaypoint(p.waypoint, p.label);
				ImGui::SameLine();
				if (ImGui::Selectable(p.label, false, ImGuiSelectableFlags_AllowDoubleClick)
					&& ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
					TeleportHeroToWaypoint(p.waypoint, p.label);
				if (ImGui::BeginItemTooltip()) {
					ImGui::TextUnformatted(p.waypoint);
					ImGui::TextDisabled(u8"Postavy chodí podle denního rozvrhu – když tu nikdo není, najdi je v Postavách ve světě.");
					ImGui::EndTooltip();
				}
				ImGui::PopID();
			}
			if (shown == 0)
				ImGui::TextDisabled(u8"Nic nenalezeno.");
		}
		ImGui::EndChild();
	}

	// ------------------------------------------------------------------ NPCs in the world

	static void RefreshWorldNpcs() {
		TeleportUiState& s = g_teleportUi;
		s.npcs.clear();
		const zVEC3 here = player->GetPositionWorld();
		ForEachWorldNpc([&](oCNpc* npc) {
			WorldNpcRow r;
			r.npc = npc;
			r.name = NpcName(npc);
			r.code = FromZ(npc->GetInstanceName());
			r.folded = ::GT::Text::Fold(r.name + " " + r.code);
			const NpcPlace place = WhereIsNpc(npc);
			r.place = place.state;
			r.distance = place.Usable() ? DistanceBetween(here, place.pos) : -1.0f;
			r.dead = npc->IsDead() != 0;
			r.person = LooksLikePerson(r.code);
			s.npcs.push_back(std::move(r));
		});
		std::sort(s.npcs.begin(), s.npcs.end(), [](const WorldNpcRow& a, const WorldNpcRow& b) {
			return a.folded != b.folded ? a.folded < b.folded : a.distance < b.distance;
		});
		s.npcRefreshAt = ImGui::GetTime() + 1.0;
	}

	static void TeleportNpcsTab() {
		TeleportUiState& s = g_teleportUi;
		if (s.npcRefreshAt < 0.0 || ImGui::GetTime() >= s.npcRefreshAt)
			RefreshWorldNpcs();

		ImGui::SetNextItemWidth(Em(18.0f));
		ImGui::InputTextWithHint("##npcworldsearch", u8"Jméno nebo kód (lares, diego, troll…)", s.npcSearch, sizeof(s.npcSearch));
		ImGui::SameLine();
		ImGui::Checkbox(u8"Jen lidé", &s.onlyPeople);
		ImGui::SameLine();
		ImGui::Checkbox(u8"Skrýt mrtvé", &s.hideDead);

		const std::string needle = ::GT::Text::Fold(s.npcSearch);
		std::vector<int> rows;
		for (int i = 0; i < (int)s.npcs.size(); i++) {
			const WorldNpcRow& r = s.npcs[i];
			if (s.onlyPeople && !r.person)
				continue;
			if (s.hideDead && r.dead)
				continue;
			if (!::GT::Text::Contains(r.folded, needle))
				continue;
			rows.push_back(i);
		}
		ImGui::TextDisabled(u8"%d z %d postav v celém světě · seznam se obnovuje každou sekundu", (int)rows.size(), (int)s.npcs.size());

		const ImGuiTableFlags flags = ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter
			| ImGuiTableFlags_SizingStretchProp;
		if (ImGui::BeginTable("##worldnpcs", 4, flags, ImVec2(0.0f, ImGui::GetContentRegionAvail().y))) {
			ImGui::TableSetupScrollFreeze(0, 1);
			ImGui::TableSetupColumn(u8"Jméno", ImGuiTableColumnFlags_WidthStretch, 2.0f);
			ImGui::TableSetupColumn(u8"Kód", ImGuiTableColumnFlags_WidthStretch, 2.0f);
			ImGui::TableSetupColumn(u8"Vzdálenost", ImGuiTableColumnFlags_WidthFixed, Em(5.5f));
			ImGui::TableSetupColumn(u8"Akce", ImGuiTableColumnFlags_WidthFixed, Em(10.5f));
			ImGui::TableHeadersRow();

			ImGuiListClipper clipper;
			clipper.Begin((int)rows.size());
			while (clipper.Step()) {
				for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++) {
					const WorldNpcRow& r = s.npcs[rows[row]];
					ImGui::PushID((void*)r.npc);
					ImGui::TableNextRow();
					ImGui::TableNextColumn();
					ImGui::AlignTextToFramePadding();
					if (r.dead)
						ImGui::TextColored(kMuted, u8"%s (mrtvý)", r.name.c_str());
					else
						ImGui::TextUnformatted(r.name.c_str());
					ImGui::TableNextColumn();
					ImGui::TextDisabled("%s", r.code.c_str());
					ImGui::TableNextColumn();
					if (r.distance >= 0.0f)
						ImGui::Text("%.0f m", r.distance / 100.0f);
					else
						ImGui::TextDisabled("%s", NpcStateText(r.place).c_str());
					if (r.place == kNpcAsleep)
						Tooltip(u8"Postava je daleko a hra ji uspala. Vzdálenost je k místu jejího denního rozvrhu, kde ji hra po příchodu probudí.");
					ImGui::TableNextColumn();
					oCNpc* npc = r.npc;
					const bool reachable = r.place == kNpcHere || r.place == kNpcAsleep;
					if (!reachable)
						ImGui::BeginDisabled();
					if (ImGui::SmallButton(u8"Teleport"))
						TeleportHeroToNpc(npc, r.name);
					if (!reachable)
						ImGui::EndDisabled();
					if (!reachable)
						Tooltip(r.place == kNpcParked ? u8"Postava teď ve hře není – hra ji odklidila mimo svět (waypoint TOT)." : u8"Polohu postavy nejde zjistit.");
					ImGui::SameLine();
					if (ImGui::SmallButton(u8"Přivolat"))
						SummonNpcToHero(npc);
					ImGui::PopID();
				}
			}
			ImGui::EndTable();
		}
	}

	// ------------------------------------------------------------------ waypoints

	static void RefreshWaypointFilter() {
		TeleportUiState& s = g_teleportUi;
		std::string needle = ::GT::Text::Fold(s.wpSearch);
		if (needle == s.wpLastSearch && g_waypoints.list.size() == s.wpLastSize)
			return;
		s.wpLastSearch = needle;
		s.wpLastSize = g_waypoints.list.size();
		s.wpFiltered.clear();
		for (int i = 0; i < (int)g_waypoints.list.size(); i++)
			if (::GT::Text::Contains(g_waypoints.list[i].folded, needle))
				s.wpFiltered.push_back(i);
		if (s.selectedWp >= (int)g_waypoints.list.size())
			s.selectedWp = -1;
	}

	static void TeleportWaypointsTab() {
		TeleportUiState& s = g_teleportUi;
		EnsureWaypoints();
		RefreshWaypointFilter();

		ImGui::SetNextItemWidth(-Em(9.0f));
		ImGui::InputTextWithHint("##wpsearch", u8"Hledat (hospoda, kovárna, přístav, city, tavern…)", s.wpSearch, sizeof(s.wpSearch));
		ImGui::SameLine();
		bool canGo = s.selectedWp >= 0 && s.selectedWp < (int)g_waypoints.list.size();
		if (!canGo)
			ImGui::BeginDisabled();
		if (ImGui::Button(u8"Teleportovat", ImVec2(-FLT_MIN, 0.0f)) && canGo) {
			const WaypointEntry& wp = g_waypoints.list[s.selectedWp];
			TeleportHeroToWaypoint(wp.raw, WaypointDisplay(wp));
		}
		if (!canGo)
			ImGui::EndDisabled();

		ImGui::TextDisabled(u8"%d z %d waypointů · česky popsané názvy · dvojklik teleportuje", (int)s.wpFiltered.size(), (int)g_waypoints.list.size());
		if (ImGui::BeginChild("##wplist", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders)) {
			ImGuiListClipper clipper;
			clipper.Begin((int)s.wpFiltered.size());
			while (clipper.Step()) {
				for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++) {
					int i = s.wpFiltered[row];
					const WaypointEntry& wp = g_waypoints.list[i];
					ImGui::PushID(i);
					// one row: Czech description, then the original name in grey
					const float rowX = ImGui::GetCursorPosX();
					if (ImGui::Selectable("##wp", s.selectedWp == i, ImGuiSelectableFlags_AllowDoubleClick | ImGuiSelectableFlags_AllowOverlap)) {
						s.selectedWp = i;
						if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
							TeleportHeroToWaypoint(wp.raw, WaypointDisplay(wp));
					}
					ImGui::SameLine(rowX > 0.0f ? rowX : 1.0f);
					ImGui::TextUnformatted(WaypointDisplay(wp));
					if (!wp.label.empty()) {
						ImGui::SameLine();
						ImGui::TextDisabled("%s", wp.name.c_str());
					}
					ImGui::PopID();
				}
			}
		}
		ImGui::EndChild();
	}

	// ------------------------------------------------------------------ saved positions

	static void TeleportPositionsTab() {
		TeleportUiState& s = g_teleportUi;
		EnsurePositionsLoaded();
		zVEC3 here = player->GetPositionWorld();

		ImGui::SetNextItemWidth(Em(14.0f));
		ImGui::InputTextWithHint("##posname", u8"Název pozice", s.posName, sizeof(s.posName));
		ImGui::SameLine();
		if (ImGui::Button(u8"Uložit tuto pozici")) {
			::GT::SavedPosition p;
			p.name = s.posName[0] ? std::string(s.posName) : ::GT::Text::Format(u8"Pozice %d", (int)g_positions.list.size() + 1);
			p.x = here.n[0];
			p.y = here.n[1];
			p.z = here.n[2];
			g_positions.list.push_back(p);
			SavePositionsNow();
			s.posName[0] = 0;
			Toast(u8"Pozice uložena: " + p.name);
		}
		Tooltip(u8"Pozice se ukládají do GothicTrainer.ini zvlášť pro každý svět a zůstanou i po restartu hry.");

		int removeIndex = -1;
		if (ImGui::BeginChild("##positions", ImVec2(0.0f, -Em(4.2f)), ImGuiChildFlags_Borders)) {
			if (ImGui::BeginTable("##positionsTable", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
				for (int i = 0; i < (int)g_positions.list.size(); i++) {
					const ::GT::SavedPosition& p = g_positions.list[i];
					ImGui::PushID(i);
					ImGui::TableNextRow();
					ImGui::TableNextColumn();
					ImGui::AlignTextToFramePadding();
					ImGui::TextUnformatted(p.name.c_str());
					ImGui::TableNextColumn();
					if (ImGui::SmallButton(u8"Teleport"))
						TeleportHeroToPosition(p.x, p.y, p.z, p.name);
					ImGui::TableNextColumn();
					if (ImGui::SmallButton(u8"Smazat"))
						removeIndex = i;
					ImGui::PopID();
				}
				ImGui::EndTable();
			}
			if (g_positions.list.empty())
				ImGui::TextDisabled(u8"Zatím žádné. Pozice se ukládají zvlášť pro každý svět.");
		}
		ImGui::EndChild();
		if (removeIndex >= 0) {
			g_positions.list.erase(g_positions.list.begin() + removeIndex);
			SavePositionsNow();
		}

		ImGui::SeparatorText(u8"Teleport na souřadnice");
		if (ImGui::SmallButton(u8"Vložit moje"))
			for (int k = 0; k < 3; k++)
				s.coords[k] = here.n[k];
		ImGui::SameLine();
		ImGui::SetNextItemWidth(Em(20.0f));
		ImGui::InputFloat3("##coords", s.coords, "%.0f");
		ImGui::SameLine();
		if (ImGui::Button(u8"Teleport##coords")) {
			bool valid = true;
			for (int k = 0; k < 3; k++)
				if (!std::isfinite(s.coords[k]) || s.coords[k] > 1.0e7f || s.coords[k] < -1.0e7f)
					valid = false;
			if (valid)
				TeleportHeroToPosition(s.coords[0], s.coords[1], s.coords[2], u8"souřadnice");
			else
				Toast(u8"Neplatné souřadnice");
		}
	}

	// ------------------------------------------------------------------ other worlds

	static void TeleportWorldsTab() {
		TeleportUiState& s = g_teleportUi;
		oCWorld* world = GameWorld();
		if (!g_transitions.ready || g_transitions.world != world) {
			if (!g_transitions.building) {
				g_transitions.building = true;
				Enqueue([]() { BuildTransitions(); });
			}
			ImGui::TextColored(kGold, u8"Hledám přechody do jiných světů…");
			return;
		}

		ImGui::TextWrapped(u8"Přechody, které hra v tomto světě má (průsmyk, portál, loď…). Trainer použije stejný přechod, jako když jím hrdina projde.");
		ImGui::TextColored(kWarn, u8"Před cestou si ulož hru. Když příběh cestu ještě neumožňuje, může se úkol zaseknout.");
		ImGui::Spacing();

		if (g_transitions.list.empty()) {
			ImGui::TextDisabled(u8"V tomto světě nejsou žádné přechody do jiných světů.");
			return;
		}

		const zVEC3 here = player->GetPositionWorld();
		const double now = ImGui::GetTime();
		if (s.confirmTransition >= 0 && now > s.confirmUntil)
			s.confirmTransition = -1;

		if (ImGui::BeginTable("##transitions", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersOuter)) {
			ImGui::TableSetupColumn(u8"Cíl", ImGuiTableColumnFlags_WidthStretch, 3.0f);
			ImGui::TableSetupColumn(u8"Vzdálenost", ImGuiTableColumnFlags_WidthFixed, Em(5.5f));
			ImGui::TableSetupColumn(u8"Akce", ImGuiTableColumnFlags_WidthFixed, Em(12.0f));
			ImGui::TableHeadersRow();
			for (int i = 0; i < (int)g_transitions.list.size(); i++) {
				const LevelTransition& t = g_transitions.list[i];
				ImGui::PushID(i);
				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				ImGui::AlignTextToFramePadding();
				ImGui::TextColored(kGold, u8"→ %s", t.display.c_str());
				if (ImGui::BeginItemTooltip()) {
					ImGui::Text(u8"Svět: %s", t.level.c_str());
					if (!t.start.empty())
						ImGui::Text(u8"Příchod: %s", t.start.c_str());
					if (!t.vobName.empty())
						ImGui::TextDisabled("%s", t.vobName.c_str());
					ImGui::EndTooltip();
				}
				ImGui::TableNextColumn();
				ImGui::Text("%.0f m", DistanceBetween(here, MakeVec(t.x, t.y, t.z)) / 100.0f);
				ImGui::TableNextColumn();
				if (s.confirmTransition == i) {
					if (SmallButtonColored(u8"Opravdu přejít?", kWarn)) {
						s.confirmTransition = -1;
						ChangeLevelNow(t);
					}
				}
				else if (ImGui::SmallButton(u8"Přejít do světa")) {
					s.confirmTransition = i;
					s.confirmUntil = now + 5.0;
				}
				ImGui::PopID();
			}
			ImGui::EndTable();
		}
	}

	// ------------------------------------------------------------------ statues and shrines

	static void TeleportShrinesTab() {
		const std::vector<ShrineEntry>& list = Shrines();
		int counts[3] = {};
		for (const ShrineEntry& e : list)
			counts[e.kind]++;
		if (list.empty()) {
			ImGui::TextWrapped(u8"V tomhle světě nejsou žádné Innosovy svatyně ani sochy. Najdeš je v Khorinisu, Beliarovy sochy v Jharkendaru.");
			return;
		}
		ImGui::TextDisabled(u8"Innosových svatyní %d · Innosových soch %d · Beliarových soch %d · jen tento svět", counts[0], counts[1], counts[2]);
		const zVEC3 here = player->GetPositionWorld();
		static const char* const kGroups[] = { u8"Innosovy svatyně – modlitba a požehnání", u8"Innosovy sochy", u8"Beliarovy sochy (Jharkendar)" };
		const ImGuiTableFlags flags = ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_SizingStretchProp;
		if (ImGui::BeginTable("##shrines", 3, flags, ImVec2(0.0f, ImGui::GetContentRegionAvail().y))) {
			ImGui::TableSetupScrollFreeze(0, 1);
			ImGui::TableSetupColumn(u8"Kde", ImGuiTableColumnFlags_WidthStretch, 4.0f);
			ImGui::TableSetupColumn(u8"Vzdálenost", ImGuiTableColumnFlags_WidthFixed, Em(5.5f));
			ImGui::TableSetupColumn(u8"Akce", ImGuiTableColumnFlags_WidthFixed, Em(5.5f));
			ImGui::TableHeadersRow();
			int lastKind = -1;
			for (int i = 0; i < (int)list.size(); i++) {
				const ShrineEntry& e = list[i];
				if (e.kind != lastKind) {
					lastKind = e.kind;
					ImGui::TableNextRow();
					ImGui::TableNextColumn();
					ImGui::TextColored(kGold, "%s", kGroups[e.kind]);
				}
				ImGui::PushID(i);
				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				ImGui::AlignTextToFramePadding();
				ImGui::TextUnformatted(e.where.empty() ? u8"(bez popisu)" : e.where.c_str());
				if (ShrineHaunted(e)) {
					ImGui::SameLine();
					ImGui::TextColored(kWarn, u8"posedlá");
					Tooltip(u8"Od 3. kapitoly jsou některé svatyně posedlé. Očistíš je svěcenou vodou u svatyně (Očista svatyně).");
				}
				if (!e.waypoint.empty())
					Tooltip(::GT::Text::FromGame(e.waypoint.c_str()).c_str());
				ImGui::TableNextColumn();
				ImGui::Text("%.0f m", DistanceBetween(here, e.pos) / 100.0f);
				ImGui::TableNextColumn();
				if (ImGui::SmallButton(u8"Teleport"))
					TeleportToShrine(i);
				ImGui::PopID();
			}
			ImGui::EndTable();
		}
	}

	// ------------------------------------------------------------------ tab

	static void TabTeleports() {
		if (!InGame()) {
			NotInGame();
			return;
		}
		TeleportHeader();
		ImGui::Spacing();
		if (ImGui::BeginTabBar("##tp_tabs")) {
			if (ImGui::BeginTabItem(u8"Známá místa")) {
				TeleportPlacesTab();
				ImGui::EndTabItem();
			}
			if (ImGui::BeginTabItem(u8"Sochy a svatyně")) {
				TeleportShrinesTab();
				ImGui::EndTabItem();
			}
			if (ImGui::BeginTabItem(u8"Postavy ve světě")) {
				TeleportNpcsTab();
				ImGui::EndTabItem();
			}
			if (ImGui::BeginTabItem(u8"Waypointy")) {
				TeleportWaypointsTab();
				ImGui::EndTabItem();
			}
			if (ImGui::BeginTabItem(u8"Uložené pozice")) {
				TeleportPositionsTab();
				ImGui::EndTabItem();
			}
			if (ImGui::BeginTabItem(u8"Jiné světy")) {
				TeleportWorldsTab();
				ImGui::EndTabItem();
			}
			ImGui::EndTabBar();
		}
	}

} // namespace Ui
} // namespace Trainer
} // namespace GOTHIC_NAMESPACE
