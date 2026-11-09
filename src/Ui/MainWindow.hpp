// The trainer window itself.

namespace GOTHIC_NAMESPACE {
namespace Trainer {
namespace Ui {

	typedef void (*TabFn)();

	static void ScrollingTab(const char* id, TabFn fn) {
		if (ImGui::BeginChild(id))
			fn();
		ImGui::EndChild();
	}

	static void DrawMainWindow() {
		ImGuiIO& io = ImGui::GetIO();
		const float em = ImGui::GetFontSize();

		ImVec2 size(Smaller(em * 56.0f, io.DisplaySize.x * 0.94f), Smaller(em * 38.0f, io.DisplaySize.y * 0.90f));
		ImGui::SetNextWindowSize(size, ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));
		ImGui::SetNextWindowSizeConstraints(
			ImVec2(Smaller(em * 32.0f, io.DisplaySize.x * 0.94f), Smaller(em * 20.0f, io.DisplaySize.y * 0.90f)),
			ImVec2(io.DisplaySize.x, io.DisplaySize.y));

		bool open = true;
		std::string title = ::GT::Text::Format(u8"Gothic Trainer  ·  %s zavře###GothicTrainer", ::GT::Overlay::KeyName(::GT::Settings::Get().toggleKey));
		if (ImGui::Begin(title.c_str(), &open, ImGuiWindowFlags_NoCollapse)) {
			if (ImGui::BeginTabBar("##tabs", ImGuiTabBarFlags_FittingPolicyScroll)) {
				if (ImGui::BeginTabItem(u8"Postava", nullptr, MainTabFlags(kTabHero))) {
					ScrollingTab("##hero_scroll", &TabHero);
					ImGui::EndTabItem();
				}
				if (ImGui::BeginTabItem(u8"Předměty", nullptr, MainTabFlags(kTabItems))) {
					TabItems();
					ImGui::EndTabItem();
				}
				if (ImGui::BeginTabItem(u8"Postavy a tvorové", nullptr, MainTabFlags(kTabNpcs))) {
					TabNpcs();
					ImGui::EndTabItem();
				}
				if (ImGui::BeginTabItem(u8"Úkoly", nullptr, MainTabFlags(kTabQuests))) {
					TabQuests();
					ImGui::EndTabItem();
				}
				if (ImGui::BeginTabItem(u8"Obchod a učení", nullptr, MainTabFlags(kTabMerchants))) {
					TabMerchants();
					ImGui::EndTabItem();
				}
				if (ImGui::BeginTabItem(u8"Mapa", nullptr, MainTabFlags(kTabMap))) {
					TabMap();
					ImGui::EndTabItem();
				}
				if (ImGui::BeginTabItem(u8"Teleporty", nullptr, MainTabFlags(kTabTeleports))) {
					TabTeleports();
					ImGui::EndTabItem();
				}
				if (ImGui::BeginTabItem(u8"Svět", nullptr, MainTabFlags(kTabWorld))) {
					ScrollingTab("##world_scroll", &TabWorld);
					ImGui::EndTabItem();
				}
				if (ImGui::BeginTabItem(u8"Skripty", nullptr, MainTabFlags(kTabScripts))) {
					TabScripts();
					ImGui::EndTabItem();
				}
				if (ImGui::BeginTabItem(u8"Konzole", nullptr, MainTabFlags(kTabConsole))) {
					TabConsole();
					ImGui::EndTabItem();
				}
				if (ImGui::BeginTabItem(u8"Nastavení", nullptr, MainTabFlags(kTabSettings))) {
					ScrollingTab("##settings_scroll", &TabSettings);
					ImGui::EndTabItem();
				}
				// the search sits on the right end of the tab bar
				bool searchOpen = false;
				if (ImGui::BeginTabItem(u8"Hledat###search", nullptr, MainTabFlags(kTabSearch) | ImGuiTabItemFlags_Trailing)) {
					searchOpen = true;
					TabSearch();
					ImGui::EndTabItem();
				}
				static bool searchWasOpen = false;
				if (searchOpen && !searchWasOpen)
					g_search.focus = true;
				searchWasOpen = searchOpen;
				ImGui::EndTabBar();
			}
		}
		ImGui::End();

		if (!open)
			::GT::Overlay::SetVisible(false);
	}

} // namespace Ui
} // namespace Trainer
} // namespace GOTHIC_NAMESPACE
