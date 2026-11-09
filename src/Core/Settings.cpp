#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <locale>
#include <sstream>
#include "Settings.h"
#include "Paths.h"
#include "Overlay.h"

namespace GT { namespace Settings {

	static const char* kSection = "GothicTrainer";

	TrainerSettings& Get() {
		// Load() is called by the plugin's static initializer in another TU.
		// Construct on first use so a later global initializer cannot undo it.
		static TrainerSettings settings;
		return settings;
	}

	const std::string& IniPath() {
		static std::string path;
		if (path.empty())
			path = Paths::DataDir() + "\\GothicTrainer.ini";
		return path;
	}

	static std::string ReadStr(const char* section, const char* key, const char* def) {
		char buf[1024] = {};
		GetPrivateProfileStringA(section, key, def, buf, (DWORD)sizeof(buf), IniPath().c_str());
		return buf;
	}

	static int ReadInt(const char* key, int def) {
		return (int)GetPrivateProfileIntA(kSection, key, def, IniPath().c_str());
	}

	static float ReadFloat(const char* key, float def) {
		std::string s = ReadStr(kSection, key, "");
		if (s.empty())
			return def;
		std::istringstream input(s);
		input.imbue(std::locale::classic());
		float value = 0.0f;
		if (!(input >> value) || !std::isfinite(value))
			return def;
		input >> std::ws;
		return input.eof() ? value : def;
	}

	static void WriteStr(const char* section, const char* key, const std::string& value) {
		WritePrivateProfileStringA(section, key, value.c_str(), IniPath().c_str());
	}

	static void WriteInt(const char* key, int value) {
		WriteStr(kSection, key, std::to_string(value));
	}

	static void WriteFloat(const char* key, float value) {
		std::ostringstream output;
		output.imbue(std::locale::classic());
		output.precision(3);
		output << std::fixed << value;
		WriteStr(kSection, key, output.str());
	}

	void Load() {
		TrainerSettings& g_settings = Get();
		TrainerSettings d;
		g_settings.toggleKey    = ReadInt("ToggleKey", d.toggleKey);
		g_settings.fontSize     = ReadInt("FontSize", d.fontSize);
		g_settings.windowAlpha  = ReadFloat("WindowAlpha", d.windowAlpha);
		g_settings.mouseSpeed   = ReadFloat("MouseSpeed", d.mouseSpeed);
		g_settings.systemCursor = ReadInt("SystemCursor", d.systemCursor ? 1 : 0) != 0;
		g_settings.codePage     = ReadInt("CodePage", d.codePage);
		g_settings.showToasts   = ReadInt("ShowToasts", d.showToasts ? 1 : 0) != 0;
		if (!Overlay::DikForVk(g_settings.toggleKey))
			g_settings.toggleKey = d.toggleKey;
		if (g_settings.codePage < 0 || (g_settings.codePage != 0 && !IsValidCodePage((UINT)g_settings.codePage)))
			g_settings.codePage = d.codePage;

		if (g_settings.windowAlpha < 0.3f || g_settings.windowAlpha > 1.0f)
			g_settings.windowAlpha = d.windowAlpha;
		if (g_settings.mouseSpeed < 0.05f || g_settings.mouseSpeed > 20.0f)
			g_settings.mouseSpeed = d.mouseSpeed;
		if (g_settings.fontSize != 0 && (g_settings.fontSize < 10 || g_settings.fontSize > 48))
			g_settings.fontSize = 0;
	}

	void Save() {
		const TrainerSettings& g_settings = Get();
		WriteInt("ToggleKey", g_settings.toggleKey);
		WriteInt("FontSize", g_settings.fontSize);
		WriteFloat("WindowAlpha", g_settings.windowAlpha);
		WriteFloat("MouseSpeed", g_settings.mouseSpeed);
		WriteInt("SystemCursor", g_settings.systemCursor ? 1 : 0);
		WriteInt("CodePage", g_settings.codePage);
		WriteInt("ShowToasts", g_settings.showToasts ? 1 : 0);
	}

	static std::string PositionSection(const std::string& world) {
		return "Pozice:" + world;
	}

	std::vector<SavedPosition> LoadPositions(const std::string& world) {
		std::vector<SavedPosition> out;
		std::string section = PositionSection(world);
		int count = (int)GetPrivateProfileIntA(section.c_str(), "Count", 0, IniPath().c_str());
		for (int i = 0; i < count && i < 500; i++) {
			std::string key = "P" + std::to_string(i + 1);
			std::string v = ReadStr(section.c_str(), key.c_str(), "");
			// name|x|y|z
			size_t a = v.find('|');
			if (a == std::string::npos)
				continue;
			SavedPosition p;
			p.name = v.substr(0, a);
			std::istringstream input(v.substr(a + 1));
			input.imbue(std::locale::classic());
			char sep1 = 0, sep2 = 0;
			if (!(input >> p.x >> sep1 >> p.y >> sep2 >> p.z) || sep1 != '|' || sep2 != '|'
				|| !std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z))
				continue;
			input >> std::ws;
			if (input.eof())
				out.push_back(p);
		}
		return out;
	}

	void SavePositions(const std::string& world, const std::vector<SavedPosition>& list) {
		std::string section = PositionSection(world);
		// remove the whole section first
		WritePrivateProfileStringA(section.c_str(), nullptr, nullptr, IniPath().c_str());
		size_t saved = 0;
		for (size_t i = 0; i < list.size() && saved < 500; i++) {
			if (!std::isfinite(list[i].x) || !std::isfinite(list[i].y) || !std::isfinite(list[i].z))
				continue;
			std::string name = list[i].name;
			for (char& c : name)
				if (c == '|' || c == '\r' || c == '\n')
					c = '/';
			std::ostringstream output;
			output.imbue(std::locale::classic());
			output.precision(2);
			output << name.substr(0, 128) << '|' << std::fixed << list[i].x << '|' << list[i].y << '|' << list[i].z;
			std::string key = "P" + std::to_string(++saved);
			WriteStr(section.c_str(), key.c_str(), output.str());
		}
		WriteStr(section.c_str(), "Count", std::to_string(saved));
	}

}}
