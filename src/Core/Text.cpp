#include <windows.h>
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include "Text.h"

namespace GT { namespace Text {

	static unsigned g_codePage = 0;

	void SetCodePage(unsigned cp) {
		g_codePage = cp;
	}

	unsigned CodePage() {
		return g_codePage ? g_codePage : GetACP();
	}

	std::wstring Utf8ToWide(const std::string& s) {
		if (s.empty())
			return std::wstring();
		int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
		std::wstring w(n, L'\0');
		if (n > 0)
			MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), &w[0], n);
		return w;
	}

	std::string WideToUtf8(const std::wstring& w) {
		if (w.empty())
			return std::string();
		int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), nullptr, 0, nullptr, nullptr);
		std::string s(n, '\0');
		if (n > 0)
			WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), &s[0], n, nullptr, nullptr);
		return s;
	}

	std::string FromGame(const char* s) {
		if (!s || !*s)
			return std::string();
		int len = (int)strlen(s);
		UINT cp = CodePage();
		int n = MultiByteToWideChar(cp, 0, s, len, nullptr, 0);
		if (n <= 0)
			return std::string(s, len);
		std::wstring w(n, L'\0');
		MultiByteToWideChar(cp, 0, s, len, &w[0], n);
		return WideToUtf8(w);
	}

	std::string ToGame(const std::string& utf8) {
		std::wstring w = Utf8ToWide(utf8);
		if (w.empty())
			return std::string();
		UINT cp = CodePage();
		int n = WideCharToMultiByte(cp, 0, w.data(), (int)w.size(), nullptr, 0, nullptr, nullptr);
		std::string s(n > 0 ? n : 0, '\0');
		if (n > 0)
			WideCharToMultiByte(cp, 0, w.data(), (int)w.size(), &s[0], n, nullptr, nullptr);
		return s;
	}

	std::string Fold(const std::string& utf8) {
		std::wstring w = Utf8ToWide(utf8);
		if (w.empty())
			return std::string();

		// decompose "č" into "c" + combining caron, then drop the combining marks
		int n = FoldStringW(MAP_COMPOSITE, w.c_str(), (int)w.size(), nullptr, 0);
		std::wstring d;
		if (n > 0) {
			d.resize(n);
			FoldStringW(MAP_COMPOSITE, w.c_str(), (int)w.size(), &d[0], n);
		}
		else {
			d = w;
		}

		std::wstring out;
		out.reserve(d.size());
		for (wchar_t c : d) {
			if (c >= 0x0300 && c <= 0x036F)
				continue;
			switch (c) {
				case 0x0141: out += L'L'; continue; // Ł
				case 0x0142: out += L'l'; continue; // ł
				case 0x0110: out += L'D'; continue; // Đ
				case 0x0111: out += L'd'; continue; // đ
				case 0x00DF: out += L"ss"; continue; // ß
			}
			out += c;
		}
		if (!out.empty())
			CharLowerBuffW(&out[0], (DWORD)out.size());
		return WideToUtf8(out);
	}

	bool Contains(const std::string& haystack, const std::string& needle) {
		if (needle.empty())
			return true;
		return haystack.find(needle) != std::string::npos;
	}

	std::string Format(const char* fmt, ...) {
		char buf[1024];
		va_list ap;
		va_start(ap, fmt);
		vsnprintf(buf, sizeof(buf), fmt, ap);
		va_end(ap);
		return buf;
	}

	std::string Thousands(long long value) {
		bool neg = value < 0;
		unsigned long long v = neg ? (unsigned long long)(-value) : (unsigned long long)value;
		std::string digits = std::to_string(v);
		std::string out;
		int count = 0;
		for (int i = (int)digits.size() - 1; i >= 0; i--) {
			out.insert(out.begin(), digits[i]);
			if (++count % 3 == 0 && i > 0)
				out.insert(out.begin(), ' ');
		}
		if (neg)
			out.insert(out.begin(), '-');
		return out;
	}

}}
