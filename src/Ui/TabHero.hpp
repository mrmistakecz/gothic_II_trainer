// Tab "Postava" – life, attributes, experience, talents, guild, protections.

namespace GOTHIC_NAMESPACE {
namespace Trainer {
namespace Ui {

	static const char* FightSkillName(int skill) {
		switch (skill) {
			case 0: return u8"nováček";
			case 1: return u8"bojovník";
			case 2: return u8"mistr";
		}
		return "?";
	}

	static void StatBar(const char* label, const char* id, int& current, int& maximum, const ImVec4& color) {
		RowLabel(label);
		ImGui::PushID(id);
		ImGui::SetNextItemWidth(Em(7.0f));
		ImGui::InputInt("##cur", &current, 10, 100);
		ImGui::SameLine();
		ImGui::TextUnformatted("/");
		ImGui::SameLine();
		ImGui::SetNextItemWidth(Em(7.0f));
		ImGui::InputInt("##max", &maximum, 10, 100);
		if (maximum < 1)
			maximum = 1;
		ImGui::SameLine();
		float frac = Clamp((float)current / (float)maximum, 0.0f, 1.0f);
		ImGui::PushStyleColor(ImGuiCol_PlotHistogram, color);
		ImGui::ProgressBar(frac, ImVec2(-FLT_MIN, 0.0f), "");
		ImGui::PopStyleColor();
		ImGui::PopID();
	}

	static void HeroLifeSection(oCNpc* h) {
		ImGui::SeparatorText(u8"Život a mana");
		StatBar(u8"Život", "hp", h->attribute[NPC_ATR_HITPOINTS], h->attribute[NPC_ATR_HITPOINTSMAX], ImVec4(0.75f, 0.20f, 0.18f, 1.0f));
		StatBar(u8"Mana", "mana", h->attribute[NPC_ATR_MANA], h->attribute[NPC_ATR_MANAMAX], ImVec4(0.25f, 0.42f, 0.85f, 1.0f));

		if (ImGui::Button(u8"Doplnit život i manu")) {
			h->attribute[NPC_ATR_HITPOINTS] = h->attribute[NPC_ATR_HITPOINTSMAX];
			h->attribute[NPC_ATR_MANA] = h->attribute[NPC_ATR_MANAMAX];
			Toast(u8"Život a mana doplněny");
		}
		ImGui::SameLine();
		if (ImGui::Button(u8"Max. život +100")) {
			h->attribute[NPC_ATR_HITPOINTSMAX] += 100;
			h->attribute[NPC_ATR_HITPOINTS] = h->attribute[NPC_ATR_HITPOINTSMAX];
		}
		ImGui::SameLine();
		if (ImGui::Button(u8"Max. mana +50")) {
			h->attribute[NPC_ATR_MANAMAX] += 50;
			h->attribute[NPC_ATR_MANA] = h->attribute[NPC_ATR_MANAMAX];
		}

		bool god = oCNpc::godmode != 0;
		if (ImGui::Checkbox(u8"Nesmrtelnost", &god)) {
			oCNpc::godmode = god ? TRUE : FALSE;
			Toast(god ? u8"Nesmrtelnost zapnuta" : u8"Nesmrtelnost vypnuta");
		}
		Tooltip(u8"Hrdina nedostává žádné zranění (stejné jako „cheat god“).");
		ImGui::SameLine(Em(11.0f));
		ImGui::Checkbox(u8"Nekonečný život", &g_state.infiniteHp);
		ImGui::Checkbox(u8"Jedna rána zabije", &g_state.oneHitKill);
		Tooltip(u8"Každý tvůj zásah je smrtelný. Lidé v Gothicu po zásahu pěstí nebo tupou zbraní jen omdlí – to je chování hry.");
		ImGui::SameLine(Em(11.0f));
		ImGui::Checkbox(u8"Nekonečná mana", &g_state.infiniteMana);
	}

	static void HeroAttributeSection(oCNpc* h) {
		ImGui::SeparatorText(u8"Atributy");
		IntField(u8"Síla", "##str", &h->attribute[NPC_ATR_STRENGTH], 1, 10);
		ImGui::SameLine();
		if (ImGui::SmallButton(u8"+10##str"))
			h->attribute[NPC_ATR_STRENGTH] += 10;
		IntField(u8"Obratnost", "##dex", &h->attribute[NPC_ATR_DEXTERITY], 1, 10);
		ImGui::SameLine();
		if (ImGui::SmallButton(u8"+10##dex"))
			h->attribute[NPC_ATR_DEXTERITY] += 10;
	}

	static void HeroExperienceSection(oCNpc* h) {
		ImGui::SeparatorText(u8"Úroveň a zkušenosti");
		IntField(u8"Úroveň", "##lvl", &h->level, 1, 5);
		U32Field(u8"Zkušenosti", "##exp", &h->experience_points);
		U32Field(u8"Další úroveň", "##expnext", &h->experience_points_next_level);
		U32Field(u8"Body učení", "##lp", &h->learn_points);
		ImGui::SameLine();
		if (ImGui::SmallButton("+10##lp"))
			h->learn_points += 10;
		ImGui::SameLine();
		if (ImGui::SmallButton("+50##lp"))
			h->learn_points += 50;

		static int xp = 1000;
		RowLabel(u8"Přidat zkušenosti");
		ImGui::SetNextItemWidth(Em(9.0f));
		ImGui::InputInt("##addxp", &xp, 100, 1000);
		if (xp < 1)
			xp = 1;
		ImGui::SameLine();
		if (ImGui::Button(u8"Přidat")) {
			int amount = xp;
			Enqueue([amount]() {
				if (!InGame())
					return;
				// B_GivePlayerXP handles level-ups, learning points and the on-screen message
				if (!CallScriptInt("B_GIVEPLAYERXP", amount))
					player->experience_points += (unsigned long)amount;
			});
		}
		Tooltip(u8"Použije herní skript B_GivePlayerXP, takže se správně přidají úrovně, body učení i život.");
		ImGui::SameLine();
		if (ImGui::Button(u8"+1 úroveň")) {
			Enqueue([]() {
				if (!InGame())
					return;
				long need = (long)player->experience_points_next_level - (long)player->experience_points;
				if (need < 1)
					need = 1;
				if (!CallScriptInt("B_GIVEPLAYERXP", (int)need))
					player->experience_points = player->experience_points_next_level;
			});
		}
	}

	static void HeroFightSection(oCNpc* h) {
		ImGui::SeparatorText(u8"Boj");
		struct FightRow { const char* label; const char* id; int talent; int hitChance; };
		static const FightRow rows[] = {
			{ u8"Jednoruční", "##1h", oCNpcTalent::NPC_TAL_1H, NPC_HITCHANCE_1H },
			{ u8"Obouruční", "##2h", oCNpcTalent::NPC_TAL_2H, NPC_HITCHANCE_2H },
			{ u8"Luk", "##bow", oCNpcTalent::NPC_TAL_BOW, NPC_HITCHANCE_BOW },
			{ u8"Kuše", "##cbow", oCNpcTalent::NPC_TAL_CROSSBOW, NPC_HITCHANCE_CROSSBOW },
		};
		for (const FightRow& r : rows) {
			RowLabel(r.label);
			int v = h->hitChance[r.hitChance];
			ImGui::SetNextItemWidth(Em(12.0f));
			if (ImGui::SliderInt(r.id, &v, 0, 100, "%d %%"))
				h->hitChance[r.hitChance] = v;
			if (ImGui::IsItemDeactivatedAfterEdit()) {
				int talent = r.talent;
				int value = h->hitChance[r.hitChance];
				Enqueue([talent, value]() {
					if (InGame())
						player->SetTalentSkill(talent, value >= 60 ? 2 : (value >= 30 ? 1 : 0));
				});
			}
			ImGui::SameLine();
			ImGui::TextDisabled("%s", FightSkillName(h->GetTalentSkill(r.talent)));
		}
		Tooltip(u8"Od 30 % se hrdina stane bojovníkem, od 60 % mistrem – změní se i animace boje.");
	}

	// Runs on the game thread. Returns how many script recipe flags were set.
	static int LearnAllRecipes() {
		int n = 0;
		n += SetScriptIntArray("PLAYER_TALENT_ALCHEMY", TRUE);
		n += SetScriptIntArray("PLAYER_TALENT_SMITH", TRUE);
		n += SetScriptIntArray("PLAYER_TALENT_RUNES", TRUE);
		n += SetScriptIntArray("PLAYER_TALENT_TAKEANIMALTROPHY", TRUE);
		n += SetScriptIntArray("PLAYER_TALENT_FOREIGNLANGUAGE", TRUE);
		n += SetScriptIntArray("PLAYER_TALENT_WISPDETECTOR", TRUE);
		player->SetTalentSkill(oCNpcTalent::NPC_TAL_ALCHEMY, 1);
		player->SetTalentSkill(oCNpcTalent::NPC_TAL_SMITH, 1);
		player->SetTalentSkill(oCNpcTalent::NPC_TAL_RUNES, 1);
		player->SetTalentSkill(oCNpcTalent::NPC_TAL_TAKEANIMALTROPHY, 1);
		player->SetTalentSkill(oCNpcTalent::NPC_TAL_A, 1);
		player->SetTalentSkill(oCNpcTalent::NPC_TAL_B, 1);
		return n;
	}

	static void RaiseTo(int& value, int minimum) {
		if (value < minimum)
			value = minimum;
	}

	// One click: a hero who can do everything.
	static void MakeLegend() {
		Enqueue([]() {
			if (!InGame())
				return;
			oCNpc* h = player;
			RaiseTo(h->attribute[NPC_ATR_HITPOINTSMAX], 1000);
			RaiseTo(h->attribute[NPC_ATR_MANAMAX], 500);
			RaiseTo(h->attribute[NPC_ATR_STRENGTH], 200);
			RaiseTo(h->attribute[NPC_ATR_DEXTERITY], 200);
			h->attribute[NPC_ATR_HITPOINTS] = h->attribute[NPC_ATR_HITPOINTSMAX];
			h->attribute[NPC_ATR_MANA] = h->attribute[NPC_ATR_MANAMAX];
			static const int fight[][2] = {
				{ oCNpcTalent::NPC_TAL_1H, NPC_HITCHANCE_1H },
				{ oCNpcTalent::NPC_TAL_2H, NPC_HITCHANCE_2H },
				{ oCNpcTalent::NPC_TAL_BOW, NPC_HITCHANCE_BOW },
				{ oCNpcTalent::NPC_TAL_CROSSBOW, NPC_HITCHANCE_CROSSBOW },
			};
			for (const auto& f : fight) {
				h->hitChance[f[1]] = 100;
				h->SetTalentSkill(f[0], 2);
			}
			h->SetTalentSkill(oCNpcTalent::NPC_TAL_MAGE, 6);
			h->SetTalentSkill(oCNpcTalent::NPC_TAL_SNEAK, 1);
			h->SetTalentSkill(oCNpcTalent::NPC_TAL_ACROBAT, 1);
			h->SetTalentSkill(oCNpcTalent::NPC_TAL_PICKLOCK, 1);
			h->SetTalentSkill(oCNpcTalent::NPC_TALENT_PICKPOCKET, 1);
			LearnAllRecipes();
			::GT::Overlay::Toast(u8"Hrdina je teď legenda: plné atributy, mistr boje, 6. kruh a všechny dovednosti");
		});
	}

	static void TalentCheckbox(oCNpc* h, const char* label, int talent) {
		bool on = h->GetTalentSkill(talent) > 0;
		if (ImGui::Checkbox(label, &on)) {
			int value = on ? 1 : 0;
			Enqueue([talent, value]() {
				if (InGame())
					player->SetTalentSkill(talent, value);
			});
		}
	}

	static void HeroSkillSection(oCNpc* h) {
		ImGui::SeparatorText(u8"Dovednosti");
		RowLabel(u8"Kruh magie");
		int circle = h->GetTalentSkill(oCNpcTalent::NPC_TAL_MAGE);
		ImGui::SetNextItemWidth(Em(12.0f));
		ImGui::SliderInt("##circle", &circle, 0, 6, circle == 0 ? u8"žádný" : u8"%d. kruh");
		if (ImGui::IsItemDeactivatedAfterEdit()) {
			int value = circle;
			Enqueue([value]() {
				if (InGame())
					player->SetTalentSkill(oCNpcTalent::NPC_TAL_MAGE, value);
			});
		}

		if (ImGui::BeginTable("##skills", 2, ImGuiTableFlags_SizingStretchSame)) {
			ImGui::TableNextColumn(); TalentCheckbox(h, u8"Plížení", oCNpcTalent::NPC_TAL_SNEAK);
			ImGui::TableNextColumn(); TalentCheckbox(h, u8"Akrobacie", oCNpcTalent::NPC_TAL_ACROBAT);
			ImGui::TableNextColumn(); TalentCheckbox(h, u8"Páčení zámků", oCNpcTalent::NPC_TAL_PICKLOCK);
			ImGui::TableNextColumn(); TalentCheckbox(h, u8"Kapsářství", oCNpcTalent::NPC_TALENT_PICKPOCKET);
			ImGui::TableNextColumn(); TalentCheckbox(h, u8"Kovářství", oCNpcTalent::NPC_TAL_SMITH);
			ImGui::TableNextColumn(); TalentCheckbox(h, u8"Výroba run", oCNpcTalent::NPC_TAL_RUNES);
			ImGui::TableNextColumn(); TalentCheckbox(h, u8"Alchymie", oCNpcTalent::NPC_TAL_ALCHEMY);
			ImGui::TableNextColumn(); TalentCheckbox(h, u8"Trofeje ze zvířat", oCNpcTalent::NPC_TAL_TAKEANIMALTROPHY);
			ImGui::EndTable();
		}

		if (ImGui::Button(u8"Naučit všechny recepty a znalosti")) {
			Enqueue([]() {
				if (!InGame())
					return;
				int n = LearnAllRecipes();
				if (n > 0)
					::GT::Overlay::Toast(::GT::Text::Format(u8"Naučeno %d receptů a znalostí", n));
				else
					::GT::Overlay::Toast(u8"Tahle verze skriptů recepty nepoužívá");
			});
		}
		Tooltip(u8"Všechny lektvary, kovářské recepty, runy, vyvrhování zvířat, starý jazyk a schopnosti bludičky.");
	}

	static void HeroGuildSection(oCNpc* h) {
		ImGui::SeparatorText(u8"Gilda");
		std::string current = GuildName(h->guild);
		RowLabel(u8"Gilda");
		ImGui::SetNextItemWidth(Em(14.0f));
		if (ImGui::BeginCombo("##guild", current.c_str())) {
			for (int g = 0; g <= 14; g++) {
				std::string name = GuildName(g);
				bool selected = h->guild == g;
				if (ImGui::Selectable((name + "##g" + std::to_string(g)).c_str(), selected)) {
					int guild = g;
					Enqueue([guild]() {
						if (!InGame())
							return;
						player->SetGuild(guild);
						player->SetTrueGuild(guild);
					});
				}
				if (selected)
					ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}
		HelpMarker(u8"Změna gildy ovlivní rozhovory a úkoly. Před změnou si ulož hru.");
	}

	static void HeroProtectionSection(oCNpc* h) {
		ImGui::SeparatorText(u8"Ochrany");
		struct ProtRow { const char* label; const char* id; int index; };
		static const ProtRow rows[] = {
			{ u8"Proti zbraním", "##p_edge", oEDamageIndex_Edge },
			{ u8"Proti tupým", "##p_blunt", oEDamageIndex_Blunt },
			{ u8"Proti šípům", "##p_point", oEDamageIndex_Point },
			{ u8"Proti ohni", "##p_fire", oEDamageIndex_Fire },
			{ u8"Proti magii", "##p_magic", oEDamageIndex_Magic },
		};
		for (const ProtRow& r : rows)
			IntField(r.label, r.id, &h->protection[r.index], 1, 10);
		if (ImGui::Button(u8"Všechny ochrany na 999")) {
			for (int i = 0; i < oEDamageIndex_MAX; i++)
				h->protection[i] = 999;
		}
		HelpMarker(u8"Hodnoty se přepočítají, když si hrdina oblékne nebo svlékne zbroj.");
	}

	static void HeroMovementSection() {
		ImGui::SeparatorText(u8"Pohyb");
		ImGui::Checkbox(u8"Rychlý běh", &g_state.fastRun);
		Tooltip(u8"Stejný efekt jako lektvar rychlosti, ale trvalý.");
	}

	static void TabHero() {
		if (!InGame()) {
			NotInGame();
			return;
		}
		oCNpc* h = player;

		const float headerRight = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x;
		ImGui::PushFont(::GT::Overlay::TitleFont());
		ImGui::TextColored(kGold, "%s", NpcName(h).c_str());
		ImGui::PopFont();
		ImGui::SameLine();
		ImGui::AlignTextToFramePadding();
		ImGui::TextDisabled(u8"úroveň %d · %s", h->level, GuildName(h->guild).c_str());
		ImGui::SameLine(headerRight - Em(10.5f));
		if (SmallButtonColored(u8"★ Udělat legendu", kGold))
			MakeLegend();
		Tooltip(u8"Jedním klikem: 1000 života, 500 many, síla a obratnost 200, všechny zbraně na 100 % (mistr), 6. kruh magie, všechny dovednosti a recepty.");

		if (ImGui::BeginTable("##hero", 2, ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_PadOuterX)) {
			ImGui::TableNextColumn();
			HeroLifeSection(h);
			HeroAttributeSection(h);
			HeroExperienceSection(h);
			HeroMovementSection();

			ImGui::TableNextColumn();
			HeroFightSection(h);
			HeroSkillSection(h);
			HeroGuildSection(h);
			HeroProtectionSection(h);
			ImGui::EndTable();
		}
	}

} // namespace Ui
} // namespace Trainer
} // namespace GOTHIC_NAMESPACE
