#include "util/info.h"

// Implementation of the System for Windows systems
#if defined(THOR_HOST_PLATFORM_WINDOWS)

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace Thor {

static Ulen convert_utf8_to_utf16(Slice<const Uint8> utf8, Slice<Uint16> utf16) {
	Uint32 cp = 0;
	Ulen len = 0;
	auto out = utf16.data();
	for (Ulen i = 0; i < utf8.length(); i++) {
		auto element = &utf8[i];
		auto ch = *element;
		if (ch <= 0x7f) {
			cp = ch;
		} else if (ch <= 0xbf) {
			cp = (cp << 6) | (ch & 0x3f);
		} else if (ch <= 0xdf) {
			cp = ch & 0x1f;
		} else if (ch <= 0xef) {
			cp = ch & 0x0f;
		} else {
			cp = ch & 0x07;
		}
		element++;
		if ((*element & 0xc0) != 0x80 && cp <= 0x10ffff) {
			if (cp > 0xffff) {
				len += 2;
				if (out) {
					*out++ = static_cast<Uint16>(0xd800 | (cp >> 10));
					*out++ = static_cast<Uint16>(0xdc00 | (cp & 0x03ff));
				}
			} else if (cp < 0xd800 || cp >= 0xe000) {
				len += 1;
				if (out) {
					*out++ = static_cast<Uint16>(cp);
				}
			}
		}
	}
	return len;
}

static Ulen convert_utf16_to_utf8(Slice<const Uint16> utf16, Slice<Uint8> utf8) {
	Uint32 cp = 0;
	Ulen len = 0;
	auto out = utf8.data();
	for (Ulen i = 0; i < utf16.length(); i++) {
		auto element = &utf16[i];
		auto ch = *element;
		if (ch >= 0xd800 && ch <= 0xdbff) {
			cp = ((ch - 0xd800) << 10) | 0x10000;
		} else {
			if (ch >= 0xdc00 && ch <= 0xdfff) {
				cp |= ch - 0xdc00;
			} else {
				cp = ch;
			}
			if (cp < 0x7f) {
				len += 1;
				if (out) {
					*out++ = static_cast<Uint8>(cp);
				}
			} else if (cp < 0x7ff) {
				len += 2;
				if (out) {
					*out++ = static_cast<Uint8>(0xc0 | ((cp >> 6) & 0x1f));
					*out++ = static_cast<Uint8>(0x80 | (cp & 0x3f));
				}
			} else if (cp < 0xffff) {
				len += 3;
				if (out) {
					*out++ = static_cast<Uint8>(0xe0 | ((cp >> 12) & 0x0f));
					*out++ = static_cast<Uint8>(0x80 | ((cp >> 6) & 0x3f));
					*out++ = static_cast<Uint8>(0x80 | (cp & 0x3f));
				}
			} else {
				len += 4;
				if (out) {
					*out++ = static_cast<Uint8>(0xf0 | ((cp >> 18) & 0x07));
					*out++ = static_cast<Uint8>(0x80 | ((cp >> 12) & 0x3f));
					*out++ = static_cast<Uint8>(0x80 | ((cp >> 6) & 0x3f));
					*out++ = static_cast<Uint8>(0x80 | (cp & 0x3f));
				}
			}
			cp = 0;
		}
	}
	return len;
}

// We need a mechanism to convert between UTF-8 and UTF-16 here, using only the
// provided allocator. The rest of this code is untested.
static Slice<Uint16> utf8_to_utf16(Allocator& allocator, Slice<const Uint8> utf8) {
	const auto len = convert_utf8_to_utf16(utf8, {});
	const auto data = allocator.allocate<Uint16>(len + 1, false);
	if (!data) {
		return {};
	}
	Slice<Uint16> utf16{ data, len };
	convert_utf8_to_utf16(utf8, utf16);
	data[len] = 0;
	return utf16;
}
static Slice<Uint8> utf16_to_utf8(Allocator& allocator, Slice<const Uint16> utf16) {
	const auto len = convert_utf16_to_utf8(utf16, {});
	const auto data = allocator.allocate<Uint8>(len + 1, false);
	if (!data) {
		return {};
	}
	Slice<Uint8> utf8{ data, len };
	convert_utf16_to_utf8(utf16, utf8);
	data[len] = 0;
	return utf8;
}

static Filesystem::File* filesystem_open_file(System& sys, StringView name, Filesystem::Access access) {
	ScratchAllocator<1024> scratch{sys.allocator};
	auto filename = utf8_to_utf16(scratch, name.cast<const Uint8>());
	if (filename.is_empty()) {
		return nullptr;
	}
	DWORD dwDesiredAccess = 0;
	DWORD dwShareMode = 0;
	DWORD dwCreationDisposition = 0;
	DWORD dwFlagsAndAttributes = FILE_ATTRIBUTE_NORMAL;
	switch (access) {
	case Filesystem::Access::RD:
		dwDesiredAccess |= GENERIC_READ;
		dwShareMode |= FILE_SHARE_READ;
		dwCreationDisposition = OPEN_EXISTING;
		break;
	case Filesystem::Access::WR:
		dwDesiredAccess |= GENERIC_WRITE;
		dwShareMode |= FILE_SHARE_WRITE;
		dwCreationDisposition = CREATE_ALWAYS;
		break;
	}
	auto handle = CreateFileW(reinterpret_cast<LPCWSTR>(filename.data()),
	                          dwDesiredAccess,
	                          dwShareMode,
	                          nullptr,
	                          dwCreationDisposition,
	                          dwFlagsAndAttributes,
	                          nullptr);
	if (handle == INVALID_HANDLE_VALUE) {
		return nullptr;
	}
	return reinterpret_cast<Filesystem::File*>(handle);
}

static void filesystem_close_file(System&, Filesystem::File* file) {
	CloseHandle(reinterpret_cast<HANDLE>(file));
}

static Uint64 filesystem_read_file(System&, Filesystem::File* file, Uint64 offset, Slice<Uint8> data) {
	OVERLAPPED overlapped{};
	overlapped.OffsetHigh = static_cast<Uint32>((offset & 0xffffffff00000000_u64) >> 32);
	overlapped.Offset     = static_cast<Uint32>((offset & 0x00000000ffffffff_u64));
	DWORD rd = 0;
	auto result = ReadFile(reinterpret_cast<HANDLE>(file),
	                       data.data(),
	                       static_cast<DWORD>(min(data.length(), 0xffffffff_ulen)),
	                       &rd,
	                       &overlapped);
	if (!result && GetLastError() != ERROR_HANDLE_EOF) {
		return 0;
	}
	return static_cast<Uint64>(rd);
}

static Uint64 filesystem_write_file(System&, Filesystem::File* file, Uint64 offset, Slice<const Uint8> data) {
	OVERLAPPED overlapped{};
	overlapped.OffsetHigh = static_cast<Uint32>((offset & 0xffffffff00000000_u64) >> 32);
	overlapped.Offset     = static_cast<Uint32>((offset & 0x00000000ffffffff_u64));
	DWORD wr = 0;
	auto result = WriteFile(reinterpret_cast<HANDLE>(file),
	                       data.data(),
	                       static_cast<DWORD>(min(data.length(), 0xffffffff_ulen)),
	                       &wr,
	                       &overlapped);
	return static_cast<Uint64>(wr);
}

static Uint64 filesystem_tell_file(System&, Filesystem::File* file) {
	auto handle = reinterpret_cast<HANDLE>(file);
	BY_HANDLE_FILE_INFORMATION info;
	if (GetFileInformationByHandle(file, &info)) {
		return (static_cast<Uint64>(info.nFileSizeHigh) << 32) | info.nFileSizeLow;
	}
	return 0;
}

struct FindData {
	FindData(Allocator& allocator)
		: allocator{allocator}
		, handle{INVALID_HANDLE_VALUE}
	{
	}
	~FindData() {
		if (handle != INVALID_HANDLE_VALUE) {
			FindClose(handle);
		}
	}
	ScratchAllocator<4096> allocator;
	HANDLE                 handle;
	WIN32_FIND_DATAW       data;
};

Filesystem::Directory* filesystem_open_dir(System& sys, StringView name) {
	auto find = sys.allocator.create<FindData>(sys.allocator);
	if (!find) {
		return nullptr;
	}
	auto path = utf8_to_utf16(find->allocator, name.cast<const Uint8>());
	if (path.is_empty()) {
		sys.allocator.destroy(find);
		return nullptr;
	}
	auto handle = FindFirstFileW(reinterpret_cast<const LPCWSTR>(path.data()),
	                             &find->data);
	if (handle != INVALID_HANDLE_VALUE) {
		find->handle = handle;
		return reinterpret_cast<Filesystem::Directory*>(find);
	}
	sys.allocator.destroy(find);
	return nullptr;
}

void filesystem_close_dir(System& sys, Filesystem::Directory* handle) {
	auto find = reinterpret_cast<FindData*>(handle);
	sys.allocator.destroy(find);
}

Bool filesystem_read_dir(System&, Filesystem::Directory* handle, Filesystem::Item& item) {
	auto find = reinterpret_cast<FindData*>(handle);
	// Skip '.' and '..'
	if (find->data.cFileName[0] == L'.' &&
	    find->data.cFileName[1 + !!(find->data.cFileName[1] == L'.')])
	{
		if (!FindNextFileW(find->handle, &find->data)) {
			return false;
		}
	}
	Slice<const Uint16> utf16 {
		reinterpret_cast<const Uint16*>(find->data.cFileName),
		wcslen(find->data.cFileName)
	};
	auto utf8 = utf16_to_utf8(find->allocator, utf16);
	if (utf8.is_empty()) {
		return false;
	}
	if (find->data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
		item = { utf8.cast<const char>(), Filesystem::Item::Kind::DIR };
	} else {
		item = { utf8.cast<const char>(), Filesystem::Item::Kind::FILE };
	}
	return true;
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

static void* heap_allocate(System&, Ulen length, Bool) {
	return VirtualAlloc(nullptr, length, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
}

static void heap_deallocate(System&, void *address, Ulen length) {
	VirtualFree(address, length, MEM_RELEASE);
}

extern const Heap STD_HEAP = {
	.allocate   = heap_allocate,
	.deallocate = heap_deallocate,
};

static void console_write(System&, StringView data) {
	auto handle = GetStdHandle(STD_OUTPUT_HANDLE);
	WriteConsoleA(handle, data.data(), data.length(), nullptr, nullptr);
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
	ExitProcess(3);
}

extern const Process STD_PROCESS = {
	.assert = process_assert,
};

static Linker::Library* linker_load(System&, StringView name) {
	InlineAllocator<1024> buf;
	StringBuilder path{buf};
	path.put(name);
	path.put('.');
	path.put("dll");
	path.put('\0');
	auto result = path.result();
	if (!result) {
		return nullptr;
	}
	if (auto lib = LoadLibraryA(result->data())) {
		return reinterpret_cast<Linker::Library*>(lib);
	}
	return nullptr;
}

static void linker_close(System&, Linker::Library* lib) {
	FreeLibrary(reinterpret_cast<HMODULE>(lib));
}

static void (*linker_link(System&, Linker::Library* lib, const char* sym))(void) {
	if (auto addr = GetProcAddress(reinterpret_cast<HMODULE>(lib), sym)) {
		return reinterpret_cast<void (*)(void)>(addr);
	}
	return nullptr;
}

extern const Linker STD_LINKER = {
	.load  = linker_load,
	.close = linker_close,
	.link  = linker_link
};

struct Thread {
	Thread(System& sys, void (*fn)(System&, void*), void* user)
		: sys{sys}
		, fn{fn}
		, user{user}
	{
	}
	System& sys;
	void    (*fn)(System& sys, void* user);
	void*   user;
	HANDLE  handle;
};

struct Mutex {
	SRWLOCK handle;
};

struct Cond {
	CONDITION_VARIABLE handle;
};

static DWORD WINAPI scheduler_thread_proc(void* user) {
	auto thread = reinterpret_cast<Thread*>(user);
	thread->fn(thread->sys, thread->user);
	return 0;
}

static Scheduler::Thread* scheduler_thread_start(System& sys, void (*fn)(System& sys, void* user), void* user) {
	auto thread = sys.allocator.create<Thread>(sys, fn, user);
	if (!thread) {
		return nullptr;
	}
	thread->handle = CreateThread(nullptr,
	                              0,
	                              scheduler_thread_proc,
	                              nullptr,
	                              0, 
	                              nullptr);
	if (thread->handle == INVALID_HANDLE_VALUE) {
		sys.allocator.destroy(thread);
		return nullptr;
	}
	return reinterpret_cast<Scheduler::Thread*>(thread);
}

static void scheduler_thread_join(System& sys, Scheduler::Thread* t) {
	auto thread = reinterpret_cast<Thread*>(t);
	THOR_ASSERT(sys, &sys == &thread->sys);
	WaitForSingleObject(thread->handle, INFINITE);
	sys.allocator.destroy(thread);
}

static Scheduler::Mutex* scheduler_mutex_create(System& sys) {
	auto mutex = sys.allocator.create<Mutex>();
	if (!mutex) {
		return nullptr;
	}
	InitializeSRWLock(&mutex->handle);
	return reinterpret_cast<Scheduler::Mutex*>(mutex);
}

static void scheduler_mutex_destroy([[maybe_unused]] System& sys, Scheduler::Mutex* m) {
	auto mutex = reinterpret_cast<Mutex*>(m);
	sys.allocator.destroy(mutex);
}

static void scheduler_mutex_lock([[maybe_unused]] System& sys, Scheduler::Mutex* m) {
	auto mutex = reinterpret_cast<Mutex*>(m);
	AcquireSRWLockExclusive(&mutex->handle);
}

static void scheduler_mutex_unlock([[maybe_unused]] System& sys, Scheduler::Mutex* m) {
	auto mutex = reinterpret_cast<Mutex*>(m);
	ReleaseSRWLockExclusive(&mutex->handle);
}

static Scheduler::Cond* scheduler_cond_create(System& sys) {
	auto cond = sys.allocator.create<Cond>();
	if (!cond) {
		return nullptr;
	}
	InitializeConditionVariable(&cond->handle);
	return reinterpret_cast<Scheduler::Cond*>(cond);
}

static void scheduler_cond_destroy([[maybe_unused]] System& sys, Scheduler::Cond* c) {
	auto cond = reinterpret_cast<Cond*>(c);
	sys.allocator.destroy(cond);
}

static void scheduler_cond_signal([[maybe_unused]] System& sys, Scheduler::Cond* c) {
	auto cond = reinterpret_cast<Cond*>(c);
	WakeConditionVariable(&cond->handle);
}

static void scheduler_cond_broadcast([[maybe_unused]] System& sys, Scheduler::Cond* c) {
	auto cond = reinterpret_cast<Cond*>(c);
	WakeAllConditionVariable(&cond->handle);
}

static void scheduler_cond_wait([[maybe_unused]] System& sys, Scheduler::Cond* c, Scheduler::Mutex* m) {
	auto cond = reinterpret_cast<Cond*>(c);
	auto mutex = reinterpret_cast<Mutex*>(m);
	SleepConditionVariableSRW(&cond->handle, &mutex->handle, INFINITE, 0);
}

static void scheduler_yield(System&) {
	SwitchToThread();
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

	.yield = scheduler_yield
};

} // namespace Thor

#endif // THOR_HOST_PLATFORM_WINDOWS