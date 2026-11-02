#pragma once
#include <string>

struct IDirect3DDevice7;
struct IDirectDraw7;
struct HWND__;
struct ImFont;

namespace GT { namespace Overlay {

	// Input gathered on the engine side (DirectInput through the game's zCInput).
	struct PlatformInput {
		float mouseDX = 0.0f;
		float mouseDY = 0.0f;
		float wheel = 0.0f;
		bool  lmb = false;
		bool  rmb = false;
		bool  mmb = false;
		bool  toggleDown = false;   // toggle key seen through DirectInput
	};

	typedef void (*DrawFn)();

	// Called right before the game presents a frame.
	void OnPresent(IDirect3DDevice7* device, IDirectDraw7* ddraw, HWND__* window, const PlatformInput& input, DrawFn draw);

	bool IsVisible();
	void SetVisible(bool visible);

	// True while the menu is open, and shortly after closing it until all
	// keys/buttons are released, so the game never sees our clicks or ESC.
	bool CapturesInput();

	// Returns true once, right after input capture ended.
	bool TakeJustReleased();

	// Short message in the corner of the screen (UTF-8).
	void Toast(const std::string& text);

	// Rebuild fonts/style after a settings change.
	void RequestReload();

	// Disables the overlay after an unrecoverable error.
	void Fault(const char* reason);
	bool IsFaulted();

	float FontPixels();
	ImFont* TitleFont();

	// DirectInput key code for a virtual key (for the toggle key fallback).
	int DikForVk(int vk);
	const char* KeyName(int vk);
}}
