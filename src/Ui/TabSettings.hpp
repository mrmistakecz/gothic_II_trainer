// Tab "Nastavení" – hotkey, font size, opacity, mouse, encoding.

namespace GOTHIC_NAMESPACE {
namespace Trainer {
namespace Ui {

	static void TabSettings() {
		::GT::TrainerSettings& st = ::GT::Settings::Get();
		bool changed = false;
		bool reload = false;

		ImGui::SeparatorText(u8"Ovládání");
		RowLabel(u8"Otevřít trainer", Em(12.0f));
		ImGui::SetNextItemWidth(Em(10.0f));
		if (ImGui::BeginCombo("##key", ::GT::Overlay::KeyName(st.toggleKey))) {
			static const int keys[] = {
				VK_F1, VK_F2, VK_F3, VK_F4, VK_F5, VK_F6, VK_F7, VK_F8, VK_F9, VK_F10, VK_F11, VK_F12,
				VK_INSERT, VK_HOME, VK_END, VK_PRIOR, VK_NEXT, VK_DELETE, VK_SCROLL, VK_PAUSE,
			};
			for (int vk : keys) {
				if (ImGui::Selectable(::GT::Overlay::KeyName(vk), st.toggleKey == vk)) {
					st.toggleKey = vk;
					changed = true;
				}
			}
			ImGui::EndCombo();
		}
		HelpMarker(u8"Pozor na klávesy, které už hra používá (F2 konzole, F5/F9 rychlé ukládání, F8 v Marvin módu).");

		RowLabel(u8"Rychlost kurzoru", Em(12.0f));
		ImGui::SetNextItemWidth(Em(12.0f));
		if (ImGui::SliderFloat("##mouse", &st.mouseSpeed, 0.1f, 8.0f, "%.2f×", ImGuiSliderFlags_Logarithmic))
			changed = true;

		if (ImGui::Checkbox(u8"Použít kurzor Windows", &st.systemCursor))
			changed = true;
		HelpMarker(u8"Zapni, pokud hraješ v okně a kurzor trainera se nehýbe nebo je posunutý.");

		ImGui::SeparatorText(u8"Vzhled");
		RowLabel(u8"Velikost písma", Em(12.0f));
		ImGui::SetNextItemWidth(Em(12.0f));
		int font = st.fontSize;
		const char* fontFmt = font == 0 ? u8"automaticky" : "%d px";
		if (ImGui::SliderInt("##font", &font, 0, 36, fontFmt)) {
			if (font != 0 && font < 12)
				font = 12;
			st.fontSize = font;
			changed = true;
		}
		if (ImGui::IsItemDeactivatedAfterEdit())
			reload = true;

		RowLabel(u8"Průhlednost okna", Em(12.0f));
		ImGui::SetNextItemWidth(Em(12.0f));
		if (ImGui::SliderFloat("##alpha", &st.windowAlpha, 0.5f, 1.0f, "%.2f")) {
			ImGui::GetStyle().Colors[ImGuiCol_WindowBg].w = st.windowAlpha;
			changed = true;
		}

		if (ImGui::Checkbox(u8"Zobrazovat oznámení", &st.showToasts))
			changed = true;

		ImGui::SeparatorText(u8"Texty hry");
		static const struct { int cp; const char* name; } pages[] = {
			{ 0, u8"automaticky (podle Windows)" },
			{ 1250, u8"1250 – čeština, polština, maďarština" },
			{ 1251, u8"1251 – ruština" },
			{ 1252, u8"1252 – angličtina, němčina" },
		};
		const char* currentPage = pages[0].name;
		for (const auto& p : pages)
			if (p.cp == st.codePage)
				currentPage = p.name;
		RowLabel(u8"Kódování", Em(12.0f));
		ImGui::SetNextItemWidth(Em(18.0f));
		if (ImGui::BeginCombo("##cp", currentPage)) {
			for (const auto& p : pages) {
				if (ImGui::Selectable(p.name, st.codePage == p.cp)) {
					st.codePage = p.cp;
					::GT::Text::SetCodePage((unsigned)p.cp);
					g_items.ready = false;       // názvy se načtou znovu
					g_itemsUi.lastDbSize = (size_t)-1;
					g_waypoints.ready = false;
					changed = true;
				}
			}
			ImGui::EndCombo();
		}
		HelpMarker(u8"Pokud jsou názvy předmětů s divnými znaky, zkus jiné kódování.");

		if (changed)
			::GT::Settings::Save();
		if (reload)
			::GT::Overlay::RequestReload();

		ImGui::SeparatorText(u8"O programu");
		ImGui::Text(u8"Gothic Trainer %s", TRAINER_VERSION);
		ImGui::TextDisabled(u8"Nastavení: %s", ::GT::Settings::IniPath().c_str());
		ImGui::TextDisabled(u8"Log: %s", ::GT::Log::Path().c_str());
		ImGui::TextDisabled(u8"Grafika: Dear ImGui %s", IMGUI_VERSION);
	}

} // namespace Ui
} // namespace Trainer
} // namespace GOTHIC_NAMESPACE
