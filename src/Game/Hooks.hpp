// Engine hooks: rendering (overlay), game frame (actions), input filtering
// while the menu is open, and damage (one-hit kill). Addresses: Gothic II NotR 2.6 fix.

namespace GOTHIC_NAMESPACE {
namespace Trainer {

	// ------------------------------------------------------------------ input filter

	static int g_inputBypass = 0;
	static ::GT::Overlay::PlatformInput g_mouseSample;
	static bool g_mouseSampleReady = false;

	struct InputBypass {
		InputBypass() { ++g_inputBypass; }
		~InputBypass() { --g_inputBypass; }
	};

	static inline bool BlockGameInput() {
		return g_inputBypass == 0 && ::GT::Overlay::CapturesInput();
	}

	// ------------------------------------------------------------------ game frame

	static void RunAction(void* ctx) {
		(*static_cast<Action*>(ctx))();
	}

	// A savegame may reuse the same addresses for the hero, world and parser.
	// Invalidate explicitly at load entry, before any object is destroyed.
	static void ResetSession() {
		++g_session;
		g_actions.clear();
		g_items.ready = g_items.building = false;
		g_items.items.clear();
		g_items.goldIndex = -1;
		g_npcDb.ready = g_npcDb.building = false;
		g_npcDb.list.clear();
		g_waypoints.ready = false;
		g_waypoints.world = nullptr;
		g_waypoints.list.clear();
		g_positions.loaded = false;
		g_transitions.ready = g_transitions.building = false;
		g_transitions.world = nullptr;
		g_transitions.list.clear();
		g_teleportBack.clear();
		ForgetShrines();
		g_questDb.building = false;        // the scripts stay, only queued work is gone
		g_diary.ready = g_diary.pending = false;
		g_diary.topics.clear();
		g_diary.byRaw.clear();
		g_diary.nextRefresh = 0.0;
		g_questLive = QuestLiveState();
		g_questNext = QuestDetailNext();
		g_questBoard = QuestBoard();
		g_questChests = QuestChests();
		g_merchants = MerchantBoard();
		ReleaseMaps();
		g_traderStock.clear();
		g_questAuto = QuestAutoState();
		g_pendingTalk = PendingTalk();
		g_state.lastPlayer = nullptr;
		g_state.lastWorld = nullptr;
		g_state.fastRunApplied = false;
		g_state.frozenCaptured = false;
		g_state.weatherSaved = false;      // the sky of the old world is gone
		g_state.weatherSky = nullptr;
		Ui::g_itemsUi.lastDbSize = (size_t)-1;
		Ui::g_itemsUi.filtered.clear();
		Ui::g_itemsUi.selected = -1;
		Ui::g_npcsUi.lastSize = (size_t)-1;
		Ui::g_npcsUi.filtered.clear();
		Ui::g_npcsUi.selected = -1;
		Ui::ResetTeleportUi();
		Ui::g_scriptsUi.ready = false;
		Ui::g_scriptsUi.funcSelected = -1;
		Ui::g_arrayCursor.clear();
	}

	struct WorldChangeGuard {
		WorldChangeGuard() {
			if (g_loadingDepth++ == 0) {
				ResetSession();
				::GT::Overlay::SetVisible(false);
			}
		}
		~WorldChangeGuard() { --g_loadingDepth; }
	};

	static void SynchronizeSession() {
		if (!InGame()) {
			if (g_state.lastWorld || g_state.lastPlayer)
				ResetSession();
			return;
		}
		oCWorld* world = GameWorld();
		if (g_state.lastWorld != world || g_state.lastPlayer != player) {
			ResetSession();
			g_state.lastWorld = world;
			g_state.lastPlayer = player;
		}
	}

	static void RunQueuedActions() {
		if (g_actions.empty())
			return;
		std::vector<QueuedAction> actions;
		actions.swap(g_actions);
		for (QueuedAction& queued : actions) {
			if (queued.session != g_session || !InGame())
				continue;
			unsigned long code = 0;
			if (!::GT::SehInvoke(&RunAction, &queued.call, &code)) {
				g_items.building = false;
				g_npcDb.building = false;
				g_transitions.building = false;
				g_questDb.building = false;
				g_diary.pending = false;
				g_questLive.pending = false;
				::GT::Log::Write("Exception 0x%08lX in a trainer action", code);
				::GT::Overlay::Toast(u8"Akce selhala (podrobnosti v GothicTrainer.log)");
			}
		}
	}

	static void ApplyPersistentEffects() {
		if (!InGame())
			return;

		oCNpc* hero = player;
		if (hero != g_state.lastPlayer) {
			g_state.lastPlayer = hero;
			g_state.fastRunApplied = false;  // overlays are not stored in savegames
		}
		oCWorld* world = GameWorld();
		if (world != g_state.lastWorld) {
			g_state.lastWorld = world;
			g_waypoints.ready = false;
			g_positions.loaded = false;
			g_state.frozenCaptured = false;
		}

		oCWorldTimer* timer = ogame->GetWorldTimer();
		if (g_state.freezeTime && timer) {
			if (!g_state.frozenCaptured) {
				g_state.frozenWorldTime = timer->worldTime;
				g_state.frozenDay = timer->day;
				g_state.frozenCaptured = true;
			}
			timer->worldTime = g_state.frozenWorldTime;
			timer->day = g_state.frozenDay;
		}
		else {
			g_state.frozenCaptured = false;
		}

		ApplyWeather();
		g_state.frameCounter++;

		if (hero->IsDead())
			return;

		if (g_state.infiniteHp)
			hero->attribute[NPC_ATR_HITPOINTS] = hero->attribute[NPC_ATR_HITPOINTSMAX];
		if (g_state.infiniteMana)
			hero->attribute[NPC_ATR_MANA] = hero->attribute[NPC_ATR_MANAMAX];
		if (g_state.infiniteAmmo && g_state.frameCounter % 10 == 0)
			RefillAmmo(hero);

		if (g_state.fastRun != g_state.fastRunApplied) {
			zSTRING mds("HUMANS_SPRINT.MDS");
			if (g_state.fastRun)
				hero->ApplyOverlay(mds);
			else
				hero->RemoveOverlay(mds);
			g_state.fastRunApplied = g_state.fastRun;
		}
	}

	static void QuestAutoThunk(void*) {
		ProcessPendingTalk();
		ProcessQuestAuto();
	}

	static void GameFrameThunk(void*) {
		SynchronizeSession();
		RunQueuedActions();
		if (g_questAuto.active || g_pendingTalk.npc) {
			unsigned long code = 0;
			if (!::GT::SehInvoke(&QuestAutoThunk, nullptr, &code)) {
				g_questAuto = QuestAutoState();
				g_pendingTalk = PendingTalk();
				::GT::Log::Write("Exception 0x%08lX while completing a quest dialog", code);
			}
		}
		ApplyPersistentEffects();
	}

	static void OnGameFrame() {
		unsigned long code = 0;
		if (!::GT::SehInvoke(&GameFrameThunk, nullptr, &code))
			::GT::Log::Write("Exception 0x%08lX in the game frame hook", code);
	}

	// ------------------------------------------------------------------ present

	static void DrawUi() {
		Ui::DrawMainWindow();
	}

	struct PresentContext {
		IDirect3DDevice7* device;
		IDirectDraw7* ddraw;
		HWND window;
		::GT::Overlay::PlatformInput input;
	};

	static void PresentThunk(void* ctx) {
		PresentContext* c = static_cast<PresentContext*>(ctx);
		::GT::Overlay::OnPresent(c->device, c->ddraw, c->window, c->input, &DrawUi);
	}

	static void CollectInput(::GT::Overlay::PlatformInput& in) {
		if (!zinput)
			return;
		InputBypass bypass;
		if (::GT::Overlay::CapturesInput()) {
			if (g_mouseSampleReady) {
				in = g_mouseSample;
				g_mouseSample = {};
				g_mouseSampleReady = false;
			}
			else {
			float x = 0.0f, y = 0.0f, z = 0.0f;
			zinput->GetMousePos(x, y, z);
			int flipX = 0, flipY = 0;
			zinput->GetMouseFlipXY(flipX, flipY);
			in.mouseDX = flipX ? -x : x;
			in.mouseDY = flipY ? -y : y;
			in.wheel = z;
			in.lmb = zinput->GetMouseButtonPressedLeft() != 0;
			in.rmb = zinput->GetMouseButtonPressedRight() != 0;
			in.mmb = zinput->GetMouseButtonPressedMid() != 0;
			}
		}
		int dik = ::GT::Overlay::DikForVk(::GT::Settings::Get().toggleKey);
		if (dik)
			in.toggleDown = zinput->KeyPressed(dik) != 0;
	}

	static void OnPresent() {
		static bool announced = false;
		if (!announced) {
			announced = true;
			::GT::Log::Write("First frame, device %p, window %p", (void*)zCRnd_D3D::xd3d_pd3dDevice7, (void*)hWndApp);
		}
		if (::GT::Overlay::IsFaulted())
			return;

		PresentContext ctx;
		ctx.device = zCRnd_D3D::xd3d_pd3dDevice7;
		ctx.ddraw = zCRnd_D3D::xd3d_pdd7;
		ctx.window = hWndApp;
		CollectInput(ctx.input);

		unsigned long code = 0;
		if (!::GT::SehInvoke(&PresentThunk, &ctx, &code)) {
			::GT::Log::Write("Exception 0x%08lX while rendering the overlay", code);
			::GT::Overlay::Fault("exception while rendering");
		}

		if (::GT::Overlay::TakeJustReleased() && zinput) {
			// drop the ESC/clicks buffered while the menu was open
			InputBypass bypass;
			zinput->ClearKeyBuffer();
		}
	}

} // namespace Trainer

	// ================================================================== hooks
	void __fastcall Hook_InputEvents(zCInput_Win32* self, void* vtable);
	auto Ivk_InputEvents = Union::CreateHook(reinterpret_cast<void*>(0x004D5700), &Hook_InputEvents, Union::HookType::Hook_Detours);
	void __fastcall Hook_InputEvents(zCInput_Win32* self, void* vtable) {
		Ivk_InputEvents(self, vtable);
		if (!::GT::Overlay::CapturesInput()) {
			Trainer::g_mouseSample = {};
			Trainer::g_mouseSampleReady = false;
			return;
		}
		Trainer::InputBypass bypass;
		float x = 0, y = 0, z = 0;
		self->GetMousePos(x, y, z);
		int flipX = 0, flipY = 0;
		self->GetMouseFlipXY(flipX, flipY);
		auto& sample = Trainer::g_mouseSample;
		sample.mouseDX += flipX ? -x : x;
		sample.mouseDY += flipY ? -y : y;
		sample.wheel += z;
		sample.lmb = self->GetMouseButtonPressedLeft() != 0;
		sample.rmb = self->GetMouseButtonPressedRight() != 0;
		sample.mmb = self->GetMouseButtonPressedMid() != 0;
		Trainer::g_mouseSampleReady = true;
	}

	void __fastcall Hook_oCGame_LoadGame(oCGame* self, void* vtable, int slot, const zSTRING& name);
	auto Ivk_oCGame_LoadGame = Union::CreateHook(reinterpret_cast<void*>(0x006C65A0), &Hook_oCGame_LoadGame, Union::HookType::Hook_Detours);
	void __fastcall Hook_oCGame_LoadGame(oCGame* self, void* vtable, int slot, const zSTRING& name) {
		Trainer::WorldChangeGuard guard;
		Ivk_oCGame_LoadGame(self, vtable, slot, name);
	}

	void __fastcall Hook_oCGame_LoadSavegame(oCGame* self, void* vtable, int slot, int loadGlobals);
	auto Ivk_oCGame_LoadSavegame = Union::CreateHook(reinterpret_cast<void*>(0x006C67D0), &Hook_oCGame_LoadSavegame, Union::HookType::Hook_Detours);
	void __fastcall Hook_oCGame_LoadSavegame(oCGame* self, void* vtable, int slot, int loadGlobals) {
		Trainer::WorldChangeGuard guard;
		Ivk_oCGame_LoadSavegame(self, vtable, slot, loadGlobals);
	}

	void __fastcall Hook_oCGame_LoadWorld(oCGame* self, void* vtable, int mode, const zSTRING& name);
	auto Ivk_oCGame_LoadWorld = Union::CreateHook(reinterpret_cast<void*>(0x006C90B0), &Hook_oCGame_LoadWorld, Union::HookType::Hook_Detours);
	void __fastcall Hook_oCGame_LoadWorld(oCGame* self, void* vtable, int mode, const zSTRING& name) {
		Trainer::WorldChangeGuard guard;
		Ivk_oCGame_LoadWorld(self, vtable, mode, name);
	}

	// --- presenting a frame: draw the overlay on top of everything
	void __fastcall Hook_zCRnd_D3D_Vid_Blit(zCRnd_D3D* self, void* vtable, int a0, tagRECT* a1, tagRECT* a2);
	auto Ivk_zCRnd_D3D_Vid_Blit = Union::CreateHook(reinterpret_cast<void*>(0x00657670), &Hook_zCRnd_D3D_Vid_Blit, Union::HookType::Hook_Detours);
	void __fastcall Hook_zCRnd_D3D_Vid_Blit(zCRnd_D3D* self, void* vtable, int a0, tagRECT* a1, tagRECT* a2) {
		Trainer::OnPresent();
		Ivk_zCRnd_D3D_Vid_Blit(self, vtable, a0, a1, a2);
	}

	// --- game frame: run queued actions on the game thread
	void __fastcall Hook_oCGame_Render(oCGame* self, void* vtable);
	auto Ivk_oCGame_Render = Union::CreateHook(reinterpret_cast<void*>(0x006C86A0), &Hook_oCGame_Render, Union::HookType::Hook_Detours);
	void __fastcall Hook_oCGame_Render(oCGame* self, void* vtable) {
		Trainer::OnGameFrame();
		Ivk_oCGame_Render(self, vtable);
	}

	// --- damage: one-hit kill, no fall damage
	void __fastcall Hook_oCNpc_OnDamage(oCNpc* self, void* vtable, oCNpc::oSDamageDescriptor& desc);
	auto Ivk_oCNpc_OnDamage = Union::CreateHook(reinterpret_cast<void*>(0x006660E0), &Hook_oCNpc_OnDamage, Union::HookType::Hook_Detours);
	void __fastcall Hook_oCNpc_OnDamage(oCNpc* self, void* vtable, oCNpc::oSDamageDescriptor& desc) {
		if (Trainer::g_state.noFallDamage && player && self == player && (desc.enuModeDamage & oEDamageType_Fall)) {
			if ((desc.enuModeDamage & ~(unsigned long)oEDamageType_Fall) == 0)
				return;   // a pure fall: nothing happens at all
			desc.aryDamage[oEDamageIndex_Fall] = 0;
		}
		if (Trainer::g_state.oneHitKill && player && self != player && desc.pNpcAttacker == player) {
			for (int i = 0; i < oEDamageIndex_MAX; i++)
				if (desc.aryDamage[i] > 0)
					desc.aryDamage[i] = 999999;
			if (desc.fDamageTotal > 0.0f)
				desc.fDamageTotal = 999999.0f;
		}
		Ivk_oCNpc_OnDamage(self, vtable, desc);
	}

	// --- input: the game sees nothing while the trainer menu is open
	float __fastcall Hook_zCInput_Win32_GetState(zCInput_Win32* self, void* vtable, unsigned short key);
	auto Ivk_zCInput_Win32_GetState = Union::CreateHook(reinterpret_cast<void*>(0x004D4D30), &Hook_zCInput_Win32_GetState, Union::HookType::Hook_Detours);
	float __fastcall Hook_zCInput_Win32_GetState(zCInput_Win32* self, void* vtable, unsigned short key) {
		if (Trainer::BlockGameInput())
			return 0.0f;
		return Ivk_zCInput_Win32_GetState(self, vtable, key);
	}

	int __fastcall Hook_zCInput_Win32_GetToggled(zCInput_Win32* self, void* vtable, unsigned short key);
	auto Ivk_zCInput_Win32_GetToggled = Union::CreateHook(reinterpret_cast<void*>(0x004D5020), &Hook_zCInput_Win32_GetToggled, Union::HookType::Hook_Detours);
	int __fastcall Hook_zCInput_Win32_GetToggled(zCInput_Win32* self, void* vtable, unsigned short key) {
		if (Trainer::BlockGameInput())
			return 0;
		return Ivk_zCInput_Win32_GetToggled(self, vtable, key);
	}

	int __fastcall Hook_zCInput_Win32_KeyPressed(zCInput_Win32* self, void* vtable, int key);
	auto Ivk_zCInput_Win32_KeyPressed = Union::CreateHook(reinterpret_cast<void*>(0x004D51C0), &Hook_zCInput_Win32_KeyPressed, Union::HookType::Hook_Detours);
	int __fastcall Hook_zCInput_Win32_KeyPressed(zCInput_Win32* self, void* vtable, int key) {
		if (Trainer::BlockGameInput())
			return 0;
		return Ivk_zCInput_Win32_KeyPressed(self, vtable, key);
	}

	int __fastcall Hook_zCInput_Win32_KeyToggled(zCInput_Win32* self, void* vtable, int key);
	auto Ivk_zCInput_Win32_KeyToggled = Union::CreateHook(reinterpret_cast<void*>(0x004D51D0), &Hook_zCInput_Win32_KeyToggled, Union::HookType::Hook_Detours);
	int __fastcall Hook_zCInput_Win32_KeyToggled(zCInput_Win32* self, void* vtable, int key) {
		if (Trainer::BlockGameInput())
			return 0;
		return Ivk_zCInput_Win32_KeyToggled(self, vtable, key);
	}

	int __fastcall Hook_zCInput_Win32_AnyKeyPressed(zCInput_Win32* self, void* vtable);
	auto Ivk_zCInput_Win32_AnyKeyPressed = Union::CreateHook(reinterpret_cast<void*>(0x004D51F0), &Hook_zCInput_Win32_AnyKeyPressed, Union::HookType::Hook_Detours);
	int __fastcall Hook_zCInput_Win32_AnyKeyPressed(zCInput_Win32* self, void* vtable) {
		if (Trainer::BlockGameInput())
			return 0;
		return Ivk_zCInput_Win32_AnyKeyPressed(self, vtable);
	}

	unsigned short __fastcall Hook_zCInput_Win32_GetKey(zCInput_Win32* self, void* vtable, int a0, int a1);
	auto Ivk_zCInput_Win32_GetKey = Union::CreateHook(reinterpret_cast<void*>(0x004D5370), &Hook_zCInput_Win32_GetKey, Union::HookType::Hook_Detours);
	unsigned short __fastcall Hook_zCInput_Win32_GetKey(zCInput_Win32* self, void* vtable, int a0, int a1) {
		if (Trainer::BlockGameInput())
			return 0;
		return Ivk_zCInput_Win32_GetKey(self, vtable, a0, a1);
	}

	unsigned char __fastcall Hook_zCInput_Win32_GetChar(zCInput_Win32* self, void* vtable);
	auto Ivk_zCInput_Win32_GetChar = Union::CreateHook(reinterpret_cast<void*>(0x004D5510), &Hook_zCInput_Win32_GetChar, Union::HookType::Hook_Detours);
	unsigned char __fastcall Hook_zCInput_Win32_GetChar(zCInput_Win32* self, void* vtable) {
		if (Trainer::BlockGameInput())
			return 0;
		return Ivk_zCInput_Win32_GetChar(self, vtable);
	}

	void __fastcall Hook_zCInput_Win32_GetMousePos(zCInput_Win32* self, void* vtable, float& x, float& y, float& z);
	auto Ivk_zCInput_Win32_GetMousePos = Union::CreateHook(reinterpret_cast<void*>(0x004D5730), &Hook_zCInput_Win32_GetMousePos, Union::HookType::Hook_Detours);
	void __fastcall Hook_zCInput_Win32_GetMousePos(zCInput_Win32* self, void* vtable, float& x, float& y, float& z) {
		if (Trainer::BlockGameInput()) {
			x = 0.0f;
			y = 0.0f;
			z = 0.0f;
			return;
		}
		Ivk_zCInput_Win32_GetMousePos(self, vtable, x, y, z);
	}

	int __fastcall Hook_zCInput_Win32_GetMouseButtonPressedLeft(zCInput_Win32* self, void* vtable);
	auto Ivk_zCInput_Win32_GetMouseButtonPressedLeft = Union::CreateHook(reinterpret_cast<void*>(0x004D5790), &Hook_zCInput_Win32_GetMouseButtonPressedLeft, Union::HookType::Hook_Detours);
	int __fastcall Hook_zCInput_Win32_GetMouseButtonPressedLeft(zCInput_Win32* self, void* vtable) {
		if (Trainer::BlockGameInput())
			return 0;
		return Ivk_zCInput_Win32_GetMouseButtonPressedLeft(self, vtable);
	}

	int __fastcall Hook_zCInput_Win32_GetMouseButtonPressedMid(zCInput_Win32* self, void* vtable);
	auto Ivk_zCInput_Win32_GetMouseButtonPressedMid = Union::CreateHook(reinterpret_cast<void*>(0x004D57A0), &Hook_zCInput_Win32_GetMouseButtonPressedMid, Union::HookType::Hook_Detours);
	int __fastcall Hook_zCInput_Win32_GetMouseButtonPressedMid(zCInput_Win32* self, void* vtable) {
		if (Trainer::BlockGameInput())
			return 0;
		return Ivk_zCInput_Win32_GetMouseButtonPressedMid(self, vtable);
	}

	int __fastcall Hook_zCInput_Win32_GetMouseButtonPressedRight(zCInput_Win32* self, void* vtable);
	auto Ivk_zCInput_Win32_GetMouseButtonPressedRight = Union::CreateHook(reinterpret_cast<void*>(0x004D57B0), &Hook_zCInput_Win32_GetMouseButtonPressedRight, Union::HookType::Hook_Detours);
	int __fastcall Hook_zCInput_Win32_GetMouseButtonPressedRight(zCInput_Win32* self, void* vtable) {
		if (Trainer::BlockGameInput())
			return 0;
		return Ivk_zCInput_Win32_GetMouseButtonPressedRight(self, vtable);
	}

} // namespace GOTHIC_NAMESPACE
