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
		ImGui::SetNextWindowSizeConstraints(ImVec2(em * 32.0f, em * 20.0f), ImVec2(FLT_MAX, FLT_MAX));

		bool open = true;
		std::string title = ::GT::Text::Format(u8"Gothic Trainer  ·  %s zavře###GothicTrainer", ::GT::Overlay::KeyName(::GT::Settings::Get().toggleKey));
		if (ImGui::Begin(title.c_str(), &open, ImGuiWindowFlags_NoCollapse)) {
			if (ImGui::BeginTabBar("##tabs", ImGuiTabBarFlags_FittingPolicyScroll)) {
				if (ImGui::BeginTabItem(u8"Postava")) {
					ScrollingTab("##hero_scroll", &TabHero);
					ImGui::EndTabItem();
				}
				if (ImGui::BeginTabItem(u8"Předměty")) {
					TabItems();
					ImGui::EndTabItem();
				}
				if (ImGui::BeginTabItem(u8"Postavy a tvorové")) {
					TabNpcs();
					ImGui::EndTabItem();
				}
				if (ImGui::BeginTabItem(u8"Svět")) {
					ScrollingTab("##world_scroll", &TabWorld);
					ImGui::EndTabItem();
				}
				if (ImGui::BeginTabItem(u8"Skripty a úkoly")) {
					TabScripts();
					ImGui::EndTabItem();
				}
				if (ImGui::BeginTabItem(u8"Konzole")) {
					TabConsole();
					ImGui::EndTabItem();
				}
				if (ImGui::BeginTabItem(u8"Nastavení")) {
					ScrollingTab("##settings_scroll", &TabSettings);
					ImGui::EndTabItem();
				}
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
