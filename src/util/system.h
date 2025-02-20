#ifndef THOR_SYSTEM_H
#define THOR_SYSTEM_H
#include "util/string.h"

namespace Thor {

struct System;

struct Filesystem {
	struct File;
	struct Directory;

	struct Item {
		enum class Kind {
			FILE,
			LINK,
			DIR,
		};
		StringView name;
		Kind       kind;
	};

	enum class Access : Uint8 { RD, WR };
	File* (*open_file)(System& sys, StringView name, Access access);
	void (*close_file)(System& sys, File* file);
	Uint64 (*read_file)(System& sys, File* file, Uint64 offset, Slice<Uint8> data);
	Uint64 (*write_file)(System& sys, File* file, Uint64 offset, Slice<const Uint8> data);
	Uint64 (*tell_file)(System& sys, File* file);

	Directory* (*open_dir)(System& sys, StringView name);
	void (*close_dir)(System& sys, Directory*);
	Bool (*read_dir)(System& sys, Directory*, Item& item);
};

struct Heap {
	void *(*allocate)(System& sys, Ulen len, Bool zero);
	void (*deallocate)(System& sys, void* addr, Ulen len);
};

struct Console {
	void (*write)(System& sys, StringView data);
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