# Gothic Trainer 0.6.0

Verze 0.6.0 dělá z úkolů přehled, podle kterého se dá hrát: u každého úkolu je vidět další krok, vzdálenost a oblast, nahoře jsou úkoly, které jdou dokončit hned, a detail začíná kartou Další krok s jedním hlavním tlačítkem. Přidává záložky Obchod a učení (kdo co prodává a učí), Mapa (herní mapa s úkoly a teleportem dvojklikem) a Hledat (jedno hledání přes všechno). Opravuje teleport k postavám, které hra daleko od hrdiny uspala, a přidává teleport ke všem Innosovým sochám a svatyním. Všechno z předchozích verzí zůstává.

Součástí zdrojů je také samostatná minimapa GothicQuestNav. Její funkce a ovládání popisuje `navigation/README.md` (v balíčku NAVOD_MINIMAPA.md).

Český trainer pro Gothic II: Noc Havrana 2.6 (fix). Běží přímo ve hře jako Union plugin. **F10** otevře a zavře menu, **Esc** ho zavře.

## Instalace hotové verze

1. Zavři Gothic II a rozbal celý instalační ZIP.
2. Spusť `Instalovat_trainer.bat`. Instalátor je připravený pro `C:\Program Files (x86)\Steam\steamapps\common\Gothic II`.
3. Spusť hru přes Steam Mod Launcher se zapnutým **Union – patch for Gothic 2**.
4. Načti uloženou pozici nebo začni novou hru a zmáčkni **F10**.

Instalátor kontroluje soubory a zálohuje předchozí DLL a zdrojový ZIP do `GothicTrainer\backups`. Hotová DLL nevyžaduje překladač ani instalaci Visual Studia. Nové zdroje uloží do `GothicTrainer\src.zip`, který umí zpracovat i dosavadní automatické sestavování.

Pro jinou složku hry: `powershell -NoProfile -ExecutionPolicy Bypass -File .\Install-Trainer.ps1 -GameDirectory "D:\Hry\Gothic II"`.

Ruční instalace: zkopíruj `System\Autorun\GothicTrainer.dll` do stejné složky hry. Odinstalace: při zavřené hře odstraň tuto DLL. Nastavení a uložené pozice trainera mohou zůstat.

## Funkce

- **Postava:** život, mana, atributy, zkušenosti, body učení, nesmrtelnost, nekonečný život a mana, jedna rána zabije, bojové dovednosti, kruh magie, recepty, gilda, ochrany, rychlý běh, bez zranění pádem, nekonečné šípy a tlačítko Udělat legendu.
- **Předměty:** katalog z aktivních skriptů, hledání bez diakritiky, kategorie, počet kusů, skrytí úkolových předmětů, rychlé zlato a přehled vlastního inventáře s přidáním +1, +10 a ×2.
- **Postavy a tvorové:** vyvolávání podle skriptů se skutečnými jmény ze hry a správa postav v okolí.
- **Úkoly:** všechny úkoly hry včetně nezačatých. V seznamu další krok každého úkolu se vzdáleností a oblastí, řazení podle postupu, oblasti nebo názvu. V detailu karta Další krok s hlavním tlačítkem (Dokončit úkol přímo v rozhovoru, Promluvit a vybrat, Vzít si úkol, teleport k předmětu, truhle nebo cíli), přehled Zadává → Uzavírá → Odměna, zaškrtávací seznam toho, co úkol chce, deník jako časová osa, upozornění a Navigovat pro minimapu. Přehled ukáže, co jde dokončit hned a které nové úkoly ti někdo nabízí. V Podrobnostech Připravit, Jen označit jako splněný, stav v deníku, všechny rozhovory, nápověda a proměnné.
- **Obchod a učení:** obchodníci se zbožím, cenami, zlatem a hledáním zboží (Obchodovat, Teleport, Doplnit zboží, Dát mu zlato); učitelé s tím, co učí a do jaké úrovně, co už umíš a kdo tě učí teď (Učit se, Teleport).
- **Mapa:** herní mapa aktuálního světa s hrdinou, dalšími kroky úkolů, obchodníky, učiteli a svatyněmi; přiblížení kolečkem, posun tažením, teleport dvojklikem.
- **Hledat:** jedno hledání přes úkoly, postavy ve světě, předměty (i kdo je prodává), lekce a místa, s tlačítkem u každého výsledku.
- **Teleporty:** známá místa s českými názvy, teleport k jakékoli postavě ve světě (i k té, kterou hra uspala), Innosovy sochy a svatyně, všechny waypointy s českým popisem, cesty do jiných světů, uložené pozice, souřadnice a návrat Zpět.
- **Svět:** čas, zastavení času, rychlost hry, počasí, odemčení truhel a dveří, sebrání předmětů ze země, uklidnění okolí, odpuštění zločinů a práce se zaměřenou postavou.
- **Skripty:** globální proměnné a pole včetně stavů úkolů a spouštění funkcí s nejvýše čtyřmi celočíselnými parametry.
- **Konzole:** příkazy Marvin módu s historií a příklady.
- **Nastavení:** klávesa pro otevření, písmo, průhlednost, kurzor a kódování textů hry.

Změny postavy a úkolů se mohou uložit do herní pozice; pro zkoušení používej samostatnou pozici. Před vyvoláváním úkolových postav, změnou úkolových proměnných nebo cestou do jiného světa si ulož hru.

## Když něco nejde

- F10 nereaguje: zkontroluj zapnutý Union a umístění DLL v `System\Autorun`.
- Poškozené názvy předmětů nebo postav: Nastavení → Kódování → 1250 pro české skripty.
- Teleport skončil ve zdi nebo pod zemí: Teleporty → Zpět, případně Nahoru 3 m.
- Kurzor stojí nebo je posunutý: Nastavení → Použít kurzor Windows, případně Rychlost kurzoru.
- Nastavení a log najdeš na cestách zobrazených v záložce Nastavení. Obvykle jsou vedle `Gothic2.exe`; pokud je složka chráněná proti zápisu, trainer použije vlastní složku v `%LOCALAPPDATA%\GothicTrainer`.

## Sestavení ze zdrojů

Vyžaduje Visual Studio Build Tools s C++ x86, Windows SDK, CMake a Ninja. Závislosti jsou ve zdrojovém ZIPu.

```powershell
.\Build.ps1 -Tests
```

Výstup: `out\build\G2A-Release\GothicTrainer.dll`. Projekt odmítne sestavení pro 64 bitů. Základní regresní testy běží samostatně bez hry; grafický test hlásí přeskočení, pokud prostředí nemá funkční DirectDraw rasterizaci.

Postaveno na Union API / Gothic API a Dear ImGui. Licenční soubory závislostí jsou zachované v jejich složkách.
