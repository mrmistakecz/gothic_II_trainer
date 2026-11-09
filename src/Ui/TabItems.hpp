// Tab "Předměty" – every item instance from the game's scripts.

namespace GOTHIC_NAMESPACE {
namespace Trainer {
namespace Ui {

	struct ItemsUiState {
		char search[128] = {};
		int category = CAT_ALL;
		int amount = 1;
		bool hideQuest = false;
		std::string lastSearch;
		int lastCategory = -1;
		bool lastHideQuest = false;
		size_t lastDbSize = 0;
		std::vector<int> filtered;
		int selected = -1;
		int mode = 0;              // 0 = catalogue, 1 = hero inventory
	};

	static ItemsUiState g_itemsUi;

	struct InventoryRow {
		int instance = -1;
		std::string name;
		std::string code;
		std::string folded;
		int amount = 0;
		int category = CAT_OTHER;
		int value = 0;
		bool equipped = false;
		bool mission = false;
	};

	static void GiveItem(int instance, int amount, const std::string& name) {
		Enqueue([instance, amount, name]() {
			if (!InGame())
				return;
			player->CreateItems(instance, amount);
			::GT::Overlay::Toast(::GT::Text::Format(u8"Přidáno: %s× %s", ::GT::Text::Thousands(amount).c_str(), name.c_str()));
		});
	}

	static void RefreshItemFilter() {
		ItemsUiState& s = g_itemsUi;
		std::string needle = ::GT::Text::Fold(s.search);
		if (needle == s.lastSearch && s.category == s.lastCategory && s.hideQuest == s.lastHideQuest && g_items.items.size() == s.lastDbSize)
			return;
		s.lastSearch = needle;
		s.lastCategory = s.category;
		s.lastHideQuest = s.hideQuest;
		s.lastDbSize = g_items.items.size();
		s.filtered.clear();
		for (int i = 0; i < (int)g_items.items.size(); i++) {
			const ItemEntry& e = g_items.items[i];
			if (s.category != CAT_ALL && e.category != s.category)
				continue;
			if (s.hideQuest && e.mission)
				continue;
			if (!::GT::Text::Contains(e.searchKey, needle))
				continue;
			s.filtered.push_back(i);
		}
	}

	static std::string ItemSummary(const ItemEntry& e) {
		switch (e.category) {
			case CAT_MELEE:
			case CAT_RANGED:
				return ::GT::Text::Format(u8"poškození %d", e.damage);
			case CAT_ARMOR:
				return ::GT::Text::Format(u8"zbraně %d · šípy %d · oheň %d · magie %d",
					e.protection[oEDamageIndex_Edge], e.protection[oEDamageIndex_Point],
					e.protection[oEDamageIndex_Fire], e.protection[oEDamageIndex_Magic]);
		}
		for (const auto& line : e.lines)
			if (line.second != 0)
				return ::GT::Text::Format("%s %d", line.first.c_str(), line.second);
		return std::string();
	}

	static void ItemTooltip(const ItemEntry& e) {
		if (!ImGui::BeginItemTooltip())
			return;
		ImGui::TextColored(kGold, "%s", e.name.c_str());
		ImGui::TextDisabled("%s", e.code.c_str());
		if (!e.description.empty() && e.description != e.name)
			ImGui::TextUnformatted(e.description.c_str());
		ImGui::Separator();
		for (const auto& line : e.lines) {
			if (line.second != 0)
				ImGui::Text("%s  %d", line.first.c_str(), line.second);
			else
				ImGui::TextUnformatted(line.first.c_str());
		}
		ImGui::Text(u8"Cena: %s", ::GT::Text::Thousands(e.value).c_str());
		if (e.mission)
			ImGui::TextColored(kWarn, u8"Úkolový předmět – vložením navíc můžeš rozbít úkol.");
		ImGui::EndTooltip();
	}

	static void QuickGold() {
		ImGui::AlignTextToFramePadding();
		ImGui::TextUnformatted(u8"Zlato:");
		static const int amounts[] = { 100, 1000, 10000, 100000 };
		for (int a : amounts) {
			ImGui::SameLine();
			std::string label = "+" + ::GT::Text::Thousands(a) + "##gold" + std::to_string(a);
			if (ImGui::Button(label.c_str())) {
				int index = g_items.goldIndex >= 0 ? g_items.goldIndex : ScriptSymbolIndex("ITMI_GOLD");
				if (index >= 0)
					GiveItem(index, a, u8"zlaťáků");
				else
					Toast(u8"V téhle hře chybí předmět ItMi_Gold");
			}
		}
	}

	// What the hero carries right now, with quick "more of this" buttons.
	static void HeroInventoryView() {
		ItemsUiState& s = g_itemsUi;
		std::vector<InventoryRow> rows;
		long long total = 0;
		for (auto* node = player->inventory2.inventory.next; node; node = node->next) {
			oCItem* it = node->data;
			if (!it)
				continue;
			InventoryRow r;
			r.instance = it->GetInstance();
			r.code = FromZ(it->GetInstanceName());
			r.name = FromZ(it->name);
			if (r.name.empty())
				r.name = r.code;
			r.amount = it->amount;
			r.category = ClassifyItem(it->mainflag, it->flags);
			r.value = it->value;
			r.equipped = (it->flags & ITM_FLAG_ACTIVE) != 0;
			r.mission = (it->flags & ITM_FLAG_MI) != 0;
			r.folded = ::GT::Text::Fold(r.name) + " " + ::GT::Text::Fold(r.code);
			total += (long long)r.value * (long long)(r.amount > 0 ? r.amount : 1);
			rows.push_back(std::move(r));
		}
		std::sort(rows.begin(), rows.end(), [](const InventoryRow& a, const InventoryRow& b) {
			return a.category != b.category ? a.category < b.category : a.folded < b.folded;
		});

		ImGui::SetNextItemWidth(Em(16.0f));
		ImGui::InputTextWithHint("##invsearch", u8"Hledat v inventáři…", s.search, sizeof(s.search));
		ImGui::SameLine();
		ImGui::SetNextItemWidth(Em(11.0f));
		if (ImGui::BeginCombo("##invcat", CategoryName(s.category))) {
			for (int c = 0; c < CAT_COUNT; c++)
				if (ImGui::Selectable(CategoryName(c), s.category == c))
					s.category = c;
			ImGui::EndCombo();
		}
		const std::string needle = ::GT::Text::Fold(s.search);
		std::vector<int> visible;
		for (int i = 0; i < (int)rows.size(); i++) {
			if (s.category != CAT_ALL && rows[i].category != s.category)
				continue;
			if (!::GT::Text::Contains(rows[i].folded, needle))
				continue;
			visible.push_back(i);
		}
		ImGui::TextDisabled(u8"%d druhů předmětů · celková hodnota %s · zlatě = nasazeno", (int)rows.size(), ::GT::Text::Thousands(total).c_str());

		const ImGuiTableFlags flags = ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter
			| ImGuiTableFlags_BordersV | ImGuiTableFlags_SizingStretchProp;
		if (ImGui::BeginTable("##inventory", 5, flags, ImVec2(0.0f, ImGui::GetContentRegionAvail().y))) {
			ImGui::TableSetupScrollFreeze(0, 1);
			ImGui::TableSetupColumn(u8"Název", ImGuiTableColumnFlags_WidthStretch, 3.0f);
			ImGui::TableSetupColumn(u8"Druh", ImGuiTableColumnFlags_WidthStretch, 1.6f);
			ImGui::TableSetupColumn(u8"Kusů", ImGuiTableColumnFlags_WidthFixed, Em(4.5f));
			ImGui::TableSetupColumn(u8"Cena", ImGuiTableColumnFlags_WidthFixed, Em(4.5f));
			ImGui::TableSetupColumn(u8"Přidat", ImGuiTableColumnFlags_WidthFixed, Em(7.5f));
			ImGui::TableHeadersRow();
			ImGuiListClipper clipper;
			clipper.Begin((int)visible.size());
			while (clipper.Step()) {
				for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++) {
					const InventoryRow& r = rows[visible[row]];
					ImGui::PushID(row);
					ImGui::TableNextRow();
					ImGui::TableNextColumn();
					ImGui::AlignTextToFramePadding();
					if (r.equipped)
						ImGui::TextColored(kGold, "%s", r.name.c_str());
					else if (r.mission)
						ImGui::TextColored(kWarn, "%s", r.name.c_str());
					else
						ImGui::TextUnformatted(r.name.c_str());
					if (ImGui::BeginItemTooltip()) {
						ImGui::TextDisabled("%s", r.code.c_str());
						if (r.equipped)
							ImGui::TextUnformatted(u8"Nasazeno");
						if (r.mission)
							ImGui::TextColored(kWarn, u8"Úkolový předmět");
						ImGui::EndTooltip();
					}
					ImGui::TableNextColumn();
					ImGui::TextDisabled("%s", CategoryName(r.category));
					ImGui::TableNextColumn();
					ImGui::Text("%s", ::GT::Text::Thousands(r.amount).c_str());
					ImGui::TableNextColumn();
					ImGui::Text("%d", r.value);
					ImGui::TableNextColumn();
					if (r.instance >= 0) {
						if (ImGui::SmallButton("+1"))
							GiveItem(r.instance, 1, r.name);
						ImGui::SameLine();
						if (ImGui::SmallButton("+10"))
							GiveItem(r.instance, 10, r.name);
						ImGui::SameLine();
						if (ImGui::SmallButton(u8"×2") && r.amount > 0)
							GiveItem(r.instance, r.amount < 100000 ? r.amount : 100000, r.name);
						Tooltip(u8"Zdvojnásobí počet kusů");
					}
					ImGui::PopID();
				}
			}
			ImGui::EndTable();
		}
	}

	static void TabItems() {
		if (!InGame()) {
			NotInGame();
			return;
		}

		if (!g_items.ready) {
			if (!g_items.building) {
				g_items.building = true;
				Enqueue([]() { BuildItemDatabase(); });
			}
			ImGui::TextColored(kGold, u8"Načítám seznam předmětů ze skriptů hry…");
			return;
		}

		ItemsUiState& s = g_itemsUi;
		ImGui::RadioButton(u8"Všechny předměty ze hry", &s.mode, 0);
		ImGui::SameLine();
		ImGui::RadioButton(u8"Můj inventář", &s.mode, 1);
		QuickGold();
		ImGui::Spacing();
		if (s.mode == 1) {
			HeroInventoryView();
			return;
		}

		ImGui::SetNextItemWidth(Em(16.0f));
		ImGui::InputTextWithHint("##search", u8"Hledat název nebo kód…", s.search, sizeof(s.search));
		ImGui::SameLine();
		ImGui::SetNextItemWidth(Em(11.0f));
		if (ImGui::BeginCombo("##cat", CategoryName(s.category))) {
			for (int c = 0; c < CAT_COUNT; c++)
				if (ImGui::Selectable(CategoryName(c), s.category == c))
					s.category = c;
			ImGui::EndCombo();
		}
		ImGui::SameLine();
		ImGui::SetNextItemWidth(Em(7.5f));
		ImGui::InputInt(u8"ks##amount", &s.amount, 1, 10);
		s.amount = Clamp(s.amount, 1, 100000);
		ImGui::SameLine();
		ImGui::Checkbox(u8"Skrýt úkolové", &s.hideQuest);

		RefreshItemFilter();

		ImGui::TextDisabled(u8"Nalezeno %d z %d předmětů · dvojklik nebo + přidá do inventáře",
			(int)s.filtered.size(), (int)g_items.items.size());

		const ImGuiTableFlags flags = ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter
			| ImGuiTableFlags_BordersV | ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp;
		if (ImGui::BeginTable("##items", 5, flags, ImVec2(0.0f, ImGui::GetContentRegionAvail().y))) {
			ImGui::TableSetupScrollFreeze(0, 1);
			ImGui::TableSetupColumn(u8"Název", ImGuiTableColumnFlags_WidthStretch, 3.0f);
			ImGui::TableSetupColumn(u8"Druh", ImGuiTableColumnFlags_WidthStretch, 1.6f);
			ImGui::TableSetupColumn(u8"Info", ImGuiTableColumnFlags_WidthStretch, 2.6f);
			ImGui::TableSetupColumn(u8"Cena", ImGuiTableColumnFlags_WidthFixed, Em(4.5f));
			ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, Em(2.2f));
			ImGui::TableHeadersRow();

			ImGuiListClipper clipper;
			clipper.Begin((int)s.filtered.size());
			while (clipper.Step()) {
				for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++) {
					const ItemEntry& e = g_items.items[s.filtered[row]];
					ImGui::PushID(e.index);
					ImGui::TableNextRow();

					ImGui::TableNextColumn();
					if (e.mission)
						ImGui::PushStyleColor(ImGuiCol_Text, kWarn);
					bool clicked = ImGui::Selectable(e.name.c_str(), s.selected == e.index,
						ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap | ImGuiSelectableFlags_AllowDoubleClick);
					if (e.mission)
						ImGui::PopStyleColor();
					ItemTooltip(e);
					if (clicked) {
						s.selected = e.index;
						if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
							GiveItem(e.index, s.amount, e.name);
					}

					ImGui::TableNextColumn();
					ImGui::TextDisabled("%s", CategoryName(e.category));

					ImGui::TableNextColumn();
					std::string summary = ItemSummary(e);
					ImGui::TextUnformatted(summary.c_str());

					ImGui::TableNextColumn();
					ImGui::Text("%d", e.value);

					// submitted after the row-wide selectable, so it stays clickable
					ImGui::TableNextColumn();
					if (ImGui::SmallButton("+"))
						GiveItem(e.index, s.amount, e.name);
					Tooltip(u8"Přidat do inventáře");

					ImGui::PopID();
				}
			}
			ImGui::EndTable();
		}
	}

} // namespace Ui
} // namespace Trainer
} // namespace GOTHIC_NAMESPACE
