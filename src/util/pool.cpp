#include "util/pool.h"

namespace Thor {

Maybe<Pool> Pool::create(Allocator& allocator, Ulen size, Ulen capacity) {
	// Ensure capacity is a multiple of 32
	capacity = ((capacity + 31) / 32) * 32;
	auto data = allocator.allocate<Uint8>(size * capacity, false);
	if (!data) {
		return {};
	}
	auto used = allocator.allocate<Uint32>(capacity / 32, true);
	if (!used) {
		allocator.deallocate(data, size * capacity);
		return {};
	}
	return Pool {
		allocator,
		size,
		capacity,
		data,
		used
	};
}

Pool::Pool(Pool&& other)
	: allocator_{other.allocator_}
	, length_{exchange(other.length_, 0)}
	, size_{exchange(other.size_, 0)}
	, capacity_{exchange(other.capacity_, 0)}
	, data_{exchange(other.data_, nullptr)}
	, used_{exchange(other.used_, nullptr)}
{
}

Maybe<PoolRef> Pool::allocate() {
	const auto n_words = Uint32(capacity_ / 32);
	for (Uint32 w_index = 0; w_index < n_words; w_index++) {
		for (Uint32 b_index = 0; b_index < 32; b_index++) {
			const auto mask = 1_u32 << b_index;
			if ((used_[w_index] & mask) == 0) {
				// Mark as used and return index to it.
				used_[w_index] |= mask;
				length_++;
				return PoolRef { w_index * 32 + b_index };
			}
		}
	}
	return {}; // Out of memory.
}

void Pool::deallocate(PoolRef ref) {
	const auto w_index = ref.index / 64;
	const auto b_index = ref.index % 64;
	used_[w_index] &= ~(1_u64 << b_index);
	length_--;
}

} // namespace Thor