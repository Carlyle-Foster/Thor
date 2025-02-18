#include "util/types.h"

using namespace Thor;

// libstdc++ ABI implementation.
#if !defined(THOR_COMPILER_MSVC)

struct Guard {
	Uint8 done;
	Uint8 pending;
	Uint8 padding[62];
};
static_assert(sizeof(Guard) == 64);

extern "C" {

int __cxa_guard_acquire(Guard* guard) {
	if (guard->done) {
		return 0;
	}
	if (guard->pending) {
		*(volatile int *)0 = 0;
	}
	guard->pending = 1;
	return 1;
}

void __cxa_guard_release(Guard* guard) {
	guard->done = 1;
}

#endif

} // extern "C"