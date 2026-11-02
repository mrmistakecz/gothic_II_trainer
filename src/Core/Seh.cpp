#include <windows.h>
#include "Seh.h"

namespace GT {

	static int Filter(unsigned long code, unsigned long* out) {
		if (out)
			*out = code;
		return EXCEPTION_EXECUTE_HANDLER;
	}

	// Must not contain C++ objects with destructors (MSVC C2712)
	bool SehInvoke(SehFn fn, void* ctx, unsigned long* code) {
		__try {
			fn(ctx);
			return true;
		}
		__except (Filter(GetExceptionCode(), code)) {
			return false;
		}
	}

}
