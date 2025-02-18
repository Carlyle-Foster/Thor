#include "util/slab.h"

namespace Thor {

Maybe<SlabRef> Slab::allocate() {
	const auto n_caches = caches_.length();
	for (Ulen i = 0; i < n_caches; i++) {
		if (auto& cache = caches_[i]; cache.is_valid()) {
			if (auto c_ref = cache->allocate()) {
				return SlabRef { Uint32(i * capacity_) + c_ref->index };
			}
		}
	}
	auto pool = Pool::create(caches_.allocator(), size_, capacity_);
	if (!pool) {
		return {};
	}
	// Search for an empty Pool in the caches array
	for (Ulen i = 0; i < n_caches; i++) {
		if (auto& cache = caches_[i]; !cache.is_valid()) {
			cache = move(*pool);
			return allocate();
		}
	}
	// No empty Pool in caches array, append a new Pool.
	if (!caches_.push_back(move(*pool))) {
		return {};
	}
	return allocate();
}

void Slab::deallocate(SlabRef slab_ref) {
	const auto cache_idx = Uint32(slab_ref.index / capacity_);
	const auto cache_ref = Uint32(slab_ref.index % capacity_);
	auto* cache = &caches_[cache_idx];
	(*cache)->deallocate(PoolRef { cache_ref });
	while (!caches_.is_empty() && (*cache)->is_empty()) {
		if (cache != &caches_.last()) {
			cache->reset();
			break;
		}
		caches_.pop_back();
		cache = &caches_.last();
		if (!cache->is_valid()) {
			break;
		}
	}
}

} // namespace Thor