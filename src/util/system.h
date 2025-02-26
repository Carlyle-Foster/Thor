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

struct Process {
	// NOTE: assert should not return
	void (*assert)(System& sys, StringView msg, StringView file, Sint32 line);
};

struct Linker {
	struct Library;
	Library* (*load)(System& sys, StringView name);
	void (*close)(System& sys, Library* library);
	void (*(*link)(System& sys, Library* library, const char* symbol))(void);
};

struct Scheduler {
	struct Thread;
	struct Mutex;
	struct Cond;

	Thread* (*thread_start)(System& sys, void (*fn)(System& sys, void* user), void* user);
	void (*thread_join)(System& sys, Thread* thread);

	Mutex* (*mutex_create)(System& sys);
	void (*mutex_destroy)(System& sys, Mutex* mutex);
	void (*mutex_lock)(System& sys, Mutex* mutex);
	void (*mutex_unlock)(System& sys, Mutex* mutex);

	Cond* (*cond_create)(System& sys);
	void (*cond_destroy)(System& sys, Cond* cond);
	void (*cond_signal)(System& sys, Cond* cond);
	void (*cond_broadcast)(System& sys, Cond* cond);
	void (*cond_wait)(System& sys, Cond* cond, Mutex* mutex);

	void (*yield)(System& sys);
};

struct Chrono {
	Float64 (*monotonic_now)(System& sys);
	Float64 (*wall_now)(System& sys);
};

struct System {
private:
	SystemAllocator allocator_;
public:
	constexpr System(const Filesystem& filesystem,
	                 const Heap&       heap,
	                 const Console&    console,
	                 const Process&    process,
	                 const Linker&     linker,
	                 const Scheduler&  scheduler,
	                 const Chrono&     chrono)
		: allocator_{*this}
		, filesystem{filesystem}
		, heap{heap}
		, console{console}
		, process{process}
		, linker{linker}
		, scheduler{scheduler}
		, chrono{chrono}
		, allocator{allocator_}
	{
	}
	const Filesystem&  filesystem;
	const Heap&        heap;
	const Console&     console;
	const Process&     process;
	const Linker&      linker;
	const Scheduler&   scheduler;
	const Chrono&      chrono;
	TemporaryAllocator allocator;
};

} // namespace Thor

#endif // THOR_SYSTEM_H