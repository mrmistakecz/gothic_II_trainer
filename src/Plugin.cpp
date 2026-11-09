// Gothic Trainer – Union plugin for Gothic II: Night of the Raven.
// Disable macro redefinition warning (Gothic API redefines some Windows macros)
#pragma warning(disable: 4005)

// Standard headers and ImGui first: Union API defines a function-like macro
// "clamp" (and "null"), which would break <algorithm> if included later.
#include <algorithm>
#include <cctype>
#include <cfloat>
#include <climits>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "imgui.h"
#include "../navigation/MapData.h"

#include <Union/Hook.h>
#include <ZenGin/zGothicAPI.h>

#include "Core/Log.h"
#include "Core/Overlay.h"
#include "Core/Paths.h"
#include "Core/Seh.h"
#include "Core/Settings.h"
#include "Core/Text.h"
#include "Core/QuestScan.h"

// Runs when Union loads the DLL (before the game starts).
namespace {
	struct PluginStartup {
		PluginStartup() {
			GT::Log::Init();
			GT::Settings::Load();
			GT::Text::SetCodePage((unsigned)GT::Settings::Get().codePage);
			GT::Log::Write("Plugin loaded from %s (engine %d)", GT::Paths::SystemDir().c_str(), GetGameVersion());
		}
	};
	PluginStartup g_startup;
}

#ifdef __G2A
#define GOTHIC_NAMESPACE Gothic_II_Addon
#define ENGINE Engine_G2A
HOOKSPACE(Gothic_II_Addon, GetGameVersion() == ENGINE);
#include "Sources.hpp"
#endif

HOOKSPACE(Global, true);
