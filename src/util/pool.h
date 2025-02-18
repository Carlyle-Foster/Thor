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
struct Pool {
	static Maybe<Pool> create(Allocator& allocator, Ulen size, Ulen capacity);

	Pool(Pool&& other);
	constexpr ~Pool() { drop(); }

	constexpr auto length() const { return length_; }
	constexpr auto is_empty() const { return length_ == 0; }

	constexpr Pool(const Pool&) = delete;
	constexpr Pool& operator=(const Pool&) = delete;

	Pool& operator=(Pool&& other) {
		return *new (drop(), Nat{}) Pool{move(other)};
	}

	Maybe<PoolRef> allocate();
	void deallocate(PoolRef ref);

	constexpr auto operator[](this auto&& self, PoolRef ref) {
		return self.data_ + self.size_ * ref.index;
	}

private:
	constexpr Pool(Allocator& allocator, Ulen size, Ulen capacity, Uint8* data, Uint32* used)
		: allocator_{allocator}
		, length_{0}
		, size_{size}
		, capacity_{capacity}
		, data_{data}
		, used_{used}
	{
	}

	Pool* drop() {
		allocator_.deallocate(data_, size_ * capacity_);
		allocator_.deallocate(used_, capacity_ / 32);
		return this;
	}

	Allocator& allocator_;
	Ulen       length_;    // # of objects in the pool
	Ulen       size_;      // Size of an object in the pool
	Ulen       capacity_;  // Always a multiple of 32 (max # of objects in pool)
	Uint8*     data_;      // Object memory
	Uint32*    used_;      // Bitset where bit N indicates object N is in-use or not.
};

}

#endif // THOR_POOL_H