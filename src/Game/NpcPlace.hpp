// Where a character really is. The game puts characters far from the hero to
// sleep (oCWorld::DisableVob): they leave the world tree and keep a stale
// position, often the world origin, which in Khorinis lies in the harbour.
// The game itself wakes them at the place of their daily routine
// (oCRtnManager::GetRoutinePos), so that is the place the trainer uses too.
// Characters parked on the "TOT" waypoint are out of the story for now.

namespace GOTHIC_NAMESPACE {
namespace Trainer {

	enum NpcPlaceState {
		kNpcHere = 0,      // active in the world, its own position
		kNpcAsleep,        // asleep far away, the place of its routine
		kNpcParked,        // moved out of the story (waypoint TOT)
		kNpcUnknown        // no usable position
	};

	struct NpcPlace {
		zVEC3 pos;
		int state = kNpcUnknown;
		std::string waypoint;   // routine waypoint, upper case
		bool Usable() const { return state == kNpcHere || state == kNpcAsleep; }
	};

	static zVEC3 PlaceVec(float x, float y, float z) {
		zVEC3 v;
		v.n[0] = x;
		v.n[1] = y;
		v.n[2] = z;
		return v;
	}

	static bool NearOrigin(const zVEC3& p) {
		return fabsf(p.n[0]) < 1.0f && fabsf(p.n[1]) < 1.0f && fabsf(p.n[2]) < 1.0f;
	}

	static bool NpcActive(oCNpc* npc) {
		return npc && npc->GetHomeWorld() != nullptr;
	}

	static std::string UpperAscii(std::string s) {
		for (char& c : s)
			c = (char)toupper((unsigned char)c);
		return s;
	}

	// A waypoint or a named spot (FP_...) of the current world.
	static bool NamedSpot(const std::string& name, zVEC3& pos) {
		oCWorld* world = GameWorld();
		if (!world || name.empty())
			return false;
		const zSTRING z(name.c_str());
		if (world->wayNet)
			if (zCWaypoint* wp = world->wayNet->GetWaypoint(z)) {
				pos = wp->pos;
				return true;
			}
		if (zCVob* vob = world->zCWorld::SearchVobByName(z)) {
			pos = vob->GetPositionWorld();
			return true;
		}
		return false;
	}

	// The waypoint of the routine the character follows now (C_NPC.wp).
	static std::string RoutineWaypoint(oCNpc* npc) {
		std::string wp = UpperAscii(npc->wpname.ToChar());
		if (!wp.empty())
			return wp;
		oCRtnEntry* now = nullptr;
		oCRtnEntry* next = nullptr;
		if (rtnMan && rtnMan->FindRoutine(npc, now, next) && now)
			return UpperAscii(now->wpname.ToChar());
		return std::string();
	}

	// Game thread.
	static NpcPlace WhereIsNpc(oCNpc* npc) {
		NpcPlace p;
		if (!npc)
			return p;
		p.waypoint = RoutineWaypoint(npc);
		const bool parked = p.waypoint == "TOT";
		if (NpcActive(npc)) {
			p.pos = npc->GetPositionWorld();
			p.state = parked ? kNpcParked : NearOrigin(p.pos) ? kNpcUnknown : kNpcHere;
			return p;
		}
		zVEC3 pos = PlaceVec(0.0f, 0.0f, 0.0f);
		if (rtnMan)
			pos = rtnMan->GetRoutinePos(npc);
		if (NearOrigin(pos) && !NamedSpot(p.waypoint, pos))
			NamedSpot(UpperAscii(npc->spawnPoint.ToChar()), pos);
		if (NearOrigin(pos))
			pos = npc->GetPositionWorld();   // last place it was seen
		p.pos = pos;
		p.state = parked ? kNpcParked : NearOrigin(pos) ? kNpcUnknown : kNpcAsleep;
		return p;
	}

	static std::string NpcStateText(int state) {
		switch (state) {
		case kNpcAsleep: return u8"daleko (spí)";
		case kNpcParked: return u8"mimo hru";
		case kNpcUnknown: return u8"neznámo kde";
		default: return std::string();
		}
	}

	// Why the hero cannot be taken to the character.
	static std::string NpcNotReachableText(const NpcPlace& p, const std::string& name) {
		if (p.state == kNpcParked)
			return name + u8" teď ve hře není (hra ho odklidila mimo svět)";
		return name + u8": polohu nejde zjistit";
	}

	// A free spot about `distance` in front of the place, nearer when a wall
	// is in the way, so the hero does not end up inside it.
	static zVEC3 FreeSpotInFront(const zVEC3& pos, const zVEC3& at, float distance, zCVob* ignore) {
		float d = distance;
		oCWorld* world = GameWorld();
		const float len = sqrtf(at.n[0] * at.n[0] + at.n[2] * at.n[2]);
		if (len < 0.01f)
			return pos;
		const float ax = at.n[0] / len, az = at.n[2] / len;
		if (world) {
			zVEC3 from = pos;
			from.n[1] += 60.0f;
			const zVEC3 ray = PlaceVec(ax * distance, 0.0f, az * distance);
			const int flags = zTRACERAY_STAT_POLY | zTRACERAY_VOB_IGNORE_NO_CD_DYN | zTRACERAY_VOB_IGNORE_CHARACTER | zTRACERAY_POLY_IGNORE_TRANSP;
			if (world->TraceRayNearestHit(from, ray, ignore, flags) && world->traceRayReport.foundHit) {
				const zVEC3& hit = world->traceRayReport.foundIntersection;
				const float dx = hit.n[0] - from.n[0], dz = hit.n[2] - from.n[2];
				d = sqrtf(dx * dx + dz * dz) - 50.0f;
				if (d < 40.0f)
					d = 40.0f;
			}
		}
		zVEC3 out = pos;
		out.n[0] += ax * d;
		out.n[2] += az * d;
		return out;
	}

	// Where the hero should stand to meet the character.
	static zVEC3 MeetingSpot(oCNpc* npc, const NpcPlace& p) {
		if (p.state == kNpcHere)
			return FreeSpotInFront(p.pos, npc->GetAtVectorWorld(), 150.0f, npc);
		// asleep: the routine waypoint faces where the character will stand
		if (oCWorld* world = GameWorld())
			if (world->wayNet && !p.waypoint.empty())
				if (zCWaypoint* wp = world->wayNet->GetWaypoint(zSTRING(p.waypoint.c_str())))
					return FreeSpotInFront(p.pos, wp->dir, 150.0f, nullptr);
		zVEC3 out = p.pos;
		out.n[0] += 120.0f;
		return out;
	}

	// The ground under a point of the map (y of the hero standing there), or
	// false outside the world.
	static bool GroundAt(float x, float z, float& y) {
		oCWorld* world = GameWorld();
		if (!world)
			return false;
		const zVEC3 from = PlaceVec(x, 60000.0f, z);
		const zVEC3 ray = PlaceVec(0.0f, -140000.0f, 0.0f);
		const int flags = zTRACERAY_STAT_POLY | zTRACERAY_VOB_IGNORE_NO_CD_DYN | zTRACERAY_VOB_IGNORE_CHARACTER | zTRACERAY_POLY_IGNORE_TRANSP;
		if (!world->TraceRayNearestHit(from, ray, (zCVob*)nullptr, flags) || !world->traceRayReport.foundHit)
			return false;
		y = world->traceRayReport.foundIntersection.n[1] + 120.0f;
		return true;
	}

	// ------------------------------------------------------------------ talking to a sleeping character

	// The hero arrives first; the game wakes the character a moment later.
	// Then the talk starts like pressing the action key.
	struct PendingTalk {
		oCNpc* npc = nullptr;
		DWORD notBefore = 0;
		DWORD giveUpAt = 0;
		std::string name;
	};

	static PendingTalk g_pendingTalk;

	// Game frame.
	static void ProcessPendingTalk() {
		PendingTalk& t = g_pendingTalk;
		if (!t.npc)
			return;
		const DWORD now = GetTickCount();
		if (!InGame() || !NpcStillExists(t.npc) || t.npc->IsDead() || now > t.giveUpAt) {
			if (InGame() && now > t.giveUpAt)
				::GT::Overlay::Toast(t.name + u8" se ještě neprobudil – zkus promluvit sám");
			g_pendingTalk = PendingTalk();
			return;
		}
		if (now < t.notBefore || !NpcActive(t.npc) || !oCInformationManager::GetInformationManager().HasFinished())
			return;
		oCNpc* npc = t.npc;
		g_pendingTalk = PendingTalk();
		npc->AssessTalk_S(player);
	}

} // namespace Trainer
} // namespace GOTHIC_NAMESPACE
