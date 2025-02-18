#include "util/allocator.h"
#include "util/system.h"

namespace Thor {

#define ASSERT(...)

void Allocator::memzero(Address addr, Ulen len) {
	const auto n_words = len / sizeof(Uint64);
	const auto n_bytes = len % sizeof(Uint64);
	const auto dst_w = reinterpret_cast<Uint64*>(addr);
	const auto dst_b = reinterpret_cast<Uint8*>(dst_w + n_words);
	for (Ulen i = 0; i < n_words; i++) dst_w[i] = 0_u64;
	for (Ulen i = 0; i < n_bytes; i++) dst_b[i] = 0_u8;
}

void Allocator::memcopy(Address dst, Address src, Ulen len) {
	const auto dst_b = reinterpret_cast<Uint8*>(dst);
	const auto src_b = reinterpret_cast<const Uint8*>(src);
	for (Ulen i = 0; i < len; i++) {
		dst_b[i] = src_b[i];
	}
}

Bool ArenaAllocator::owns(Address addr, Ulen len) const {
	return addr >= region_.beg && (addr + len <= region_.end);
}

Address ArenaAllocator::alloc(Ulen new_len, Bool zero) {
	new_len = round(new_len);
	if (cursor_ + new_len > region_.end) {
		return 0;
	}
	auto addr = cursor_;
	cursor_ += new_len;
	if (zero) {
		memzero(addr, new_len);
	}
	return addr;
}

void ArenaAllocator::free(Address addr, Ulen old_len) {
	old_len = round(old_len);
	ASSERT(addr >= region_.beg);
	if (addr + old_len == cursor_) {
		cursor_ -= old_len;
	}
}

void ArenaAllocator::shrink(Address addr, Ulen old_len, Ulen new_len) {
	old_len = round(old_len);
	new_len = round(new_len);
	ASSERT(addr >= region_.beg);
	if (addr + old_len == cursor_) {
		cursor_ -= old_len;
		cursor_ += new_len;
	}
}

Address ArenaAllocator::grow(Address src_addr, Ulen old_len, Ulen new_len, Bool zero) {
	old_len = round(old_len);
	new_len = round(new_len);
	ASSERT(src_addr >= region_.beg);
	const auto delta = new_len - old_len;
	if (src_addr + old_len == cursor_) {
		if (cursor_ + delta >= region_.end) {
			// Out of memory.
			return 0;
		}
		if (zero) {
			memzero(src_addr + old_len, delta);
		}
		cursor_ += delta;
		return src_addr;
	}
	const auto dst_addr = alloc(new_len, false);
	if (!dst_addr) {
		// Out of memory.
		return 0;
	}
	memcopy(dst_addr, src_addr, old_len);
	if (zero) {
		memzero(dst_addr + old_len, delta);
	}
	free(src_addr, old_len);
	return dst_addr;
}

TemporaryAllocator::~TemporaryAllocator() {
	for (auto node = tail_; node; /**/) {
		const auto addr = reinterpret_cast<Address>(node);
		const auto prev = node->prev_;
		allocator_.free(addr, sizeof(Block) + node->arena_.length());
		node = prev;
	}
}

Bool TemporaryAllocator::add(Ulen len) {
	// 16 KiB block size, double until large enough for 'len'.
	Ulen block_size = 16384;
	while (block_size < len) {
		block_size *= 2;
	}
	const auto addr = allocator_.alloc(sizeof(Block) + block_size, false);
	if (!addr) {
		return false;
	}
	const auto ptr = reinterpret_cast<void*>(addr);
	const auto node = new (ptr, Nat{}) Block{block_size};
	if (tail_) {
		tail_->next_ = node;
		node->prev_ = tail_;
		tail_ = node;
	} else {
		tail_ = node;
		head_ = node;
	}
	tail_ = node;
	return true;
}

Address TemporaryAllocator::alloc(Ulen new_len, Bool zero) {
	new_len = round(new_len);
	if (!tail_ && !add(new_len)) {
		return 0;
	}
	if (const auto addr = tail_->arena_.alloc(new_len, zero)) {
		return addr;
	}
	if (!add(new_len)) {
		return 0;
	}
	return alloc(new_len, zero);
}

void TemporaryAllocator::free(Address addr, Ulen old_len) {
	for (auto node = head_; node; node = node->next_) {
		if (node->arena_.owns(addr, old_len)) {
			node->arena_.free(addr, old_len);
			return;
		}
	}
}

void TemporaryAllocator::shrink(Address addr, Ulen old_len, Ulen new_len) {
	for (auto node = head_; node; node = node->next_) {
		if (node->arena_.owns(addr, old_len)) {
			node->arena_.shrink(addr, old_len, new_len);
			return;
		}
	}
}

Address TemporaryAllocator::grow(Address old_addr, Ulen old_len, Ulen new_len, Bool zero) {
	// Attempt in-place growth.
	for (auto node = head_; node; node = node->next_) {
		if (!node->arena_.owns(old_addr, old_len)) {
			continue;
		}
		if (auto new_addr = node->arena_.grow(old_addr, old_len, new_len, zero)) {
			return new_addr;
		}
	}
	// Could not grow in-place, allocate fresh memory.
	const auto new_addr = alloc(new_len, false);
	if (!new_addr) {
		return 0;
	}
	// Copy the old part over.
	memcopy(new_addr, old_addr, old_len);
	if (zero) {
		// Zero the remainder part if requested.
		memzero(new_addr + old_len, new_len - old_len);
	}
	free(old_addr, old_len);
	return new_addr;
}

Address SystemAllocator::alloc(Ulen new_len, Bool zero) {
	if (const auto ptr = sys_.heap.allocate(sys_, new_len, zero)) {
		return reinterpret_cast<Address>(ptr);
	}
	return 0;
}

void SystemAllocator::free(Address addr, Ulen old_len) {
	const auto ptr = reinterpret_cast<void *>(addr);
	sys_.heap.deallocate(sys_, ptr, old_len);
}

void SystemAllocator::shrink(Address, Ulen, Ulen) {
	// no-op
}

Address SystemAllocator::grow(Address old_addr, Ulen old_len, Ulen new_len, Bool zero) {
	const auto new_ptr = sys_.heap.allocate(sys_, new_len, false);
	if (!new_ptr) {
		return 0;
	}
	const auto new_addr = reinterpret_cast<Address>(new_ptr);
	memcopy(new_addr, old_addr, old_len);
	if (zero) {
		memzero(new_addr + old_len, new_len - old_len);
	}
	const auto old_ptr = reinterpret_cast<void *>(old_addr);
	sys_.heap.deallocate(sys_, old_ptr, old_len);
	return new_addr;
}

} // namespace Thor