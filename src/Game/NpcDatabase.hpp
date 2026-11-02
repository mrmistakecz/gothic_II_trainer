// Engine side: NPC and creature instances from the scripts (for spawning) and
// helpers for the NPCs currently living in the world.

namespace GOTHIC_NAMESPACE {
namespace Trainer {

	struct NpcEntry {
		int index = -1;
		std::string code;     // instance name, e.g. VLK_439_VATRAS
		std::string label;    // readable label
		std::string folded;
		bool person = false;  // human NPC (has a number in its name)
	};

	struct NpcDatabase {
		std::vector<NpcEntry> list;
		bool ready = false;
		bool building = false;
	};

	static NpcDatabase g_npcDb;

	struct CreatureName { const char* prefix; const char* czech; };
	// A few well-known creatures get Czech names; the rest shows the script name.
	static const CreatureName kCreatureNames[] = {
		{ "WOLF", u8"Vlk" }, { "YWOLF", u8"Mladý vlk" }, { "WARG", u8"Warg" }, { "BLACKWOLF", u8"Černý vlk" },
		{ "SCAVENGER", u8"Mrchožrout" }, { "MOLERAT", u8"Krysokrt" }, { "GIANT_RAT", u8"Obří krysa" },
		{ "YGIANT_RAT", u8"Mladá obří krysa" }, { "BLOODFLY", u8"Krvavá moucha" }, { "YBLOODFLY", u8"Mladá krvavá moucha" },
		{ "GIANT_BUG", u8"Polní škůdce" }, { "YGIANT_BUG", u8"Mladý polní škůdce" }, { "MEATBUG", u8"Žravá štěnice" },
		{ "LURKER", u8"Číhavec" }, { "SNAPPER", u8"Chňapavec" }, { "DRAGONSNAPPER", u8"Dračí chňapavec" },
		{ "SHADOWBEAST", u8"Stínová šelma" }, { "TROLL", u8"Troll" }, { "TROLL_BLACK", u8"Černý troll" },
		{ "WARAN", u8"Varan" }, { "FIREWARAN", u8"Ohnivý varan" }, { "HARPIE", u8"Harpyje" },
		{ "MINECRAWLER", u8"Důlní červ" }, { "MINECRAWLERWARRIOR", u8"Důlní červ válečník" },
		{ "GOBBO_GREEN", u8"Goblin" }, { "GOBBO_BLACK", u8"Černý goblin" }, { "GOBBO_SKELETON", u8"Kostlivec goblina" },
		{ "SKELETON", u8"Kostlivec" }, { "LESSER_SKELETON", u8"Menší kostlivec" }, { "SKELETON_LORD", u8"Stínový válečník" },
		{ "SKELETONMAGE", u8"Kostlivý mág" }, { "ZOMBIE01", u8"Zombie" }, { "DEMON", u8"Démon" }, { "DEMONLORD", u8"Pán démonů" },
		{ "STONEGOLEM", u8"Kamenný golem" }, { "FIREGOLEM", u8"Ohnivý golem" }, { "ICEGOLEM", u8"Ledový golem" },
		{ "SWAMPSHARK", u8"Bažinný žralok" }, { "SHEEP", u8"Ovce" }, { "HAMMEL", u8"Beran" }, { "WISP", u8"Bludička" },
		{ "DRACONIAN", u8"Ještěrčí muž" }, { "ORCWARRIOR_ROAM", u8"Skřetí válečník" }, { "ORCELITE_ROAM", u8"Skřetí elita" },
		{ "ORCSHAMAN_SIT", u8"Skřetí šaman" }, { "UNDEADORCWARRIOR", u8"Nemrtvý skřet" },
		{ "DRAGON_FIRE", u8"Ohnivý drak" }, { "DRAGON_ICE", u8"Ledový drak" }, { "DRAGON_ROCK", u8"Kamenný drak" },
		{ "DRAGON_SWAMP", u8"Bažinný drak" }, { "DRAGON_UNDEAD", u8"Nemrtvý drak" },
		{ "SWAMPGOLEM", u8"Bažinný golem" }, { "SWAMPRAT", u8"Bažinná krysa" },
		{ "ALLIGATOR", u8"Aligátor" }, { "ZOMBIE_ADDON", u8"Zombie" }, { "STONEGUARDIAN", u8"Kamenný strážce" },
		{ "GOBBO_WARRIOR", u8"Goblin válečník" }, { "BEACHWARAN", u8"Plážový varan" },
	};

	static bool LooksLikePerson(const std::string& code) {
		// PREFIX_123_NAME
		size_t a = code.find('_');
		if (a == std::string::npos || a + 1 >= code.size())
			return false;
		size_t i = a + 1;
		size_t digits = 0;
		while (i < code.size() && isdigit((unsigned char)code[i])) {
			i++;
			digits++;
		}
		return digits > 0 && (i == code.size() || code[i] == '_');
	}

	static std::string NpcLabel(const std::string& code, bool person) {
		if (!person) {
			for (const CreatureName& c : kCreatureNames)
				if (code == c.prefix)
					return std::string(c.czech);
			return code;
		}
		// VLK_439_VATRAS -> Vatras
		size_t last = code.find_last_of('_');
		std::string tail = last == std::string::npos ? code : code.substr(last + 1);
		if (tail.size() >= 2 && !isdigit((unsigned char)tail[0])) {
			std::string nice;
			nice += tail[0];
			for (size_t k = 1; k < tail.size(); k++)
				nice += (char)tolower((unsigned char)tail[k]);
			return nice;
		}
		return code;
	}

	static void BuildNpcDatabase() {
		g_npcDb.list.clear();
		g_npcDb.ready = false;
		if (!parser) {
			g_npcDb.building = false;
			return;
		}
		const int classIndex = parser->GetIndex(zSTRING("C_NPC"));
		const int symbols = parser->symtab.table.GetNumInList();
		for (int i = 0; classIndex >= 0 && i < symbols; i++) {
			zCPar_Symbol* sym = parser->GetSymbol(i);
			if (!sym || sym->type != zPAR_TYPE_INSTANCE || sym->single_intdata <= 0)
				continue;
			if (parser->GetBaseClass(i) != classIndex)
				continue;
			NpcEntry e;
			e.index = i;
			e.code = sym->name.ToChar();
			e.person = LooksLikePerson(e.code);
			e.label = NpcLabel(e.code, e.person);
			e.folded = ::GT::Text::Fold(e.label + " " + e.code);
			g_npcDb.list.push_back(std::move(e));
		}
		std::sort(g_npcDb.list.begin(), g_npcDb.list.end(), [](const NpcEntry& a, const NpcEntry& b) {
			if (a.person != b.person)
				return !a.person; // creatures first
			return a.label < b.label;
		});
		g_npcDb.ready = true;
		g_npcDb.building = false;
		::GT::Log::Write("NPC database: %d instances", (int)g_npcDb.list.size());
	}

	// ------------------------------------------------------------------ world NPCs

	static float DistanceBetween(const zVEC3& a, const zVEC3& b) {
		float dx = a.n[0] - b.n[0], dy = a.n[1] - b.n[1], dz = a.n[2] - b.n[2];
		return sqrtf(dx * dx + dy * dy + dz * dz);
	}

	// Calls fn(npc) for every NPC in the current world except the hero.
	template<typename Fn>
	static void ForEachWorldNpc(Fn fn) {
		oCWorld* world = GameWorld();
		if (!world || !world->voblist_npcs)
			return;
		for (auto* node = world->voblist_npcs->next; node; node = node->next) {
			oCNpc* npc = node->data;
			if (npc && npc != player)
				fn(npc);
		}
	}

	// Is the NPC still in the world (pointers kept by the UI may get stale)?
	static bool NpcStillExists(oCNpc* target) {
		bool found = false;
		ForEachWorldNpc([&](oCNpc* npc) {
			if (npc == target)
				found = true;
		});
		return found;
	}

	static zVEC3 PointInFrontOfHero(float distance) {
		zVEC3 pos = player->GetPositionWorld();
		zVEC3 at = player->GetAtVectorWorld();
		pos.n[0] += at.n[0] * distance;
		pos.n[2] += at.n[2] * distance;
		return pos;
	}

} // namespace Trainer
} // namespace GOTHIC_NAMESPACE
