#ifndef THOR_LOCK_H
#define THOR_LOCK_H
#include "util/atomic.h"

namespace Thor {

struct System;

// Low-level adaptive mutex that requires sizeof(void*) of storage. Has a fast
// path that is similar to a spin-lock but doesn't burn CPU and a slow path that
// is similar to the mutex in any language you've used, except uses far less
// storage and amoritizs the cost of constructing system sync primitives which
// require heap allocation.
struct Lock {
	void lock(System& sys) {
		if (word_.compare_exchange_weak(0, IS_LOCKED_BIT, MemoryOrder::acquire)) {
			return;
		}
		lock_slow(sys);
	}

	void unlock(System& sys) {
		if (word_.compare_exchange_weak(IS_LOCKED_BIT, 0, MemoryOrder::release)) {
			return;
		}
		unlock_slow(sys);
	}

private:
	static inline constexpr const Address IS_LOCKED_BIT = 1;
	static inline constexpr const Address IS_QUEUE_LOCKED_BIT = 2;
	static inline constexpr const Address QUEUE_HEAD_MASK = 3;

	void lock_slow(System& sys);
	void unlock_slow(System& sys);

	Atomic<Address> word_;
};

} // namespace Thor

#endif // THOR_LOCK_H