#include <windows.h>
#include "Core/Paths.h"

namespace GT { namespace Paths {
	const std::string& SystemDir() {
		static const std::string dir = [] {
			char path[MAX_PATH * 2] = {};
			GetModuleFileNameA(nullptr, path, sizeof(path));
			std::string value(path);
			value = value.substr(0, value.find_last_of("\\/")) + "\\test-data";
			CreateDirectoryA(value.c_str(), nullptr);
			return value;
		}();
		return dir;
	}
	const std::string& DataDir() { return SystemDir(); }
	const std::string& FontsDir() {
		static const std::string dir = "C:\\Windows\\Fonts";
		return dir;
	}
	bool FileExists(const std::string& path) {
		DWORD attr = GetFileAttributesA(path.c_str());
		return attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY);
	}
}}
