#pragma once
#include <string>

namespace GT { namespace Log {
	void Init();
	void Write(const char* fmt, ...);
	const std::string& Path();
}}
