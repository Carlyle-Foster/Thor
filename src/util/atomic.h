#ifndef THOR_ATOMIC_H
#define THOR_ATOMIC_H
#include <atomic> // TODO(dweiler): replace

#include "util/forward.h"
#include "util/types.h"

namespace Thor {

using MemoryOrder = std::memory_order; // TODO(dweiler): replace

template<typename T>
struct Atomic {
	Atomic() = default;
	constexpr Atomic(T initial)
		: value_{forward<T>(initial)}
	{
	}

	THOR_FORCEINLINE T load(MemoryOrder order = MemoryOrder::seq_cst) const {
		return value_.load(order);
	}

	THOR_FORCEINLINE void store(T desired, MemoryOrder order = MemoryOrder::seq_cst) {
		value_.store(desired, order);
	}

	THOR_FORCEINLINE T exchange(T desired, MemoryOrder order = MemoryOrder::seq_cst) {
		return value_.exchange(desired, order);
	}

	THOR_FORCEINLINE Bool compare_exchange_weak(T expected, T desired, MemoryOrder order = MemoryOrder::seq_cst) {
		T expected_or_actual = expected;
		return value_.compare_exchange_weak(expected_or_actual, desired, order);
	}

private:
	std::atomic<T> value_; // TODO(dweiler): replace
};

} // namespace Thor

#endif // THOR_ATOMIC_H