// The trainer window itself.

namespace GOTHIC_NAMESPACE {
namespace Trainer {
namespace Ui {

	static void DrawMainWindow() {
		ImGuiIO& io = ImGui::GetIO();
		const float em = ImGui::GetFontSize();

		ImVec2 size(Smaller(em * 52.0f, io.DisplaySize.x * 0.92f), Smaller(em * 36.0f, io.DisplaySize.y * 0.88f));
		ImGui::SetNextWindowSize(size, ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));
		ImGui::SetNextWindowSizeConstraints(ImVec2(em * 30.0f, em * 18.0f), ImVec2(FLT_MAX, FLT_MAX));

		bool open = true;
		std::string title = ::GT::Text::Format(u8"Gothic Trainer  ·  %s zavře###GothicTrainer", ::GT::Overlay::KeyName(::GT::Settings::Get().toggleKey));
		if (ImGui::Begin(title.c_str(), &open, ImGuiWindowFlags_NoCollapse)) {
			if (ImGui::BeginTabBar("##tabs", ImGuiTabBarFlags_FittingPolicyScroll)) {
				if (ImGui::BeginTabItem(u8"Postava")) {
					if (ImGui::BeginChild("##hero_scroll"))
						TabHero();
					ImGui::EndChild();
					ImGui::EndTabItem();
				}
				if (ImGui::BeginTabItem(u8"Předměty")) {
					TabItems();
					ImGui::EndTabItem();
				}
				if (ImGui::BeginTabItem(u8"Svět")) {
					if (ImGui::BeginChild("##world_scroll"))
						TabWorld();
					ImGui::EndChild();
					ImGui::EndTabItem();
				}
				if (ImGui::BeginTabItem(u8"Konzole")) {
					TabConsole();
					ImGui::EndTabItem();
				}
				if (ImGui::BeginTabItem(u8"Nastavení")) {
					if (ImGui::BeginChild("##settings_scroll"))
						TabSettings();
					ImGui::EndChild();
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
