// Tab "Skripty" – every global script variable (quest states, chapter, ...)
// can be viewed and changed; script functions can be called.

namespace GOTHIC_NAMESPACE {
namespace Trainer {
namespace Ui {

	struct ScriptVar {
		int index;
		std::string name;
		std::string folded;
		int type;      // zPAR_TYPE_INT / FLOAT / STRING
		int elements;
	};

	struct ScriptFunc {
		int index;
		std::string name;
		std::string folded;
		int params;
		bool allIntParams;
	};

	struct ScriptsUiState {
		std::vector<ScriptVar> vars;
		std::vector<ScriptFunc> funcs;
		bool ready = false;
		int symbolCount = -1;
		char varSearch[96] = {};
		char funcSearch[96] = {};
		bool onlyQuests = false;
		int funcSelected = -1;
		int funcArgs[4] = {};
		std::vector<int> varFiltered;
		std::string lastVarSearch;
		bool lastOnlyQuests = false;
		bool varDirty = true;
	};

	static ScriptsUiState g_scriptsUi;

	static void BuildScriptLists() {
		ScriptsUiState& s = g_scriptsUi;
		s.vars.clear();
		s.funcs.clear();
		s.ready = false;
		if (!parser)
			return;
		const int symbols = parser->symtab.table.GetNumInList();
		s.symbolCount = symbols;
		for (int i = 0; i < symbols; i++) {
			zCPar_Symbol* sym = parser->GetSymbol(i);
			if (!sym)
				continue;
			const int type = (int)sym->type;
			const int flags = (int)sym->flags;
			std::string name = sym->name.ToChar();
			if (name.empty() || name[0] == (char)0xFF || name.find('.') != std::string::npos)
				continue; // locals ("FUNC.VAR") and generated symbols
			if (type == zPAR_TYPE_INT || type == zPAR_TYPE_FLOAT || type == zPAR_TYPE_STRING) {
				if (flags & (zPAR_FLAG_CONST | zPAR_FLAG_CLASSVAR | zPAR_FLAG_RETURN))
					continue;
				ScriptVar v;
				v.index = i;
				v.name = name;
				v.folded = ::GT::Text::Fold(name);
				v.type = type;
				v.elements = (int)sym->ele > 0 ? (int)sym->ele : 1;
				s.vars.push_back(std::move(v));
			}
			else if (type == zPAR_TYPE_FUNC && !(flags & zPAR_FLAG_EXTERNAL) && (flags & zPAR_FLAG_CONST)) {
				ScriptFunc f;
				f.index = i;
				f.name = name;
				f.folded = ::GT::Text::Fold(name);
				f.params = (int)sym->ele;
				f.allIntParams = true;
				for (int p = 0; p < f.params; p++) {
					zCPar_Symbol* param = parser->GetSymbol(i + 1 + p);
					if (!param || param->type != zPAR_TYPE_INT)
						f.allIntParams = false;
				}
				if (f.params <= 4 && f.allIntParams)
					s.funcs.push_back(std::move(f));
			}
		}
		std::sort(s.vars.begin(), s.vars.end(), [](const ScriptVar& a, const ScriptVar& b) { return a.name < b.name; });
		std::sort(s.funcs.begin(), s.funcs.end(), [](const ScriptFunc& a, const ScriptFunc& b) { return a.name < b.name; });
		s.ready = true;
		s.varDirty = true;
	}

	static bool IsQuestVariable(const std::string& name) {
		return name.compare(0, 4, "MIS_") == 0 || name.compare(0, 7, "KAPITEL") == 0 || name.find("_QUEST") != std::string::npos;
	}

	// selected element of array variables (symbol index -> element)
	static std::vector<std::pair<int, int>> g_arrayCursor;

	static int& ArrayCursor(int symbolIndex) {
		for (auto& p : g_arrayCursor)
			if (p.first == symbolIndex)
				return p.second;
		g_arrayCursor.push_back(std::make_pair(symbolIndex, 0));
		return g_arrayCursor.back().second;
	}

	// One line per variable (the list is virtualized, rows must keep one height).
	static void ScriptVarEditor(const ScriptVar& v) {
		zCPar_Symbol* sym = parser->GetSymbol(v.index);
		if (!sym)
			return;
		int e = 0;
		if (v.elements > 1) {
			int& cursor = ArrayCursor(v.index);
			cursor = Clamp(cursor, 0, v.elements - 1);
			ImGui::SetNextItemWidth(Em(4.5f));
			ImGui::DragInt("##elem", &cursor, 0.2f, 0, v.elements - 1, "[%d]");
			Tooltip(u8"Pole – vyber prvek (táhni nebo dvojklik a napiš číslo).");
			ImGui::SameLine();
			e = cursor;
		}
		ImGui::SetNextItemWidth(-FLT_MIN);
		if (v.type == zPAR_TYPE_INT) {
			int value = 0;
			sym->GetValue(value, e);
			if (ImGui::InputInt("##v", &value, 1, 10, ImGuiInputTextFlags_EnterReturnsTrue))
				sym->SetValue(value, e);
		}
		else if (v.type == zPAR_TYPE_FLOAT) {
			float value = 0.0f;
			sym->GetValue(value, e);
			if (ImGui::InputFloat("##v", &value, 0.1f, 1.0f, "%.3f", ImGuiInputTextFlags_EnterReturnsTrue))
				sym->SetValue(value, e);
		}
		else {
			zSTRING value;
			sym->GetValue(value, e);
			char buf[256];
			snprintf(buf, sizeof(buf), "%s", FromZ(value).c_str());
			if (ImGui::InputText("##v", buf, sizeof(buf), ImGuiInputTextFlags_EnterReturnsTrue)) {
				zSTRING nv = ToZ(buf);
				sym->SetValue(nv, e);
			}
		}
	}

	static void ScriptVariablesSection() {
		ScriptsUiState& s = g_scriptsUi;
		ImGui::SeparatorText(u8"Proměnné skriptů");
		ImGui::SetNextItemWidth(Em(16.0f));
		ImGui::InputTextWithHint("##varsearch", u8"Hledat (např. MIS_, KAPITEL)…", s.varSearch, sizeof(s.varSearch));
		ImGui::SameLine();
		ImGui::Checkbox(u8"Jen úkoly", &s.onlyQuests);
		HelpMarker(u8"MIS_ proměnné drží stav úkolů (0 = nezadán, 1 = běží, 2 = splněn, 3 = nesplněn, 4 = zastaralý). Změna hodnoty se projeví v rozhovorech. Uprav jen to, čemu rozumíš, a ulož si hru předem.");

		std::string needle = ::GT::Text::Fold(s.varSearch);
		if (s.varDirty || needle != s.lastVarSearch || s.onlyQuests != s.lastOnlyQuests) {
			s.varDirty = false;
			s.lastVarSearch = needle;
			s.lastOnlyQuests = s.onlyQuests;
			s.varFiltered.clear();
			for (int i = 0; i < (int)s.vars.size(); i++) {
				if (s.onlyQuests && !IsQuestVariable(s.vars[i].name))
					continue;
				if (::GT::Text::Contains(s.vars[i].folded, needle))
					s.varFiltered.push_back(i);
			}
		}

		ImGui::TextDisabled(u8"%d z %d proměnných · hodnotu potvrď Enterem", (int)s.varFiltered.size(), (int)s.vars.size());
		const ImGuiTableFlags flags = ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_SizingStretchProp;
		if (ImGui::BeginTable("##vars", 2, flags, ImVec2(0.0f, ImGui::GetContentRegionAvail().y))) {
			ImGui::TableSetupScrollFreeze(0, 1);
			ImGui::TableSetupColumn(u8"Název", ImGuiTableColumnFlags_WidthStretch, 1.6f);
			ImGui::TableSetupColumn(u8"Hodnota", ImGuiTableColumnFlags_WidthStretch, 1.0f);
			ImGui::TableHeadersRow();
			ImGuiListClipper clipper;
			clipper.Begin((int)s.varFiltered.size());
			while (clipper.Step()) {
				for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++) {
					const ScriptVar& v = s.vars[s.varFiltered[row]];
					ImGui::PushID(v.index);
					ImGui::TableNextRow();
					ImGui::TableNextColumn();
					ImGui::AlignTextToFramePadding();
					ImGui::TextUnformatted(v.name.c_str());
					ImGui::TableNextColumn();
					ScriptVarEditor(v);
					ImGui::PopID();
				}
			}
			ImGui::EndTable();
		}
	}

	static void ScriptFunctionsSection() {
		ScriptsUiState& s = g_scriptsUi;
		ImGui::SeparatorText(u8"Spustit funkci skriptu");
		ImGui::TextWrapped(u8"Pro pokročilé: zavolá funkci ze skriptů hry (jen funkce s celočíselnými parametry). Špatná funkce může rozbít úkoly – ulož si hru předem.");
		ImGui::SetNextItemWidth(-FLT_MIN);
		ImGui::InputTextWithHint("##funcsearch", u8"Hledat funkci (např. B_GIVEPLAYERXP)…", s.funcSearch, sizeof(s.funcSearch));
		std::string needle = ::GT::Text::Fold(s.funcSearch);

		if (ImGui::BeginChild("##funcs", ImVec2(0.0f, Em(10.0f)), ImGuiChildFlags_Borders)) {
			int shown = 0;
			for (int i = 0; i < (int)s.funcs.size() && shown < 400; i++) {
				const ScriptFunc& f = s.funcs[i];
				if (!::GT::Text::Contains(f.folded, needle))
					continue;
				shown++;
				ImGui::PushID(f.index);
				std::string label = f.params > 0 ? ::GT::Text::Format("%s (%d)", f.name.c_str(), f.params) : f.name;
				if (ImGui::Selectable(label.c_str(), s.funcSelected == f.index))
					s.funcSelected = f.index;
				ImGui::PopID();
			}
		}
		ImGui::EndChild();

		const ScriptFunc* fn = nullptr;
		for (const ScriptFunc& f : s.funcs)
			if (f.index == s.funcSelected)
				fn = &f;
		if (!fn) {
			ImGui::TextDisabled(u8"Vyber funkci ze seznamu.");
			return;
		}
		ImGui::TextColored(kGold, "%s", fn->name.c_str());
		for (int p = 0; p < fn->params; p++) {
			zCPar_Symbol* param = parser->GetSymbol(fn->index + 1 + p);
			std::string pname = param ? std::string(param->name.ToChar()) : ::GT::Text::Format("P%d", p + 1);
			size_t dot = pname.find('.');
			if (dot != std::string::npos)
				pname = pname.substr(dot + 1);
			ImGui::SetNextItemWidth(Em(9.0f));
			ImGui::InputInt((pname + "##arg" + std::to_string(p)).c_str(), &s.funcArgs[p]);
		}
		if (SmallButtonColored(u8"Spustit", kGold)) {
			int index = fn->index;
			int params = fn->params;
			std::string name = fn->name;
			int a0 = s.funcArgs[0], a1 = s.funcArgs[1], a2 = s.funcArgs[2], a3 = s.funcArgs[3];
			Enqueue([index, params, name, a0, a1, a2, a3]() {
				if (!parser)
					return;
				switch (params) {
					case 0: parser->CallFunc(index); break;
					case 1: parser->CallFunc(index, a0); break;
					case 2: parser->CallFunc(index, a0, a1); break;
					case 3: parser->CallFunc(index, a0, a1, a2); break;
					default: parser->CallFunc(index, a0, a1, a2, a3); break;
				}
				::GT::Overlay::Toast(u8"Spuštěno: " + name);
			});
		}
	}

	static void TabScripts() {
		if (!parser) {
			NotInGame();
			return;
		}
		ScriptsUiState& s = g_scriptsUi;
		if (!s.ready || s.symbolCount != parser->symtab.table.GetNumInList())
			BuildScriptLists();

		if (ImGui::BeginTable("##scripts", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_PadOuterX, ImGui::GetContentRegionAvail())) {
			ImGui::TableSetupColumn("a", ImGuiTableColumnFlags_WidthStretch, 1.5f);
			ImGui::TableSetupColumn("b", ImGuiTableColumnFlags_WidthStretch, 1.0f);
			ImGui::TableNextColumn();
			ScriptVariablesSection();
			ImGui::TableNextColumn();
			ScriptFunctionsSection();
			ImGui::EndTable();
		}
	}

} // namespace Ui
} // namespace Trainer
} // namespace GOTHIC_NAMESPACE
