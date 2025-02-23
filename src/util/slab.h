#ifndef THOR_SLAB_H
#define THOR_SLAB_H
#include "util/pool.h"
#include "util/array.h"

namespace Thor {

struct SlabRef {
	Uint32 index;
};

// Simple Slab allocator. Works exactly like Pool except doesn't have a fixed
// capacity. Instead you specify a per-pool fixed capacity and when the pool the
// slab uses is full, it allocates another pool with the same fixed capacity.
// These pools are typically called "caches" in the slab allocator literature.
// Does not communicate using pointers but rather SlabRef (plain typed index).
// Allocate object with allocate(), deallocate with deallocate(). The address
// (pointer) of the object can be looked-up by passing the SlabRef to operator[]
// like a key.
struct Stream;
struct Slab {
	constexpr Slab(Allocator& allocator, Ulen size, Ulen capacity)
		: caches_{allocator}
		, size_{size}
		, capacity_{capacity}
	{
	}
	static Maybe<Slab> load(Allocator& allocator, Stream& stream);
	Bool save(Stream& stream) const;
	Maybe<SlabRef> allocate();
	void deallocate(SlabRef slab_ref);
	constexpr Uint8* operator[](SlabRef slab_ref) {
		const auto cache_idx = Uint32(slab_ref.index / capacity_);
		const auto cache_ref = Uint32(slab_ref.index % capacity_);
		return (*caches_[cache_idx])[PoolRef { cache_ref }];
	}
	constexpr const Uint8* operator[](SlabRef slab_ref) const {
		const auto cache_idx = Uint32(slab_ref.index / capacity_);
		const auto cache_ref = Uint32(slab_ref.index % capacity_);
		return (*caches_[cache_idx])[PoolRef { cache_ref }];
	}
private:
	Slab(Array<Maybe<Pool>>&& caches, Ulen size, Ulen capacity)
		: caches_{move(caches)}
		, size_{size}
		, capacity_{capacity}
	{
	}
	Array<Maybe<Pool>> caches_;
	Ulen               size_;
	Ulen               capacity_;
};

} // namespace Thor

#endif // THOR_SLAB_H