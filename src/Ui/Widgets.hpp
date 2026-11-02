// Small UI helpers shared by all tabs.

namespace GOTHIC_NAMESPACE {
namespace Trainer {
namespace Ui {

	static const ImVec4 kGold      = ImVec4(0.96f, 0.74f, 0.31f, 1.00f);
	static const ImVec4 kMuted     = ImVec4(0.62f, 0.57f, 0.48f, 1.00f);
	static const ImVec4 kGood      = ImVec4(0.55f, 0.82f, 0.45f, 1.00f);
	static const ImVec4 kWarn      = ImVec4(0.95f, 0.55f, 0.35f, 1.00f);

	static float Em(float n = 1.0f) {
		return ImGui::GetFontSize() * n;
	}

	template<typename T>
	static T Clamp(T v, T lo, T hi) {
		return v < lo ? lo : (v > hi ? hi : v);
	}

	template<typename T>
	static T Smaller(T a, T b) {
		return a < b ? a : b;
	}

	static void HelpMarker(const char* text) {
		ImGui::SameLine();
		ImGui::TextDisabled("(?)");
		if (ImGui::BeginItemTooltip()) {
			ImGui::PushTextWrapPos(Em(24.0f));
			ImGui::TextUnformatted(text);
			ImGui::PopTextWrapPos();
			ImGui::EndTooltip();
		}
	}

	static void Tooltip(const char* text) {
		if (ImGui::BeginItemTooltip()) {
			ImGui::PushTextWrapPos(Em(24.0f));
			ImGui::TextUnformatted(text);
			ImGui::PopTextWrapPos();
			ImGui::EndTooltip();
		}
	}

	static void NotInGame() {
		ImGui::Spacing();
		ImGui::TextColored(kGold, u8"Hra zatím neběží.");
		ImGui::TextWrapped(u8"Spusť novou hru nebo načti uloženou pozici. Tahle záložka se zpřístupní, jakmile bude hrdina ve světě.");
	}

	// Label on the left, value editor aligned to a fixed column.
	static void RowLabel(const char* label, float column = 0.0f) {
		if (column <= 0.0f)
			column = Em(9.0f);
		ImGui::AlignTextToFramePadding();
		ImGui::TextUnformatted(label);
		ImGui::SameLine(column);
	}

	static bool IntField(const char* label, const char* id, int* value, int step, int stepFast, float width = 0.0f) {
		RowLabel(label);
		ImGui::SetNextItemWidth(width > 0.0f ? width : Em(9.0f));
		return ImGui::InputInt(id, value, step, stepFast);
	}

	static bool U32Field(const char* label, const char* id, unsigned long* value, float width = 0.0f) {
		RowLabel(label);
		ImGui::SetNextItemWidth(width > 0.0f ? width : Em(9.0f));
		unsigned int v = (unsigned int)*value;
		static const unsigned int step = 1, stepFast = 100;
		bool changed = ImGui::InputScalar(id, ImGuiDataType_U32, &v, &step, &stepFast);
		if (changed)
			*value = (unsigned long)v;
		return changed;
	}

	static bool SmallButtonColored(const char* label, const ImVec4& color) {
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(color.x * 0.45f, color.y * 0.45f, color.z * 0.45f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(color.x * 0.65f, color.y * 0.65f, color.z * 0.65f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(color.x * 0.80f, color.y * 0.80f, color.z * 0.80f, 1.0f));
		bool r = ImGui::Button(label);
		ImGui::PopStyleColor(3);
		return r;
	}

	static void Toast(const std::string& text) {
		::GT::Overlay::Toast(text);
	}

} // namespace Ui
} // namespace Trainer
} // namespace GOTHIC_NAMESPACE
