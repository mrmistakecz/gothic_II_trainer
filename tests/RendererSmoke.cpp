// Runs the real overlay renderer on an isolated DirectX 7 render target.
// No game, saved game, or desktop input is involved.
#define INITGUID
#include <windows.h>
#include <ZenGin/DirectX7/include/d3d.h>
#include <cstdio>
#include "imgui.h"
#include "Core/Overlay.h"
#include "Core/Settings.h"
#include "Core/Log.h"
#include "Core/Pointer.h"

namespace {
	void DrawSmokeWindow() {
		ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_Always);
		ImGui::SetNextWindowSize(ImVec2(600, 400), ImGuiCond_Always);
		ImGui::Begin("Gothic Trainer - renderer regression");
		ImGui::TextUnformatted(u8"Příšerně žluťoučký kůň - české písmo");
		ImGui::Button(u8"Předměty a postavy");
		ImGui::BeginChild("clipped", ImVec2(300, 200), ImGuiChildFlags_Borders);
		for (int i = 0; i < 60; ++i)
			ImGui::Text("Clipped row %d", i);
		ImGui::EndChild();
		ImGui::End();
		GT::DrawPointer(ImGui::GetForegroundDrawList(), ImVec2(615,440));
	}
}

int RendererSmokeTest() {
	GT::Log::Init();
	HMODULE library = LoadLibraryA("ddraw.dll");
	if (!library) { std::puts("SKIP: DirectDraw unavailable."); return 77; }
	using CreateFn = HRESULT (WINAPI*)(GUID*, void**, REFIID, IUnknown*);
	auto create = reinterpret_cast<CreateFn>(GetProcAddress(library, "DirectDrawCreateEx"));
	if (!create) { std::puts("SKIP: DirectDraw 7 unavailable."); return 77; }
	WNDCLASSA wc = {};
	wc.lpfnWndProc = DefWindowProcA;
	wc.hInstance = GetModuleHandleA(nullptr);
	wc.lpszClassName = "GothicTrainerRendererTest";
	RegisterClassA(&wc);
	HWND window = CreateWindowA(wc.lpszClassName, "Gothic Trainer renderer test", WS_POPUP, 0, 0, 640, 480, nullptr, nullptr, wc.hInstance, nullptr);
	IDirectDraw7* ddraw = nullptr;
	HRESULT hr = create(nullptr, reinterpret_cast<void**>(&ddraw), IID_IDirectDraw7, nullptr);
	if (FAILED(hr) || !ddraw || FAILED(ddraw->SetCooperativeLevel(window, DDSCL_NORMAL))) {
		std::puts("SKIP: DirectDraw cooperative mode unavailable."); return 77;
	}
	IDirect3D7* d3d = nullptr;
	if (FAILED(ddraw->QueryInterface(IID_IDirect3D7, reinterpret_cast<void**>(&d3d)))) {
		std::puts("SKIP: Direct3D 7 unavailable."); return 77;
	}
	DDSURFACEDESC2 desc = {};
	desc.dwSize = sizeof(desc);
	desc.dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT | DDSD_PIXELFORMAT;
	desc.dwWidth = 640; desc.dwHeight = 480;
	desc.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_3DDEVICE | DDSCAPS_SYSTEMMEMORY;
	desc.ddpfPixelFormat.dwSize = sizeof(DDPIXELFORMAT);
	desc.ddpfPixelFormat.dwFlags = DDPF_RGB;
	desc.ddpfPixelFormat.dwRGBBitCount = 32;
	desc.ddpfPixelFormat.dwRBitMask = 0xFF0000;
	desc.ddpfPixelFormat.dwGBitMask = 0x00FF00;
	desc.ddpfPixelFormat.dwBBitMask = 0x0000FF;
	IDirectDrawSurface7* target = nullptr;
	IDirect3DDevice7* device = nullptr;
	hr = ddraw->CreateSurface(&desc, &target, nullptr);
	if (SUCCEEDED(hr)) hr = d3d->CreateDevice(IID_IDirect3DRGBDevice, target, &device);
	if (FAILED(hr) || !device) {
		if (target) { target->Release(); target = nullptr; }
		desc.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_3DDEVICE | DDSCAPS_VIDEOMEMORY;
		hr = ddraw->CreateSurface(&desc, &target, nullptr);
		if (SUCCEEDED(hr)) hr = d3d->CreateDevice(IID_IDirect3DHALDevice, target, &device);
	}
	if (FAILED(hr) || !device) {
		std::printf("SKIP: Direct3D 7 device unavailable (0x%08lX).\n", (unsigned long)hr); return 77;
	}
	// A device can be created on an isolated Windows desktop even when its
	// driver cannot draw there. Probe that separately from the overlay.
	D3DTLVERTEX triangle[3] = {};
	triangle[0].sx = 10; triangle[0].sy = 10;
	triangle[1].sx = 100; triangle[1].sy = 10;
	triangle[2].sx = 10; triangle[2].sy = 100;
	for (auto& vertex : triangle) { vertex.rhw = 1; vertex.color = 0xFFFFFFFF; }
	device->BeginScene();
	hr = device->DrawPrimitive(D3DPT_TRIANGLELIST, D3DFVF_TLVERTEX, triangle, 3, 0);
	device->EndScene();
	if (hr == DDERR_NODIRECTDRAWHW) {
		std::puts("SKIP: Windows desktop has no DirectDraw rasterization (control triangle also fails).");
		return 77;
	}
	if (FAILED(hr)) { std::printf("SKIP: control triangle failed (0x%08lX).\n", (unsigned long)hr); return 77; }
	device->Clear(0, nullptr, D3DCLEAR_TARGET, 0xFF020304, 1.0f, 0);
	device->SetRenderState(D3DRENDERSTATE_FOGENABLE, TRUE);
	D3DVIEWPORT7 viewport = { 5, 6, 100, 90, 0.0f, 1.0f };
	device->SetViewport(&viewport);
	GT::Settings::Get().fontSize = 18;
	GT::Overlay::SetVisible(true);
	GT::Overlay::OnPresent(device, ddraw, window, GT::Overlay::PlatformInput{}, &DrawSmokeWindow);
	GT::Overlay::OnPresent(device, ddraw, window, GT::Overlay::PlatformInput{}, &DrawSmokeWindow);
	std::printf("Draw data: %d lists, %d vertices, visible=%d.\n", ImGui::GetDrawData()->CmdListsCount, ImGui::GetDrawData()->TotalVtxCount, GT::Overlay::IsVisible());
	DWORD fog = 0;
	D3DVIEWPORT7 after = {};
	device->GetRenderState(D3DRENDERSTATE_FOGENABLE, &fog);
	device->GetViewport(&after);
	if (GT::Overlay::IsFaulted() || fog != TRUE || after.dwX != 5 || after.dwY != 6 || after.dwWidth != 100 || after.dwHeight != 90) {
		std::puts("FAIL: overlay changed the engine's render state."); return 1;
	}
	DDSURFACEDESC2 pixels = {};
	pixels.dwSize = sizeof(pixels);
	if (FAILED(target->Lock(nullptr, &pixels, DDLOCK_WAIT | DDLOCK_READONLY, nullptr))) {
		std::puts("FAIL: cannot read rendered pixels."); return 1;
	}
	size_t changed = 0;
	size_t pointerPixels = 0;
	DWORD cursorExample = 0;
	for (DWORD y = 0; y < pixels.dwHeight; ++y) {
		const auto* row = reinterpret_cast<const DWORD*>(static_cast<const char*>(pixels.lpSurface) + y * pixels.lPitch);
		for (DWORD x = 0; x < pixels.dwWidth; ++x) {
			if ((row[x] & 0xFFFFFF) != 0x020304) ++changed;
			// DX7 texture filtering may darken the atlas white texel slightly.
			if(x>=615 && x<=633 && y>=440 && y<=464 && ((row[x]>>16)&255)>=240 && ((row[x]>>8)&255)>=200 && (row[x]&255)>=100 && (row[x]&255)<=170)++pointerPixels;
			if(x>=615 && x<=633 && y>=440 && y<=464 && ((row[x]>>16)&255)>200 && ((row[x]>>8)&255)>120)cursorExample=row[x];
		}
	}
	target->Unlock(nullptr);
	if(pointerPixels<30){std::printf("FAIL: cursor invisible (%zu gold pixels, sample=%08lX, total=%zu).\n",pointerPixels,(unsigned long)cursorExample,changed);return 1;}
	std::printf("PASS: cursor visible, %zu gold pixels outside the menu.\n",pointerPixels);
	if (changed < 1000) { std::printf("FAIL: overlay did not render visible pixels (%zu changed).\n", changed); return 1; }
	std::printf("PASS: real DirectX 7 overlay rendered %zu pixels; game render state restored.\n", changed);
	return 0;
}
