#ifndef THOR_ALLOCATOR_H
#define THOR_ALLOCATOR_H
#include "util/types.h"
#include "util/forward.h"
#include "util/exchange.h"
namespace Thor {

struct System;

struct Allocator {
	static void memzero(Address addr, Ulen len);
	static void memcopy(Address dst, Address src, Ulen len);

	static constexpr Ulen round(Ulen len) {
		return ((len + 16 - 1) / 16) * 16;
	}

	virtual Address alloc(Ulen length, Bool zero) = 0;
	virtual void free(Address addr, Ulen old_len) = 0;
	virtual void shrink(Address addr, Ulen old_len, Ulen new_len) = 0;
	virtual Address grow(Address addr, Ulen old_len, Ulen new_len, Bool zero) = 0;

	// Helper functions when working with typed data
	template<typename T>
	T* allocate(Ulen count, Bool zero) {
		if (auto addr = alloc(count * sizeof(T), zero)) {
			return reinterpret_cast<T*>(addr);
		}
		return nullptr;
	}

	template<typename T>
	void deallocate(T* ptr, Ulen count) {
		auto addr = reinterpret_cast<Address>(ptr);
		free(addr, count * sizeof(T));
	}

	// Helpers for allocating+construct and destruct+deallocate objects.
	template<typename T, typename... Ts>
	T* create(Ts&&... args) {
		if (auto data = allocate<T>(1, false)) {
			return new (data, Nat{}) T{forward<Ts>(args)...};
		}
		return nullptr;
	}

	template<typename T>
	void destroy(T* obj) {
		obj->~T();
		deallocate(obj, 1);
	}
};

struct ArenaAllocator : Allocator {
	ArenaAllocator(Address base, Ulen length);
	ArenaAllocator(const ArenaAllocator&) = delete;
	ArenaAllocator(ArenaAllocator&& other) = delete;
	Bool owns(Address addr, Ulen len) const;
	virtual Address alloc(Ulen new_len, Bool zero);
	virtual void free(Address addr, Ulen old_len);
	virtual void shrink(Address addr, Ulen old_len, Ulen new_len);
	virtual Address grow(Address addr, Ulen old_len, Ulen new_len, Bool zero);
	constexpr Ulen length() const {
		return region_.end - region_.beg;
	}
private:
	struct { Address beg, end; } region_;
	Address                      cursor_;
};

template<Ulen E>
struct InlineAllocator : ArenaAllocator {
	constexpr InlineAllocator()
		: ArenaAllocator{reinterpret_cast<Address>(data_), E}
	{
	}
	InlineAllocator(const InlineAllocator&) = delete;
	InlineAllocator(InlineAllocator&&) = delete;
private:
	alignas(16) Uint8 data_[E];
};

struct TemporaryAllocator : Allocator {
	TemporaryAllocator(const TemporaryAllocator&) = delete;
	TemporaryAllocator(TemporaryAllocator&& other)
		: allocator_{other.allocator_}
		, head_{exchange(other.head_, nullptr)}
		, tail_{exchange(other.tail_, nullptr)}
	{
	}
	constexpr TemporaryAllocator(Allocator& allocator)
		: allocator_{allocator}
	{
	}
	~TemporaryAllocator();
	virtual Address alloc(Ulen new_len, Bool zero);
	virtual void free(Address addr, Ulen old_len);
	virtual void shrink(Address addr, Ulen old_len, Ulen new_len);
	virtual Address grow(Address addr, Ulen old_len, Ulen new_len, Bool zero);
private:
	// Add a new block to the temporary allocator.
	Bool add(Ulen len);
	struct Block {
		Block(Ulen length)
			: arena_{reinterpret_cast<Address>(data_), length}
		{
		}
		ArenaAllocator arena_;
		Block*         prev_ = nullptr;
		Block*         next_ = nullptr;
		Uint8          data_[];
	};
	Allocator& allocator_;
	Block*     head_ = nullptr;
	Block*     tail_ = nullptr;
};

template<Ulen E>
struct ScratchAllocator : Allocator {
	constexpr ScratchAllocator(Allocator& allocator)
		: temporary_{allocator}
	{
	}
	ScratchAllocator(const ScratchAllocator&) = delete;
	ScratchAllocator(ScratchAllocator&&) = delete;
	virtual Address alloc(Ulen new_len, Bool zero) {
		if (auto addr = inline_.alloc(new_len, zero)) return addr;
		return temporary_.alloc(new_len, zero);
	}
	virtual void free(Address addr, Ulen old_len) {
		if (inline_.owns(addr, old_len)) {
			inline_.free(addr, old_len);
		} else {
			temporary_.free(addr, old_len);
		}
	}
	virtual void shrink(Address addr, Ulen old_len, Ulen new_len) {
		if (inline_.owns(addr, old_len)) {
			inline_.shrink(addr, old_len, new_len);
		} else {
			temporary_.shrink(addr, old_len, new_len);
		}
	}
	virtual Address grow(Address old_addr, Ulen old_len, Ulen new_len, Bool zero) {
		if (inline_.owns(old_addr, old_len)) {
			if (auto new_addr = inline_.grow(old_addr, old_len, new_len, zero)) {
				return new_addr;
			} else if (auto new_addr = temporary_.alloc(new_len, false)) {
				// Could not grow in-place inside the inline allocator. Going to have to
				// move the result to the temporary allocator instead.
				memcopy(new_addr, old_addr, old_len);
				if (zero) {
					memzero(new_addr + old_len, new_len - old_len);
				}
				inline_.free(old_addr, old_len);
				return new_addr;
			} else {
				return 0;
			}
		}
		return temporary_.grow(old_addr, old_len, new_len, zero);
	}
private:
	InlineAllocator<E> inline_;
	TemporaryAllocator temporary_;
};

struct SystemAllocator : Allocator {
	constexpr SystemAllocator(System& sys)
		: sys_{sys}
	{
	}
	virtual Address alloc(Ulen new_len, Bool zero);
	virtual void free(Address addr, Ulen old_len);
	virtual void shrink(Address, Ulen, Ulen);
	virtual Address grow(Address addr, Ulen old_len, Ulen new_len, Bool zero);
private:
	System& sys_;
};

} // namespace Thor

#endif // THOR_ALLOCATOR_H