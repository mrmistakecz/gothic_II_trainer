// Engine side: Czech names for worlds, well-known places and waypoints.
// The places point to waypoints of the original worlds; only the ones that
// exist in the loaded world are offered, so mods with other worlds are safe.

namespace GOTHIC_NAMESPACE {
namespace Trainer {

	// "NEWWORLD\\NEWWORLD.ZEN", "newworld.zen" -> "NEWWORLD"
	static std::string WorldKeyFromPath(const std::string& path) {
		std::string key = path;
		size_t slash = key.find_last_of("\\/");
		if (slash != std::string::npos)
			key = key.substr(slash + 1);
		size_t dot = key.find('.');
		if (dot != std::string::npos)
			key = key.substr(0, dot);
		for (char& c : key)
			c = (char)toupper((unsigned char)c);
		return key;
	}

	static std::string WorldDisplayName(const std::string& key) {
		if (key == "NEWWORLD")     return u8"Khorinis";
		if (key == "OLDWORLD")     return u8"Hornické údolí";
		if (key == "ADDONWORLD")   return u8"Jharkendar";
		if (key == "DRAGONISLAND") return u8"Dračí ostrov";
		std::string nice;
		for (size_t i = 0; i < key.size(); i++)
			nice += i == 0 ? key[i] : (char)tolower((unsigned char)key[i]);
		return nice.empty() ? std::string(u8"neznámý svět") : nice;
	}

	// ------------------------------------------------------------------ known places

	struct KnownPlace {
		const char* world;     // world key, e.g. "NEWWORLD"
		const char* group;     // UTF-8
		const char* label;     // UTF-8
		const char* waypoint;  // waypoint name in the world
	};

	static const KnownPlace kKnownPlaces[] = {
		// ---------------------------------------------------------------- Khorinis
		{ "NEWWORLD", u8"Město Khorinis", u8"Městská brána", "NW_CITY_ENTRANCE_01" },
		{ "NEWWORLD", u8"Město Khorinis", u8"Vatras – Adanův chrám na tržišti", "NW_CITY_MERCHANT_TEMPLE_FRONT" },
		{ "NEWWORLD", u8"Město Khorinis", u8"Lord Hagen – radnice", "NW_CITY_HAGEN" },
		{ "NEWWORLD", u8"Město Khorinis", u8"Andre – kasárna domobrany", "NW_CITY_HABOUR_KASERN_MAIN_ENTRY" },
		{ "NEWWORLD", u8"Město Khorinis", u8"Harad – kovárna", "NW_CITY_SMITH_FIRE" },
		{ "NEWWORLD", u8"Město Khorinis", u8"Constantino – alchymista", "NW_CITY_ALCHEMY_VELAX" },
		{ "NEWWORLD", u8"Město Khorinis", u8"Bosper – lukař", "NW_CITY_MERCHANT_SHOP01_IN_02" },
		{ "NEWWORLD", u8"Město Khorinis", u8"Thorben – truhlář", "NW_CITY_MERCHANT_SHOP01_FRONT_01" },
		{ "NEWWORLD", u8"Město Khorinis", u8"Matteo – obchodník", "NW_CITY_MATTEO" },
		{ "NEWWORLD", u8"Město Khorinis", u8"Coragon – hospoda", "NW_CITY_TAVERN_IN_03" },
		{ "NEWWORLD", u8"Město Khorinis", u8"Kardif – přístavní krčma", "NW_CITY_KARDIF" },
		{ "NEWWORLD", u8"Město Khorinis", u8"Bromor – Rudá lucerna", "NW_CITY_PUFF_COUNTER" },
		{ "NEWWORLD", u8"Město Khorinis", u8"Lehmar – lichvář", "NW_CITY_HABOUR_HUT_08_IN_C" },
		{ "NEWWORLD", u8"Město Khorinis", u8"Lares – přístav", "NW_CITY_HABOUR_02_B" },
		{ "NEWWORLD", u8"Město Khorinis", u8"Ignaz – přístavní čtvrť", "NW_CITY_HABOUR_POOR_AREA_HUT_08_WORK" },
		{ "NEWWORLD", u8"Město Khorinis", u8"Garvell – loděnice", "NW_CITY_HABOUR_07" },
		{ "NEWWORLD", u8"Město Khorinis", u8"Jack – přístav", "NW_CITY_HABOUR_06_C" },
		{ "NEWWORLD", u8"Město Khorinis", u8"Cassia – zlodějský cech v kanálech", "NW_CITY_KANAL_ROOM_05_02" },
		{ "NEWWORLD", u8"Město Khorinis", u8"Maják", "NW_LIGHTHOUSE_IN_01" },

		{ "NEWWORLD", u8"Klášter", u8"Brána kláštera", "NW_MONASTERY_ENTRY_01" },
		{ "NEWWORLD", u8"Klášter", u8"Pyrokar – trůnní sál", "NW_MONASTERY_THRONE_01" },
		{ "NEWWORLD", u8"Klášter", u8"Isgaroth – svatyně", "NW_MONASTERY_SHRINE_04" },

		{ "NEWWORLD", u8"Farmy a hospody", u8"Onarova farma", "BIGFARM" },
		{ "NEWWORLD", u8"Farmy a hospody", u8"Onar – statkář", "NW_BIGFARM_HOUSE_ONAR_SIT" },
		{ "NEWWORLD", u8"Farmy a hospody", u8"Lee – vůdce žoldáků", "NW_BIGFARM_HOUSE_LEE_GUARDING" },
		{ "NEWWORLD", u8"Farmy a hospody", u8"Bennet – kovář žoldáků", "NW_BIGFARM_SMITH_FIRE" },
		{ "NEWWORLD", u8"Farmy a hospody", u8"Lobartova farma", "FARM1" },
		{ "NEWWORLD", u8"Farmy a hospody", u8"Lobart", "NW_FARM1_LOBART" },
		{ "NEWWORLD", u8"Farmy a hospody", u8"Akilova farma", "FARM2" },
		{ "NEWWORLD", u8"Farmy a hospody", u8"Akil", "NW_FARM2_PATH_02" },
		{ "NEWWORLD", u8"Farmy a hospody", u8"Bengar – horní pastviny", "NW_FARM3_BENGAR" },
		{ "NEWWORLD", u8"Farmy a hospody", u8"Sekob", "NW_FARM4_SEKOB" },
		{ "NEWWORLD", u8"Farmy a hospody", u8"Hospoda U Mrtvé harpyje", "TAVERNE" },
		{ "NEWWORLD", u8"Farmy a hospody", u8"Orlan – hostinský", "NW_TAVERNE_IN_04" },

		{ "NEWWORLD", u8"Krajina a tajemná místa", u8"Xardas – začátek hry", "NW_XARDAS_START" },
		{ "NEWWORLD", u8"Krajina a tajemná místa", u8"Lester – u Xardasovy věže", "NW_XARDAS_TOWER_LESTER" },
		{ "NEWWORLD", u8"Krajina a tajemná místa", u8"Sagitta – jeskyně bylinkářky", "NW_SAGITTA_CAVE_IN_02" },
		{ "NEWWORLD", u8"Krajina a tajemná místa", u8"Dexter – tábor banditů", "NW_CASTLEMINE_HUT_10" },
		{ "NEWWORLD", u8"Krajina a tajemná místa", u8"Kamenný kruh", "NW_TROLLAREA_RITUAL_04" },
		{ "NEWWORLD", u8"Krajina a tajemná místa", u8"Malý kamenný kruh", "NW_LITTLESTONEHENDGE_01" },
		{ "NEWWORLD", u8"Krajina a tajemná místa", u8"Saturas – vykopávky", "NW_TROLLAREA_PORTALTEMPEL_STUDY_01" },
		{ "NEWWORLD", u8"Krajina a tajemná místa", u8"Portál do Jharkendaru", "NW_TROLLAREA_PORTAL_02" },
		{ "NEWWORLD", u8"Krajina a tajemná místa", u8"Cesta k průsmyku", "NW_TO_PASS_01" },
		{ "NEWWORLD", u8"Krajina a tajemná místa", u8"Průsmyk do Hornického údolí", "NW_PASS_01" },

		// ---------------------------------------------------------------- Hornické údolí
		{ "OLDWORLD", u8"Hrad", u8"Hradní nádvoří", "OC1" },
		{ "OLDWORLD", u8"Hrad", u8"Velitel Garond – trůnní sál", "OC_EBR_HALL_THRONE" },
		{ "OLDWORLD", u8"Hrad", u8"Milten – alchymistická dílna", "OC_MAGE_LAB_ALCHEMY" },
		{ "OLDWORLD", u8"Hrad", u8"Knihovna mágů", "OC_MAGE_LIBRARY_IN" },
		{ "OLDWORLD", u8"Hrad", u8"Gorn – vězení", "OC_PRISON_CELL_03_SIT_GROUND" },
		{ "OLDWORLD", u8"Hrad", u8"Dobar – kovárna", "OC_SMITH_FIRE" },
		{ "OLDWORLD", u8"Hrad", u8"Brutus – mučírna", "OC_FOLTER_SHARP" },

		{ "OLDWORLD", u8"Doly a tábory", u8"Fajeth – nový důl", "OW_NEWMINE_09" },
		{ "OLDWORLD", u8"Doly a tábory", u8"Marcos – chata u pily", "OW_SAWHUT_MOLERAT_SPAWN01" },
		{ "OLDWORLD", u8"Doly a tábory", u8"Grimes – důl", "OW_MINE2_GRIMES" },
		{ "OLDWORLD", u8"Doly a tábory", u8"Silvestro – důl", "OW_MINE3_LEICHE_01" },
		{ "OLDWORLD", u8"Doly a tábory", u8"Tábor drakobijců", "OW_DJG_STARTCAMP_01" },
		{ "OLDWORLD", u8"Doly a tábory", u8"Hlídka drakobijců", "OW_DJG_VORPOSTEN_01" },
		{ "OLDWORLD", u8"Doly a tábory", u8"Gestath – skalní tábor", "OW_DJG_ROCKCAMP_01" },
		{ "OLDWORLD", u8"Doly a tábory", u8"Diego", "LOCATION_02_05" },
		{ "OLDWORLD", u8"Doly a tábory", u8"Olav", "OW_WOLFHUT_01" },
		{ "OLDWORLD", u8"Doly a tábory", u8"Skřetí palisáda", "OW_ORCBARRIER_08_01" },
		{ "OLDWORLD", u8"Doly a tábory", u8"Kamenný kruh", "OW_DJG_WATCH_STONEHENGE_01" },
		{ "OLDWORLD", u8"Doly a tábory", u8"Ledová oblast – vstup", "OW_ICEREGION_ENTRANCE" },

		{ "OLDWORLD", u8"Draci", u8"Bažinný drak Pandrodor", "OW_SWAMPDRAGON_01" },
		{ "OLDWORLD", u8"Draci", u8"Kamenný drak Pedrakhan", "OW_ROCKDRAGON_11" },
		{ "OLDWORLD", u8"Draci", u8"Ohnivý drak Feomathar", "CASTLE_36" },
		{ "OLDWORLD", u8"Draci", u8"Ledový drak Finkregh", "OW_ICEDRAGON_01" },

		// ---------------------------------------------------------------- Jharkendar
		{ "ADDONWORLD", u8"Mágové vody a teleporty", u8"Příchod portálem", "ADW_ENTRANCE" },
		{ "ADDONWORLD", u8"Mágové vody a teleporty", u8"Saturas – tábor mágů vody", "ADW_SATURAS_START" },
		{ "ADDONWORLD", u8"Mágové vody a teleporty", u8"Merdarion", "ADW_ENTRANCE_01" },
		{ "ADDONWORLD", u8"Mágové vody a teleporty", u8"Duch Quarhodron", "ADW_ANCIENTGHOST" },
		{ "ADDONWORLD", u8"Mágové vody a teleporty", u8"Teleportační kámen – u portálu", "ADW_PORTALTEMPEL_TELEPORTSTATION" },
		{ "ADDONWORLD", u8"Mágové vody a teleporty", u8"Teleportační kámen – Adanův chrám", "ADW_ADANOSTEMPEL_TELEPORTSTATION" },
		{ "ADDONWORLD", u8"Mágové vody a teleporty", u8"Teleportační kámen – piráti", "ADW_PIRATES_TELEPORTSTATION" },
		{ "ADDONWORLD", u8"Mágové vody a teleporty", u8"Teleportační kámen – jihovýchod", "ADW_SOUTHEAST_TELEPORTSTATION" },
		{ "ADDONWORLD", u8"Mágové vody a teleporty", u8"Teleportační kámen – jihozápad", "ADW_SOUTHWEST_TELEPORTSTATION" },

		{ "ADDONWORLD", u8"Piráti", u8"Pláž u pirátů", "STRAND" },
		{ "ADDONWORLD", u8"Piráti", u8"Kapitán Greg", "ADW_PIRATECAMP_GREG" },
		{ "ADDONWORLD", u8"Piráti", u8"Krokodýl Jack", "ADW_ENTRANCE_2_PIRATECAMP_01" },
		{ "ADDONWORLD", u8"Piráti", u8"Francis", "ADW_PIRATECAMP_HUT4_01" },
		{ "ADDONWORLD", u8"Piráti", u8"Samuel – pálenice", "ADW_PIRATECAMP_CAVE1_04" },
		{ "ADDONWORLD", u8"Piráti", u8"Henry", "ADW_PIRATECAMP_WAY_04" },
		{ "ADDONWORLD", u8"Piráti", u8"Bones", "ADW_PIRATECAMP_TRAIN_01" },
		{ "ADDONWORLD", u8"Piráti", u8"Hank", "ADW_PIRATECAMP_WATERHOLE_CAVE" },

		{ "ADDONWORLD", u8"Bandité", u8"Tábor banditů", "BANDIT" },
		{ "ADDONWORLD", u8"Bandité", u8"Franco – před táborem", "ADW_PATH_TO_BL_02" },
		{ "ADDONWORLD", u8"Bandité", u8"Thorus", "BL_STAIRS_03" },
		{ "ADDONWORLD", u8"Bandité", u8"Esteban", "BL_MID_07" },
		{ "ADDONWORLD", u8"Bandité", u8"Fisk", "BL_MID_05" },
		{ "ADDONWORLD", u8"Bandité", u8"Snaf – hospoda", "BL_INN_BAR_02" },
		{ "ADDONWORLD", u8"Bandité", u8"Zlatý důl", "ADDON_GOLDMINE" },
		{ "ADDONWORLD", u8"Bandité", u8"Garaz – důl", "ADW_MINE_TO_MC_04" },
		{ "ADDONWORLD", u8"Bandité", u8"Patrick – důl", "ADW_MINE_LAGER_05" },
		{ "ADDONWORLD", u8"Bandité", u8"Raven – Adanův chrám", "ADW_ADANOSTEMPEL_RAVEN_11" },

		// ---------------------------------------------------------------- Dračí ostrov
		{ "DRAGONISLAND", u8"Dračí ostrov", u8"Loď – paluba", "SHIP_DECK_01" },
		{ "DRAGONISLAND", u8"Dračí ostrov", u8"Kapitán lodi", "SHIP_CREW_CAPTAIN" },
		{ "DRAGONISLAND", u8"Dračí ostrov", u8"Pedro", "DI_ORKAREA_PEDRO" },
		{ "DRAGONISLAND", u8"Dračí ostrov", u8"Skřetí plukovník", "DI_ORKOBERST" },
		{ "DRAGONISLAND", u8"Dračí ostrov", u8"Území ještěrů – ohnivý drak", "DI_DRACONIANAREA_FIREDRAGON" },
		{ "DRAGONISLAND", u8"Dračí ostrov", u8"Archol", "DI_ARCHOL" },
		{ "DRAGONISLAND", u8"Dračí ostrov", u8"Černý mág", "DI_SCHWARZMAGIER" },
		{ "DRAGONISLAND", u8"Dračí ostrov", u8"Nemrtvý drak", "DI_UNDEADDRAGON" },
	};

	// ------------------------------------------------------------------ waypoint names

	struct TokenName { const char* token; const char* czech; };

	// Words used in waypoint names of the original worlds (English, German, abbreviations).
	static const TokenName kWaypointWords[] = {
		{ "CITY", u8"město" }, { "HABOUR", u8"přístav" }, { "HARBOUR", u8"přístav" }, { "MERCHANT", u8"tržiště" },
		{ "UPTOWN", u8"horní čtvrť" }, { "UPTOWNPARADE", u8"nádvoří horní čtvrti" }, { "KASERN", u8"kasárna" },
		{ "BARRACK", u8"kasárna" }, { "TAVERN", u8"hospoda" }, { "TAVERNE", u8"hospoda" }, { "INN", u8"hostinec" },
		{ "PUFF", u8"Rudá lucerna" }, { "KANAL", u8"kanály" }, { "CITYWALL", u8"hradby" }, { "CITYHALL", u8"radnice" },
		{ "CITYFOREST", u8"les u města" }, { "MAINSTREET", u8"hlavní ulice" }, { "STREET", u8"ulice" },
		{ "ALLEE", u8"alej" }, { "ALLEY", u8"ulička" }, { "SHARKSTREET", u8"Žraločí ulice" }, { "PALCAMP", u8"tábor paladinů" },
		{ "MONASTERY", u8"klášter" }, { "MONASTER", u8"klášter" }, { "LIBRARY", u8"knihovna" }, { "CHURCH", u8"kostel" },
		{ "TEMPLE", u8"chrám" }, { "TEMPEL", u8"chrám" }, { "SHRINE", u8"svatyně" }, { "CHAPELL", u8"kaple" },
		{ "BIGFARM", u8"Onarova farma" }, { "FARM", u8"farma" }, { "FARM1", u8"Lobartova farma" }, { "FARM2", u8"Akilova farma" },
		{ "FARM3", u8"Bengarova farma" }, { "FARM4", u8"Sekobova farma" }, { "BIGMILL", u8"velký mlýn" }, { "MILL", u8"mlýn" },
		{ "FIELD", u8"pole" }, { "SHEEP", u8"ovce" }, { "STABLE", u8"stáj" },
		{ "TROLLAREA", u8"území trollů" }, { "PORTALTEMPEL", u8"chrám s portálem" }, { "PORTAL", u8"portál" },
		{ "RITUAL", u8"rituál" }, { "RITUALFOREST", u8"rituální les" }, { "RITUALPATH", u8"rituální stezka" },
		{ "STONEHENGE", u8"kamenný kruh" }, { "LITTLESTONEHENDGE", u8"malý kamenný kruh" }, { "XARDAS", u8"Xardas" },
		{ "PASS", u8"průsmyk" }, { "CASTLEMINE", u8"starý důl" }, { "CASTLE", u8"hrad" }, { "TOWER", u8"věž" },
		{ "LIGHTHOUSE", u8"maják" }, { "MAGECAVE", u8"jeskyně mágů" }, { "SAGITTA", u8"Sagitta" },
		{ "FOREST", u8"les" }, { "SMFOREST", u8"lesík" }, { "BIGWOOD", u8"velký les" }, { "WOOD", u8"les" }, { "DEADWOOD", u8"mrtvý les" },
		{ "PATH", u8"cesta" }, { "WAY", u8"cesta" }, { "TRACK", u8"stezka" }, { "ENTRANCE", u8"vchod" }, { "ENTRY", u8"vchod" },
		{ "EXIT", u8"východ" }, { "GATE", u8"brána" }, { "GATEGUARD", u8"strážce brány" }, { "MAIN", u8"hlavní" },
		{ "HUT", u8"chatrč" }, { "HOUSE", u8"dům" }, { "INHOUSE", u8"v domě" }, { "ROOM", u8"pokoj" }, { "HALL", u8"síň" },
		{ "KITCHEN", u8"kuchyně" }, { "CELLAR", u8"sklep" }, { "STAIRS", u8"schody" }, { "FLOOR", u8"patro" }, { "YARD", u8"dvůr" },
		{ "BED", u8"postel" }, { "SLEEP", u8"spaní" }, { "SIT", u8"sezení" }, { "STAND", u8"stání" }, { "BENCH", u8"lavice" },
		{ "CHAIR", u8"židle" }, { "THRONE", u8"trůn" }, { "SMALLTALK", u8"hovor" }, { "WAIT", u8"čekání" }, { "GUARD", u8"stráž" },
		{ "GUARDS", u8"stráže" }, { "WATCH", u8"hlídka" }, { "LOOKOUT", u8"vyhlídka" }, { "VIEW", u8"výhled" },
		{ "COOK", u8"vaření" }, { "ROAST", u8"opékání" }, { "PICK", u8"sběr" }, { "HERB", u8"byliny" }, { "SAW", u8"pila" },
		{ "SAWHUT", u8"chata u pily" }, { "ANVIL", u8"kovadlina" }, { "SMITH", u8"kovárna" }, { "SHARP", u8"broušení" },
		{ "COOL", u8"chlazení" }, { "FIRE", u8"oheň" }, { "CAMPFIRE", u8"táborák" }, { "CAULDRON", u8"kotel" },
		{ "ALCHEMY", u8"alchymie" }, { "RUNEMAKER", u8"výroba run" }, { "ARMORY", u8"zbrojnice" }, { "SHOP", u8"obchod" },
		{ "TRADE", u8"obchod" }, { "BAR", u8"bar" }, { "COUNTER", u8"pult" }, { "TRAIN", u8"výcvik" }, { "FIGHT", u8"boj" },
		{ "READ", u8"čtení" }, { "BOOK", u8"kniha" }, { "STUDY", u8"studium" }, { "WORK", u8"práce" }, { "DANCE", u8"tanec" },
		{ "REPAIR", u8"oprava" }, { "BEER", u8"pivo" }, { "RAKEPLACE", u8"hrabání" },
		{ "CAVE", u8"jeskyně" }, { "BIGCAVE", u8"velká jeskyně" }, { "SECRETCAVE", u8"tajná jeskyně" }, { "HOEHLE", u8"jeskyně" },
		{ "CRYPT", u8"krypta" }, { "GRUFT", u8"krypta" }, { "DUNGEON", u8"podzemí" }, { "PRISON", u8"vězení" }, { "CELL", u8"cela" },
		{ "FOLTER", u8"mučírna" }, { "MINE", u8"důl" }, { "NEWMINE", u8"nový důl" }, { "GOLDMINE", u8"zlatý důl" },
		{ "MINELAGER", u8"tábor u dolu" },
		{ "CAMP", u8"tábor" }, { "LAGER", u8"tábor" }, { "PIRATECAMP", u8"pirátský tábor" }, { "PIRATCAMP", u8"pirátský tábor" },
		{ "BANDITSCAMP", u8"tábor banditů" }, { "SWAMPCAMP", u8"tábor v bažině" }, { "ICECAMP", u8"ledový tábor" },
		{ "STARTCAMP", u8"výchozí tábor" }, { "ROCKCAMP", u8"skalní tábor" }, { "NEWCAMP", u8"Nový tábor" },
		{ "BANDIT", u8"bandité" }, { "BANDITS", u8"bandité" }, { "RANGERBANDITS", u8"bandité" }, { "ORK", u8"skřeti" },
		{ "ORKS", u8"skřeti" }, { "ORC", u8"skřeti" }, { "ORCS", u8"skřeti" }, { "ORKAREA", u8"území skřetů" },
		{ "ORCBARRIER", u8"skřetí palisáda" }, { "PALISADE", u8"palisáda" }, { "DJG", u8"drakobijci" },
		{ "VORPOSTEN", u8"předsunutá hlídka" }, { "PIRATES", u8"piráti" },
		{ "VALLEY", u8"údolí" }, { "CANYON", u8"kaňon" }, { "PLATEAU", u8"náhorní plošina" }, { "HILL", u8"kopec" },
		{ "HILLS", u8"kopce" }, { "MOUNTAINLAKE", u8"horské jezero" }, { "LAKE", u8"jezero" }, { "SEA", u8"moře" },
		{ "BEACH", u8"pláž" }, { "STRAND", u8"pláž" }, { "LONEBEACH", u8"opuštěná pláž" }, { "COAST", u8"pobřeží" },
		{ "RIVERSIDE", u8"břeh řeky" }, { "RIVER", u8"řeka" }, { "SMALLRIVER", u8"říčka" }, { "WATERFALL", u8"vodopád" },
		{ "WATERHOLE", u8"napajedlo" }, { "SWAMP", u8"bažina" }, { "PLAIN", u8"pláň" }, { "GRASS", u8"louka" },
		{ "PLACE", u8"místo" }, { "CENTER", u8"střed" }, { "CENTRE", u8"střed" }, { "MID", u8"střed" }, { "SIDE", u8"strana" },
		{ "BACK", u8"vzadu" }, { "FRONT", u8"vpředu" }, { "UP", u8"nahoře" }, { "DOWN", u8"dole" }, { "IN", u8"uvnitř" },
		{ "OUT", u8"venku" }, { "OUTSIDE", u8"venku" }, { "LEFT", u8"vlevo" }, { "RIGHT", u8"vpravo" }, { "LITTLE", u8"malý" },
		{ "NORTH", u8"sever" }, { "SOUTH", u8"jih" }, { "EAST", u8"východ" }, { "WEST", u8"západ" },
		{ "SOUTHWEST", u8"jihozápad" }, { "SOUTHEAST", u8"jihovýchod" }, { "BETWEEN", u8"mezi" }, { "TO", u8"→" },
		{ "BRIDGE", u8"most" }, { "BRIGDE", u8"most" }, { "TREE", u8"strom" }, { "STONES", u8"kameny" }, { "RUINS", u8"ruiny" },
		{ "WOODRUIN", u8"zřícenina" }, { "SECRET", u8"tajné" }, { "TRAP", u8"past" }, { "HOHLWEG", u8"úvoz" }, { "RING", u8"kruh" },
		{ "SHIP", u8"loď" }, { "DECK", u8"paluba" }, { "CREW", u8"posádka" }, { "CAPTAIN", u8"kapitán" }, { "WERFT", u8"loděnice" },
		{ "MONSTER", u8"příšery" }, { "SPAWN", u8"výskyt" }, { "SKELETON", u8"kostlivci" }, { "TROLL", u8"troll" },
		{ "WOLF", u8"vlci" }, { "SCAVENGER", u8"mrchožrouti" }, { "MOLERAT", u8"krysokrti" }, { "LURKER", u8"číhavci" },
		{ "GOBBO", u8"goblini" }, { "GOLEM", u8"golem" }, { "WARAN", u8"varani" }, { "SWAMPSHARK", u8"bažinní žraloci" },
		{ "BLOODFLY", u8"krvavé mouchy" }, { "MEATBUG", u8"žravé štěnice" }, { "ORCDOG", u8"skřetí psi" }, { "RATS", u8"krysy" },
		{ "SNAPPER", u8"chňapavci" }, { "HARPIE", u8"harpyje" }, { "SHADOWBEAST", u8"stínová šelma" },
		{ "ROCKDRAGON", u8"kamenný drak" }, { "ICEDRAGON", u8"ledový drak" }, { "FIREDRAGON", u8"ohnivý drak" },
		{ "SWAMPDRAGON", u8"bažinný drak" }, { "DRAGONSWAMP", u8"bažinný drak" }, { "UNDEADDRAGON", u8"nemrtvý drak" },
		{ "UNDEADDRAGONTEMPEL", u8"chrám nemrtvého draka" }, { "DRACONIANAREA", u8"území ještěrů" }, { "ICEREGION", u8"ledová oblast" },
		{ "ADANOSTEMPEL", u8"Adanův chrám" }, { "TELEPORT", u8"teleport" }, { "TELEPORTSTATION", u8"teleportační kámen" },
		{ "TREASUREPITS", u8"jámy s pokladem" }, { "GREGTREASURE", u8"Gregův poklad" }, { "SENAT", u8"rada" },
		{ "EBR", u8"síň" }, { "WARROOM", u8"válečná porada" }, { "LEICHE", u8"mrtvola" }, { "ANCIENTGHOST", u8"prastarý duch" },
		{ "SCHWARZMAGIER", u8"černý mág" }, { "ORKOBERST", u8"skřetí plukovník" }, { "OC", u8"hrad" }, { "BL", u8"tábor banditů" },
		{ "START", u8"začátek" }, { "SHOWCASE", u8"vitrína" }, { "JUDGE", u8"soudce" }, { "RICHTER", u8"soudce" },
		{ "WINEMAKER", u8"vinař" }, { "FISCHER", u8"rybář" }, { "NAVIGATION", u8"navigace" }, { "MAGE", u8"mágové" },
		{ "STORE", u8"sklad" }, { "AREA", u8"oblast" }, { "NOVICE", u8"novicové" }, { "CONNECT", u8"spojka" },
		{ "LOCH", u8"díra" }, { "OPEN", u8"otevřené" }, { "PEASANT", u8"rolníci" }, { "CORRIDOR", u8"chodba" },
		{ "MOVEMENT", u8"pohyb" }, { "PLANE", u8"plocha" }, { "GROUND", u8"zem" }, { "FLUCHT", u8"útěk" },
		{ "TROLLROCKCAVE", u8"jeskyně trollů" }, { "TROLLLAKECAVE", u8"jeskyně trollů u jezera" }, { "SHARK", u8"žraloci" },
		{ "RAVEN", u8"Raven" }, { "GREG", u8"Greg" }, { "SKLAVEN", u8"otroci" }, { "SLAVE", u8"otroci" },
	};

	struct TokenPhrase { const char* a; const char* b; const char* czech; };
	static const TokenPhrase kWaypointPhrases[] = {
		{ "POOR", "AREA", u8"chudinská čtvrť" }, { "MAIN", "ENTRY", u8"hlavní vchod" }, { "MAIN", "ENTRANCE", u8"hlavní vchod" },
		{ "MAIN", "STREET", u8"hlavní ulice" }, { "TO", "PASS", u8"k průsmyku" }, { "PORTAL", "TEMPEL", u8"chrám s portálem" },
	};

	static const char* WaypointWord(const std::string& token) {
		for (const TokenName& w : kWaypointWords)
			if (token == w.token)
				return w.czech;
		return nullptr;
	}

	static bool IsWorldPrefix(const std::string& token) {
		return token == "NW" || token == "OW" || token == "ADW" || token == "DI";
	}

	// Upper-cases the first letter, including the Czech ones.
	static std::string CapitalizeFirst(const std::string& s) {
		if (s.empty())
			return s;
		unsigned char c = (unsigned char)s[0];
		if (c < 0x80) {
			std::string r = s;
			r[0] = (char)toupper(c);
			return r;
		}
		static const char* pairs[][2] = {
			{ u8"á", u8"Á" }, { u8"č", u8"Č" }, { u8"ď", u8"Ď" }, { u8"é", u8"É" }, { u8"ě", u8"Ě" }, { u8"í", u8"Í" },
			{ u8"ň", u8"Ň" }, { u8"ó", u8"Ó" }, { u8"ř", u8"Ř" }, { u8"š", u8"Š" }, { u8"ť", u8"Ť" }, { u8"ú", u8"Ú" },
			{ u8"ů", u8"Ů" }, { u8"ý", u8"Ý" }, { u8"ž", u8"Ž" },
		};
		for (const auto& p : pairs) {
			size_t n = std::strlen(p[0]);
			if (s.compare(0, n, p[0]) == 0)
				return std::string(p[1]) + s.substr(n);
		}
		return s;
	}

	// NW_CITY_HABOUR_TAVERN01_SMALLTALK_01 -> "Město · přístav · hospoda 01 · hovor 01"
	static std::string ReadableWaypoint(const std::string& raw) {
		std::vector<std::string> tokens;
		std::string cur;
		for (char ch : raw) {
			if (ch == '_' || ch == ' ' || ch == '-') {
				if (!cur.empty())
					tokens.push_back(cur);
				cur.clear();
			}
			else {
				cur += (char)toupper((unsigned char)ch);
			}
		}
		if (!cur.empty())
			tokens.push_back(cur);
		if (tokens.size() > 1 && IsWorldPrefix(tokens[0]))
			tokens.erase(tokens.begin());

		std::vector<std::string> parts;   // one entry per word, numbers appended
		std::vector<bool> arrowBefore;    // "A_TO_B" -> "A → B"
		bool anyWord = false;
		bool arrow = false;
		auto push = [&](const std::string& text) {
			parts.push_back(text);
			arrowBefore.push_back(arrow && parts.size() > 1);
			arrow = false;
		};
		for (size_t i = 0; i < tokens.size(); i++) {
			const std::string& t = tokens[i];

			// two-word phrases
			if (i + 1 < tokens.size()) {
				const char* phrase = nullptr;
				for (const TokenPhrase& p : kWaypointPhrases)
					if (t == p.a && tokens[i + 1] == p.b)
						phrase = p.czech;
				if (phrase) {
					push(phrase);
					anyWord = true;
					i++;
					continue;
				}
			}

			if (t == "TO" && !parts.empty() && i + 1 < tokens.size()) {
				arrow = true;
				continue;
			}

			// numbers ("01", "02A") and single letters stick to the previous word
			bool startsWithDigit = isdigit((unsigned char)t[0]) != 0;
			if (startsWithDigit || t.size() == 1) {
				if (!parts.empty() && !arrow)
					parts.back() += " " + t;
				else
					push(t);
				continue;
			}

			// "TAVERN01" -> "TAVERN" + "01"
			std::string base = t, number;
			size_t cut = base.size();
			while (cut > 0 && isdigit((unsigned char)base[cut - 1]))
				cut--;
			if (cut > 0 && cut < base.size()) {
				number = base.substr(cut);
				base = base.substr(0, cut);
			}

			const char* word = WaypointWord(t);   // whole token first (FARM1)
			if (word)
				number.clear();
			else
				word = WaypointWord(base);
			std::string text;
			if (word) {
				text = word;
				anyWord = true;
			}
			else {
				// unknown word, mostly a name (LARES -> Lares)
				text = base;
				for (size_t k = 1; k < text.size(); k++)
					text[k] = (char)tolower((unsigned char)text[k]);
			}
			if (!number.empty())
				text += " " + number;
			push(text);
		}

		if (parts.empty() || !anyWord)
			return std::string();   // nothing known – the raw name says it best
		std::string out;
		for (size_t i = 0; i < parts.size(); i++) {
			if (i)
				out += arrowBefore[i] ? u8" → " : u8" · ";
			out += parts[i];
		}
		return CapitalizeFirst(out);
	}

} // namespace Trainer
} // namespace GOTHIC_NAMESPACE
