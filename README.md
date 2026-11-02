# Gothic Trainer

Trainer pro **Gothic II: Noc Havrana** (včetně Steam verze s Workshopem). Běží přímo ve hře jako Union plugin a otevírá se jako menu přes hru.

## Co umí (verze 0.1)

- **Postava** – život, mana, síla, obratnost, úroveň, zkušenosti, body učení, nesmrtelnost, nekonečný život a mana, jedna rána zabije, boj (jednoruční, obouruční, luk, kuše), kruh magie, dovednosti (plížení, akrobacie, páčení, kapsářství, kovářství, runy, alchymie, trofeje), všechny recepty jedním klikem, gilda, ochrany, rychlý běh.
- **Předměty** – všechny předměty ze skriptů hry (funguje i s mody), hledání bez diakritiky („mec“ najde „Meč“), kategorie, libovolný počet kusů, rychlé zlato.
- **Svět** – teleport na kterýkoli waypoint, ukládání vlastních pozic, nastavení času, rychlost hry, zabití / uzdravení / uklidnění postavy, na kterou se díváš.
- **Konzole** – libovolný příkaz z Marvin módu bez zapínání Marvin módu, s historií a příklady.
- **Nastavení** – klávesa pro otevření, velikost písma, průhlednost, rychlost kurzoru, kódování textů.

## Požadavky

- Gothic II: Noc Havrana (verze 2.6 – Steam, GOG i CD verze s patchem)
- [Union](https://steamcommunity.com/sharedfiles/filedetails/?id=2787458679) (na Steamu stačí odebírat „Union – patch for Gothic 2“ ve Workshopu a zapnout ho v Mod Launcheru)

## Instalace

1. Stáhni `GothicTrainer.zip` z [Releases](../../releases/tag/latest).
2. Rozbal ho do složky hry, např. `C:\Program Files (x86)\Steam\steamapps\common\Gothic II\`.
   Vznikne soubor `System\Autorun\GothicTrainer.dll`.
3. Spusť hru (na Steamu přes Mod Launcher se zapnutým Unionem).

## Ovládání

- **F10** otevře a zavře trainer (klávesu jde změnit v Nastavení).
- Myš ovládá kurzor trainera, hra mezitím nereaguje na klávesy ani myš.
- **ESC** trainer zavře.

## Když něco nejde

- Ve složce `System` je soubor `GothicTrainer.log` – pošli ho, když něco nefunguje.
- Kurzor se nehýbe nebo je posunutý → Nastavení → *Použít kurzor Windows*, případně uprav *Rychlost kurzoru*.
- Divné znaky v názvech předmětů → Nastavení → *Kódování*.

## Sestavení

Projekt se sestavuje automaticky přes GitHub Actions (Visual Studio 2022, x86). Lokálně:

```
git clone --recursive <url>
cmake --preset G2A-Release
cmake --build --preset G2A-Release
```

Postaveno na [Union API / Gothic API](https://gitlab.com/union-framework) a [Dear ImGui](https://github.com/ocornut/imgui).
