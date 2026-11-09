#include <windows.h>
#include <ZenGin/DirectX7/include/d3d.h>
#include "imgui.h"
#include "imgui_internal.h"
#include "ImGuiDX7.h"
#include "Log.h"

namespace GT { namespace ImGuiDX7 {

	struct Vertex {
		float x, y, z, rhw;
		DWORD color;
		float u, v;
	};

	static const DWORD kFVF = D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1;
	static const int kMaxBatch = 65535; // D3DMAXNUMVERTICES, divisible by 3

	static IDirect3DDevice7*    g_device  = nullptr;
	static IDirectDraw7*        g_ddraw   = nullptr;
	static IDirectDrawSurface7* g_fontTex = nullptr;
	static ImVector<Vertex>     g_batch;

	// ---------------------------------------------------------------- textures

	static IDirectDrawSurface7* CreateTexture(int w, int h, bool use16Bit, bool managed) {
		DDSURFACEDESC2 desc;
		ZeroMemory(&desc, sizeof(desc));
		desc.dwSize = sizeof(desc);
		desc.dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT | DDSD_PIXELFORMAT;
		desc.ddsCaps.dwCaps = DDSCAPS_TEXTURE;
		D3DDEVICEDESC7 caps = {};
		const bool software = g_device && SUCCEEDED(g_device->GetCaps(&caps)) && !(caps.dwDevCaps & D3DDEVCAPS_HWRASTERIZATION);
		if (software)
			desc.ddsCaps.dwCaps |= DDSCAPS_SYSTEMMEMORY;
		else if (managed)
			desc.ddsCaps.dwCaps2 = DDSCAPS2_TEXTUREMANAGE;
		else
			desc.ddsCaps.dwCaps |= DDSCAPS_VIDEOMEMORY;
		desc.dwWidth = (DWORD)w;
		desc.dwHeight = (DWORD)h;
		desc.ddpfPixelFormat.dwSize = sizeof(DDPIXELFORMAT);
		desc.ddpfPixelFormat.dwFlags = DDPF_RGB | DDPF_ALPHAPIXELS;
		if (!use16Bit) {
			desc.ddpfPixelFormat.dwRGBBitCount = 32;
			desc.ddpfPixelFormat.dwRBitMask = 0x00FF0000;
			desc.ddpfPixelFormat.dwGBitMask = 0x0000FF00;
			desc.ddpfPixelFormat.dwBBitMask = 0x000000FF;
			desc.ddpfPixelFormat.dwRGBAlphaBitMask = 0xFF000000;
		}
		else {
			desc.ddpfPixelFormat.dwRGBBitCount = 16;
			desc.ddpfPixelFormat.dwRBitMask = 0x0F00;
			desc.ddpfPixelFormat.dwGBitMask = 0x00F0;
			desc.ddpfPixelFormat.dwBBitMask = 0x000F;
			desc.ddpfPixelFormat.dwRGBAlphaBitMask = 0xF000;
		}

		IDirectDrawSurface7* surface = nullptr;
		HRESULT hr = g_ddraw->CreateSurface(&desc, &surface, nullptr);
		if (FAILED(hr)) {
			Log::Write("CreateSurface %dx%d %s %s failed: 0x%08X", w, h, use16Bit ? "16bit" : "32bit", managed ? "managed" : "vidmem", (unsigned)hr);
			return nullptr;
		}
		return surface;
	}

	static bool Upload(IDirectDrawSurface7* surface) {
		unsigned char* pixels = nullptr;
		int w = 0, h = 0;
		ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&pixels, &w, &h);
		if (!pixels)
			return false;

		DDSURFACEDESC2 desc;
		ZeroMemory(&desc, sizeof(desc));
		desc.dwSize = sizeof(desc);
		HRESULT hr = surface->Lock(nullptr, &desc, DDLOCK_WAIT | DDLOCK_WRITEONLY, nullptr);
		if (FAILED(hr)) {
			Log::Write("Font texture Lock failed: 0x%08X", (unsigned)hr);
			return false;
		}

		const bool is16 = desc.ddpfPixelFormat.dwRGBBitCount == 16;
		for (int y = 0; y < h; y++) {
			const unsigned char* src = pixels + (size_t)y * w * 4;
			unsigned char* row = (unsigned char*)desc.lpSurface + (size_t)y * desc.lPitch;
			if (!is16) {
				DWORD* dst = (DWORD*)row;
				for (int x = 0; x < w; x++, src += 4)
					dst[x] = ((DWORD)src[3] << 24) | ((DWORD)src[0] << 16) | ((DWORD)src[1] << 8) | (DWORD)src[2];
			}
			else {
				WORD* dst = (WORD*)row;
				for (int x = 0; x < w; x++, src += 4)
					dst[x] = (WORD)(((src[3] >> 4) << 12) | ((src[0] >> 4) << 8) | ((src[1] >> 4) << 4) | (src[2] >> 4));
			}
		}
		surface->Unlock(nullptr);
		return true;
	}

	bool CreateFontsTexture() {
		DestroyFontsTexture();
		if (!g_ddraw)
			return false;

		unsigned char* pixels = nullptr;
		int w = 0, h = 0;
		ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&pixels, &w, &h);

		IDirectDrawSurface7* tex = CreateTexture(w, h, false, true);
		if (!tex) tex = CreateTexture(w, h, true, true);
		if (!tex) tex = CreateTexture(w, h, false, false);
		if (!tex) tex = CreateTexture(w, h, true, false);
		if (!tex)
			return false;

		if (!Upload(tex)) {
			tex->Release();
			return false;
		}

		g_fontTex = tex;
		ImGui::GetIO().Fonts->SetTexID((ImTextureID)(intptr_t)g_fontTex);
		Log::Write("Font texture %dx%d ready", w, h);
		return true;
	}

	void DestroyFontsTexture() {
		if (g_fontTex) {
			g_fontTex->Release();
			g_fontTex = nullptr;
		}
		if (ImGui::GetCurrentContext())
			ImGui::GetIO().Fonts->SetTexID((ImTextureID)0);
	}

	bool Init(IDirect3DDevice7* device, IDirectDraw7* ddraw) {
		g_device = device;
		g_ddraw = ddraw;
		ImGuiIO& io = ImGui::GetIO();
		io.BackendRendererName = "gothic_dx7";
		io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;
		return g_device != nullptr && g_ddraw != nullptr;
	}

	void Shutdown() {
		// The font texture belongs to the old DirectDraw object; dropping it
		// after a device reset is safe, the engine owns the device itself.
		DestroyFontsTexture();
		g_device = nullptr;
		g_ddraw = nullptr;
	}

	// ---------------------------------------------------------------- clipping

	struct ClipVertex {
		float x, y, u, v;
		float r, g, b, a;
	};

	static inline ClipVertex ToClip(const ImDrawVert& v) {
		ClipVertex c;
		c.x = v.pos.x; c.y = v.pos.y; c.u = v.uv.x; c.v = v.uv.y;
		c.r = (float)((v.col >> IM_COL32_R_SHIFT) & 0xFF);
		c.g = (float)((v.col >> IM_COL32_G_SHIFT) & 0xFF);
		c.b = (float)((v.col >> IM_COL32_B_SHIFT) & 0xFF);
		c.a = (float)((v.col >> IM_COL32_A_SHIFT) & 0xFF);
		return c;
	}

	static inline ClipVertex Lerp(const ClipVertex& p, const ClipVertex& q, float t) {
		ClipVertex o;
		o.x = p.x + (q.x - p.x) * t; o.y = p.y + (q.y - p.y) * t;
		o.u = p.u + (q.u - p.u) * t; o.v = p.v + (q.v - p.v) * t;
		o.r = p.r + (q.r - p.r) * t; o.g = p.g + (q.g - p.g) * t;
		o.b = p.b + (q.b - p.b) * t; o.a = p.a + (q.a - p.a) * t;
		return o;
	}

	// Sutherland-Hodgman against one axis-aligned plane
	static int ClipAgainst(const ClipVertex* in, int n, ClipVertex* out, int axis, float bound, bool keepGreater) {
		int m = 0;
		for (int i = 0; i < n; i++) {
			const ClipVertex& p = in[i];
			const ClipVertex& q = in[(i + 1) % n];
			float pc = axis == 0 ? p.x : p.y;
			float qc = axis == 0 ? q.x : q.y;
			bool pin = keepGreater ? (pc >= bound) : (pc <= bound);
			bool qin = keepGreater ? (qc >= bound) : (qc <= bound);
			if (pin)
				out[m++] = p;
			if (pin != qin) {
				float t = (bound - pc) / (qc - pc);
				out[m++] = Lerp(p, q, t);
			}
		}
		return m;
	}

	static inline void Emit(float x, float y, float u, float v, DWORD color) {
		Vertex vtx;
		vtx.x = x - 0.5f; // Direct3D 7 samples pixel centers at integer coordinates
		vtx.y = y - 0.5f;
		vtx.z = 0.0f;
		vtx.rhw = 1.0f;
		vtx.color = color;
		vtx.u = u;
		vtx.v = v;
		g_batch.push_back(vtx);
	}

	static inline DWORD ToD3DColor(ImU32 c) {
		// ImGui: A B G R (R in the low byte) -> Direct3D: A R G B
		return (c & 0xFF00FF00) | ((c & 0x000000FF) << 16) | ((c & 0x00FF0000) >> 16);
	}

	static inline void EmitClip(const ClipVertex& c) {
		DWORD color = ((DWORD)(c.a + 0.5f) << 24) | ((DWORD)(c.r + 0.5f) << 16) | ((DWORD)(c.g + 0.5f) << 8) | (DWORD)(c.b + 0.5f);
		Emit(c.x, c.y, c.u, c.v, color);
	}

	static void AddTriangle(const ImDrawVert& a, const ImDrawVert& b, const ImDrawVert& c, const ImVec4& clip, const ImVec2& off) {
		float ax = a.pos.x - off.x, ay = a.pos.y - off.y;
		float bx = b.pos.x - off.x, by = b.pos.y - off.y;
		float cx = c.pos.x - off.x, cy = c.pos.y - off.y;

		float minX = ImMin(ax, ImMin(bx, cx)), maxX = ImMax(ax, ImMax(bx, cx));
		float minY = ImMin(ay, ImMin(by, cy)), maxY = ImMax(ay, ImMax(by, cy));

		if (maxX <= clip.x || minX >= clip.z || maxY <= clip.y || minY >= clip.w)
			return; // completely outside

		if (minX >= clip.x && maxX <= clip.z && minY >= clip.y && maxY <= clip.w) {
			Emit(ax, ay, a.uv.x, a.uv.y, ToD3DColor(a.col));
			Emit(bx, by, b.uv.x, b.uv.y, ToD3DColor(b.col));
			Emit(cx, cy, c.uv.x, c.uv.y, ToD3DColor(c.col));
			return;
		}

		ClipVertex poly[16], tmp[16];
		poly[0] = ToClip(a); poly[0].x = ax; poly[0].y = ay;
		poly[1] = ToClip(b); poly[1].x = bx; poly[1].y = by;
		poly[2] = ToClip(c); poly[2].x = cx; poly[2].y = cy;
		int n = 3;
		n = ClipAgainst(poly, n, tmp, 0, clip.x, true);  if (n < 3) return;
		n = ClipAgainst(tmp, n, poly, 0, clip.z, false); if (n < 3) return;
		n = ClipAgainst(poly, n, tmp, 1, clip.y, true);  if (n < 3) return;
		n = ClipAgainst(tmp, n, poly, 1, clip.w, false); if (n < 3) return;

		for (int i = 1; i + 1 < n; i++) {
			EmitClip(poly[0]);
			EmitClip(poly[i]);
			EmitClip(poly[i + 1]);
		}
	}

	// ---------------------------------------------------------------- state

	static const D3DRENDERSTATETYPE kRenderStates[] = {
		D3DRENDERSTATE_ZENABLE,
		D3DRENDERSTATE_ZWRITEENABLE,
		D3DRENDERSTATE_FILLMODE,
		D3DRENDERSTATE_SHADEMODE,
		D3DRENDERSTATE_ALPHATESTENABLE,
		D3DRENDERSTATE_ALPHABLENDENABLE,
		D3DRENDERSTATE_SRCBLEND,
		D3DRENDERSTATE_DESTBLEND,
		D3DRENDERSTATE_CULLMODE,
		D3DRENDERSTATE_DITHERENABLE,
		D3DRENDERSTATE_FOGENABLE,
		D3DRENDERSTATE_SPECULARENABLE,
		D3DRENDERSTATE_COLORKEYENABLE,
		D3DRENDERSTATE_COLORKEYBLENDENABLE,
		D3DRENDERSTATE_STENCILENABLE,
		D3DRENDERSTATE_WRAP0,
		D3DRENDERSTATE_CLIPPING,
		D3DRENDERSTATE_LIGHTING,
		D3DRENDERSTATE_COLORVERTEX,
		D3DRENDERSTATE_VERTEXBLEND,
	};
	static const int kNumRenderStates = (int)(sizeof(kRenderStates) / sizeof(kRenderStates[0]));

	static const D3DTEXTURESTAGESTATETYPE kStage0States[] = {
		D3DTSS_COLOROP,
		D3DTSS_COLORARG1,
		D3DTSS_COLORARG2,
		D3DTSS_ALPHAOP,
		D3DTSS_ALPHAARG1,
		D3DTSS_ALPHAARG2,
		D3DTSS_TEXCOORDINDEX,
		D3DTSS_ADDRESSU,
		D3DTSS_ADDRESSV,
		D3DTSS_MAGFILTER,
		D3DTSS_MINFILTER,
		D3DTSS_MIPFILTER,
		D3DTSS_TEXTURETRANSFORMFLAGS,
	};
	static const int kNumStage0States = (int)(sizeof(kStage0States) / sizeof(kStage0States[0]));

	struct SavedState {
		DWORD render[kNumRenderStates];
		DWORD stage0[kNumStage0States];
		bool haveRender[kNumRenderStates];
		bool haveStage0[kNumStage0States];
		bool haveStage1ColorOp, haveStage1AlphaOp;
		DWORD stage1ColorOp, stage1AlphaOp;
		IDirectDrawSurface7* tex0;
		IDirectDrawSurface7* tex1;
		D3DVIEWPORT7 viewport;
		bool haveViewport;
	};

	static void Save(SavedState& s) {
		for (int i = 0; i < kNumRenderStates; i++)
			s.haveRender[i] = SUCCEEDED(g_device->GetRenderState(kRenderStates[i], &s.render[i]));
		for (int i = 0; i < kNumStage0States; i++)
			s.haveStage0[i] = SUCCEEDED(g_device->GetTextureStageState(0, kStage0States[i], &s.stage0[i]));
		s.haveStage1ColorOp = SUCCEEDED(g_device->GetTextureStageState(1, D3DTSS_COLOROP, &s.stage1ColorOp));
		s.haveStage1AlphaOp = SUCCEEDED(g_device->GetTextureStageState(1, D3DTSS_ALPHAOP, &s.stage1AlphaOp));
		s.tex0 = nullptr;
		s.tex1 = nullptr;
		g_device->GetTexture(0, &s.tex0); // AddRef'd
		g_device->GetTexture(1, &s.tex1);
		s.haveViewport = SUCCEEDED(g_device->GetViewport(&s.viewport));
	}

	static void Restore(SavedState& s) {
		for (int i = 0; i < kNumRenderStates; i++)
			if (s.haveRender[i]) g_device->SetRenderState(kRenderStates[i], s.render[i]);
		for (int i = 0; i < kNumStage0States; i++)
			if (s.haveStage0[i]) g_device->SetTextureStageState(0, kStage0States[i], s.stage0[i]);
		if (s.haveStage1ColorOp) g_device->SetTextureStageState(1, D3DTSS_COLOROP, s.stage1ColorOp);
		if (s.haveStage1AlphaOp) g_device->SetTextureStageState(1, D3DTSS_ALPHAOP, s.stage1AlphaOp);
		g_device->SetTexture(0, s.tex0);
		g_device->SetTexture(1, s.tex1);
		if (s.tex0) s.tex0->Release();
		if (s.tex1) s.tex1->Release();
		if (s.haveViewport)
			g_device->SetViewport(&s.viewport);
	}

	static void Setup(float width, float height) {
		g_device->SetRenderState(D3DRENDERSTATE_ZENABLE, FALSE);
		g_device->SetRenderState(D3DRENDERSTATE_ZWRITEENABLE, FALSE);
		g_device->SetRenderState(D3DRENDERSTATE_FILLMODE, D3DFILL_SOLID);
		g_device->SetRenderState(D3DRENDERSTATE_SHADEMODE, D3DSHADE_GOURAUD);
		g_device->SetRenderState(D3DRENDERSTATE_ALPHATESTENABLE, FALSE);
		g_device->SetRenderState(D3DRENDERSTATE_ALPHABLENDENABLE, TRUE);
		g_device->SetRenderState(D3DRENDERSTATE_SRCBLEND, D3DBLEND_SRCALPHA);
		g_device->SetRenderState(D3DRENDERSTATE_DESTBLEND, D3DBLEND_INVSRCALPHA);
		g_device->SetRenderState(D3DRENDERSTATE_CULLMODE, D3DCULL_NONE);
		g_device->SetRenderState(D3DRENDERSTATE_DITHERENABLE, FALSE);
		g_device->SetRenderState(D3DRENDERSTATE_FOGENABLE, FALSE);
		g_device->SetRenderState(D3DRENDERSTATE_SPECULARENABLE, FALSE);
		g_device->SetRenderState(D3DRENDERSTATE_COLORKEYENABLE, FALSE);
		g_device->SetRenderState(D3DRENDERSTATE_COLORKEYBLENDENABLE, FALSE);
		g_device->SetRenderState(D3DRENDERSTATE_STENCILENABLE, FALSE);
		g_device->SetRenderState(D3DRENDERSTATE_WRAP0, 0);
		g_device->SetRenderState(D3DRENDERSTATE_CLIPPING, TRUE);
		g_device->SetRenderState(D3DRENDERSTATE_LIGHTING, FALSE);
		g_device->SetRenderState(D3DRENDERSTATE_COLORVERTEX, TRUE);
		g_device->SetRenderState(D3DRENDERSTATE_VERTEXBLEND, D3DVBLEND_DISABLE);

		g_device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
		g_device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
		g_device->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
		g_device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
		g_device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
		g_device->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
		g_device->SetTextureStageState(0, D3DTSS_TEXCOORDINDEX, 0);
		g_device->SetTextureStageState(0, D3DTSS_ADDRESSU, D3DTADDRESS_CLAMP);
		g_device->SetTextureStageState(0, D3DTSS_ADDRESSV, D3DTADDRESS_CLAMP);
		g_device->SetTextureStageState(0, D3DTSS_MAGFILTER, D3DTFG_LINEAR);
		g_device->SetTextureStageState(0, D3DTSS_MINFILTER, D3DTFN_LINEAR);
		g_device->SetTextureStageState(0, D3DTSS_MIPFILTER, D3DTFP_NONE);
		g_device->SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
		g_device->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
		g_device->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
		g_device->SetTexture(1, nullptr);

		D3DVIEWPORT7 vp;
		vp.dwX = 0;
		vp.dwY = 0;
		vp.dwWidth = (DWORD)width;
		vp.dwHeight = (DWORD)height;
		vp.dvMinZ = 0.0f;
		vp.dvMaxZ = 1.0f;
		g_device->SetViewport(&vp);
	}

	static void Flush(IDirectDrawSurface7* texture) {
		if (g_batch.Size == 0)
			return;
		g_device->SetTexture(0, texture);
		int offset = 0;
		while (offset < g_batch.Size) {
			int count = ImMin(kMaxBatch, g_batch.Size - offset);
			HRESULT hr = g_device->DrawPrimitive(D3DPT_TRIANGLELIST, kFVF, (LPVOID)(g_batch.Data + offset), (DWORD)count, 0);
			if (FAILED(hr)) {
				static bool reported = false;
				if (!reported) {
					Log::Write("Overlay DrawPrimitive failed: 0x%08X (%d vertices)", (unsigned)hr, count);
					reported = true;
				}
			}
			offset += count;
		}
		g_batch.resize(0);
	}

	void RenderDrawData(ImDrawData* drawData) {
		if (!g_device || !drawData || drawData->CmdListsCount == 0)
			return;
		const float width = drawData->DisplaySize.x;
		const float height = drawData->DisplaySize.y;
		if (width <= 0.0f || height <= 0.0f)
			return;

		if (g_fontTex && g_fontTex->IsLost() == DDERR_SURFACELOST) {
			if (FAILED(g_fontTex->Restore()) || !Upload(g_fontTex))
				return;
		}

		SavedState saved = {};
		Save(saved);
		struct RestoreState {
			SavedState& saved;
			~RestoreState() { Restore(saved); }
		} restoreState{ saved };

		HRESULT sceneHr = g_device->BeginScene();
		const bool ownScene = SUCCEEDED(sceneHr);
		if (!ownScene && sceneHr != D3DERR_SCENE_IN_SCENE) {
			return;
		}
		struct EndScene {
			bool own;
			~EndScene() { if (own) g_device->EndScene(); }
		} endScene{ ownScene };

		Setup(width, height);

		const ImVec2 off = drawData->DisplayPos;
		IDirectDrawSurface7* currentTex = nullptr;
		g_batch.resize(0);

		for (int n = 0; n < drawData->CmdListsCount; n++) {
			const ImDrawList* list = drawData->CmdLists[n];
			const ImDrawVert* vtx = list->VtxBuffer.Data;
			const ImDrawIdx* idx = list->IdxBuffer.Data;

			for (int c = 0; c < list->CmdBuffer.Size; c++) {
				const ImDrawCmd* cmd = &list->CmdBuffer[c];
				if (cmd->UserCallback) {
					if (cmd->UserCallback != ImDrawCallback_ResetRenderState) {
						Flush(currentTex);
						cmd->UserCallback(list, cmd);
					}
					Setup(width, height);
					continue;
				}

				ImVec4 clip(cmd->ClipRect.x - off.x, cmd->ClipRect.y - off.y, cmd->ClipRect.z - off.x, cmd->ClipRect.w - off.y);
				clip.x = ImMax(clip.x, 0.0f);
				clip.y = ImMax(clip.y, 0.0f);
				clip.z = ImMin(clip.z, width);
				clip.w = ImMin(clip.w, height);
				if (clip.z <= clip.x || clip.w <= clip.y)
					continue;

				IDirectDrawSurface7* tex = (IDirectDrawSurface7*)(intptr_t)cmd->GetTexID();
				if (tex != currentTex) {
					Flush(currentTex);
					currentTex = tex;
				}

				const ImDrawIdx* tri = idx + cmd->IdxOffset;
				const ImDrawVert* base = vtx + cmd->VtxOffset;
				for (unsigned int i = 0; i + 2 < cmd->ElemCount; i += 3)
					AddTriangle(base[tri[i]], base[tri[i + 1]], base[tri[i + 2]], clip, off);

				if (g_batch.Size >= kMaxBatch * 4)
					Flush(currentTex);
			}
		}
		Flush(currentTex);

	}

}}
