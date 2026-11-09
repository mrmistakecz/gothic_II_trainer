#include <windows.h>
#include <cstdio>
#include <cctype>
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

	const std::string& DataDir() {
		static const std::string dir = []() {
			const std::string system = SystemDir();
			char probeName[80];
			snprintf(probeName, sizeof(probeName), "\\GothicTrainer_write_%lu_%lu.tmp", GetCurrentProcessId(), GetTickCount());
			HANDLE probe = CreateFileA((system + probeName).c_str(), GENERIC_WRITE, 0, nullptr,
				CREATE_NEW, FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, nullptr);
			if (probe != INVALID_HANDLE_VALUE) {
				CloseHandle(probe);
				return system;
			}
			char local[MAX_PATH * 2] = {};
			DWORD length = GetEnvironmentVariableA("LOCALAPPDATA", local, (DWORD)sizeof(local));
			if (!length || length >= sizeof(local))
				return system;
			// Keep separate settings for each game installation.
			unsigned hash = 2166136261u;
			for (unsigned char c : system) {
				hash ^= (unsigned char)tolower(c);
				hash *= 16777619u;
			}
			std::string fallback = std::string(local) + "\\GothicTrainer";
			CreateDirectoryA(fallback.c_str(), nullptr);
			char key[16];
			snprintf(key, sizeof(key), "\\%08X", hash);
			fallback += key;
			if (!CreateDirectoryA(fallback.c_str(), nullptr) && GetLastError() != ERROR_ALREADY_EXISTS)
				return system;
			CopyFileA((system + "\\GothicTrainer.ini").c_str(), (fallback + "\\GothicTrainer.ini").c_str(), TRUE);
			return fallback;
		}();
		return dir;
	}

	bool FileExists(const std::string& path) {
		DWORD a = GetFileAttributesA(path.c_str());
		return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
	}

}}
