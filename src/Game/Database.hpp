// Engine side: databases built from the game's own data, so the trainer
// automatically works with every mod (all item instances come from the scripts).

namespace GOTHIC_NAMESPACE {
namespace Trainer {

	enum ItemCategory {
		CAT_ALL = 0,
		CAT_MELEE,
		CAT_RANGED,
		CAT_AMMO,
		CAT_ARMOR,
		CAT_FOOD,
		CAT_DOCS,
		CAT_POTION,
		CAT_RUNE,
		CAT_SCROLL,
		CAT_MAGIC,
		CAT_LIGHT,
		CAT_OTHER,
		CAT_COUNT
	};

	static const char* CategoryName(int cat) {
		switch (cat) {
			case CAT_ALL:    return u8"Vše";
			case CAT_MELEE:  return u8"Zbraně na blízko";
			case CAT_RANGED: return u8"Luky a kuše";
			case CAT_AMMO:   return u8"Munice";
			case CAT_ARMOR:  return u8"Zbroje a oblečení";
			case CAT_FOOD:   return u8"Jídlo a rostliny";
			case CAT_DOCS:   return u8"Dokumenty a mapy";
			case CAT_POTION: return u8"Lektvary";
			case CAT_RUNE:   return u8"Runy";
			case CAT_SCROLL: return u8"Svitky";
			case CAT_MAGIC:  return u8"Prsteny a amulety";
			case CAT_LIGHT:  return u8"Pochodně";
			case CAT_OTHER:  return u8"Ostatní";
		}
		return "?";
	}

	struct ItemEntry {
		int index = -1;               // parser symbol index (instance)
		std::string code;             // instance name, e.g. ITMW_1H_PAL_SWORD
		std::string name;             // UTF-8 display name
		std::string searchKey;        // folded name + code
		int category = CAT_OTHER;
		int mainflag = 0;
		int flags = 0;
		int value = 0;
		int damage = 0;
		int range = 0;
		int protection[oEDamageIndex_MAX] = {};
		int condAtr[ITM_COND_MAX] = {};
		int condValue[ITM_COND_MAX] = {};
		bool mission = false;
		std::string description;      // UTF-8
		std::vector<std::pair<std::string, int>> lines; // text[i] / count[i]
	};

	struct ItemDatabase {
		std::vector<ItemEntry> items;
		bool ready = false;
		bool building = false;
		int goldIndex = -1;
		int symbolCount = 0;
	};

	static ItemDatabase g_items;

	static int ClassifyItem(int mainflag, int flags) {
		if (mainflag & ITM_CAT_NF)     return CAT_MELEE;
		if (mainflag & ITM_CAT_FF)     return CAT_RANGED;
		if (mainflag & ITM_CAT_MUN)    return CAT_AMMO;
		if (mainflag & ITM_CAT_ARMOR)  return CAT_ARMOR;
		if (mainflag & ITM_CAT_FOOD)   return CAT_FOOD;
		if (mainflag & ITM_CAT_DOCS)   return CAT_DOCS;
		if (mainflag & ITM_CAT_POTION) return CAT_POTION;
		if (mainflag & ITM_CAT_RUNE)   return (flags & ITM_FLAG_MULTI) ? CAT_SCROLL : CAT_RUNE;
		if (mainflag & ITM_CAT_MAGIC)  return CAT_MAGIC;
		if (mainflag & ITM_CAT_LIGHT)  return CAT_LIGHT;
		return CAT_OTHER;
	}

	// Runs on the game thread (action queue).
	static void BuildItemDatabase() {
		g_items.items.clear();
		g_items.ready = false;
		g_items.goldIndex = -1;

		if (!parser) {
			g_items.building = false;
			return;
		}

		const int classIndex = parser->GetIndex(zSTRING("C_ITEM"));
		const int symbols = parser->symtab.table.GetNumInList();
		g_items.symbolCount = symbols;
		if (classIndex < 0) {
			::GT::Log::Write("C_ITEM class not found");
			g_items.building = false;
			g_items.ready = true;
			return;
		}

		int failed = 0;

		for (int i = 0; i < symbols; i++) {
			zCPar_Symbol* sym = parser->GetSymbol(i);
			if (!sym || sym->type != zPAR_TYPE_INSTANCE)
				continue;
			// global instance variables (item, self, other ...) have no code
			if (sym->single_intdata <= 0)
				continue;
			if (parser->GetBaseClass(i) != classIndex)
				continue;

			// a fresh object per instance, so no field leaks from the previous one;
			// only the script runs, no 3D visual is loaded
			oCItem* scratch = new oCItem();
			if (!parser->CreateInstance(i, scratch)) {
				scratch->Release();
				failed++;
				continue;
			}

			ItemEntry e;
			e.index = i;
			e.code = sym->name.ToChar();
			e.name = FromZ(scratch->name);
			if (e.name.empty())
				e.name = e.code;
			e.mainflag = scratch->mainflag;
			e.flags = scratch->flags;
			e.value = scratch->value;
			e.damage = scratch->damageTotal;
			e.range = scratch->range;
			for (int d = 0; d < oEDamageIndex_MAX; d++)
				e.protection[d] = scratch->protection[d];
			for (int c = 0; c < ITM_COND_MAX; c++) {
				e.condAtr[c] = scratch->cond_atr[c];
				e.condValue[c] = scratch->cond_value[c];
			}
			e.mission = (scratch->flags & ITM_FLAG_MI) != 0;
			e.category = ClassifyItem(scratch->mainflag, scratch->flags);
			e.description = FromZ(scratch->description);
			for (int t = 0; t < ITM_TEXT_MAX; t++) {
				std::string line = FromZ(scratch->text[t]);
				if (!line.empty())
					e.lines.push_back(std::make_pair(line, scratch->count[t]));
			}
			e.searchKey = ::GT::Text::Fold(e.name) + " " + ::GT::Text::Fold(e.code);

			if (e.code == "ITMI_GOLD")
				g_items.goldIndex = i;

			scratch->Release();
			g_items.items.push_back(std::move(e));
		}

		std::sort(g_items.items.begin(), g_items.items.end(), [](const ItemEntry& a, const ItemEntry& b) {
			return a.searchKey < b.searchKey;
		});

		g_items.ready = true;
		g_items.building = false;
		::GT::Log::Write("Item database: %d items (%d failed)", (int)g_items.items.size(), failed);
	}

	// ------------------------------------------------------------------ waypoints

	struct WaypointEntry {
		std::string raw;     // game encoding, used for BeamTo
		std::string name;    // UTF-8
		std::string folded;
	};

	struct WaypointDatabase {
		std::vector<WaypointEntry> list;
		oCWorld* world = nullptr;
		bool ready = false;
	};

	static WaypointDatabase g_waypoints;

	static void BuildWaypointDatabase() {
		g_waypoints.list.clear();
		g_waypoints.ready = false;
		oCWorld* world = GameWorld();
		g_waypoints.world = world;
		if (!world || !world->wayNet) {
			g_waypoints.ready = true;
			return;
		}
		for (auto* node = world->wayNet->wplist.next; node; node = node->next) {
			zCWaypoint* wp = node->data;
			if (!wp)
				continue;
			WaypointEntry e;
			e.raw = wp->name.ToChar();
			if (e.raw.empty())
				continue;
			e.name = ::GT::Text::FromGame(e.raw.c_str());
			e.folded = ::GT::Text::Fold(e.name);
			g_waypoints.list.push_back(std::move(e));
		}
		std::sort(g_waypoints.list.begin(), g_waypoints.list.end(), [](const WaypointEntry& a, const WaypointEntry& b) {
			return a.raw < b.raw;
		});
		g_waypoints.ready = true;
		::GT::Log::Write("Waypoints: %d", (int)g_waypoints.list.size());
	}

	// ------------------------------------------------------------------ saved positions

	struct PositionStore {
		std::string world;
		std::vector<::GT::SavedPosition> list;
		bool loaded = false;
	};

	static PositionStore g_positions;

	static std::string CurrentWorldKey() {
		oCWorld* world = GameWorld();
		if (!world)
			return std::string();
		std::string key = world->GetWorldFilename().ToChar();
		size_t slash = key.find_last_of("\\/");
		if (slash != std::string::npos)
			key = key.substr(slash + 1);
		for (char& c : key)
			c = (char)toupper((unsigned char)c);
		return key;
	}

	static void EnsurePositionsLoaded() {
		std::string key = CurrentWorldKey();
		if (key.empty())
			return;
		if (g_positions.loaded && g_positions.world == key)
			return;
		g_positions.world = key;
		g_positions.list = ::GT::Settings::LoadPositions(key);
		g_positions.loaded = true;
	}

	static void SavePositionsNow() {
		if (g_positions.loaded && !g_positions.world.empty())
			::GT::Settings::SavePositions(g_positions.world, g_positions.list);
	}

} // namespace Trainer
} // namespace GOTHIC_NAMESPACE
