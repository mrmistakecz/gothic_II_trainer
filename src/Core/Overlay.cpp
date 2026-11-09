#include <windows.h>
#include <ZenGin/DirectX7/include/d3d.h>
#include <cmath>
#include <cfloat>
#include <vector>
#include "imgui.h"
#include "imgui_internal.h"
#include "Overlay.h"
#include "ImGuiDX7.h"
#include "Settings.h"
#include "Paths.h"
#include "Text.h"
#include "Log.h"
#include "Seh.h"
#include "ToggleHotkey.h"
#include "Pointer.h"

namespace GT { namespace Overlay {

	namespace {
		bool        g_contextReady = false;
		bool        g_faulted = false;
		bool        g_visible = false;
		bool        g_guard = false;
		double      g_guardStart = 0.0;
		bool        g_justReleased = false;
		ToggleHotkey g_hotkey;
		int         g_hotkeyVk = 0;
		bool        g_fontsDirty = true;
		bool        g_textureDirty = true;
		bool        g_clearInputPending = false;
		bool        g_seenDirectInputMouse = false;
		POINT       g_lastSysCursor = { -1, -1 };
		float       g_wheelAccum = 0.0f;

		IDirect3DDevice7* g_device = nullptr;
		IDirectDraw7*     g_ddraw = nullptr;
		HWND              g_window = nullptr;
		WNDPROC           g_origWndProc = nullptr;

		ImVec2      g_mouse(-1.0f, -1.0f);
		double      g_lastTime = 0.0;
		std::string g_iniPath;
		float       g_fontPx = 18.0f;
		ImFont*     g_titleFont = nullptr;

		struct ToastItem {
			std::string text;
			double until;
		};
		std::vector<ToastItem> g_toasts;
	}

	static double Now() {
		static LARGE_INTEGER freq = {};
		if (!freq.QuadPart)
			QueryPerformanceFrequency(&freq);
		LARGE_INTEGER c;
		QueryPerformanceCounter(&c);
		return (double)c.QuadPart / (double)freq.QuadPart;
	}

	static bool AsyncDown(int vk) {
		return (GetAsyncKeyState(vk) & 0x8000) != 0;
	}

	// ------------------------------------------------------------------ keys

	struct KeyInfo { int vk; int dik; const char* name; };
	static const KeyInfo kToggleKeys[] = {
		{ VK_F1, 0x3B, "F1" },  { VK_F2, 0x3C, "F2" },  { VK_F3, 0x3D, "F3" },  { VK_F4, 0x3E, "F4" },
		{ VK_F5, 0x3F, "F5" },  { VK_F6, 0x40, "F6" },  { VK_F7, 0x41, "F7" },  { VK_F8, 0x42, "F8" },
		{ VK_F9, 0x43, "F9" },  { VK_F10, 0x44, "F10" }, { VK_F11, 0x57, "F11" }, { VK_F12, 0x58, "F12" },
		{ VK_INSERT, 0xD2, "Insert" }, { VK_HOME, 0xC7, "Home" }, { VK_END, 0xCF, "End" },
		{ VK_PRIOR, 0xC9, "Page Up" }, { VK_NEXT, 0xD1, "Page Down" }, { VK_DELETE, 0xD3, "Delete" },
		{ VK_SCROLL, 0x46, "Scroll Lock" }, { VK_PAUSE, 0xC5, "Pause" }, { VK_OEM_3, 0x29, u8"~ / ;" },
	};

	int DikForVk(int vk) {
		for (const KeyInfo& k : kToggleKeys)
			if (k.vk == vk)
				return k.dik;
		return 0;
	}

	const char* KeyName(int vk) {
		for (const KeyInfo& k : kToggleKeys)
			if (k.vk == vk)
				return k.name;
		return "?";
	}

	struct NavKey { int vk; ImGuiKey key; };
	static const NavKey kNavKeys[] = {
		{ VK_TAB, ImGuiKey_Tab }, { VK_LEFT, ImGuiKey_LeftArrow }, { VK_RIGHT, ImGuiKey_RightArrow },
		{ VK_UP, ImGuiKey_UpArrow }, { VK_DOWN, ImGuiKey_DownArrow }, { VK_PRIOR, ImGuiKey_PageUp },
		{ VK_NEXT, ImGuiKey_PageDown }, { VK_HOME, ImGuiKey_Home }, { VK_END, ImGuiKey_End },
		{ VK_INSERT, ImGuiKey_Insert }, { VK_DELETE, ImGuiKey_Delete }, { VK_BACK, ImGuiKey_Backspace },
		{ VK_SPACE, ImGuiKey_Space }, { VK_RETURN, ImGuiKey_Enter }, { VK_ESCAPE, ImGuiKey_Escape },
		{ 'A', ImGuiKey_A }, { 'C', ImGuiKey_C }, { 'V', ImGuiKey_V }, { 'X', ImGuiKey_X },
		{ 'Y', ImGuiKey_Y }, { 'Z', ImGuiKey_Z },
		{ VK_LCONTROL, ImGuiKey_LeftCtrl }, { VK_RCONTROL, ImGuiKey_RightCtrl },
		{ VK_LSHIFT, ImGuiKey_LeftShift }, { VK_RSHIFT, ImGuiKey_RightShift },
		{ VK_LMENU, ImGuiKey_LeftAlt }, { VK_RMENU, ImGuiKey_RightAlt },
	};

	// ------------------------------------------------------------------ window procedure

	static UINT KeyboardCodePage() {
		char buf[16] = {};
		LCID lcid = MAKELCID(LOWORD((DWORD_PTR)GetKeyboardLayout(0)), SORT_DEFAULT);
		if (GetLocaleInfoA(lcid, LOCALE_IDEFAULTANSICODEPAGE, buf, (int)sizeof(buf)) > 0) {
			UINT cp = (UINT)atoi(buf);
			if (cp)
				return cp;
		}
		return CP_ACP;
	}

	static LRESULT CALLBACK HookedWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
		if (g_contextReady && !g_faulted) {
			const int toggleVk = Settings::Get().toggleKey;
			// F10 would otherwise activate the (non-existent) window menu
			if ((msg == WM_SYSKEYDOWN || msg == WM_SYSKEYUP || msg == WM_KEYDOWN || msg == WM_KEYUP) && (int)wp == toggleVk) {
				if (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN)
					g_hotkey.KeyDown((lp & (1L << 30)) != 0);
				else
					g_hotkey.KeyUp();
				return 0;
			}

			if (g_visible) {
				ImGuiIO& io = ImGui::GetIO();
				switch (msg) {
					case WM_CHAR: {
						if (IsWindowUnicode(hwnd)) {
							if (wp > 0 && wp < 0x10000)
								io.AddInputCharacterUTF16((ImWchar16)wp);
						}
						else {
							char c = (char)wp;
							wchar_t wc = 0;
							if (MultiByteToWideChar(KeyboardCodePage(), 0, &c, 1, &wc, 1) == 1)
								io.AddInputCharacterUTF16((ImWchar16)wc);
						}
						return 0;
					}
					case WM_KEYDOWN: {
						// The game's message loop might not call TranslateMessage. If no
						// WM_CHAR is waiting in the queue, translate the key ourselves.
						MSG pending;
						if (!PeekMessageA(&pending, hwnd, WM_CHAR, WM_DEADCHAR, PM_NOREMOVE)) {
							BYTE state[256];
							if (GetKeyboardState(state)) {
								const bool ctrl = (state[VK_CONTROL] & 0x80) != 0;
								const bool alt = (state[VK_MENU] & 0x80) != 0;
								if (!(ctrl && !alt)) {
									wchar_t chars[8];
									UINT scan = (UINT)((lp >> 16) & 0xFF);
									int n = ToUnicodeEx((UINT)wp, scan, state, chars, 8, 0, GetKeyboardLayout(0));
									for (int i = 0; i < n; i++)
										if (chars[i] >= 32)
											io.AddInputCharacterUTF16((ImWchar16)chars[i]);
								}
							}
						}
						return 0;
					}
					case WM_KEYUP:
					case WM_DEADCHAR:
						return 0;
					case WM_MOUSEWHEEL:
						g_wheelAccum += (float)GET_WHEEL_DELTA_WPARAM(wp) / (float)WHEEL_DELTA;
						return 0;
				}
			}
		}
		return CallWindowProcA(g_origWndProc, hwnd, msg, wp, lp);
	}

	static void InstallWndProc(HWND window) {
		if (g_window == window)
			return;
		if (g_window && g_origWndProc && IsWindow(g_window))
			SetWindowLongPtrA(g_window, GWLP_WNDPROC, (LONG_PTR)g_origWndProc);
		g_window = window;
		g_origWndProc = (WNDPROC)SetWindowLongPtrA(window, GWLP_WNDPROC, (LONG_PTR)&HookedWndProc);
		Log::Write("Window procedure hooked (unicode=%d)", IsWindowUnicode(window) ? 1 : 0);
	}

	// ------------------------------------------------------------------ fonts & style

	static ImFont* TryAddFont(const std::string& path, float px, const ImFontConfig* cfg, const ImWchar* ranges) {
		if (!Paths::FileExists(path))
			return nullptr;
		return ImGui::GetIO().Fonts->AddFontFromFileTTF(path.c_str(), px, cfg, ranges);
	}

	static float AutoFontPixels(float screenHeight) {
		float px = std::floor(screenHeight / 54.0f + 0.5f);
		if (px < 15.0f) px = 15.0f;
		if (px > 30.0f) px = 30.0f;
		return px;
	}

	static void ApplyStyle(float px) {
		ImGuiStyle& st = ImGui::GetStyle();
		st = ImGuiStyle();
		ImGui::StyleColorsDark(&st);

		st.WindowRounding = 8.0f;
		st.ChildRounding = 6.0f;
		st.FrameRounding = 5.0f;
		st.PopupRounding = 6.0f;
		st.ScrollbarRounding = 8.0f;
		st.GrabRounding = 4.0f;
		st.TabRounding = 5.0f;
		st.WindowPadding = ImVec2(12.0f, 10.0f);
		st.FramePadding = ImVec2(8.0f, 4.0f);
		st.ItemSpacing = ImVec2(8.0f, 6.0f);
		st.ItemInnerSpacing = ImVec2(6.0f, 4.0f);
		st.CellPadding = ImVec2(6.0f, 3.0f);
		st.ScrollbarSize = 14.0f;
		st.GrabMinSize = 12.0f;
		st.WindowBorderSize = 1.0f;
		st.FrameBorderSize = 0.0f;
		st.TabBorderSize = 0.0f;
		st.WindowTitleAlign = ImVec2(0.5f, 0.5f);
		st.SeparatorTextBorderSize = 2.0f;

		ImVec4* c = st.Colors;
		const float alpha = Settings::Get().windowAlpha;
		c[ImGuiCol_Text]                 = ImVec4(0.93f, 0.88f, 0.78f, 1.00f);
		c[ImGuiCol_TextDisabled]         = ImVec4(0.60f, 0.55f, 0.47f, 1.00f);
		c[ImGuiCol_WindowBg]             = ImVec4(0.10f, 0.08f, 0.06f, alpha);
		c[ImGuiCol_ChildBg]              = ImVec4(0.13f, 0.11f, 0.08f, 0.55f);
		c[ImGuiCol_PopupBg]              = ImVec4(0.12f, 0.10f, 0.07f, 0.98f);
		c[ImGuiCol_Border]               = ImVec4(0.42f, 0.32f, 0.18f, 0.65f);
		c[ImGuiCol_BorderShadow]         = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
		c[ImGuiCol_FrameBg]              = ImVec4(0.19f, 0.15f, 0.11f, 1.00f);
		c[ImGuiCol_FrameBgHovered]       = ImVec4(0.27f, 0.21f, 0.14f, 1.00f);
		c[ImGuiCol_FrameBgActive]        = ImVec4(0.33f, 0.25f, 0.16f, 1.00f);
		c[ImGuiCol_TitleBg]              = ImVec4(0.14f, 0.11f, 0.08f, 1.00f);
		c[ImGuiCol_TitleBgActive]        = ImVec4(0.27f, 0.19f, 0.09f, 1.00f);
		c[ImGuiCol_TitleBgCollapsed]     = ImVec4(0.14f, 0.11f, 0.08f, 0.85f);
		c[ImGuiCol_MenuBarBg]            = ImVec4(0.14f, 0.11f, 0.08f, 1.00f);
		c[ImGuiCol_ScrollbarBg]          = ImVec4(0.07f, 0.06f, 0.04f, 0.60f);
		c[ImGuiCol_ScrollbarGrab]        = ImVec4(0.38f, 0.29f, 0.17f, 1.00f);
		c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.50f, 0.38f, 0.21f, 1.00f);
		c[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.64f, 0.48f, 0.24f, 1.00f);
		c[ImGuiCol_CheckMark]            = ImVec4(0.96f, 0.74f, 0.31f, 1.00f);
		c[ImGuiCol_SliderGrab]           = ImVec4(0.84f, 0.63f, 0.27f, 1.00f);
		c[ImGuiCol_SliderGrabActive]     = ImVec4(0.96f, 0.74f, 0.31f, 1.00f);
		c[ImGuiCol_Button]               = ImVec4(0.34f, 0.25f, 0.14f, 1.00f);
		c[ImGuiCol_ButtonHovered]        = ImVec4(0.48f, 0.35f, 0.18f, 1.00f);
		c[ImGuiCol_ButtonActive]         = ImVec4(0.60f, 0.44f, 0.20f, 1.00f);
		c[ImGuiCol_Header]               = ImVec4(0.34f, 0.25f, 0.14f, 0.75f);
		c[ImGuiCol_HeaderHovered]        = ImVec4(0.48f, 0.35f, 0.18f, 0.90f);
		c[ImGuiCol_HeaderActive]         = ImVec4(0.60f, 0.44f, 0.20f, 1.00f);
		c[ImGuiCol_Separator]            = ImVec4(0.42f, 0.32f, 0.18f, 0.55f);
		c[ImGuiCol_SeparatorHovered]     = ImVec4(0.64f, 0.48f, 0.24f, 0.80f);
		c[ImGuiCol_SeparatorActive]      = ImVec4(0.84f, 0.63f, 0.27f, 1.00f);
		c[ImGuiCol_ResizeGrip]           = ImVec4(0.48f, 0.35f, 0.18f, 0.35f);
		c[ImGuiCol_ResizeGripHovered]    = ImVec4(0.64f, 0.48f, 0.24f, 0.70f);
		c[ImGuiCol_ResizeGripActive]     = ImVec4(0.84f, 0.63f, 0.27f, 0.95f);
		c[ImGuiCol_Tab]                  = ImVec4(0.20f, 0.15f, 0.10f, 1.00f);
		c[ImGuiCol_TabHovered]           = ImVec4(0.48f, 0.35f, 0.18f, 1.00f);
		c[ImGuiCol_TabSelected]          = ImVec4(0.38f, 0.27f, 0.14f, 1.00f);
		c[ImGuiCol_TabSelectedOverline]  = ImVec4(0.96f, 0.74f, 0.31f, 1.00f);
		c[ImGuiCol_TabDimmed]            = ImVec4(0.16f, 0.12f, 0.09f, 1.00f);
		c[ImGuiCol_TabDimmedSelected]    = ImVec4(0.28f, 0.21f, 0.12f, 1.00f);
		c[ImGuiCol_TableHeaderBg]        = ImVec4(0.20f, 0.15f, 0.10f, 1.00f);
		c[ImGuiCol_TableBorderStrong]    = ImVec4(0.42f, 0.32f, 0.18f, 0.80f);
		c[ImGuiCol_TableBorderLight]     = ImVec4(0.30f, 0.23f, 0.14f, 0.60f);
		c[ImGuiCol_TableRowBg]           = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
		c[ImGuiCol_TableRowBgAlt]        = ImVec4(1.00f, 0.90f, 0.70f, 0.04f);
		c[ImGuiCol_TextSelectedBg]       = ImVec4(0.84f, 0.63f, 0.27f, 0.35f);
		c[ImGuiCol_NavCursor]            = ImVec4(0.96f, 0.74f, 0.31f, 0.90f);
		c[ImGuiCol_PlotHistogram]        = ImVec4(0.84f, 0.63f, 0.27f, 1.00f);
		c[ImGuiCol_ModalWindowDimBg]     = ImVec4(0.00f, 0.00f, 0.00f, 0.55f);

		st.ScaleAllSizes(px / 16.0f);
	}

	static void BuildFonts(float screenHeight) {
		ImGuiIO& io = ImGui::GetIO();
		io.Fonts->Clear();
		g_titleFont = nullptr;

		float px = (float)Settings::Get().fontSize;
		if (px <= 0.0f)
			px = AutoFontPixels(screenHeight);
		g_fontPx = px;

		// Kept small on purpose: DirectX 7 drivers may refuse huge textures.
		static const ImWchar textRanges[] = {
			0x0020, 0x00FF, // Basic Latin + Latin-1
			0x0100, 0x017F, // Latin Extended-A (č ď ě ň ř š ť ů ž ...)
			0x0400, 0x045F, // Cyrillic (ruské mody)
			0x2010, 0x2027, // – — ‘ ’ „ “ • …
			0x20AC, 0x20AC, // €
			0x2190, 0x2193, // ← ↑ → ↓
			0x221E, 0x221E, // ∞
			0,
		};
		static const ImWchar titleRanges[] = {
			0x0020, 0x00FF,
			0x0100, 0x017F,
			0x0400, 0x045F,
			0x2010, 0x2027,
			0,
		};
		static const ImWchar symbolRanges[] = {
			0x2190, 0x2193, // arrows
			0x25B2, 0x25BC, // ▲ ▼
			0x2605, 0x2606, // ★ ☆
			0x2713, 0x2718, // ✓ ✔ ✕ ✖ ✗ ✘
			0,
		};

		const std::string dir = Paths::FontsDir();
		ImFontConfig cfg;
		cfg.OversampleH = 2;
		cfg.OversampleV = 1;
		cfg.PixelSnapH = true;

		ImFont* mainFont = TryAddFont(dir + "\\segoeui.ttf", px, &cfg, textRanges);
		if (!mainFont) mainFont = TryAddFont(dir + "\\tahoma.ttf", px, &cfg, textRanges);
		if (!mainFont) mainFont = TryAddFont(dir + "\\arial.ttf", px, &cfg, textRanges);

		if (mainFont) {
			ImFontConfig merge;
			merge.MergeMode = true;
			merge.PixelSnapH = true;
			merge.OversampleH = 2;
			merge.OversampleV = 1;
			TryAddFont(dir + "\\seguisym.ttf", px, &merge, symbolRanges);
		}
		else {
			Log::Write("No system TTF font found, using the built-in font (no diacritics)");
			ImFontConfig def;
			def.SizePixels = px;
			mainFont = io.Fonts->AddFontDefault(&def);
		}

		ImFontConfig titleCfg;
		titleCfg.OversampleH = 1;
		titleCfg.OversampleV = 1;
		titleCfg.PixelSnapH = true;
		g_titleFont = TryAddFont(dir + "\\seguisb.ttf", px * 1.3f, &titleCfg, titleRanges);
		if (!g_titleFont)
			g_titleFont = TryAddFont(dir + "\\segoeuib.ttf", px * 1.3f, &titleCfg, titleRanges);
		if (!g_titleFont)
			g_titleFont = mainFont;

		io.FontDefault = mainFont;
		io.Fonts->TexDesiredWidth = 1024;
		io.Fonts->Build();
		ApplyStyle(px);
		Log::Write("Fonts built: %.0f px", px);
	}

	// ------------------------------------------------------------------ context

	static bool CreateContextOnce() {
		if (g_contextReady)
			return true;
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO();
			g_iniPath = Paths::DataDir() + "\\GothicTrainer_okna.ini";
		io.IniFilename = g_iniPath.c_str();
		io.LogFilename = nullptr;
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
		io.BackendPlatformName = "gothic_win32";
		io.MouseDrawCursor = false;
		io.ConfigWindowsMoveFromTitleBarOnly = true;
		io.ConfigErrorRecoveryEnableAssert = false;
		g_contextReady = true;
		Log::Write("ImGui %s context created", IMGUI_VERSION);
		return true;
	}

	static bool RenderTargetSize(IDirect3DDevice7* device, float& w, float& h) {
		IDirectDrawSurface7* rt = nullptr;
		if (FAILED(device->GetRenderTarget(&rt)) || !rt)
			return false;
		DDSURFACEDESC2 desc;
		ZeroMemory(&desc, sizeof(desc));
		desc.dwSize = sizeof(desc);
		bool ok = SUCCEEDED(rt->GetSurfaceDesc(&desc));
		rt->Release();
		if (!ok || desc.dwWidth == 0 || desc.dwHeight == 0)
			return false;
		w = (float)desc.dwWidth;
		h = (float)desc.dwHeight;
		return true;
	}

	// ------------------------------------------------------------------ public API

	bool IsVisible() {
		return g_visible;
	}

	void SetVisible(bool visible) {
		if (visible == g_visible)
			return;
		g_visible = visible;
		Log::Write("Trainer menu %s", visible ? "opened" : "closed");
		if (visible) {
			g_guard = false;
			g_mouse = ImVec2(-1, -1);
			g_seenDirectInputMouse = false;
		}
		else {
			g_guard = true;
			g_guardStart = Now();
			g_clearInputPending = true; // done before the next frame, never mid-frame
		}
	}

	bool CapturesInput() {
		return !g_faulted && (g_visible || g_guard);
	}

	bool TakeJustReleased() {
		bool r = g_justReleased;
		g_justReleased = false;
		return r;
	}

	void Toast(const std::string& text) {
		if (!Settings::Get().showToasts)
			return;
		g_toasts.push_back({ text, Now() + 2.6 });
		if (g_toasts.size() > 6)
			g_toasts.erase(g_toasts.begin());
	}

	void RequestReload() {
		g_fontsDirty = true;
	}

	void Fault(const char* reason) {
		if (!g_faulted)
			Log::Write("Overlay disabled: %s", reason ? reason : "?");
		g_faulted = true;
		g_visible = false;
		g_guard = false;
		g_justReleased = true;
	}

	bool IsFaulted() {
		return g_faulted;
	}

	float FontPixels() {
		return g_fontPx;
	}

	ImFont* TitleFont() {
		return g_titleFont;
	}

	static void UpdateGuard(const PlatformInput& in, bool foreground, double now) {
		if (!g_guard)
			return;
		bool anyDown = in.lmb || in.rmb || in.mmb || in.toggleDown;
		if (foreground) {
			anyDown = anyDown || AsyncDown(Settings::Get().toggleKey) || AsyncDown(VK_ESCAPE) || AsyncDown(VK_RETURN)
				|| AsyncDown(VK_LBUTTON) || AsyncDown(VK_RBUTTON);
		}
		if (!anyDown && now - g_guardStart > 0.12) {
			g_guard = false;
			g_justReleased = true;
		}
	}

	static void FeedInput(const PlatformInput& in, HWND window, bool foreground) {
		ImGuiIO& io = ImGui::GetIO();
		const TrainerSettings& s = Settings::Get();
		io.AddFocusEvent(foreground);

		if (!g_visible || !foreground) {
			io.MouseDrawCursor = false;
			io.AddMousePosEvent(-FLT_MAX, -FLT_MAX);
			g_wheelAccum = 0.0f;
			return;
		}
		io.MouseDrawCursor = false; // explicit foreground cursor below

		if (g_mouse.x < 0.0f || g_mouse.y < 0.0f)
			g_mouse = ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f);

		// Where the cursor movement comes from: the game's DirectInput mouse
		// (exclusive, Windows cursor frozen) or the Windows cursor (mouse
		// disabled in the game options, windowed mode). Detected automatically.
		POINT sysPos = { 0, 0 };
		RECT rc = { 0, 0, 0, 0 };
		bool haveSys = GetCursorPos(&sysPos) && ScreenToClient(window, &sysPos) && GetClientRect(window, &rc);
		const bool sysMoved = haveSys && (sysPos.x != g_lastSysCursor.x || sysPos.y != g_lastSysCursor.y);
		if (haveSys)
			g_lastSysCursor = sysPos;
		if (in.mouseDX != 0.0f || in.mouseDY != 0.0f)
			g_seenDirectInputMouse = true;

		const bool useSystem = s.systemCursor;
		if (useSystem) {
			if (haveSys && (sysMoved || s.systemCursor)) {
				float cw = (float)ImMax(1L, rc.right - rc.left);
				float ch = (float)ImMax(1L, rc.bottom - rc.top);
				g_mouse = ImVec2((float)sysPos.x * io.DisplaySize.x / cw, (float)sysPos.y * io.DisplaySize.y / ch);
			}
		}
		else {
			g_mouse.x += in.mouseDX * s.mouseSpeed;
			g_mouse.y += in.mouseDY * s.mouseSpeed;
		}
		g_mouse.x = ImClamp(g_mouse.x, 0.0f, io.DisplaySize.x - 1.0f);
		g_mouse.y = ImClamp(g_mouse.y, 0.0f, io.DisplaySize.y - 1.0f);
		io.AddMousePosEvent(g_mouse.x, g_mouse.y);

		const bool l = in.lmb || (foreground && AsyncDown(VK_LBUTTON));
		const bool r = in.rmb || (foreground && AsyncDown(VK_RBUTTON));
		const bool m = in.mmb || (foreground && AsyncDown(VK_MBUTTON));
		io.AddMouseButtonEvent(0, l);
		io.AddMouseButtonEvent(1, r);
		io.AddMouseButtonEvent(2, m);

		float wheel = 0.0f;
		if (!useSystem && in.wheel != 0.0f) {
			wheel = in.wheel;
			if (std::fabs(wheel) >= 30.0f)
				wheel /= 120.0f;
		}
		else {
			wheel = g_wheelAccum;
		}
		g_wheelAccum = 0.0f;
		if (wheel != 0.0f)
			io.AddMouseWheelEvent(0.0f, ImClamp(wheel, -6.0f, 6.0f));

		for (const NavKey& k : kNavKeys)
			io.AddKeyEvent(k.key, foreground && AsyncDown(k.vk));
		io.AddKeyEvent(ImGuiMod_Ctrl, foreground && AsyncDown(VK_CONTROL));
		io.AddKeyEvent(ImGuiMod_Shift, foreground && AsyncDown(VK_SHIFT));
		io.AddKeyEvent(ImGuiMod_Alt, foreground && AsyncDown(VK_MENU));
	}

	static void DrawToasts(double now) {
		if (g_toasts.empty())
			return;
		ImGuiIO& io = ImGui::GetIO();
		ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.07f), ImGuiCond_Always, ImVec2(0.5f, 0.0f));
		ImGui::SetNextWindowBgAlpha(0.88f);
		ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoInputs
			| ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove;
		if (ImGui::Begin("##gt_toasts", nullptr, flags)) {
			for (const ToastItem& t : g_toasts) {
				float left = (float)(t.until - now);
				float a = ImClamp(left / 0.4f, 0.0f, 1.0f);
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.98f, 0.86f, 0.55f, a));
				ImGui::TextUnformatted(t.text.c_str());
				ImGui::PopStyleColor();
			}
		}
		ImGui::End();
	}

	static void CallDraw(void* ctx) {
		DrawFn fn = (DrawFn)ctx;
		fn();
	}

	void OnPresent(IDirect3DDevice7* device, IDirectDraw7* ddraw, HWND__* window, const PlatformInput& in, DrawFn draw) {
		if (g_faulted || !device || !ddraw || !window)
			return;
		if (!CreateContextOnce())
			return;

		InstallWndProc(window);

		const double now = Now();
		const bool foreground = GetForegroundWindow() == window;

		// open / close
		const int vk = Settings::Get().toggleKey;
		if (vk != g_hotkeyVk || !foreground) {
			g_hotkey.Reset();
			g_hotkeyVk = vk;
		}
		if (foreground && g_hotkey.Update(AsyncDown(vk) || in.toggleDown))
			SetVisible(!g_visible);

		UpdateGuard(in, foreground, now);

		while (!g_toasts.empty() && g_toasts.front().until < now)
			g_toasts.erase(g_toasts.begin());

		if (!g_visible && g_toasts.empty()) {
			g_lastTime = now;
			return;
		}

		ImGuiIO& io = ImGui::GetIO();
		float w = 0.0f, h = 0.0f;
		if (!RenderTargetSize(device, w, h)) {
			RECT rc;
			GetClientRect(window, &rc);
			w = (float)ImMax(1L, rc.right - rc.left);
			h = (float)ImMax(1L, rc.bottom - rc.top);
		}
		io.DisplaySize = ImVec2(w, h);
		if (Settings::Get().fontSize == 0 && AutoFontPixels(h) != g_fontPx)
			g_fontsDirty = true;

		if (device != g_device || ddraw != g_ddraw) {
			ImGuiDX7::Shutdown();
			ImGuiDX7::Init(device, ddraw);
			g_device = device;
			g_ddraw = ddraw;
			g_textureDirty = true;
			Log::Write("Direct3D device %p, screen %.0fx%.0f", (void*)device, w, h);
		}

		if (g_fontsDirty) {
			ImGuiDX7::DestroyFontsTexture();
			BuildFonts(h);
			g_fontsDirty = false;
			g_textureDirty = true;
		}
		if (g_textureDirty) {
			if (!ImGuiDX7::CreateFontsTexture()) {
				Fault("font texture could not be created");
				return;
			}
			g_textureDirty = false;
		}

		double dt = now - g_lastTime;
		if (dt <= 0.0 || dt > 0.25)
			dt = 1.0 / 60.0;
		io.DeltaTime = (float)dt;
		g_lastTime = now;

		if (g_clearInputPending) {
			io.ClearInputKeys();
			io.ClearInputMouse();
			g_clearInputPending = false;
		}

		FeedInput(in, window, foreground);

		ImGui::NewFrame();
		const bool itemWasActive = ImGui::IsAnyItemActive();

		if (g_visible && draw) {
			ImGuiErrorRecoveryState recovery;
			ImGui::ErrorRecoveryStoreState(&recovery);
			unsigned long code = 0;
			if (!SehInvoke(&CallDraw, (void*)draw, &code)) {
				static int reported = 0;
				ImGui::ErrorRecoveryTryToRecoverState(&recovery);
				if (reported < 20) {
					reported++;
					Log::Write("Exception 0x%08lX while drawing the trainer window", code);
					Toast(u8"Chyba při vykreslování (podrobnosti v GothicTrainer.log)");
				}
			}

			// ESC closes the menu unless it was meant for a text field or a popup
			if (g_visible && ImGui::IsKeyPressed(ImGuiKey_Escape, false) && !itemWasActive && !ImGui::IsAnyItemActive()
				&& !ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId))
				SetVisible(false);
		}

		DrawToasts(now);
		if (g_visible && foreground && g_mouse.x >= 0 && g_mouse.y >= 0) {
			DrawPointer(ImGui::GetForegroundDrawList(),g_mouse);
		}

		ImGui::Render();
		ImGuiDX7::RenderDrawData(ImGui::GetDrawData());
	}

}}
