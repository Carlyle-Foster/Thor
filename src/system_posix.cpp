#include "util/info.h"
#include "util/assert.h"

#if defined(THOR_HOST_PLATFORM_POSIX)

// Implementation of the System for POSIX systems
#include <sys/stat.h> // fstat, struct stat
#include <sys/mman.h> // PROT_READ, PROT_WRITE, MAP_PRIVATE, MAP_ANONYMOUS
#include <unistd.h> // open, close, pread, pwrite
#include <fcntl.h> // O_CLOEXEC, O_RDONLY, O_WRONLY
#include <dirent.h> // opendir, readdir, closedir
#include <string.h> // strlen
#include <dlfcn.h> // dlopen, dlclose, dlsym, RTLD_NOW
#include <stdio.h> // printf
#include <pthread.h> // pthread_create, pthread_join, pthread_sigblock
#include <signal.h> // sigset, sigfillset
#include <stdlib.h> // exit needed from libc since it calls destructors

#include "util/system.h"

namespace Thor {

static Filesystem::File* filesystem_open_file(System& sys, StringView name, Filesystem::Access access) {
	int flags = O_CLOEXEC;
	switch (access) {
	case Filesystem::Access::RD:
		flags |= O_RDONLY;
		break;
	case Filesystem::Access::WR:
		flags |= O_WRONLY;
		flags |= O_CREAT;
		flags |= O_TRUNC;
		break;
	}
	ScratchAllocator<1024> scratch{sys.allocator};
	auto path = scratch.allocate<char>(name.length() + 1, false);
	if (!path) {
		// Out of memory.
		return nullptr;
	}
	memcpy(path, name.data(), name.length());
	path[name.length()] = '\0';
	auto fd = open(path, flags, 0666);
	if (fd < 0) {
		return nullptr;
	}
	return reinterpret_cast<Filesystem::File*>(fd);
}

static void filesystem_close_file(System&, Filesystem::File* file) {
	close(reinterpret_cast<Address>(file));
}

static Uint64 filesystem_read_file(System&, Filesystem::File* file, Uint64 offset, Slice<Uint8> data) {
	auto fd = reinterpret_cast<Address>(file);
	auto v = pread(fd, data.data(), data.length(), offset);
	if (v < 0) {
		return 0;
	}
	return Uint64(v);
}

static Uint64 filesystem_write_file(System&, Filesystem::File* file, Uint64 offset, Slice<const Uint8> data) {
	auto fd = reinterpret_cast<Address>(file);
	auto v = pwrite(fd, data.data(), data.length(), offset);
	if (v < 0) {
		return 0;
	}
	return Uint64(v);
}

static Uint64 filesystem_tell_file(System&, Filesystem::File* file) {
	auto fd = reinterpret_cast<Address>(file);
	struct stat buf;
	if (fstat(fd, &buf) != -1) {
		return Uint64(buf.st_size);
	}
	return 0;
}

Filesystem::Directory* filesystem_open_dir(System& sys, StringView name) {
	ScratchAllocator<1024> scratch{sys.allocator};
	auto path = scratch.allocate<char>(name.length() + 1, false);
	if (!path) {
		return nullptr;
	}
	memcpy(path, name.data(), name.length());
	path[name.length()] = '\0';
	auto dir = opendir(path);
	return reinterpret_cast<Filesystem::Directory*>(dir);
}

void filesystem_close_dir(System&, Filesystem::Directory* handle) {
	auto dir = reinterpret_cast<DIR*>(handle);
	closedir(dir);
}

Bool filesystem_read_dir(System&, Filesystem::Directory* handle, Filesystem::Item& item) {
	auto dir = reinterpret_cast<DIR*>(handle);
	auto next = readdir(dir);
	using Kind = Filesystem::Item::Kind;
	while (next) {
		// Skip '.' and '..'
		while (next && next->d_name[0] == '.' && !(next->d_name[1 + (next->d_name[1] == '.')])) {
			next = readdir(dir);
		}
		if (!next) {
			return false;
		}
		switch (next->d_type) {
		case DT_LNK:
			item = { StringView { next->d_name, strlen(next->d_name) }, Kind::LINK };
			return true;
		case DT_DIR:
			item = { StringView { next->d_name, strlen(next->d_name) }, Kind::DIR };
			return true;
		case DT_REG:
			item = { StringView { next->d_name, strlen(next->d_name) }, Kind::FILE };
			return true;
		}
		next = readdir(dir);
	}
	return false;
}

extern const Filesystem STD_FILESYSTEM = {
	.open_file  = filesystem_open_file,
	.close_file = filesystem_close_file,
	.read_file  = filesystem_read_file,
	.write_file = filesystem_write_file,
	.tell_file  = filesystem_tell_file,
	.open_dir   = filesystem_open_dir,
	.close_dir  = filesystem_close_dir,
	.read_dir   = filesystem_read_dir,
};

static void* heap_allocate(System&, Ulen length, [[maybe_unused]] Bool zero) {
#if defined(THOR_CFG_USE_MALLOC)
	return zero ? calloc(length, 1) : malloc(length);
#else
	auto addr = mmap(nullptr,
	                 length,
	                 PROT_WRITE,
	                 MAP_PRIVATE | MAP_ANONYMOUS,
	                 -1,
	                 0);
	if (addr == MAP_FAILED) {
		return nullptr;
	}
	return addr;
#endif
}

static void heap_deallocate(System&, void *addr, [[maybe_unused]] Ulen length) {
#if defined(THOR_CFG_USE_MALLOC)
	free(addr);
#else
	munmap(addr, length);
#endif
}

extern const Heap STD_HEAP = {
	.allocate   = heap_allocate,
	.deallocate = heap_deallocate,
};

static void console_write(System&, StringView data) {
	write(1, data.data(), data.length());
}

extern const Console STD_CONSOLE = {
	.write = console_write,
};

static void process_assert(System& sys, StringView msg, StringView file, Sint32 line) {
	InlineAllocator<4096> data;
	StringBuilder builder{data};
	builder.put(file);
	builder.put(':');
	builder.put(line);
	builder.put(' ');
	builder.put("Assertion failure");
	builder.put(':');
	builder.put(' ');
	builder.put(msg);
	builder.put('\n');
	if (auto result = builder.result()) {
		sys.console.write(sys, *result);
	} else {
		sys.console.write(sys, StringView { "Out of memory" });
	}
	exit(3);
}

extern const Process STD_PROCESS = {
	.assert = process_assert,
};

static Linker::Library* linker_load(System&, StringView name) {
	// Search for the librart next to the executable first.
	InlineAllocator<1024> buf;
	StringBuilder path{buf};
	path.put("./");
	path.put(name);
	path.put('.');
	path.put("so");
	path.put('\0');
	auto result = path.result();
	if (!result) {
		return nullptr;
	}
	if (auto lib = dlopen(result->data(), RTLD_NOW)) {
		return static_cast<Linker::Library*>(lib);
	}
	// Skip the "./" in result to try the system path now.
	if (auto lib = dlopen(result->data() + 2, RTLD_NOW)) {
		return static_cast<Linker::Library*>(lib);
	}
	return nullptr;
}

static void linker_close(System&, Linker::Library* lib) {
	dlclose(static_cast<void*>(lib));
}

static void (*linker_link(System&, Linker::Library* lib, const char* sym))(void) {
	if (auto addr = dlsym(static_cast<void*>(lib), sym)) {
		return reinterpret_cast<void (*)(void)>(addr);
	}
	return nullptr;
}

extern const Linker STD_LINKER = {
	.load  = linker_load,
	.close = linker_close,
	.link  = linker_link
};

struct SystemThread {
	SystemThread(System& sys, void (*fn)(System&, void*), void* user)
		: sys{sys}
		, fn{fn}
		, user{user}
	{
	}
	System&   sys;
	void      (*fn)(System& sys, void* user);
	void*     user;
	pthread_t handle;
};

struct Mutex {
	pthread_mutex_t handle;
};

struct Cond {
	pthread_cond_t handle;
};

static void* scheduler_thread_proc(void* user) {
	auto thread = reinterpret_cast<SystemThread*>(user);
	thread->fn(thread->sys, thread->user);
	return nullptr;
}

static Scheduler::Thread* scheduler_thread_start(System& sys, void (*fn)(System& sys, void* user), void* user) {
	auto thread = sys.allocator.create<SystemThread>(sys, fn, user);
	if (!thread) {
		return nullptr;
	}

	// When constructing the thread we need to block all signals. Once the thread
	// is constructed it will inherit our signal mask. We do this because we don't
	// want any of our threads to be delivered signals as they're a source of many
	// dataraces and deadlocks. Many applications do this wrong and block signals
	// at the start of the thread but this has a datarace where a signal can still
	// be delivered to the thread after it's constructed but before the thread has
	// executed the code to block signals.
	sigset_t nset;
	sigset_t oset;
	sigfillset(&nset);
	if (pthread_sigmask(SIG_SETMASK, &nset, &oset) != 0) {
		return nullptr;
	}

	// The thread can now be constructed and during construction it will inherit
	// the signal mask set in this process, which in this case is one that blocks
	// all signals.
	if (pthread_create(&thread->handle, nullptr, scheduler_thread_proc, thread) != 0) {
		// Restore the previous signal mask. This cannot fail since [oset] is derived
		// from the previous (valid) signal mask state.
		pthread_sigmask(SIG_SETMASK, &oset, nullptr);
		sys.allocator.destroy<SystemThread>(thread);
		return nullptr;
	}

	// Restore the previous signal mask. This cannot fail since [oset] is derived
	// from the previous (valid) signal mask state.
	pthread_sigmask(SIG_SETMASK, &oset, nullptr);

	return reinterpret_cast<Scheduler::Thread*>(thread);
}

static void scheduler_thread_join(System& sys, Scheduler::Thread* thr) {
	auto thread = reinterpret_cast<SystemThread*>(thr);
	THOR_ASSERT(sys, &thread->sys == &sys);
	pthread_join(thread->handle, nullptr);
	sys.allocator.destroy(thread);
}

static Scheduler::Mutex* scheduler_mutex_create(System& sys) {
	// There is no specified default type on POSIX. While most should default to
	// non-recursive (i.e normal) mutexes, we cannot be sure and so to ensure we
	// only get non-recursive mutexes just be explicit and request it.
	pthread_mutexattr_t attr;
	if (pthread_mutexattr_init(&attr) != 0) {
		return nullptr;
	}
	if (pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_NORMAL) != 0) {
		pthread_mutexattr_destroy(&attr);
		return nullptr;
	}

	auto mutex = sys.allocator.create<Mutex>();
	if (!mutex) {
		pthread_mutexattr_destroy(&attr);
		return nullptr;
	}

	if (pthread_mutex_init(&mutex->handle, &attr) != 0) {
		sys.allocator.destroy(mutex);
		pthread_mutexattr_destroy(&attr);
		return nullptr;
	}

	pthread_mutexattr_destroy(&attr);

	return reinterpret_cast<Scheduler::Mutex*>(mutex);
}

static void scheduler_mutex_destroy([[maybe_unused]] System& sys, Scheduler::Mutex* m) {
	auto mutex = reinterpret_cast<Mutex*>(m);
	if (pthread_mutex_destroy(&mutex->handle) != 0) {
		THOR_ASSERT(sys, !"Could not destroy mutex");
	}
	sys.allocator.destroy(mutex);
}


static void scheduler_mutex_lock([[maybe_unused]] System& sys, Scheduler::Mutex* m) {
	auto mutex = reinterpret_cast<Mutex*>(m);
	if (pthread_mutex_lock(&mutex->handle) != 0) {
		THOR_ASSERT(sys, !"Could not lock mutex");
	}
}

static void scheduler_mutex_unlock([[maybe_unused]] System& sys, Scheduler::Mutex* m) {
	auto mutex = reinterpret_cast<Mutex*>(m);
	if (pthread_mutex_unlock(&mutex->handle) != 0) {
		THOR_ASSERT(sys, !"Could not unlock mutex");
	}
}

static Scheduler::Cond* scheduler_cond_create(System& sys) {
	auto cond = sys.allocator.create<Cond>();
	if (!cond) {
		return nullptr;
	}
	if (pthread_cond_init(&cond->handle, nullptr) != 0) {
		sys.allocator.destroy(cond);
		return nullptr;
	}
	return reinterpret_cast<Scheduler::Cond*>(cond);
}

static void scheduler_cond_destroy([[maybe_unused]] System& sys, Scheduler::Cond* c) {
	auto cond = reinterpret_cast<Cond*>(c);
	if (pthread_cond_destroy(&cond->handle) != 0) {
		THOR_ASSERT(sys, !"Could not destroy condition variable");
	}
	sys.allocator.destroy(cond);
}

static void scheduler_cond_signal([[maybe_unused]] System& sys, Scheduler::Cond* c) {
	auto cond = reinterpret_cast<Cond*>(c);
	if (pthread_cond_signal(&cond->handle) != 0) {
		THOR_ASSERT(sys, !"Could not signal condition variable");
	}
}

static void scheduler_cond_broadcast([[maybe_unused]] System& sys, Scheduler::Cond* c) {
	auto cond = reinterpret_cast<Cond*>(c);
	if (pthread_cond_broadcast(&cond->handle) != 0) {
		THOR_ASSERT(sys, !"Could not broadcast condition variable");
	}
}

static void scheduler_cond_wait([[maybe_unused]] System& sys, Scheduler::Cond* c, Scheduler::Mutex* m) {
	auto cond = reinterpret_cast<Cond*>(c);
	auto mutex = reinterpret_cast<Mutex*>(m);
	if (pthread_cond_wait(&cond->handle, &mutex->handle) != 0) {
		THOR_ASSERT(sys, !"Could not wait condition variable");
	}
}

static void scheduler_yield(System&) {
	sched_yield();
}

extern const Scheduler STD_SCHEDULER = {
	.thread_start = scheduler_thread_start,
	.thread_join = scheduler_thread_join,

	.mutex_create = scheduler_mutex_create,
	.mutex_destroy = scheduler_mutex_destroy,
	.mutex_lock = scheduler_mutex_lock,
	.mutex_unlock = scheduler_mutex_unlock,

	.cond_create = scheduler_cond_create,
	.cond_destroy = scheduler_cond_destroy,
	.cond_signal = scheduler_cond_signal,
	.cond_broadcast = scheduler_cond_broadcast,
	.cond_wait = scheduler_cond_wait,

	.yield = scheduler_yield,
};

static Float64 chrono_monotonic_now(System&) {
	struct timespec ts{};
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return static_cast<Float64>(ts.tv_sec) + ts.tv_nsec / 1.0e9;
}

static Float64 chrono_wall_now(System&) {
	struct timespec ts{};
	clock_gettime(CLOCK_REALTIME, &ts);
	return static_cast<Float64>(ts.tv_sec) + ts.tv_nsec / 1.0e9;
}

extern const Chrono STD_CHRONO = {
	.monotonic_now = chrono_monotonic_now,
	.wall_now = chrono_wall_now,
};

} // namespace Thor

#endif // THOR_HOST_PLATFORM_POSIX