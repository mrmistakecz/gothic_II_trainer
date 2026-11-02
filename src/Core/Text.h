#pragma once
#include <string>

namespace GT { namespace Text {
	// Code page used by the game's texts (0 = automatic).
	void SetCodePage(unsigned cp);
	unsigned CodePage();

	std::string FromGame(const char* s);             // herní text (ANSI) -> UTF-8
	std::string ToGame(const std::string& utf8);     // UTF-8 -> herní text (ANSI)
	std::wstring Utf8ToWide(const std::string& s);
	std::string WideToUtf8(const std::wstring& s);

	// Lowercase + diacritics removed ("Meč" -> "mec") for forgiving search.
	std::string Fold(const std::string& utf8);
	bool Contains(const std::string& foldedHaystack, const std::string& foldedNeedle);

	std::string Format(const char* fmt, ...);
	// 12345 -> "12 345"
	std::string Thousands(long long value);
}}
