// Engine side: shared trainer state, action queue and small engine helpers.
// Included once per engine namespace (only Gothic II NotR is built).

namespace GOTHIC_NAMESPACE {
namespace Trainer {

	using Action = std::function<void()>;
	struct QueuedAction {
		Action call;
		unsigned session;
	};

	struct TrainerState {
		bool infiniteHp = false;
		bool infiniteMana = false;
		bool fastRun = false;
		bool fastRunApplied = false;
		bool oneHitKill = false;

		bool  freezeTime = false;
		bool  frozenCaptured = false;
		float frozenWorldTime = 0.0f;
		int   frozenDay = 0;

		bool noFallDamage = false;
		bool infiniteAmmo = false;
		unsigned frameCounter = 0;

		// weather: 0 = as the game wants, 1 = rain, 2 = clear sky
		int   weatherMode = 0;
		bool  weatherSaved = false;
		zCSkyControler_Outdoor* weatherSky = nullptr;
		float savedRainStart = 0.0f;
		float savedRainStop = 0.0f;
		int   savedDontRain = 0;

		oCNpc*   lastPlayer = nullptr;
		oCWorld* lastWorld = nullptr;
		std::string worldKey;          // e.g. "NEWWORLD.ZEN"
	};

	static TrainerState g_state;
	static std::vector<QueuedAction> g_actions;
	static unsigned g_session = 0;
	static int g_loadingDepth = 0;

	// Engine calls that change the world are queued and executed at the start
	// of the next game frame (oCGame::Render), never in the middle of rendering.
	static void Enqueue(Action action) {
		g_actions.push_back({ std::move(action), g_session });
	}

	static bool IsLoading() {
		return g_loadingDepth > 0 || (ogame && (ogame->inLoadSaveGame || ogame->inLevelChange || ogame->inScriptStartup));
	}

	static oCWorld* GameWorld() {
		return ogame ? ogame->GetGameWorld() : nullptr;
	}

	// The hero exists and lives in the current world
	static bool InGame() {
		if (!ogame || !player || IsLoading())
			return false;
		oCWorld* world = GameWorld();
		return world && player->GetHomeWorld() == world;
	}

	static std::string FromZ(const zSTRING& s) {
		return ::GT::Text::FromGame(s.ToChar());
	}

	static zSTRING ToZ(const std::string& utf8) {
		return zSTRING(::GT::Text::ToGame(utf8).c_str());
	}

	static std::string NpcName(oCNpc* npc) {
		if (!npc)
			return std::string();
		std::string n = FromZ(npc->name[0]);
		if (n.empty())
			n = FromZ(npc->GetInstanceName());
		return n;
	}

	static std::string GuildName(int guild) {
		if (ogame && ogame->guilds) {
			std::string s = FromZ(ogame->guilds->GetGuildName(guild));
			if (!s.empty())
				return s;
		}
		static const char* names[] = {
			u8"Bez gildy", u8"Paladin", u8"Domobrana", u8"Občan", u8"Mág ohně", u8"Novic", u8"Drakobijce",
			u8"Žoldák", u8"Rolník", u8"Bandita", u8"Trestanec", u8"Pátrač", u8"Cizinec", u8"Pirát", u8"Mág vody",
		};
		if (guild >= 0 && guild < (int)(sizeof(names) / sizeof(names[0])))
			return names[guild];
		return ::GT::Text::Format(u8"Gilda %d", guild);
	}

	// Executes a console command exactly as if typed into the Marvin console.
	static bool RunConsoleCommand(const std::string& utf8) {
		if (!InGame() || !zcon)
			return false;
		struct RestoreTestMode {
			oCGame* game;
			int value;
			~RestoreTestMode() { if (ogame == game) game->game_testmode = value; }
		} restore{ ogame, ogame->game_testmode };
		ogame->game_testmode = TRUE;
		int ok = zcon->Evaluate(ToZ(utf8));
		::GT::Log::Write("Console: %s -> %d", utf8.c_str(), ok);
		return ok != 0;
	}

	static int ScriptSymbolIndex(const char* name) {
		if (!parser)
			return -1;
		return parser->GetIndex(zSTRING(name));
	}

	// Item constructors and script calls change the parser's current instance.
	// Never leave it pointing at a temporary item released by the catalogue.
	struct ScriptInstanceScope {
		zCParser* savedParser = parser;
		zCPar_Symbol* symbol = zCPar_Symbol::instance_sym;
		void* address = zCPar_Symbol::instance_adr;
		int instance = savedParser ? savedParser->instance : 0;
		int helper = savedParser ? savedParser->instance_help : 0;
		int indices[4] = { -1, -1, -1, -1 };
		void* addresses[4] = {};

		ScriptInstanceScope() {
			if (!savedParser)
				return;
			const char* names[] = { "SELF", "OTHER", "VICTIM", "ITEM" };
			for (int i = 0; i < 4; i++) {
				indices[i] = ScriptSymbolIndex(names[i]);
				zCPar_Symbol* value = indices[i] >= 0 ? savedParser->GetSymbol(indices[i]) : nullptr;
				if (value && value->type == zPAR_TYPE_INSTANCE)
					addresses[i] = value->GetInstanceAdr();
				else
					indices[i] = -1;
			}
		}

		~ScriptInstanceScope() {
			if (!savedParser || parser != savedParser)
				return;
			for (int i = 0; i < 4; i++)
				if (indices[i] >= 0)
					savedParser->SetInstance(indices[i], addresses[i]);
			savedParser->instance = instance;
			savedParser->instance_help = helper;
			zCPar_Symbol::instance_sym = symbol;
			zCPar_Symbol::instance_adr = address;
		}
	};

	// Calls a Daedalus function with one int argument if it exists.
	static bool CallScriptInt(const char* name, int arg) {
		int index = ScriptSymbolIndex(name);
		if (index < 0)
			return false;
		zCPar_Symbol* sym = parser->GetSymbol(index);
		if (!sym || sym->type != zPAR_TYPE_FUNC || sym->ele != 1)
			return false;
		zCPar_Symbol* param = parser->GetSymbol(index + 1);
		if (!param || param->type != zPAR_TYPE_INT)
			return false;
		parser->CallFunc(index, arg);
		return true;
	}

	// Sets every element of an integer script array (e.g. PLAYER_TALENT_ALCHEMY).
	static int SetScriptIntArray(const char* name, int value) {
		int index = ScriptSymbolIndex(name);
		if (index < 0)
			return 0;
		zCPar_Symbol* sym = parser->GetSymbol(index);
		if (!sym || sym->type != zPAR_TYPE_INT || (sym->flags & (zPAR_FLAG_CONST | zPAR_FLAG_CLASSVAR)))
			return 0;
		int count = (int)sym->ele;
		for (int i = 0; i < count; i++)
			sym->SetValue(value, i);
		return count;
	}

} // namespace Trainer
} // namespace GOTHIC_NAMESPACE
