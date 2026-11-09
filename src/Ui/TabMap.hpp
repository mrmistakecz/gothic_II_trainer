// Tab "Mapa" – the game's map of this world with the hero, the next steps of
// the running quests, traders, teachers and shrines. Wheel zooms, dragging
// moves the map, a double click teleports the hero there.

namespace GOTHIC_NAMESPACE {
namespace Trainer {
namespace Ui {

	struct MapUiState {
		int map = 0;
		float zoom = 1.0f;
		ImVec2 center = ImVec2(0.5f, 0.5f);   // the texture point in the middle of the view
		bool quests = true;
		bool traders = false;
		bool teachers = false;
		bool shrines = false;
		bool follow = false;
		bool dragging = false;
		oCWorld* world = nullptr;
	};

	static MapUiState g_mapUi;

	struct MapMarker {
		ImVec2 uv;
		ImU32 color;
		float radius;
		std::string text;      // tooltip
		int quest = -1;        // a quest marker opens the quest on a click
		bool ring = false;
	};

	static void RequestQuestBoard() {
		if (g_questDb.ready && (g_questDb.parser != parser || g_questDb.symbols != parser->symtab.table.GetNumInList()))
			g_questDb.ready = false;
		if (!g_questDb.ready) {
			if (!g_questDb.building) {
				g_questDb.building = true;
				Enqueue([]() { BuildQuestDatabase(); });
			}
			return;
		}
		const double now = ImGui::GetTime();
		if (g_questBoard.pending && now > g_questBoard.nextRefresh + 5.0)
			g_questBoard.pending = false;
		if (!g_questBoard.pending && now >= g_questBoard.nextRefresh) {
			g_questBoard.pending = true;
			g_questBoard.nextRefresh = now + 2.0;
			Enqueue([]() { RefreshQuestBoard(); });
		}
	}

	static ImVec2 MapUV(const TrainerMap& m, const zVEC3& p) {
		const QuestMap::Point uv = m.def.UV(p.n[0], p.n[2]);
		return ImVec2(uv.x, uv.y);
	}

	static void TabMap() {
		if (!InGame()) {
			NotInGame();
			return;
		}
		MapUiState& s = g_mapUi;
		oCWorld* world = GameWorld();
		if (!g_maps.read || g_maps.world != world) {
			if (!g_maps.pending) {
				g_maps.pending = true;
				Enqueue([]() { ReadMaps(); });
			}
			ImGui::TextColored(kGold, u8"Načítám mapu…");
			return;
		}
		if (s.world != world) {
			// another world: its own maps, the switches stay
			s.map = 0;
			s.zoom = 1.0f;
			s.center = ImVec2(0.5f, 0.5f);
			s.dragging = false;
			s.world = world;
		}
		if (g_maps.maps.empty()) {
			WrappedText(u8"Pro tenhle svět hra nemá mapu (interiér, Dračí ostrov nebo svět z modu). Teleporty najdeš v záložce Teleporty.", kMuted);
			return;
		}
		if (s.map < 0 || s.map >= (int)g_maps.maps.size())
			s.map = 0;
		TrainerMap& m = g_maps.maps[s.map];

		// --- the header
		for (int i = 0; i < (int)g_maps.maps.size(); i++) {
			if (i)
				ImGui::SameLine();
			if (ImGui::RadioButton(::GT::Text::Format("%s##map%d", g_maps.maps[i].title.c_str(), i).c_str(), s.map == i)) {
				s.map = i;
				s.zoom = 1.0f;
				s.center = ImVec2(0.5f, 0.5f);
			}
		}
		if (g_maps.maps.size() > 1)
			ImGui::SameLine();
		ImGui::Checkbox(u8"Úkoly", &s.quests);
		ImGui::SameLine();
		ImGui::Checkbox(u8"Obchodníci", &s.traders);
		ImGui::SameLine();
		ImGui::Checkbox(u8"Učitelé", &s.teachers);
		ImGui::SameLine();
		ImGui::Checkbox(u8"Svatyně", &s.shrines);
		ImGui::SameLine();
		ImGui::Checkbox(u8"Sledovat hrdinu", &s.follow);
		ImGui::SameLine();
		if (ImGui::SmallButton(u8"Celá mapa")) {
			s.zoom = 1.0f;
			s.center = ImVec2(0.5f, 0.5f);
			s.follow = false;
		}
		if (s.quests)
			RequestQuestBoard();
		if (s.traders || s.teachers)
			RequestMerchants();

		const zVEC3 heroPos = player->GetPositionWorld();
		const ImVec2 heroUV = MapUV(m, heroPos);
		if (s.follow) {
			s.center = heroUV;
			if (s.zoom < 2.0f)
				s.zoom = 2.0f;
		}

		// --- the canvas
		const float legend = ImGui::GetTextLineHeightWithSpacing();
		ImVec2 size = ImGui::GetContentRegionAvail();
		size.y -= legend;
		if (size.x < 50.0f || size.y < 50.0f)
			return;
		const ImVec2 origin = ImGui::GetCursorScreenPos();
		ImGui::InvisibleButton("##mapcanvas", size, ImGuiButtonFlags_MouseButtonLeft);
		const bool hovered = ImGui::IsItemHovered();
		ImGuiIO& io = ImGui::GetIO();
		const float fit = Smaller(size.x / (float)m.width, size.y / (float)m.height);
		auto pxU = [&]() { return (float)m.width * fit * s.zoom; };
		auto pxV = [&]() { return (float)m.height * fit * s.zoom; };
		const ImVec2 mid(origin.x + size.x * 0.5f, origin.y + size.y * 0.5f);
		auto toScreen = [&](const ImVec2& uv) { return ImVec2(mid.x + (uv.x - s.center.x) * pxU(), mid.y + (uv.y - s.center.y) * pxV()); };
		auto toUV = [&](const ImVec2& p) { return ImVec2(s.center.x + (p.x - mid.x) / pxU(), s.center.y + (p.y - mid.y) / pxV()); };

		if (hovered && io.MouseWheel != 0.0f) {
			// zoom around the mouse
			const ImVec2 before = toUV(io.MousePos);
			s.zoom = Clamp(s.zoom * (io.MouseWheel > 0.0f ? 1.25f : 0.8f), 1.0f, 12.0f);
			const ImVec2 after = toUV(io.MousePos);
			s.center.x += before.x - after.x;
			s.center.y += before.y - after.y;
			s.follow = false;
		}
		if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 3.0f)) {
			s.center.x -= io.MouseDelta.x / pxU();
			s.center.y -= io.MouseDelta.y / pxV();
			s.follow = false;
			s.dragging = true;
		}
		if (!ImGui::IsItemActive())
			s.dragging = false;
		// keep some of the map in view
		const float halfU = size.x * 0.5f / pxU(), halfV = size.y * 0.5f / pxV();
		s.center.x = halfU >= 0.5f ? 0.5f : Clamp(s.center.x, halfU, 1.0f - halfU);
		s.center.y = halfV >= 0.5f ? 0.5f : Clamp(s.center.y, halfV, 1.0f - halfV);

		ImDrawList* dl = ImGui::GetWindowDrawList();
		dl->PushClipRect(origin, ImVec2(origin.x + size.x, origin.y + size.y), true);
		dl->AddRectFilled(origin, ImVec2(origin.x + size.x, origin.y + size.y), ImGui::GetColorU32(ImVec4(0.08f, 0.07f, 0.05f, 1.0f)));
		if (IDirectDrawSurface7* surface = MapSurface(m))
			dl->AddImage((ImTextureID)(intptr_t)surface, toScreen(ImVec2(0.0f, 0.0f)), toScreen(ImVec2(1.0f, 1.0f)));
		else
			dl->AddText(ImVec2(origin.x + Em(1.0f), origin.y + Em(1.0f)), ImGui::GetColorU32(kMuted), u8"Načítám obrázek mapy…");

		// --- markers
		std::vector<MapMarker> markers;
		if (s.shrines)
			for (const ShrineEntry& e : Shrines()) {
				MapMarker k;
				k.uv = MapUV(m, e.pos);
				k.color = ImGui::GetColorU32(e.kind == kIdolBeliar ? ImVec4(0.75f, 0.35f, 0.85f, 1.0f) : ImVec4(1.0f, 0.95f, 0.70f, 1.0f));
				k.radius = Em(0.28f);
				k.ring = true;
				static const char* const kNames[] = { u8"Innosova svatyně", u8"Innosova socha", u8"Beliarova socha" };
				k.text = std::string(kNames[e.kind]) + (e.where.empty() ? std::string() : u8" – " + e.where);
				markers.push_back(k);
			}
		if (s.traders && g_merchants.ready)
			for (const TraderEntry& t : g_merchants.traders) {
				if (t.metres < 0.0f || t.dead)
					continue;
				MapMarker k;
				k.uv = MapUV(m, t.pos);
				k.color = ImGui::GetColorU32(ImVec4(0.40f, 0.75f, 0.95f, 1.0f));
				k.radius = Em(0.24f);
				k.text = u8"Obchodník: " + t.name + (t.tradesNow ? std::string() : std::string(u8" (zatím s tebou neobchoduje)"));
				markers.push_back(k);
			}
		if (s.teachers && g_merchants.ready)
			for (const TeacherEntry& t : g_merchants.teachers) {
				if (t.metres < 0.0f || t.dead)
					continue;
				MapMarker k;
				k.uv = MapUV(m, t.pos);
				k.color = ImGui::GetColorU32(ImVec4(0.55f, 0.85f, 0.55f, 1.0f));
				k.radius = Em(0.24f);
				std::vector<std::string> what;
				for (const LessonEntry& l : t.lessons)
					AddUnique(what, LessonKindName(l.kind));
				k.text = u8"Učitel: " + t.name + u8" – " + JoinTexts(what, ", ", u8" a ");
				markers.push_back(k);
			}
		if (s.quests && g_questBoard.ready)
			for (const std::pair<const int, QuestBrief>& b : g_questBoard.brief) {
				if (b.second.tier >= 9)
					continue;
				MapMarker k;
				k.uv = MapUV(m, b.second.pos);
				k.color = ImGui::GetColorU32(TierColor(b.second.tier == 2 ? 1 : b.second.tier));
				k.radius = Em(b.second.tier == 0 ? 0.36f : 0.3f);
				k.text = g_questDb.quests[b.first].title + u8"\n" + b.second.text;
				k.quest = b.first;
				markers.push_back(k);
			}
		const MapMarker* hot = nullptr;
		float hotDistance = Em(0.7f);
		for (const MapMarker& k : markers) {
			if (k.uv.x < -0.02f || k.uv.x > 1.02f || k.uv.y < -0.02f || k.uv.y > 1.02f)
				continue;   // off this map (the city map shows only the city)
			const ImVec2 p = toScreen(k.uv);
			if (k.ring)
				dl->AddCircle(p, k.radius, k.color, 0, Em(0.12f));
			else {
				dl->AddCircleFilled(p, k.radius + 1.5f, IM_COL32(20, 14, 6, 230));
				dl->AddCircleFilled(p, k.radius, k.color);
			}
			if (hovered) {
				const float dx = p.x - io.MousePos.x, dy = p.y - io.MousePos.y;
				const float d = sqrtf(dx * dx + dy * dy);
				if (d < hotDistance) {
					hotDistance = d;
					hot = &k;
				}
			}
		}
		// the hero: an arrow in the direction he looks
		{
			const ImVec2 p = toScreen(heroUV);
			const zVEC3 at = player->GetAtVectorWorld();
			const QuestMap::Point a = m.def.UV(heroPos.n[0] + at.n[0] * 100.0f, heroPos.n[2] + at.n[2] * 100.0f);
			ImVec2 dir(a.x - heroUV.x, a.y - heroUV.y);
			const float len = sqrtf(dir.x * dir.x + dir.y * dir.y);
			if (len > 0.0f) {
				dir.x /= len;
				dir.y /= len;
			}
			const float r = Em(0.55f);
			const ImVec2 tip(p.x + dir.x * r, p.y + dir.y * r);
			const ImVec2 left(p.x - dir.x * r * 0.6f - dir.y * r * 0.6f, p.y - dir.y * r * 0.6f + dir.x * r * 0.6f);
			const ImVec2 right(p.x - dir.x * r * 0.6f + dir.y * r * 0.6f, p.y - dir.y * r * 0.6f - dir.x * r * 0.6f);
			dl->AddTriangleFilled(tip, left, right, IM_COL32(20, 14, 6, 255));
			dl->AddTriangleFilled(ImVec2(tip.x - dir.x, tip.y - dir.y), ImVec2(left.x * 0.85f + p.x * 0.15f, left.y * 0.85f + p.y * 0.15f),
				ImVec2(right.x * 0.85f + p.x * 0.15f, right.y * 0.85f + p.y * 0.15f), ImGui::GetColorU32(kGold));
		}
		dl->PopClipRect();

		// --- mouse: tooltip, double click teleports, a click on a quest opens it
		if (hovered && !s.dragging) {
			const ImVec2 uv = toUV(io.MousePos);
			const bool inside = uv.x >= 0.0f && uv.x <= 1.0f && uv.y >= 0.0f && uv.y <= 1.0f;
			if (hot && ImGui::BeginTooltip()) {
				ImGui::PushTextWrapPos(Em(22.0f));
				ImGui::TextUnformatted(hot->text.c_str());
				ImGui::TextDisabled("%s", hot->quest >= 0 ? u8"dvojklik: teleport · pravé tlačítko: otevřít úkol" : u8"dvojklik: teleport");
				ImGui::PopTextWrapPos();
				ImGui::EndTooltip();
			}
			else if (inside && ImGui::BeginTooltip()) {
				ImGui::TextDisabled(u8"dvojklik: teleport sem");
				ImGui::EndTooltip();
			}
			if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && inside) {
				const ImVec2 target = hot ? hot->uv : uv;
				const float x = m.def.left + target.x * (m.def.right - m.def.left);
				const float z = m.def.top + target.y * (m.def.bottom - m.def.top);
				TeleportToMapPoint(x, z);
			}
			else if (ImGui::IsMouseClicked(ImGuiMouseButton_Right) && hot && hot->quest >= 0) {
				SelectQuest(hot->quest, kShowRunning);
				RequestMainTab(kTabQuests);
			}
		}

		// --- the legend
		ImGui::TextColored(kGold, u8"▲");
		ImGui::SameLine();
		ImGui::TextDisabled(u8"ty");
		if (s.quests) {
			ImGui::SameLine();
			ImGui::TextColored(kGood, u8"•");
			ImGui::SameLine();
			ImGui::TextDisabled(u8"dokončit hned");
			ImGui::SameLine();
			ImGui::TextColored(kGold, u8"•");
			ImGui::SameLine();
			ImGui::TextDisabled(u8"další krok úkolu");
		}
		ImGui::SameLine();
		ImGui::TextDisabled(u8"· kolečko: přiblížení · tažení: posun · dvojklik: teleport");
	}

} // namespace Ui
} // namespace Trainer
} // namespace GOTHIC_NAMESPACE
