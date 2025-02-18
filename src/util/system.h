#ifndef THOR_SYSTEM_H
#define THOR_SYSTEM_H
#include "util/string.h"

namespace Thor {

struct System;

struct Filesystem {
	struct File;
	enum class Access : Uint8 { RD, WR };
	File* (*open)(System& sys, StringView name, Access access);
	void (*close)(System& sys, File* file);
	Uint64 (*read)(System& sys, File* file, Uint64 offset, Slice<Uint8> data);
	Uint64 (*write)(System& sys, File* file, Uint64 offset, Slice<const Uint8> data);
	Uint64 (*tell)(System& sys, File* file);
};

struct Heap {
	void *(*allocate)(System& sys, Ulen len, Bool zero);
	void (*deallocate)(System& sys, void* addr, Ulen len);
};

struct Console {
	void (*write)(System& sys, StringView data);
	void (*flush)(System& sys);
};

struct System {
	constexpr System(const Filesystem& filesystem,
	                 const Heap&       heap,
	                 const Console&    console)
		: filesystem{filesystem}
		, heap{heap}
		, console{console}
		, allocator{*this}
	{
	}
	const Filesystem& filesystem;
	const Heap&       heap;
	const Console&    console;
	SystemAllocator   allocator;
};

} // namespace Thor

#endif // THOR_SYSTEM_H