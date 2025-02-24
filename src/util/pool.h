#ifndef THOR_POOL_H
#define THOR_POOL_H
// #include "util/types.h"
#include "util/maybe.h"
#include "util/allocator.h"

namespace Thor {

struct PoolRef {
	Uint32 index;
};

struct Allocator;

// Pool allocator, used to allocate objects of a fixed size. The pool also has a
// fixed capacity (# of objects). Does not communicate using pointers but rather
// PoolRef (plain typed index). Allocate object with allocate(), deallocate with
// deallocate(). The address (pointer) of the object can be looked-up by passing
// the PoolRef to operator[] like a key.
struct Stream;

struct Pool {
	static Maybe<Pool> create(Allocator& allocator, Ulen size, Ulen capacity);

	static Maybe<Pool> load(Allocator& allocator, Stream& stream);
	Bool save(Stream& stream) const;

	Pool(Pool&& other);
	~Pool() { drop(); }

	[[nodiscard]] THOR_FORCEINLINE constexpr auto length() const { return length_; }
	[[nodiscard]] THOR_FORCEINLINE constexpr auto is_empty() const { return length_ == 0; }

	constexpr Pool(const Pool&) = delete;
	constexpr Pool& operator=(const Pool&) = delete;

	Pool& operator=(Pool&& other) {
		return *new (drop(), Nat{}) Pool{move(other)};
	}

	Maybe<PoolRef> allocate();
	void deallocate(PoolRef ref);

	THOR_FORCEINLINE constexpr auto operator[](PoolRef ref) { return data_ + size_ * ref.index; }
	THOR_FORCEINLINE constexpr auto operator[](PoolRef ref) const { return data_ + size_ * ref.index; }

private:
	using Word = Uint64;
	static constexpr const auto BITS = Uint32(sizeof(Word) * 8);
	constexpr Pool(Allocator& allocator, Ulen size, Ulen length, Ulen capacity, Uint8* data, Word* used)
		: allocator_{allocator}
		, size_{size}
		, length_{length}
		, capacity_{capacity}
		, data_{data}
		, used_{used}
		, last_{0}
	{
	}

	Pool* drop() {
		allocator_.deallocate(data_, size_ * capacity_);
		allocator_.deallocate(used_, capacity_ / BITS);
		return this;
	}

	Allocator& allocator_;
	Ulen       size_;      // Size of an object in the pool
	Ulen       length_;    // # of objects in the pool
	Ulen       capacity_;  // Always a multiple of 32 (max # of objects in pool)
	Uint8*     data_;      // Object memory
	Word*      used_;      // Bitset where bit N indicates object N is in-use or not.
	Uint32     last_;      // Last w_index
};

}

#endif // THOR_POOL_H