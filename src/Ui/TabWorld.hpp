// Tab "Svět" – time, game speed, teleports, saved positions, target NPC.

namespace GOTHIC_NAMESPACE {
namespace Trainer {
namespace Ui {

	struct WorldUiState {
		char wpSearch[96] = {};
		int selectedWp = -1;
		char posName[64] = {};
		int hour = 12;
		int minute = 0;
	};

	static WorldUiState g_worldUi;

	static void SetGameTime(int hour, int minute, int dayOffset = 0) {
		Enqueue([hour, minute, dayOffset]() {
			if (!InGame() || !ogame->GetWorldTimer())
				return;
			int day = ogame->GetWorldTimer()->GetDay() + dayOffset;
			ogame->SetTime(day, hour, minute);
			g_state.frozenCaptured = false; // a frozen clock stays at the new time
			::GT::Overlay::Toast(::GT::Text::Format(u8"Čas nastaven na %d:%02d", hour, minute));
		});
	}

	static void WorldTimeSection() {
		ImGui::SeparatorText(u8"Čas");
		oCWorldTimer* timer = ogame->GetWorldTimer();
		if (timer) {
			int h = 0, m = 0;
			timer->GetTime(h, m);
			ImGui::Text(u8"Den %d, %d:%02d", timer->GetDay(), h, m);
		}

		WorldUiState& s = g_worldUi;
		ImGui::SetNextItemWidth(Em(6.0f));
		ImGui::InputInt(u8"h##hour", &s.hour, 1, 6);
		s.hour = Clamp(s.hour, 0, 23);
		ImGui::SameLine();
		ImGui::SetNextItemWidth(Em(6.0f));
		ImGui::InputInt(u8"min##minute", &s.minute, 5, 15);
		s.minute = Clamp(s.minute, 0, 59);
		ImGui::SameLine();
		if (ImGui::Button(u8"Nastavit čas"))
			SetGameTime(s.hour, s.minute);

		if (ImGui::Button(u8"Ráno 6:00")) SetGameTime(6, 0);
		ImGui::SameLine();
		if (ImGui::Button(u8"Poledne")) SetGameTime(12, 0);
		ImGui::SameLine();
		if (ImGui::Button(u8"Večer 20:00")) SetGameTime(20, 0);
		ImGui::SameLine();
		if (ImGui::Button(u8"Půlnoc")) SetGameTime(0, 0, 1);
		ImGui::SameLine();
		if (ImGui::Button(u8"Další den")) SetGameTime(8, 0, 1);

		ImGui::Checkbox(u8"Zastavit čas", &g_state.freezeTime);
		Tooltip(u8"Denní doba se nemění – ideální na věčné poledne.");
		ImGui::SameLine(Em(11.0f));
		bool marvin = ogame->game_testmode != 0;
		if (ImGui::Checkbox(u8"Marvin mód", &marvin)) {
			ogame->game_testmode = marvin ? TRUE : FALSE;
			Toast(marvin ? u8"Marvin mód zapnut (F2 = konzole hry)" : u8"Marvin mód vypnut");
		}
		Tooltip(u8"Původní testovací režim hry – zpřístupní konzoli F2 a klávesy Marvin módu.");
	}

	static void WorldSpeedSection() {
		ImGui::SeparatorText(u8"Rychlost hry");
		if (!ztimer)
			return;
		float f = ztimer->factorMotion;
		ImGui::SetNextItemWidth(Em(14.0f));
		if (ImGui::SliderFloat("##speed", &f, 0.1f, 4.0f, "%.2f×", ImGuiSliderFlags_Logarithmic))
			ztimer->SetMotionFactor(f);
		ImGui::SameLine();
		if (ImGui::Button(u8"Normální"))
			ztimer->SetMotionFactor(1.0f);
		ImGui::SameLine();
		if (ImGui::Button(u8"Zpomalit 0,5×"))
			ztimer->SetMotionFactor(0.5f);
		ImGui::SameLine();
		if (ImGui::Button(u8"Zrychlit 2×"))
			ztimer->SetMotionFactor(2.0f);
	}

	static void TeleportToWaypoint(const WaypointEntry& wp) {
		std::string raw = wp.raw;
		std::string name = wp.name;
		Enqueue([raw, name]() {
			if (!InGame())
				return;
			player->BeamTo(zSTRING(raw.c_str()));
			::GT::Overlay::Toast(u8"Teleport: " + name);
		});
	}

	static void WorldWaypointSection() {
		ImGui::SeparatorText(u8"Teleport na waypoint");
		oCWorld* world = GameWorld();
		if (!g_waypoints.ready || g_waypoints.world != world)
			BuildWaypointDatabase();

		WorldUiState& s = g_worldUi;
		ImGui::SetNextItemWidth(-Em(9.0f));
		ImGui::InputTextWithHint("##wpsearch", u8"Hledat waypoint (např. city, tavern, xardas)…", s.wpSearch, sizeof(s.wpSearch));
		ImGui::SameLine();
		bool canGo = s.selectedWp >= 0 && s.selectedWp < (int)g_waypoints.list.size();
		if (!canGo)
			ImGui::BeginDisabled();
		if (ImGui::Button(u8"Teleportovat", ImVec2(-FLT_MIN, 0.0f)) && canGo)
			TeleportToWaypoint(g_waypoints.list[s.selectedWp]);
		if (!canGo)
			ImGui::EndDisabled();

		std::string needle = ::GT::Text::Fold(s.wpSearch);
		static std::vector<int> filtered;
		filtered.clear();
		for (int i = 0; i < (int)g_waypoints.list.size(); i++)
			if (::GT::Text::Contains(g_waypoints.list[i].folded, needle))
				filtered.push_back(i);

		if (ImGui::BeginChild("##wplist", ImVec2(0.0f, Em(11.0f)), ImGuiChildFlags_Borders)) {
			ImGuiListClipper clipper;
			clipper.Begin((int)filtered.size());
			while (clipper.Step()) {
				for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++) {
					int i = filtered[row];
					const WaypointEntry& wp = g_waypoints.list[i];
					ImGui::PushID(i);
					if (ImGui::Selectable(wp.name.c_str(), s.selectedWp == i, ImGuiSelectableFlags_AllowDoubleClick)) {
						s.selectedWp = i;
						if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
							TeleportToWaypoint(wp);
					}
					ImGui::PopID();
				}
			}
		}
		ImGui::EndChild();
		ImGui::TextDisabled(u8"%d waypointů v tomto světě · dvojklik teleportuje", (int)g_waypoints.list.size());
	}

	static void TeleportToPosition(float x, float y, float z, const std::string& name) {
		Enqueue([x, y, z, name]() {
			if (!InGame())
				return;
			zVEC3 pos;
			pos.n[0] = x;
			pos.n[1] = y;
			pos.n[2] = z;
			player->ResetPos(pos);
			::GT::Overlay::Toast(u8"Teleport: " + name);
		});
	}

	static void WorldPositionsSection() {
		ImGui::SeparatorText(u8"Uložené pozice");
		EnsurePositionsLoaded();
		WorldUiState& s = g_worldUi;

		zVEC3 here = player->GetPositionWorld();
		ImGui::TextDisabled("X %.0f   Y %.0f   Z %.0f", here.n[0], here.n[1], here.n[2]);

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

		int removeIndex = -1;
		if (ImGui::BeginTable("##positions", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
			for (int i = 0; i < (int)g_positions.list.size(); i++) {
				const ::GT::SavedPosition& p = g_positions.list[i];
				ImGui::PushID(i);
				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				ImGui::AlignTextToFramePadding();
				ImGui::TextUnformatted(p.name.c_str());
				ImGui::TableNextColumn();
				if (ImGui::SmallButton(u8"Teleport"))
					TeleportToPosition(p.x, p.y, p.z, p.name);
				ImGui::TableNextColumn();
				if (ImGui::SmallButton(u8"Smazat"))
					removeIndex = i;
				ImGui::PopID();
			}
			ImGui::EndTable();
		}
		if (removeIndex >= 0) {
			g_positions.list.erase(g_positions.list.begin() + removeIndex);
			SavePositionsNow();
		}
		if (g_positions.list.empty())
			ImGui::TextDisabled(u8"Zatím žádné. Pozice se ukládají zvlášť pro každý svět.");
	}

	static void WorldTargetSection() {
		ImGui::SeparatorText(u8"Cíl (na koho se díváš)");
		oCNpc* target = player->GetFocusNpc();
		if (!target) {
			ImGui::TextDisabled(u8"Zaměř postavu nebo zvíře a otevři trainer znovu.");
			return;
		}
		ImGui::TextColored(kGold, "%s", NpcName(target).c_str());
		ImGui::SameLine();
		ImGui::TextDisabled(u8"život %d / %d · úroveň %d", target->attribute[NPC_ATR_HITPOINTS],
			target->attribute[NPC_ATR_HITPOINTSMAX], target->level);

		if (SmallButtonColored(u8"Zabít", kWarn)) {
			Enqueue([target]() {
				if (!InGame() || player->GetFocusNpc() != target || target->IsDead())
					return;
				target->attribute[NPC_ATR_HITPOINTS] = 0;
				target->DoDie(player);
			});
		}
		ImGui::SameLine();
		if (ImGui::Button(u8"Uzdravit")) {
			Enqueue([target]() {
				if (InGame() && player->GetFocusNpc() == target)
					target->CompleteHeal();
			});
		}
		ImGui::SameLine();
		if (ImGui::Button(u8"Uklidnit")) {
			Enqueue([target]() {
				if (!InGame() || player->GetFocusNpc() != target)
					return;
				target->SetAttitude(2);    // ATT_NEUTRAL
				target->SetTmpAttitude(2);
			});
		}
		Tooltip(u8"Nastaví postoj k hrdinovi na neutrální.");
	}

	static void TabWorld() {
		if (!InGame()) {
			NotInGame();
			return;
		}
		if (ImGui::BeginTable("##world", 2, ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_PadOuterX)) {
			ImGui::TableNextColumn();
			WorldWaypointSection();
			WorldPositionsSection();

			ImGui::TableNextColumn();
			WorldTimeSection();
			WorldSpeedSection();
			WorldTargetSection();
			ImGui::EndTable();
		}
	}

} // namespace Ui
} // namespace Trainer
} // namespace GOTHIC_NAMESPACE
