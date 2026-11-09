# Gothic QuestNav 0.5.1 – minimapa a mapa úkolů

Kruhová minimapa vpravo dole a velká mapa pro Gothic II: Noc Havrana 2.6 fix s Unionem a původním rendererem DirectX 7. Samostatná DLL, trainer nepotřebuje. Když je trainer nainstalovaný, umí jí poslat úkol tlačítkem **Navigovat**.

## Co je nového v 0.5.1

- Cíle u postav, které hra daleko od hrdiny uspala, ukazuje na místě jejich denního programu, kde je hra probudí. Dřív měly zastaralou polohu, často počátek světa v přístavu Khorinisu. Postavy odklizené mimo hru (waypoint TOT) se jako cíl nenabízejí.

## Co je nového v 0.5.0

- **Cíle pro všechny úkoly v deníku**, ne jen pro šest vybraných. Minimapa čte rozhovory úkolu přímo ze skriptů hry (stejný rozbor jako záložka Úkoly v traineru) a podle aktuálního stavu hry ukáže, co dělat dál:
  - **Odevzdej: postava**, když postava už nabízí závěrečný rozhovor.
  - **Promluv si: postava**, když tě úkol posune rozhovor, který je právě dostupný, nebo když je potřeba nejdřív mluvit s někým jiným.
  - **Najdi: předmět (máš/potřebuješ)**: nejbližší kus ležící ve světě. **V truhle: předmět**: truhla, kde leží. **Postava má: předmět**: kdo ho nosí u sebe (i obchodník nebo mrtvola). **Dá ti to: postava**: kdo ho dává v rozhovoru.
  - **Zabij: postava**, když ji úkol opravdu vyžaduje. Kde stačí jedna z možností (třeba „promluv si s ní, nebo ji zab“), zabití nenabízí.
  - **Úkol uzavírá: postava** jako poslední možnost. Když je cíl jinde, napíše **Cíl je v jiném světě: Hornické údolí**.
- Hlavní cíl je zlatá značka s cestou. Ostatní cíle úkolu jsou menší značky: modrá rozhovor, zelená předmět, červená nepřítel. Velká mapa je vypíše i se vzdáleností.
- **Detailní terén konečně funguje.** Verze 0.4.0 vyráběla textury ve 32bitovém formátu, který původní renderer Gothicu nepodporuje (bere jen 16bitové ARGB 4444, ARGB 1555, RGB 565 a DXT). Proto zůstávala na pergamenu a bez hladkého rámečku. Teď se terén, výplně i mosazný rámeček vyrábějí v 16bitovém formátu. Výsledek každé textury je v GothicQuestNav.log.
- Texty česky s diakritikou, ve stejném kódování jako trainer (Nastavení → Kódování).
- Ruční cíl (Ctrl+F6) a skriptový GQN_SetTarget dál fungují a mají přednost. S novou etapou deníku se minimapa vrátí k automatickým cílům.

## Ovládání

| Klávesa | Funkce |
| --- | --- |
| M | Otevřít / zavřít velkou mapu aktuálního světa |
| Esc | Zavřít velkou mapu |
| F6 | Zapnout / vypnout minimapu; ve velké mapě další úkol |
| Shift+F6 | Další probíhající úkol |
| Ctrl+F6 | Přiřadit zaměřenou postavu jako ruční cíl aktuální etapy úkolu |
| + / − (i numerické) | Rozsah minimapy 90 / 180 / 300 metrů; ve velké mapě přiblížení |
| F7 | Detailní terén / původní pergamen, v minimapě i velké mapě |
| Šipky | Posun přiblížené mapy |
| Home | Vycentrovat velkou mapu na hráče |
| Tab | Mapa oblasti / města, pokud má svět obě |

Velká mapa hru pozastaví a po zavření ji zase pustí. M se během rozhovoru, inventáře, cutscény nebo jiného menu nepřebírá. F10 zavře velkou mapu a ponechá klávesu traineru.

Z traineru: Úkoly → vyber probíhající úkol → **Navigovat**. Minimapa se zapne a vede k dalšímu kroku toho úkolu.

## Jak mapa vzniká

Detailní vrstva se skládá přímo z polygonů načteného světa (cesty, pobřeží, voda, skály, střechy). Vzniká postupně asi 2 ms v každém snímku, takže po načtení světa chvíli trvá. Do té doby se ukazuje původní pergamen. Pergamen i kalibraci souřadnic plugin čte z herních skriptů USE_MAP_NEWWORLD, USE_MAP_NEWWORLD_CITY, USE_MAP_OLDWORLD a USE_MAP_ADDONWORLD. Skripty kvůli tomu nespouští a žádné mapy hráči nepřidává. Svět bez kalibrované mapy (interiér, mod) plugin ohlásí.

Cestu počítá herní síť waypointů (zCWayNet). Poslední úsek od waypointu k cíli nemusí být v síti. Když cesta neexistuje, ukáže se jen směr.

## Instalace a soubory

Instalovat_minimapu.bat (při zavřené hře) zkopíruje System/Autorun/GothicQuestNav.dll. Pro vypnutí stačí při zavřené hře tu DLL odebrat. Sestavovací okno traineru (Sestavit_trainer.bat) od verze 0.5.0 sestavuje a instaluje minimapu spolu s trainerem. Předchozí DLL minimapy zálohuje do GothicQuestNav/backups/pred-sestavenim.

Nastavení a ruční cíle jsou v GothicQuestNav.ini, záznam v GothicQuestNav.log (načtené mapy, textury, rozbor úkolů, případná zachycená chyba). Při zachycené chybě se plugin vypne a hru nechá běžet.

Pro skripty a mody zůstává externí funkce GQN_SetTarget(topic, world, waypoint): tři řetězce, téma musí přesně odpovídat běžícímu úkolu, world je název souboru světa velkými písmeny a waypoint musí existovat. Volá se až po Log_AddEntry nové etapy.

## Ověření

Prošla kontrola překladačem clang proti Gothic API a ostré sestavení MSVC x86. Rozbor úkolů je stejný kód jako v traineru a byl ověřený nad GOTHIC.DAT české verze: 225 témat, 145 úkolů se závěrečným rozhovorem. Formát textur a postup kreslení jsou ověřené rozborem Gothic2.exe (zCRnd_D3D::SupportsTextureFormat, zCTex_D3D::XTEX_BuildSurfaces, DrawPolySimple). Vzhled v běžící hře je potřeba potvrdit; kdyby něco nesedělo, F7 přepne na pergamen.

Původní minimapu 0.3.0 a 0.4.0 (kruhová minimapa, velká mapa, terén, kalibrace) napsal Codex; verze 0.5.0 na ni navazuje.
