// Engine side: teleports with history, world transitions, weather, locks,
// loot and other actions on the world around the hero. Everything that changes
// the world runs on the game thread (queued actions or the game frame).

namespace GOTHIC_NAMESPACE {
namespace Trainer {

	// ------------------------------------------------------------------ teleport history

	struct HeroSpot {
		float x, y, z;
	};

	static std::vector<HeroSpot> g_teleportBack;   // cleared on every world change

	static zVEC3 MakeVec(float x, float y, float z) {
		zVEC3 v;
		v.n[0] = x;
		v.n[1] = y;
		v.n[2] = z;
		return v;
	}

	// Remembers where the hero stands, so "Zpět" can return there.
	static void RememberHeroSpot() {
		if (!InGame())
			return;
		zVEC3 p = player->GetPositionWorld();
		HeroSpot spot = { p.n[0], p.n[1], p.n[2] };
		if (!g_teleportBack.empty()) {
			const HeroSpot& last = g_teleportBack.back();
			float dx = last.x - spot.x, dy = last.y - spot.y, dz = last.z - spot.z;
			if (dx * dx + dy * dy + dz * dz < 100.0f * 100.0f)
				return;   // practically the same place
		}
		g_teleportBack.push_back(spot);
		if (g_teleportBack.size() > 30)
			g_teleportBack.erase(g_teleportBack.begin());
	}

	// Game thread only.
	static void MoveHero(zVEC3 pos) {
		player->ResetPos(pos);
	}

	static void TeleportHeroToPosition(float x, float y, float z, const std::string& name) {
		Enqueue([x, y, z, name]() {
			if (!InGame())
				return;
			RememberHeroSpot();
			MoveHero(MakeVec(x, y, z));
			if (!name.empty())
				::GT::Overlay::Toast(u8"Teleport: " + name);
		});
	}

	static void TeleportHeroToWaypoint(const std::string& raw, const std::string& name) {
		Enqueue([raw, name]() {
			if (!InGame())
				return;
			RememberHeroSpot();
			if (player->BeamTo(zSTRING(raw.c_str())))
				::GT::Overlay::Toast(u8"Teleport: " + name);
			else
				::GT::Overlay::Toast(u8"Místo v tomto světě není: " + name);
		});
	}

	static void TeleportHeroToNpc(oCNpc* npc, const std::string& name) {
		Enqueue([npc, name]() {
			if (!InGame() || !NpcStillExists(npc))
				return;
			const NpcPlace place = WhereIsNpc(npc);
			if (!place.Usable()) {
				::GT::Overlay::Toast(NpcNotReachableText(place, name));
				return;
			}
			RememberHeroSpot();
			MoveHero(MeetingSpot(npc, place));
			::GT::Overlay::Toast(place.state == kNpcAsleep ? u8"Teleport k: " + name + u8" (hra ho za chvíli probudí)" : u8"Teleport k: " + name);
		});
	}

	// Brings the character in front of the hero, waking it up when it sleeps.
	static void SummonNpcToHero(oCNpc* npc) {
		Enqueue([npc]() {
			if (!InGame() || !NpcStillExists(npc))
				return;
			zVEC3 pos = PointInFrontOfHero(250.0f);
			if (NpcActive(npc))
				npc->ResetPos(pos);
			else
				npc->oCNpc::Enable(pos);
		});
	}

	static void TeleportHeroBack() {
		Enqueue([]() {
			if (!InGame() || g_teleportBack.empty())
				return;
			HeroSpot spot = g_teleportBack.back();
			g_teleportBack.pop_back();
			MoveHero(MakeVec(spot.x, spot.y, spot.z));
			::GT::Overlay::Toast(u8"Zpět na předchozí místo");
		});
	}

	// Moves the hero by the given distance forward and up (centimetres).
	static void NudgeHero(float forward, float up) {
		Enqueue([forward, up]() {
			if (!InGame())
				return;
			RememberHeroSpot();
			zVEC3 pos = player->GetPositionWorld();
			zVEC3 at = player->GetAtVectorWorld();
			pos.n[0] += at.n[0] * forward;
			pos.n[2] += at.n[2] * forward;
			pos.n[1] += up;
			MoveHero(pos);
		});
	}

	// ------------------------------------------------------------------ world transitions

	struct LevelTransition {
		std::string level;     // as stored in the trigger, e.g. "OLDWORLD\\OLDWORLD.ZEN"
		std::string start;     // start point in the target world
		std::string target;    // world key, e.g. "OLDWORLD"
		std::string display;   // Czech name of the target world
		std::string vobName;
		float x = 0.0f, y = 0.0f, z = 0.0f;
	};

	struct TransitionList {
		std::vector<LevelTransition> list;
		oCWorld* world = nullptr;
		bool ready = false;
		bool building = false;
	};

	static TransitionList g_transitions;

	// Game thread: every level-change trigger placed in the current world.
	static void BuildTransitions() {
		struct FinishBuild { ~FinishBuild() { g_transitions.building = false; } } finish;
		g_transitions.list.clear();
		g_transitions.ready = false;
		oCWorld* world = GameWorld();
		g_transitions.world = world;
		if (!world) {
			g_transitions.ready = true;
			return;
		}
		zCArray<zCVob*> found;
		world->SearchVobListByClass(oCTriggerChangeLevel::classDef, found, &world->globalVobTree);
		for (int i = 0; i < found.GetNumInList(); i++) {
			oCTriggerChangeLevel* trigger = zDYNAMIC_CAST<oCTriggerChangeLevel>(found[i]);
			if (!trigger)
				continue;
			LevelTransition t;
			t.level = trigger->levelName.ToChar();
			t.start = trigger->startVob.ToChar();
			if (t.level.empty())
				continue;
			t.target = WorldKeyFromPath(t.level);
			t.display = WorldDisplayName(t.target);
			t.vobName = FromZ(trigger->GetObjectName());
			zVEC3 pos = trigger->GetPositionWorld();
			t.x = pos.n[0];
			t.y = pos.n[1];
			t.z = pos.n[2];
			g_transitions.list.push_back(std::move(t));
		}
		std::sort(g_transitions.list.begin(), g_transitions.list.end(), [](const LevelTransition& a, const LevelTransition& b) {
			return a.display != b.display ? a.display < b.display : a.start < b.start;
		});
		g_transitions.ready = true;
		::GT::Log::Write("World transitions: %d", (int)g_transitions.list.size());
	}

	// Same call the game makes when the hero walks into the transition.
	static void ChangeLevelNow(const LevelTransition& t) {
		std::string level = t.level;
		std::string start = t.start;
		std::string display = t.display;
		Enqueue([level, start, display]() {
			if (!InGame())
				return;
			::GT::Log::Write("Changing level to %s (%s)", level.c_str(), start.c_str());
			::GT::Overlay::Toast(u8"Cesta do světa: " + display);
			ogame->TriggerChangeLevel(zSTRING(level.c_str()), zSTRING(start.c_str()));
		});
	}

	// ------------------------------------------------------------------ weather

	static zCSkyControler_Outdoor* ActiveOutdoorSky() {
		zCSkyControler* sky = zCSkyControler::s_activeSkyControler;
		if (!sky)
			return nullptr;
		return zDYNAMIC_CAST<zCSkyControler_Outdoor>(sky);
	}

	static void RestoreWeather() {
		if (!g_state.weatherSaved)
			return;
		zCSkyControler_Outdoor* sky = ActiveOutdoorSky();
		if (sky && sky == g_state.weatherSky) {
			sky->rainFX.timeStartRain = g_state.savedRainStart;
			sky->rainFX.timeStopRain = g_state.savedRainStop;
			sky->m_bDontRain = g_state.savedDontRain;
		}
		g_state.weatherSaved = false;
		g_state.weatherSky = nullptr;
	}

	// Game frame. The rain window is kept around the current time of day
	// (masterTime, 0..1 per day), so the game's own rain effect does the rest.
	static void ApplyWeather() {
		if (g_state.weatherMode == 0) {
			RestoreWeather();
			return;
		}
		zCSkyControler_Outdoor* sky = ActiveOutdoorSky();
		if (!sky)
			return;
		if (!g_state.weatherSaved || g_state.weatherSky != sky) {
			g_state.weatherSky = sky;
			g_state.savedRainStart = sky->rainFX.timeStartRain;
			g_state.savedRainStop = sky->rainFX.timeStopRain;
			g_state.savedDontRain = sky->m_bDontRain;
			g_state.weatherSaved = true;
		}
		const float now = sky->masterTime;
		if (g_state.weatherMode == 1) {
			sky->m_bDontRain = FALSE;
			sky->rainFX.timeStartRain = now - 0.05f;
			sky->rainFX.timeStopRain = now + 0.30f;
		}
		else {
			sky->m_bDontRain = TRUE;
			sky->rainFX.timeStartRain = now + 0.60f;
			sky->rainFX.timeStopRain = now + 0.70f;
		}
	}

	static bool WeatherAvailable() {
		return ActiveOutdoorSky() != nullptr;
	}

	// ------------------------------------------------------------------ chests and doors

	static void UnlockNearby(float radius) {
		Enqueue([radius]() {
			if (!InGame())
				return;
			oCWorld* world = GameWorld();
			zCArray<zCVob*> found;
			world->SearchVobListByBaseClass(oCMobLockable::classDef, found, &world->globalVobTree);
			const zVEC3 here = player->GetPositionWorld();
			int chests = 0, doors = 0;
			for (int i = 0; i < found.GetNumInList(); i++) {
				oCMobLockable* mob = zDYNAMIC_CAST<oCMobLockable>(found[i]);
				if (!mob || !mob->locked)
					continue;
				if (DistanceBetween(here, mob->GetPositionWorld()) > radius)
					continue;
				mob->SetLocked(FALSE);
				if (zDYNAMIC_CAST<oCMobContainer>(mob))
					chests++;
				else
					doors++;
			}
			::GT::Log::Write("Unlocked %d chests and %d doors", chests, doors);
			if (chests + doors == 0)
				::GT::Overlay::Toast(u8"V okolí není nic zamčeného");
			else
				::GT::Overlay::Toast(::GT::Text::Format(u8"Odemčeno: %d truhel a %d dveří", chests, doors));
		});
	}

	// ------------------------------------------------------------------ items on the ground

	static bool ItemStillInWorld(oCWorld* world, oCItem* target) {
		if (!world || !world->voblist_items)
			return false;
		for (auto* node = world->voblist_items->next; node; node = node->next)
			if (node->data == target)
				return true;
		return false;
	}

	// Picks up everything lying around: same items into the inventory,
	// the originals are removed from the world (like Wld_RemoveItem).
	static void CollectItemsNearby(float radius) {
		Enqueue([radius]() {
			if (!InGame())
				return;
			oCWorld* world = GameWorld();
			if (!world->voblist_items)
				return;
			struct Target { oCItem* item; int instance; int amount; };
			std::vector<Target> targets;
			const zVEC3 here = player->GetPositionWorld();
			for (auto* node = world->voblist_items->next; node; node = node->next) {
				oCItem* item = node->data;
				if (!item || item->HasFlag(ITM_FLAG_NFOCUS))
					continue;
				zCTree<zCVob>* tree = item->globalVobTreeNode;
				if (!tree)
					continue;    // not placed in the world
				if (tree->parent && tree->parent->data && zDYNAMIC_CAST<oCNpc>(tree->parent->data))
					continue;    // held by someone
				if (DistanceBetween(here, item->GetPositionWorld()) > radius)
					continue;
				int instance = item->GetInstance();
				if (instance < 0)
					continue;
				Target t = { item, instance, item->amount > 0 ? item->amount : 1 };
				targets.push_back(t);
			}
			int stacks = 0, pieces = 0;
			for (const Target& t : targets) {
				if (!ItemStillInWorld(world, t.item))
					continue;
				player->CreateItems(t.instance, t.amount);
				world->RemoveVob(t.item);
				stacks++;
				pieces += t.amount;
			}
			::GT::Log::Write("Collected %d item stacks (%d pieces)", stacks, pieces);
			if (stacks == 0)
				::GT::Overlay::Toast(u8"V okolí neleží žádné předměty");
			else
				::GT::Overlay::Toast(::GT::Text::Format(u8"Sebráno %d předmětů (%d ks)", stacks, pieces));
		});
	}

	// ------------------------------------------------------------------ people around

	static void CalmNearby(float radius) {
		Enqueue([radius]() {
			if (!InGame())
				return;
			const zVEC3 here = player->GetPositionWorld();
			int calmed = 0;
			ForEachWorldNpc([&](oCNpc* npc) {
				if (!NpcActive(npc) || npc->IsDead() || DistanceBetween(here, npc->GetPositionWorld()) > radius)
					return;
				npc->SetAttitude(2);    // ATT_NEUTRAL
				npc->SetTmpAttitude(2);
				calmed++;
			});
			::GT::Overlay::Toast(::GT::Text::Format(u8"Uklidněno postav: %d", calmed));
		});
	}

	// Pays every bounty at once: the game's own B_GrantAbsolution(LOC_ALL).
	static void ForgiveAllCrimes() {
		Enqueue([]() {
			if (!InGame())
				return;
			int location = 6;   // LOC_ALL in the original scripts
			int index = ScriptSymbolIndex("LOC_ALL");
			zCPar_Symbol* sym = index >= 0 ? parser->GetSymbol(index) : nullptr;
			if (sym && sym->type == zPAR_TYPE_INT)
				sym->GetValue(location, 0);
			if (CallScriptInt("B_GRANTABSOLUTION", location))
				::GT::Overlay::Toast(u8"Všechny zločiny jsou odpuštěny");
			else
				::GT::Overlay::Toast(u8"Tahle verze skriptů odpuštění zločinů nemá");
		});
	}

	// ------------------------------------------------------------------ hero helpers

	// Game frame: keeps the ammunition of the equipped bow or crossbow topped up.
	static void RefillAmmo(oCNpc* hero) {
		oCItem* weapon = hero->GetEquippedRangedWeapon();
		if (!weapon) {
			oCItem* inHand = hero->GetWeapon();
			if (inHand && (inHand->mainflag & ITM_CAT_FF))
				weapon = inHand;
		}
		if (!weapon || weapon->munition <= 0 || !parser)
			return;
		const int ammo = weapon->munition;
		zCPar_Symbol* sym = parser->GetSymbol(ammo);
		if (!sym || sym->type != zPAR_TYPE_INSTANCE)
			return;
		int have = hero->inventory2.GetAmount(ammo);
		if (have < 50)
			hero->CreateItems(ammo, 99 - have);
	}

	// ------------------------------------------------------------------ statues and shrines

	// Innos shrines (praying gives a blessing, PrayShrine_S1), Innos statues the
	// people pray at (scheme INNOS) and Beliar's idols in Jharkendar (PrayIdol_S1).
	enum ShrineKind { kShrineInnos = 0, kStatueInnos, kIdolBeliar };

	struct ShrineEntry {
		int kind = kShrineInnos;
		zVEC3 pos;
		zVEC3 at;
		std::string where;      // Czech description of the nearest waypoint
		std::string waypoint;   // its raw name
		std::string hauntVar;   // the script variable telling whether this shrine is haunted
	};

	struct ShrineList {
		oCWorld* world = nullptr;
		std::vector<ShrineEntry> list;
	};

	static ShrineList g_shrines;

	static std::string UpperZ(const zSTRING& z) {
		std::string s = z.ToChar();
		for (char& c : s)
			c = (char)toupper((unsigned char)c);
		return s;
	}

	// The seven haunted shrines of chapter 3 (PrayShrine.d: C_IsShrineObsessed
	// checks the hero within 20 m of these waypoints).
	static std::string ShrineHauntVariable(oCWorld* world, const zVEC3& pos) {
		static const char* const kHaunted[] = { "NW_TROLLAREA_PATH_37", "NW_FARM1_CONNECT_XARDAS", "NW_TROLLAREA_PATH_66",
			"NW_TROLLAREA_PATH_04", "SAGITTA", "NW_BIGMILL_MALAKSVERSTECK_02", "NW_FARM3_BIGWOOD_02" };
		if (!world || !world->wayNet)
			return std::string();
		for (const char* wpName : kHaunted) {
			zCWaypoint* wp = world->wayNet->GetWaypoint(zSTRING(wpName));
			if (wp && DistanceBetween(wp->pos, pos) < 2000.0f)
				return std::string("SHRINEISOBSESSED_") + wpName;
		}
		return std::string();
	}

	// Is the shrine haunted right now (to be cleansed with holy water)?
	static bool ShrineHaunted(const ShrineEntry& e) {
		if (e.hauntVar.empty() || !parser)
			return false;
		zCPar_Symbol* sym = parser->GetSymbol(zSTRING(e.hauntVar.c_str()));
		int value = 0;
		if (sym)
			sym->GetValue(value, 0);
		return value != 0;
	}

	// Game thread or UI; scans the world once and keeps the list until it changes.
	static const std::vector<ShrineEntry>& Shrines() {
		oCWorld* world = GameWorld();
		if (world == g_shrines.world)
			return g_shrines.list;
		g_shrines.world = world;
		g_shrines.list.clear();
		if (!world || !world->voblist)
			return g_shrines.list;
		for (auto* node = world->voblist->next; node; node = node->next) {
			oCMobInter* mob = zDYNAMIC_CAST<oCMobInter>(node->data);
			if (!mob)
				continue;
			const std::string state = UpperZ(mob->onStateFuncName), scheme = UpperZ(mob->sceme), name = UpperZ(mob->name);
			ShrineEntry e;
			if (state.find("PRAYSHRINE") != std::string::npos || scheme.find("PRAYSHRINE") != std::string::npos)
				e.kind = kShrineInnos;
			else if (state.find("PRAYIDOL") != std::string::npos || scheme.find("PRAYIDOL") != std::string::npos)
				e.kind = kIdolBeliar;
			else if (scheme == "INNOS" || name == "MOBNAME_INNOS")
				e.kind = kStatueInnos;
			else
				continue;
			e.pos = mob->GetPositionWorld();
			e.at = mob->GetAtVectorWorld();
			if (world->wayNet)
				if (zCWaypoint* wp = world->wayNet->GetNearestWaypoint(e.pos)) {
					e.waypoint = wp->name.ToChar();
					e.where = ReadableWaypoint(e.waypoint);
					if (e.where.empty())
						e.where = ::GT::Text::FromGame(e.waypoint.c_str());
				}
			if (e.kind == kShrineInnos)
				e.hauntVar = ShrineHauntVariable(world, e.pos);
			g_shrines.list.push_back(e);
		}
		std::stable_sort(g_shrines.list.begin(), g_shrines.list.end(), [](const ShrineEntry& a, const ShrineEntry& b) {
			return a.kind != b.kind ? a.kind < b.kind : a.where < b.where;
		});
		::GT::Log::Write("Shrines and statues in this world: %d", (int)g_shrines.list.size());
		return g_shrines.list;
	}

	static void ForgetShrines() {
		g_shrines = ShrineList();
	}

	static void TeleportToShrine(int index) {
		Enqueue([index]() {
			if (!InGame())
				return;
			const std::vector<ShrineEntry>& list = Shrines();
			if (index < 0 || index >= (int)list.size())
				return;
			const ShrineEntry& e = list[index];
			RememberHeroSpot();
			MoveHero(FreeSpotInFront(e.pos, e.at, 170.0f, nullptr));
			static const char* const kNames[] = { u8"Innosova svatyně", u8"Innosova socha", u8"Beliarova socha" };
			::GT::Overlay::Toast(std::string(u8"Teleport: ") + kNames[e.kind] + (e.where.empty() ? std::string() : u8" – " + e.where));
		});
	}

} // namespace Trainer
} // namespace GOTHIC_NAMESPACE
