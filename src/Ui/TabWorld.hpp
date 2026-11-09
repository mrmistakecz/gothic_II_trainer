// Tab "Svět" – time, game speed, weather, the area around the hero, crimes,
// target NPC. Teleports have their own tab (TabTeleports.hpp).

namespace GOTHIC_NAMESPACE {
namespace Trainer {
namespace Ui {

	struct WorldUiState {
		int hour = 12;
		int minute = 0;
		float areaRadius = 1500.0f;   // cm
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

	static void WorldWeatherSection() {
		ImGui::SeparatorText(u8"Počasí");
		if (!WeatherAvailable()) {
			ImGui::TextDisabled(u8"Tady je počasí pevně dané (podzemí nebo vlastní svět módu).");
			return;
		}
		int mode = g_state.weatherMode;
		ImGui::RadioButton(u8"Podle hry", &mode, 0);
		ImGui::SameLine();
		ImGui::RadioButton(u8"Déšť", &mode, 1);
		ImGui::SameLine();
		ImGui::RadioButton(u8"Jasno", &mode, 2);
		if (mode != g_state.weatherMode) {
			g_state.weatherMode = mode;
			Toast(mode == 0 ? u8"Počasí řídí hra" : (mode == 1 ? u8"Začíná pršet" : u8"Obloha se vyjasní"));
		}
		Tooltip(u8"Déšť a jasno vydrží, dokud nevybereš Podle hry. Změna se projeví během několika vteřin.");
	}

	static void WorldAreaSection() {
		ImGui::SeparatorText(u8"Okolí hrdiny");
		WorldUiState& s = g_worldUi;
		ImGui::SetNextItemWidth(Em(12.0f));
		float meters = s.areaRadius / 100.0f;
		if (ImGui::SliderFloat(u8"##arearadius", &meters, 3.0f, 60.0f, u8"okruh %.0f m"))
			s.areaRadius = meters * 100.0f;

		if (ImGui::Button(u8"Odemknout truhly a dveře"))
			UnlockNearby(s.areaRadius);
		Tooltip(u8"Odemkne všechny zamčené truhly a dveře v okruhu – i ty, ke kterým je potřeba klíč.");
		ImGui::SameLine();
		if (ImGui::Button(u8"Sebrat předměty ze země"))
			CollectItemsNearby(s.areaRadius);
		Tooltip(u8"Všechno, co v okruhu leží na zemi, přesune do inventáře. Pozor: sebrané cizí věci hra nepovažuje za krádež.");

		if (ImGui::Button(u8"Uklidnit všechny v okolí"))
			CalmNearby(s.areaRadius);
		Tooltip(u8"Nastaví všem postavám v okruhu neutrální postoj k hrdinovi (jako tlačítko Uklidnit u cíle).");
		ImGui::SameLine();
		if (ImGui::Button(u8"Odpustit všechny zločiny"))
			ForgiveAllCrimes();
		Tooltip(u8"Smaže hledání a pokuty za vraždy, krádeže a napadení ve městě, klášteře, na farmách, na hradě i v táboře banditů (herní skript B_GrantAbsolution).");
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
		ImGui::TextDisabled(u8"Teleporty, waypointy, uložené pozice a cesty do jiných světů jsou v záložce Teleporty.");
		if (ImGui::BeginTable("##world", 2, ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_PadOuterX)) {
			ImGui::TableNextColumn();
			WorldTimeSection();
			WorldSpeedSection();
			WorldWeatherSection();

			ImGui::TableNextColumn();
			WorldAreaSection();
			WorldTargetSection();
			ImGui::EndTable();
		}
	}

} // namespace Ui
} // namespace Trainer
} // namespace GOTHIC_NAMESPACE
