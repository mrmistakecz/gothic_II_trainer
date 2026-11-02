#pragma once

struct IDirect3DDevice7;
struct IDirectDraw7;
struct ImDrawData;

// Dear ImGui renderer for the game's own Direct3D 7 device.
// Every render state we touch is saved and restored, because the engine
// caches device states and would get out of sync otherwise.
namespace GT { namespace ImGuiDX7 {
	bool Init(IDirect3DDevice7* device, IDirectDraw7* ddraw);
	void Shutdown();
	bool CreateFontsTexture();
	void DestroyFontsTexture();
	void RenderDrawData(ImDrawData* drawData);
}}
