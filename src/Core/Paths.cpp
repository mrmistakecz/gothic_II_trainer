#include <windows.h>
#include "Paths.h"

namespace GT { namespace Paths {

	const std::string& SystemDir() {
		static std::string dir;
		if (dir.empty()) {
			char buf[MAX_PATH * 2] = {};
			DWORD n = GetModuleFileNameA(nullptr, buf, (DWORD)sizeof(buf));
			std::string path(buf, n);
			size_t slash = path.find_last_of("\\/");
			dir = (slash == std::string::npos) ? std::string(".") : path.substr(0, slash);
		}
		return dir;
	}

	const std::string& FontsDir() {
		static std::string dir;
		if (dir.empty()) {
			char buf[MAX_PATH] = {};
			UINT n = GetWindowsDirectoryA(buf, MAX_PATH);
			dir = std::string(buf, n) + "\\Fonts";
		}
		return dir;
	}

	bool FileExists(const std::string& path) {
		DWORD a = GetFileAttributesA(path.c_str());
		return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
	}

}}
