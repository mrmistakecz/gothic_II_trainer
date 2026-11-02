#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include "Settings.h"
#include "Paths.h"

namespace GT { namespace Settings {

	static const char* kSection = "GothicTrainer";

	static TrainerSettings g_settings;

	TrainerSettings& Get() {
		return g_settings;
	}

	const std::string& IniPath() {
		static std::string path;
		if (path.empty())
			path = Paths::SystemDir() + "\\GothicTrainer.ini";
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
		return (float)atof(s.c_str());
	}

	static void WriteStr(const char* section, const char* key, const std::string& value) {
		WritePrivateProfileStringA(section, key, value.c_str(), IniPath().c_str());
	}

	static void WriteInt(const char* key, int value) {
		WriteStr(kSection, key, std::to_string(value));
	}

	static void WriteFloat(const char* key, float value) {
		char buf[64];
		snprintf(buf, sizeof(buf), "%.3f", value);
		WriteStr(kSection, key, buf);
	}

	void Load() {
		TrainerSettings d;
		g_settings.toggleKey    = ReadInt("ToggleKey", d.toggleKey);
		g_settings.fontSize     = ReadInt("FontSize", d.fontSize);
		g_settings.windowAlpha  = ReadFloat("WindowAlpha", d.windowAlpha);
		g_settings.mouseSpeed   = ReadFloat("MouseSpeed", d.mouseSpeed);
		g_settings.systemCursor = ReadInt("SystemCursor", d.systemCursor ? 1 : 0) != 0;
		g_settings.codePage     = ReadInt("CodePage", d.codePage);
		g_settings.showToasts   = ReadInt("ShowToasts", d.showToasts ? 1 : 0) != 0;

		if (g_settings.windowAlpha < 0.3f || g_settings.windowAlpha > 1.0f)
			g_settings.windowAlpha = d.windowAlpha;
		if (g_settings.mouseSpeed < 0.05f || g_settings.mouseSpeed > 20.0f)
			g_settings.mouseSpeed = d.mouseSpeed;
		if (g_settings.fontSize != 0 && (g_settings.fontSize < 10 || g_settings.fontSize > 48))
			g_settings.fontSize = 0;
	}

	void Save() {
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
			if (sscanf(v.c_str() + a + 1, "%f|%f|%f", &p.x, &p.y, &p.z) == 3)
				out.push_back(p);
		}
		return out;
	}

	void SavePositions(const std::string& world, const std::vector<SavedPosition>& list) {
		std::string section = PositionSection(world);
		// remove the whole section first
		WritePrivateProfileStringA(section.c_str(), nullptr, nullptr, IniPath().c_str());
		WriteStr(section.c_str(), "Count", std::to_string(list.size()));
		for (size_t i = 0; i < list.size(); i++) {
			std::string name = list[i].name;
			for (char& c : name)
				if (c == '|' || c == '\r' || c == '\n')
					c = '/';
			char buf[1200];
			snprintf(buf, sizeof(buf), "%s|%.2f|%.2f|%.2f", name.c_str(), list[i].x, list[i].y, list[i].z);
			std::string key = "P" + std::to_string(i + 1);
			WriteStr(section.c_str(), key.c_str(), buf);
		}
	}

}}
