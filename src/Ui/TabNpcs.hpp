// Tab "Postavy" – spawn any NPC or creature, manage the ones around you.

namespace GOTHIC_NAMESPACE {
namespace Trainer {
namespace Ui {

	struct NpcsUiState {
		char search[96] = {};
		int kind = 0;            // 0 = vše, 1 = tvorové, 2 = lidé
		int count = 1;
		int selected = -1;
		float radius = 3000.0f;  // cm
		std::vector<int> filtered;
		std::string lastSearch;
		int lastKind = -1;
		size_t lastSize = 0;
	};

	static NpcsUiState g_npcsUi;

	struct NearbyNpc {
		oCNpc* npc;
		std::string name;
		std::string code;
		float distance;
		int hp, hpMax, level;
		bool dead;
		bool hostile;
	};

	static void SpawnNpc(const NpcEntry& e, int count) {
		std::string code = e.code;
		std::string label = e.label;
		Enqueue([code, label, count]() {
			if (!InGame())
				return;
			int ok = 0;
			for (int i = 0; i < count; i++)
				if (RunConsoleCommand("insert " + code))
					ok++;
			if (ok > 0)
				::GT::Overlay::Toast(::GT::Text::Format(u8"Vyvoláno: %d× %s", ok, label.c_str()));
			else
				::GT::Overlay::Toast(u8"Vyvolání se nepovedlo: " + label);
		});
	}

	static void RefreshNpcFilter() {
		NpcsUiState& s = g_npcsUi;
		std::string needle = ::GT::Text::Fold(s.search);
		if (needle == s.lastSearch && s.kind == s.lastKind && g_npcDb.list.size() == s.lastSize)
			return;
		s.lastSearch = needle;
		s.lastKind = s.kind;
		s.lastSize = g_npcDb.list.size();
		s.filtered.clear();
		for (int i = 0; i < (int)g_npcDb.list.size(); i++) {
			const NpcEntry& e = g_npcDb.list[i];
			if (s.kind == 1 && e.person)
				continue;
			if (s.kind == 2 && !e.person)
				continue;
			if (!::GT::Text::Contains(e.folded, needle))
				continue;
			s.filtered.push_back(i);
		}
	}

	static void NpcSpawnSection() {
		ImGui::SeparatorText(u8"Vyvolat postavu nebo tvora");
		if (!g_npcDb.ready) {
			if (!g_npcDb.building) {
				g_npcDb.building = true;
				Enqueue([]() { BuildNpcDatabase(); });
			}
			ImGui::TextColored(kGold, u8"Načítám seznam postav ze skriptů…");
			return;
		}

		NpcsUiState& s = g_npcsUi;
		ImGui::SetNextItemWidth(-FLT_MIN);
		ImGui::InputTextWithHint("##npcsearch", u8"Hledat (vlk, troll, vatras, drak…)", s.search, sizeof(s.search));
		ImGui::RadioButton(u8"Vše", &s.kind, 0);
		ImGui::SameLine();
		ImGui::RadioButton(u8"Tvorové", &s.kind, 1);
		ImGui::SameLine();
		ImGui::RadioButton(u8"Lidé", &s.kind, 2);
		ImGui::SameLine();
		ImGui::SetNextItemWidth(Em(6.5f));
		ImGui::InputInt(u8"ks##npccount", &s.count, 1, 5);
		s.count = Clamp(s.count, 1, 50);

		RefreshNpcFilter();

		const float listHeight = Smaller(Em(16.0f), ImGui::GetContentRegionAvail().y - Em(2.5f));
		if (ImGui::BeginChild("##npclist", ImVec2(0.0f, listHeight > Em(6.0f) ? listHeight : Em(6.0f)), ImGuiChildFlags_Borders)) {
			ImGuiListClipper clipper;
			clipper.Begin((int)s.filtered.size());
			while (clipper.Step()) {
				for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++) {
					const NpcEntry& e = g_npcDb.list[s.filtered[row]];
					ImGui::PushID(e.index);
					std::string label = e.label == e.code ? e.code : e.label + "  ·  " + e.code;
					if (ImGui::Selectable(label.c_str(), s.selected == e.index, ImGuiSelectableFlags_AllowDoubleClick)) {
						s.selected = e.index;
						if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
							SpawnNpc(e, s.count);
					}
					ImGui::PopID();
				}
			}
		}
		ImGui::EndChild();

		const NpcEntry* chosen = nullptr;
		for (const NpcEntry& e : g_npcDb.list)
			if (e.index == s.selected)
				chosen = &e;
		if (!chosen)
			ImGui::BeginDisabled();
		if (ImGui::Button(u8"Vyvolat před hrdinou") && chosen)
			SpawnNpc(*chosen, s.count);
		if (!chosen)
			ImGui::EndDisabled();
		ImGui::SameLine();
		ImGui::TextDisabled(u8"%d z %d · dvojklik vyvolá", (int)s.filtered.size(), (int)g_npcDb.list.size());
	}

	static void NpcActionKill(oCNpc* npc) {
		Enqueue([npc]() {
			if (!InGame() || !NpcStillExists(npc) || npc->IsDead())
				return;
			npc->attribute[NPC_ATR_HITPOINTS] = 0;
			npc->DoDie(player);
		});
	}

	static void NpcsNearbySection() {
		ImGui::SeparatorText(u8"Postavy v okolí");
		NpcsUiState& s = g_npcsUi;
		ImGui::SetNextItemWidth(Em(12.0f));
		float meters = s.radius / 100.0f;
		if (ImGui::SliderFloat(u8"##radius", &meters, 5.0f, 200.0f, u8"okruh %.0f m"))
			s.radius = meters * 100.0f;

		const zVEC3 here = player->GetPositionWorld();
		std::vector<NearbyNpc> nearby;
		ForEachWorldNpc([&](oCNpc* npc) {
			float d = DistanceBetween(here, npc->GetPositionWorld());
			if (d > s.radius)
				return;
			NearbyNpc n;
			n.npc = npc;
			n.name = NpcName(npc);
			n.code = FromZ(npc->GetInstanceName());
			n.distance = d;
			n.hp = npc->attribute[NPC_ATR_HITPOINTS];
			n.hpMax = npc->attribute[NPC_ATR_HITPOINTSMAX];
			n.level = npc->level;
			n.dead = npc->IsDead() != 0;
			n.hostile = !n.dead && npc->IsHostile(player) != 0;
			nearby.push_back(n);
		});
		std::sort(nearby.begin(), nearby.end(), [](const NearbyNpc& a, const NearbyNpc& b) { return a.distance < b.distance; });

		int hostiles = 0;
		for (const NearbyNpc& n : nearby)
			if (n.hostile)
				hostiles++;

		ImGui::SameLine();
		if (hostiles == 0)
			ImGui::BeginDisabled();
		std::string killAll = ::GT::Text::Format(u8"Zabít nepřátele (%d)", hostiles);
		if (SmallButtonColored(killAll.c_str(), kWarn)) {
			float radius = s.radius;
			Enqueue([radius]() {
				if (!InGame())
					return;
				const zVEC3 pos = player->GetPositionWorld();
				int killed = 0;
				std::vector<oCNpc*> targets;
				ForEachWorldNpc([&](oCNpc* npc) {
					if (!npc->IsDead() && npc->IsHostile(player) && DistanceBetween(pos, npc->GetPositionWorld()) <= radius)
						targets.push_back(npc);
				});
				for (oCNpc* npc : targets) {
					npc->attribute[NPC_ATR_HITPOINTS] = 0;
					npc->DoDie(player);
					killed++;
				}
				::GT::Overlay::Toast(::GT::Text::Format(u8"Zabito nepřátel: %d", killed));
			});
		}
		if (hostiles == 0)
			ImGui::EndDisabled();

		const ImGuiTableFlags flags = ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter
			| ImGuiTableFlags_SizingStretchProp;
		if (ImGui::BeginTable("##nearby", 4, flags, ImVec2(0.0f, ImGui::GetContentRegionAvail().y))) {
			ImGui::TableSetupScrollFreeze(0, 1);
			ImGui::TableSetupColumn(u8"Jméno", ImGuiTableColumnFlags_WidthStretch, 2.4f);
			ImGui::TableSetupColumn(u8"Vzdálenost", ImGuiTableColumnFlags_WidthFixed, Em(5.0f));
			ImGui::TableSetupColumn(u8"Život", ImGuiTableColumnFlags_WidthFixed, Em(6.5f));
			ImGui::TableSetupColumn(u8"Akce", ImGuiTableColumnFlags_WidthFixed, Em(17.0f));
			ImGui::TableHeadersRow();

			for (const NearbyNpc& n : nearby) {
				ImGui::PushID((void*)n.npc);
				ImGui::TableNextRow();

				ImGui::TableNextColumn();
				ImVec4 color = n.dead ? kMuted : (n.hostile ? kWarn : ImGui::GetStyleColorVec4(ImGuiCol_Text));
				ImGui::PushStyleColor(ImGuiCol_Text, color);
				ImGui::AlignTextToFramePadding();
				ImGui::TextUnformatted(n.name.c_str());
				ImGui::PopStyleColor();
				if (ImGui::BeginItemTooltip()) {
					ImGui::Text("%s", n.code.c_str());
					ImGui::Text(u8"Úroveň %d%s", n.level, n.dead ? u8" · mrtvý" : (n.hostile ? u8" · nepřátelský" : ""));
					ImGui::EndTooltip();
				}

				ImGui::TableNextColumn();
				ImGui::Text("%.0f m", n.distance / 100.0f);

				ImGui::TableNextColumn();
				ImGui::Text("%d / %d", n.hp, n.hpMax);

				ImGui::TableNextColumn();
				oCNpc* npc = n.npc;
				if (!n.dead) {
					if (ImGui::SmallButton(u8"Zabít"))
						NpcActionKill(npc);
					ImGui::SameLine();
					if (ImGui::SmallButton(u8"Přátelský")) {
						Enqueue([npc]() {
							if (!InGame() || !NpcStillExists(npc))
								return;
							npc->SetAttitude(3);    // ATT_FRIENDLY
							npc->SetTmpAttitude(3);
						});
					}
					ImGui::SameLine();
				}
				if (ImGui::SmallButton(u8"Jít k ní")) {
					Enqueue([npc]() {
						if (!InGame() || !NpcStillExists(npc))
							return;
						zVEC3 pos = npc->GetPositionWorld();
						pos.n[0] += 120.0f;
						player->ResetPos(pos);
					});
				}
				ImGui::SameLine();
				if (ImGui::SmallButton(u8"Přivolat")) {
					Enqueue([npc]() {
						if (!InGame() || !NpcStillExists(npc))
							return;
						zVEC3 pos = PointInFrontOfHero(250.0f);
						npc->ResetPos(pos);
					});
				}

				ImGui::PopID();
			}
			ImGui::EndTable();
		}
	}

	static void TabNpcs() {
		if (!InGame()) {
			NotInGame();
			return;
		}
		if (ImGui::BeginTable("##npcs", 2, ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_PadOuterX, ImGui::GetContentRegionAvail())) {
			ImGui::TableNextColumn();
			NpcSpawnSection();
			ImGui::TableNextColumn();
			NpcsNearbySection();
			ImGui::EndTable();
		}
	}

} // namespace Ui
} // namespace Trainer
} // namespace GOTHIC_NAMESPACE
