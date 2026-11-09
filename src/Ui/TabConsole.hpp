// Tab "Konzole" – any Marvin-mode console command, with history and examples.

namespace GOTHIC_NAMESPACE {
namespace Trainer {
namespace Ui {

	struct ConsoleUiState {
		char input[256] = {};
		std::vector<std::pair<std::string, bool>> history; // command, success
		bool focusInput = false;
	};

	static ConsoleUiState g_consoleUi;

	static void RunCommand(const std::string& cmd) {
		if (cmd.empty())
			return;
		Enqueue([cmd]() {
			bool ok = RunConsoleCommand(cmd);
			ConsoleUiState& s = g_consoleUi;
			s.history.push_back(std::make_pair(cmd, ok));
			if (s.history.size() > 200)
				s.history.erase(s.history.begin());
		});
	}

	struct ExampleCommand { const char* cmd; const char* desc; };
	static const ExampleCommand kExamples[] = {
		{ "cheat full",          u8"doplní život a manu" },
		{ "cheat god",           u8"přepne nesmrtelnost" },
		{ "insert ItMi_Gold",    u8"vloží předmět nebo postavu podle kódu" },
		{ "insert Wolf",         u8"vyvolá vlka před hrdinou" },
		{ "kill",                u8"zabije postavu, na kterou se díváš" },
		{ "set time 12 00",      u8"nastaví čas na poledne" },
		{ "goto waypoint ",      u8"teleport na waypoint (doplň název)" },
		{ "toggle desktop",      u8"skryje nebo zobrazí ukazatele (HUD)" },
		{ "toggle frame",        u8"zobrazí počet snímků za sekundu" },
		{ "toggle time",         u8"zobrazí herní hodiny" },
		{ "zfogzone",            u8"vypne mlhu ve vzdálenosti" },
		{ "edit abilities",      u8"původní editor postavy v konzoli" },
	};

	static void TabConsole() {
		if (!InGame()) {
			NotInGame();
			return;
		}
		ConsoleUiState& s = g_consoleUi;
		ImGui::TextWrapped(u8"Příkazy jsou stejné jako v Marvin módu (F2), jen nemusíš nic zapínat. Výsledek uvidíš i ve hře.");
		ImGui::Spacing();

		if (s.focusInput) {
			ImGui::SetKeyboardFocusHere();
			s.focusInput = false;
		}
		ImGui::SetNextItemWidth(-Em(7.0f));
		bool enter = ImGui::InputTextWithHint("##cmd", u8"Napiš příkaz, např. insert ItMw_1H_Pal_Sword", s.input, sizeof(s.input),
			ImGuiInputTextFlags_EnterReturnsTrue);
		ImGui::SameLine();
		bool run = ImGui::Button(u8"Spustit", ImVec2(-FLT_MIN, 0.0f));
		if ((enter || run) && s.input[0]) {
			RunCommand(s.input);
			s.input[0] = 0;
			s.focusInput = true;
		}

		if (ImGui::BeginTable("##console", 2, ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_PadOuterX)) {
			ImGui::TableNextColumn();
			ImGui::SeparatorText(u8"Užitečné příkazy");
			for (const ExampleCommand& ex : kExamples) {
				ImGui::PushID(ex.cmd);
				if (ImGui::Selectable(ex.cmd)) {
					snprintf(s.input, sizeof(s.input), "%s", ex.cmd);
					s.focusInput = true;
				}
				ImGui::SameLine(Em(10.0f));
				ImGui::TextDisabled("%s", ex.desc);
				ImGui::PopID();
			}

			ImGui::TableNextColumn();
			ImGui::SeparatorText(u8"Historie");
			if (ImGui::BeginChild("##history", ImVec2(0.0f, Em(16.0f)), ImGuiChildFlags_Borders)) {
				for (int i = (int)s.history.size() - 1; i >= 0; i--) {
					const auto& h = s.history[i];
					ImGui::PushID(i);
					ImGui::TextColored(h.second ? kGood : kWarn, "%s", h.second ? u8"✓" : u8"✗");
					ImGui::SameLine();
					if (ImGui::Selectable(h.first.c_str())) {
						snprintf(s.input, sizeof(s.input), "%s", h.first.c_str());
						s.focusInput = true;
					}
					ImGui::PopID();
				}
				if (s.history.empty())
					ImGui::TextDisabled(u8"Zatím nic.");
			}
			ImGui::EndChild();
			ImGui::EndTable();
		}
	}

} // namespace Ui
} // namespace Trainer
} // namespace GOTHIC_NAMESPACE
