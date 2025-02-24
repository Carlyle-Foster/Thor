#ifndef THOR_MAP_H
#define THOR_MAP_H
#include "util/allocator.h"
#include "util/hash.h"

namespace Thor {

template<typename K, typename V>
struct Map {
	using H = Hash;
	constexpr Map(Allocator& allocator)
		: allocator_{allocator}
	{
	}
	Map(Map&& other)
		: allocator_{other.allocator_}
		, ks_{exchange(other.ks_, nullptr)}
		, vs_{exchange(other.vs_, nullptr)}
		, hs_{exchange(other.hs_, nullptr)}
		, length_{exchange(other.length_, 0)}
		, capacity_{exchange(other.capacity_, 0)}
	{
	}
	Map& operator=(Map&& other) {
		return *new (drop(), Nat{}) Map{move(other)};
	}
	void reset() {
		drop();
		length_ = 0;
		capacity_ = 0;
		ks_ = nullptr;
		vs_ = nullptr;
		hs_ = nullptr;
	}
	~Map() { drop(); }
	[[nodiscard]] THOR_FORCEINLINE constexpr auto length() const { return length_; }
	[[nodiscard]] THOR_FORCEINLINE constexpr auto capacity() const { return capacity_; }
	struct Tuple {
		const K& k;
		V&       v;
	};
	Maybe<Tuple> find(const K& k) {
		if (length_ == 0) return {};
		auto h = hash(k);
		h |= h == 0;
		auto q = capacity_ - 1;
		auto m = h & q;
		while (hs_[m] != 0) {
			if (ks_[m] == k) {
				return Tuple { ks_[m], vs_[m] };
			}
			m = (m + 1) & q;
		}
		return {};
	}
	Bool insert(K k, V v) {
		if (length_ >= capacity_ / 2) {
			if (!expand()) {
				return false;
			}
		}
		auto h = hash(k);
		h |= h == 0;
		assign(ks_, vs_, hs_, forward<K>(k), forward<V>(v), h, capacity_);
		length_++;
		return true;
	}
	[[nodiscard]] THOR_FORCEINLINE constexpr Allocator& allocator() const {
		return allocator_;
	}
	struct Iterator {
		constexpr Iterator(Map& map, Ulen n)
			: map_{map}
			, n_{n}
		{
			while (n_ < map_.capacity_ && map_.hs_[n_] == 0) n_++;
		}
		THOR_FORCEINLINE constexpr Tuple operator*() {
			return { map_.ks_[n_], map_.vs_[n_] };
		}
		constexpr Iterator& operator++() {
			do ++n_; while (n_ < map_.capacity_ && map_.hs_[n_] == 0);
			return *this;
		}
		[[nodiscard]] THOR_FORCEINLINE friend Bool operator==(const Iterator& lhs, const Iterator& rhs) { return lhs.n_ == rhs.n_; }
		[[nodiscard]] THOR_FORCEINLINE friend Bool operator!=(const Iterator& lhs, const Iterator& rhs) { return lhs.n_ != rhs.n_; }
	private:
		Map& map_;
		Ulen n_;
	};

	THOR_FORCEINLINE constexpr Iterator begin() { return Iterator{*this, 0}; }
	THOR_FORCEINLINE constexpr Iterator end() { return Iterator{*this, capacity_}; }

private:
	friend struct Iterator;

	static Uint64 assign(K* ks, V* vs, H* hs, K&& k, V&& v, H h, Ulen capacity) {
		auto q = capacity - 1;
		auto m = h & q;
		while (hs[m] != 0) {
			if (ks[m] == k) {
				ks[m] = forward<K>(k);
				vs[m] = forward<V>(v);
				hs[m] = h;
				return m;
			}
			m = (m + 1) & q;
		}
		ks[m] = forward<K>(k);
		vs[m] = forward<V>(v);
		hs[m] = h;
		return m;
	}
	Bool expand() {
		const auto old_capacity = capacity_;
		const auto new_capacity = (old_capacity ? old_capacity : 1) * 2;
		auto ks = allocator_.allocate<K>(new_capacity, false);
		auto vs = allocator_.allocate<V>(new_capacity, false);
		auto hs = allocator_.allocate<H>(new_capacity, true);
		for (Ulen i = 0; i < old_capacity; i++) if (hs_[i]) {
			assign(ks, vs, hs, move(ks_[i]), move(vs_[i]), hs_[i], new_capacity);
		}
		drop();
		ks_ = ks;
		vs_ = vs;
		hs_ = hs;
		capacity_ = new_capacity;
		return true;
	}
	Map* drop() {
		for (Ulen i = 0; i < capacity_; i++) if (hs_[i]) {
			ks_[i].~K();
			vs_[i].~V();
		}
		allocator_.deallocate(ks_, capacity_);
		allocator_.deallocate(vs_, capacity_);
		allocator_.deallocate(hs_, capacity_);
		return this;
	}
	Allocator& allocator_;
	K*         ks_       = nullptr;
	V*         vs_       = nullptr;
	Uint64*    hs_       = nullptr;
	Ulen       length_   = 0;
	Ulen       capacity_ = 0;
};

} // Thor

#endif // THOR_MAP_H