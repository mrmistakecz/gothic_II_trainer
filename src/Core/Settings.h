#pragma once
#include <string>
#include <vector>

namespace GT {

	struct TrainerSettings {
		int   toggleKey    = 0x79;   // VK_F10
		int   fontSize     = 0;      // 0 = automaticky podle rozlišení
		float windowAlpha  = 0.96f;
		float mouseSpeed   = 1.0f;
		bool  systemCursor = false;  // použít kurzor Windows místo DirectInputu
		int   codePage     = 0;      // 0 = automaticky (kódová stránka Windows)
		bool  showToasts   = true;
	};

	struct SavedPosition {
		std::string name;            // UTF-8
		float x = 0.0f, y = 0.0f, z = 0.0f;
	};

	namespace Settings {
		TrainerSettings& Get();
		void Load();
		void Save();
		const std::string& IniPath();

		std::vector<SavedPosition> LoadPositions(const std::string& world);
		void SavePositions(const std::string& world, const std::vector<SavedPosition>& list);
	}

}
