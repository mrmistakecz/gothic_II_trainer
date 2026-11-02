#pragma once

namespace GT {
	// Runs fn(ctx) and swallows structured exceptions (access violations etc.)
	// so that a fault inside the trainer never takes the whole game down.
	// Returns false when an exception was caught; *code receives its code.
	typedef void (*SehFn)(void* ctx);
	bool SehInvoke(SehFn fn, void* ctx, unsigned long* code);
}
