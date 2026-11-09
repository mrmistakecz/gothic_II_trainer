# Změny

## 0.6.0

- **Úkoly přehledně a s dalším krokem**:
  - V seznamu je u každého probíhajícího úkolu hned vidět, co dál: například „Odevzdej: Lobart“, „Najdi: Ovčí kůže (1/3)“ nebo „Promluv si: Vatras“, se vzdáleností a oblastí (Město · tržiště, Onarova farma…). Zeleně je to, co jde dokončit hned, zlatě rozhovor, který úkol posune.
  - Řazení **Podle postupu**: nahoře úkoly, které jdou dokončit hned, pak úkoly v tomto světě podle vzdálenosti dalšího kroku, pak úkoly v jiném světě. Dále **Podle oblasti** a **Podle názvu**.
  - Detail úkolu začíná kartou **Další krok** s jedním hlavním tlačítkem (Dokončit úkol, Promluvit a vybrat, Teleport k předmětu, Teleport k truhle, Teleport k cíli…) a dalšími možnostmi pod ním. Tlačítko Dokončit úkol zůstává u každého probíhajícího úkolu.
  - **Přehled** Zadává → Uzavírá → Odměna (zkušenosti, zlato, předměty, i odměny podle volby v rozhovoru).
  - **Co úkol chce** jako zaškrtávací seznam ✓/✗: předměty s tím, kolik jich máš, kdo musí padnout (s tlačítky K cíli a Zabít), rozhovory, které musí proběhnout dřív, a kapitola.
  - **Deník** jako časová osa, nejnovější zápis dole.
  - Technické věci jsou sbalené v **Podrobnostech**: Připravit, Jen označit jako splněný, stav v deníku, všechny rozhovory úkolu, nápověda a proměnné.
  - Upozornění: když postava nabízí rozhovor, který úkol pokazí, když je mrtvý ten, kdo úkol uzavírá, a když úkol s příchodem další kapitoly zastará.
  - **Kde vzít nové úkoly**: ve filtru Nezačaté jsou nahoře úkoly, které ti někdo nabízí právě teď. Tlačítko **Přehled** ukáže, co jde hned dokončit a které nové úkoly si můžeš vzít; **Vzít si úkol** tě k postavě přenese a vybere větu, kterou úkol dostaneš.
- **Opravený teleport k postavám**, který končil na nábřeží v Khorinisu: postavy daleko od hrdiny hra uspí a nechá jim zastaralou polohu (často počátek světa v přístavu). Trainer teď bere místo jejich denního programu, kde je hra probudí, a postavy odklizené mimo hru nahlásí. Platí to pro Teleporty, Úkoly i minimapu.
- **Teleporty → Sochy a svatyně**: všechny Innosovy svatyně a sochy (i Beliarovy) v aktuálním světě s teleportem. Posedlé svatyně ze 3. kapitoly jsou označené.
- Nová záložka **Obchod a učení**:
  - **Obchodníci**: kdo co prodává a za kolik, kolik má zlata a jestli s tebou teď obchoduje. Hledání zboží (luk, lektvar…) ukáže, kdo ho prodává nejlevněji a kde je. Tlačítka Obchodovat (přenese tě k němu a otevře obchod), Teleport, Doplnit zboží a Dát mu 500 zlatých.
  - **Učitelé**: co kdo učí a do jaké úrovně, podle dovednosti i podle učitele. Texty jsou stejné jako v jejich nabídce ve hře. Ukazuje, co už umíš a kdo tě učí právě teď. Tlačítko Učit se tě přenese k učiteli a otevře nabídku lekcí.
- Nová záložka **Mapa**: herní mapa světa s hrdinou, dalšími kroky úkolů, obchodníky, učiteli a svatyněmi. Kolečko přibližuje, tažení posouvá a dvojklik teleportuje na místo.
- **Hledat** (vpravo na liště záložek): jedno hledání přes úkoly, postavy ve světě, předměty (i kdo je prodává), co se dá naučit a místa. U každého výsledku je rovnou tlačítko.
- Rozbor skriptů navíc pozná lekce (funkce B_Teach*). Rozbor úkolů zůstal beze změny, ověřeno nad GOTHIC.DAT české verze.

## 0.5.2

- Úkoly: tlačítko **Navigovat**. Když je nainstalovaná minimapa GothicQuestNav, předá jí probíhající úkol a ta ukáže cestu k jeho dalšímu kroku.
- Přesnější rozbor úkolů ze skriptů:
  - Rozhovory, které končí předčasným „return“, se čtou celé. Úkoly jako Matteo a Gritta nebo Bosperovy kožešiny tak mají závěrečný rozhovor.
  - „Stačí jedna z možností“ (promluv si s ní, nebo ji zab) se už neukazuje jako povinné zabití. Alternativní rozhovory se ukazují jako „stačí jeden z rozhovorů“.
  - Poznámky (učitelé a podobně) nemají splnění.
- Dokončit úkol volí přednostně čisté splnění před rozhovorem, který může úkol i pokazit („příliš pozdě“). Když je potřeba nejdřív mluvit s jinou postavou, napíše s kým.
- Minimapa GothicQuestNav 0.5.0 (samostatná DLL): cíle u všech úkolů v deníku, funkční detailní terén a texty s diakritikou. Podrobnosti jsou v navigation/README.md.

## 0.5.1

- Úkoly: **Dokončit úkol** dokončí úkol opravdu ve světě. Trainer doplní potřebné předměty, přenese hrdinu k postavě, začne rozhovor a sám v něm vybere závěrečnou větu – stejně jako hráč. Postava si vezme, co chtěla, dá odměnu a příběh pokračuje, jako bys úkol odehrál, takže už nezůstane „splněno v deníku, ale ne ve světě“.
  - Když úkol u stejné postavy ještě nezačal nebo závěrečná věta potřebuje předchozí rozhovor, trainer nejdřív vybere i ten. Kde se v rozhovoru vybírá odpověď, vybereš ji sám.
  - Když to rozhovorem nejde, trainer napíše proč (úkol ještě nezačal a u koho ho vzít, chybí kapitola, někdo musí padnout, postava rozhovor ukončila…).
  - Během dokončování je v záložce Úkoly vidět, co se děje, a dá se zrušit.
- U úkolů s více možnými konci má každý závěrečný rozhovor vlastní tlačítko Dokončit tímto rozhovorem.
- Připravit (dřív Připravit splnění) dá předměty i pro rozhovory, ve kterých se předměty odevzdávají postupně.
- Dokončit hned se teď jmenuje **Jen označit jako splněný** a hodí se jen pro úkoly, které rozhovorem dokončit nejdou: změní deník a stav úkolu, ale postavy ve světě o tom nevědí.
- Přesnější podmínky úkolů: „postava musí žít“ a „tenhle rozhovor ještě nesmí proběhnout“ se už neukazují obráceně jako „zabít“ a „nejdřív rozhovor“.
- Tlačítka úkolu se v úzkém okně zalomí na další řádek.

## 0.5.0

- Nová záložka **Úkoly**:
  - Všechny úkoly hry (přes 200): probíhající, splněné, nesplněné i ty, které ještě nezačaly, s hledáním.
  - U každého úkolu zápisy z deníku, kdo ho zadává a kdo ho uzavírá (s teleportem a tlačítkem Promluvit), co je potřeba (předměty s počtem, kdo musí být mrtvý, potřebné rozhovory, kapitola) a jaká je odměna.
  - **Co můžeš udělat teď**: rozhovory, které úkol právě teď posunou.
  - **Připravit splnění**: doplní potřebné předměty, přenese hrdinu k postavě a začne rozhovor; hra úkol dokončí sama i s odměnou.
  - **Dokončit hned**: úkol se splní bez rozhovoru a hrdina dostane odměnu (zkušenosti a předměty).
  - Změna stavu v deníku, proměnné úkolu, srovnání deníku a nápověda se všemi zápisy, které úkol může do deníku napsat.
  - Všechno se čte přímo ze skriptů hry, takže to funguje s češtinou i s módy.
- Záložka Skripty a úkoly se teď jmenuje Skripty (proměnné a funkce zůstaly beze změny).

## 0.4.0

- Nová záložka **Teleporty**:
  - Známá místa s českými názvy pro Khorinis, Hornické údolí, Jharkendar a Dračí ostrov (přes 100 míst a postav: Vatras, Lord Hagen, Onar, Xardas, draci, teleportační kameny…). Ukazují se jen místa, která v načteném světě opravdu existují, takže módy s jinými světy nic nerozbijí.
  - Teleport ke kterékoli postavě v celém světě podle jména a její přivolání k hrdinovi.
  - Všechny waypointy s česky popsaným názvem (např. „Město · přístav · hospoda 01“); hledá se česky i podle původního názvu.
  - Cesta do jiných světů stejným přechodem, jaký používá hra (průsmyk, portál, loď), s potvrzením.
  - Uložené pozice (přesunuté ze záložky Svět) a teleport na souřadnice.
  - Zpět na 30 posledních míst, Vpřed 5 m a Nahoru 3 m pro zaseknutého hrdinu.
- Vyvolávání postav ukazuje skutečná jména ze skriptů hry (s českými skripty česky) u všech lidí i tvorů.
- Svět: počasí (podle hry, déšť, jasno), odemčení truhel a dveří v okolí, sebrání předmětů ze země, uklidnění všech v okolí a odpuštění všech zločinů.
- Postava: bez zranění pádem a nekonečné šípy i šipky.
- Předměty: přepínač Můj inventář s přehledem nesených věcí a rychlým přidáním +1, +10 a ×2.

## 0.3.0

- Vstup myši se zachytí po ProcessInputEvents před zpracováním herní kamerou.
- Vlastní zlatý kurzor nad oknem, nezávislý na systémovém kurzoru a fontových kurzorech ImGui.
- Grafický test kontroluje i skutečné pixely kurzoru mimo okno menu.
- Samostatná experimentální minimapa GothicQuestNav používající nativní zCView, renderer, deník a zCWayNet. Automatické cíle pro šest konkrétních úkolů podle stavu skriptů a inventáře; další úkoly zatím nepokryté.

## 0.2.1

- Opravené bliknutí menu po F10: zpráva Windows a stav DirectInputu používají společnou západku, takže jeden stisk přepne menu pouze jednou.
- Testy pro oba možné sledy vstupů, držení klávesy, krátký stisk a opožděné uvolnění.

## 0.2.0

- Hotová x86 DLL a instalátor se zálohou předchozí verze.
- Načítání nastavení při startu; kontrola neplatných kláves, kódování a čísel.
- Opravený posuvník kruhu magie a přepínání bojových animací podle dovednosti.
- Při načtení hry a přechodu světa se zahodí staré akce, odkazy a katalogy.
- Katalog předmětů obnovuje kontext parseru a uvolňuje dočasné předměty i při chybě.
- Konzole obnoví předchozí Marvin režim i při chybě. Skripty a konzole jsou dostupné po načtení hry.
- Zachycení krátkého stisku F10, uvolnění ovládání po zavření menu a obnova grafického stavu hry.
- Kontrola souřadnic a práce s desetinnou tečkou nezávisle na jazyku Windows.
- Ukládání nastavení a logu do uživatelské složky, pokud je adresář hry chráněný.
- 24 samostatných regresních kontrol a test DirectX 7 rendereru.

## 0.1.0

- První verze: menu přímo ve hře (F10) s českým rozhraním.
- Záložky Postava, Předměty, Svět, Konzole a Nastavení.
- Sestavení přes GitHub Actions, balíček `GothicTrainer.zip` v Releases.

