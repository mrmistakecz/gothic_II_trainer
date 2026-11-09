#include <windows.h>
#include <cstdio>
#include <cstdarg>
#include "Log.h"
#include "Paths.h"

namespace GT { namespace Log {

	// Function-local storage: the log may be used during static initialization
	// of another translation unit (plugin startup), before globals here exist.
	static std::string& Storage() {
		static std::string path;
		return path;
	}

	static bool g_ready = false;

	void Init() {
		if (g_ready)
			return;
		Storage() = Paths::DataDir() + "\\GothicTrainer.log";
		FILE* f = fopen(Storage().c_str(), "w");
		if (f) {
			fputs("Gothic Trainer " TRAINER_VERSION "\n", f);
			fclose(f);
			g_ready = true;
		}
	}

	void Write(const char* fmt, ...) {
		if (!g_ready)
			return;
		char buf[2048];
		va_list ap;
		va_start(ap, fmt);
		vsnprintf(buf, sizeof(buf), fmt, ap);
		va_end(ap);

		SYSTEMTIME t;
		GetLocalTime(&t);
		FILE* f = fopen(Storage().c_str(), "a");
		if (f) {
			fprintf(f, "[%02d:%02d:%02d.%03d] %s\n", t.wHour, t.wMinute, t.wSecond, t.wMilliseconds, buf);
			fclose(f);
		}
	}

	const std::string& Path() {
		return Storage();
	}

}}
