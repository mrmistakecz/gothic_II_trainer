// The game's own maps of the current world for the map tab. The parchment and
// its calibration come from the scripts that show the map item
// (USE_MAP_NEWWORLD, ...: Doc_SetPage, Doc_SetLevel, Doc_SetLevelCoords), read
// the same way as the minimap does it (navigation/MapData.h) without running
// them. The texture is the game's own; the overlay draws its Direct3D surface.

namespace GOTHIC_NAMESPACE {
namespace Trainer {

	struct TrainerMap {
		QuestMap::Definition def;
		zCTexture* texture = nullptr;
		int width = 1;
		int height = 1;
		std::string title;   // UTF-8
	};

	struct MapState {
		oCWorld* world = nullptr;
		std::vector<TrainerMap> maps;
		bool read = false;
		bool pending = false;
	};

	static MapState g_maps;

	static void ReleaseMaps() {
		for (TrainerMap& m : g_maps.maps)
			if (m.texture)
				m.texture->Release();
		g_maps = MapState();
	}

	static std::string UpperBase(std::string s) {
		const size_t slash = s.find_last_of("\\/");
		if (slash != std::string::npos)
			s = s.substr(slash + 1);
		for (char& c : s)
			c = (char)toupper((unsigned char)c);
		return s;
	}

	// Game thread, once per world.
	static void ReadMaps() {
		struct Finish { ~Finish() { g_maps.pending = false; } } finish;
		oCWorld* world = GameWorld();
		if (!InGame() || !parser || (g_maps.read && g_maps.world == world))
			return;
		for (TrainerMap& m : g_maps.maps)
			if (m.texture)
				m.texture->Release();
		g_maps.maps.clear();
		g_maps.world = world;
		g_maps.read = true;
		const std::string here = UpperBase(world->GetWorldFilename().ToChar());
		const int symbols = parser->symtab.table.GetNumInList();
		auto symbol = [&](int id) -> zCPar_Symbol* { return id >= 0 && id < symbols ? parser->GetSymbol(id) : nullptr; };
		auto value = [&](int id) {
			zCPar_Symbol* s = symbol(id);
			if (!s || !(s->flags & zPAR_FLAG_CONST) || !s->ele)
				return QuestMap::Value();
			if (s->type == zPAR_TYPE_INT) {
				int n = 0;
				s->GetValue(n, 0);
				return QuestMap::Value::Int(n);
			}
			if (s->type == zPAR_TYPE_STRING) {
				zSTRING text;
				s->GetValue(text, 0);
				return QuestMap::Value::Str(text.ToChar());
			}
			return QuestMap::Value();
		};
		auto name = [&](int id) {
			zCPar_Symbol* s = symbol(id);
			return s ? std::string(s->name.ToChar()) : std::string();
		};
		const int size = parser->stack.GetStatSize();
		if (size <= 0 || size > parser->stack.stacksize)
			return;
		struct Source { const char* function; const char* title; };
		static const Source kSources[] = {
			{ "USE_MAP_NEWWORLD", u8"Khorinis" },
			{ "USE_MAP_NEWWORLD_CITY", u8"Město Khorinis" },
			{ "USE_MAP_OLDWORLD", u8"Hornické údolí" },
			{ "USE_MAP_ADDONWORLD", u8"Jharkendar" },
		};
		for (const Source& src : kSources) {
			zCPar_Symbol* s = symbol(parser->GetIndex(zSTRING(src.function)));
			if (!s || s->type != zPAR_TYPE_FUNC || (s->flags & zPAR_FLAG_EXTERNAL))
				continue;
			int start = -1;
			s->GetStackPos(start, 0);
			if (start < 0)
				continue;
			TrainerMap m;
			if (!QuestMap::Extract(parser->stack.stack, (size_t)size, (size_t)start, value, name, m.def))
				continue;
			if (UpperBase(m.def.world) != here)
				continue;
			zSTRING texName(m.def.texture.c_str());
			m.texture = zCTexture::Load(texName, 0);
			if (!m.texture)
				continue;
			m.texture->CacheIn(-1.0f);
			m.texture->GetPixelSize(m.width, m.height);
			if (m.width <= 0 || m.height <= 0) {
				m.texture->Release();
				continue;
			}
			m.title = src.title;
			g_maps.maps.push_back(m);
		}
		::GT::Log::Write("Map tab: %s, %d maps", here.c_str(), (int)g_maps.maps.size());
	}

	// UI: the Direct3D surface of the map for this frame, nullptr while it loads.
	// Asked every frame, the surface cache of the renderer may move it.
	static IDirectDrawSurface7* MapSurface(TrainerMap& m) {
		if (!m.texture)
			return nullptr;
		m.texture->CacheIn(-1.0f);
		if (m.texture->cacheState != zRES_CACHED_IN)
			return nullptr;
		zCTex_D3D* tex = static_cast<zCTex_D3D*>(m.texture);
		IDirectDrawSurface7* surface = tex->xtex_pddtex[0];
		if (!surface || surface->IsLost() == DDERR_SURFACELOST)
			return nullptr;
		return surface;
	}

	// Teleport to a point of the map: onto the ground there, or onto a
	// waypoint right next to it (a place where people stand).
	static void TeleportToMapPoint(float x, float z) {
		Enqueue([x, z]() {
			if (!InGame())
				return;
			float y = 0.0f;
			if (!GroundAt(x, z, y)) {
				::GT::Overlay::Toast(u8"Tady není kam stoupnout – mimo svět");
				return;
			}
			RememberHeroSpot();
			oCWorld* world = GameWorld();
			if (world && world->wayNet)
				if (zCWaypoint* wp = world->wayNet->GetNearestWaypoint(PlaceVec(x, y - 120.0f, z))) {
					const float dx = wp->pos.n[0] - x, dz = wp->pos.n[2] - z, dy = wp->pos.n[1] - (y - 120.0f);
					if (dx * dx + dz * dz < 500.0f * 500.0f && fabsf(dy) < 300.0f && player->BeamTo(wp->name)) {
						const std::string area = AreaOfWaypoint(wp->name.ToChar());
						::GT::Overlay::Toast(area.empty() ? std::string(u8"Teleport na místo z mapy") : u8"Teleport: " + area);
						return;
					}
				}
			MoveHero(PlaceVec(x, y, z));
			::GT::Overlay::Toast(u8"Teleport na místo z mapy");
		});
	}

} // namespace Trainer
} // namespace GOTHIC_NAMESPACE
