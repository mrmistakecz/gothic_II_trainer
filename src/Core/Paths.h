#pragma once
#include <string>

namespace GT { namespace Paths {
	// Directory containing Gothic2.exe (the game's System folder)
	const std::string& SystemDir();
	// Windows font directory (C:\Windows\Fonts)
	const std::string& FontsDir();
	bool FileExists(const std::string& path);
}}
