#ifndef THOR_SYSTEM_H
#define THOR_SYSTEM_H
#include "util/string.h"

namespace Thor {

struct System;

struct Filesystem {
	struct File;
	enum class Access : Uint8 { RD, WR };
	File* (*open)(const System& sys, StringView name, Access access);
	void (*close)(const System& sys, File* file);
	Uint64 (*read)(const System& sys, File* file, Uint64 offset, Slice<Uint8> data);
	Uint64 (*write)(const System& sys, File* file, Uint64 offset, Slice<const Uint8> data);
	Uint64 (*tell)(const System& sys, File* file);
};

struct Heap {
	void *(*allocate)(const System& sys, Ulen len, Bool zero);
	void (*deallocate)(const System& sys, void* addr, Ulen len);
};

struct Console {
	void (*write)(const System& sys, StringView data);
	void (*flush)(const System& sys);
};

struct System {
	const Filesystem& filesystem;
	const Heap&       heap;
	const Console&    console;
};

} // namespace Thor

#endif // THOR_SYSTEM_H