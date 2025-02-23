#include "util/pool.h"
#include "util/slice.h"
#include "util/stream.h"

namespace Thor {

// The serialized representation of the Pool
struct PoolHeader {
	Uint8  magic[4]; // 'Pool'
	Uint32 version;
	Uint64 length;
	Uint64 size;
	Uint64 capacity;
};
// Following the header:
// 	Uint32 used[PoolHeader::capacity / 32]
// 	Uint8  data[PoolHeader::size * PoolHeader::capacity]
static_assert(sizeof(PoolHeader) == 32);

Maybe<Pool> Pool::create(Allocator& allocator, Ulen size, Ulen capacity) {
	// Ensure capacity is a multiple of 32
	capacity = ((capacity + 31) / 32) * 32;
	auto data = allocator.allocate<Uint8>(size * capacity, true);
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
		0_ulen,
		capacity,
		data,
		used
	};
}

Maybe<Pool> Pool::load(Allocator& allocator, Stream& stream) {
	PoolHeader header;
	if (!stream.read(Slice{&header, 1}.cast<Uint8>())) {
		return {};
	}
	if (Slice<const Uint8>{header.magic} != Slice{"pool"}.cast<const Uint8>()) {
		return {};
	}
	if (header.version != 1) {
		return {};
	}
	const auto n_words = header.capacity / 32;
	const auto n_bytes = header.size * header.capacity;
	auto used = allocator.allocate<Uint32>(n_words, false);
	auto data = allocator.allocate<Uint8>(n_bytes, false);
	if (!used || !data) {
		allocator.deallocate(used, n_words);
		allocator.deallocate(data, n_bytes);
		return {};
	}
	if (!stream.read(Slice{used, n_words}.cast<Uint8>()) ||
	    !stream.read(Slice{data, n_bytes}.cast<Uint8>()))
	{
		allocator.deallocate(used, n_words);
		allocator.deallocate(data, n_bytes);
		return {};
	}
	return Pool {
		allocator,
		Ulen(header.size),
		Ulen(header.length),
		Ulen(header.capacity),
		data,
		used
	};
}

Bool Pool::save(Stream& stream) const {
	PoolHeader header = {
		.magic    = { 'p', 'o', 'o', 'l' },
		.version  = Uint64(1),
		.length   = Uint64(length_),
		.size     = Uint64(size_),
		.capacity = Uint64(capacity_),
	};
	return stream.write(Slice{&header, 1}.cast<const Uint8>())
	    && stream.write(Slice{used_, capacity_ / 32}.cast<const Uint8>())
	    && stream.write(Slice{data_, size_ * capacity_}.cast<const Uint8>());
}

Pool::Pool(Pool&& other)
	: allocator_{other.allocator_}
	, size_{exchange(other.size_, 0)}
	, length_{exchange(other.length_, 0)}
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