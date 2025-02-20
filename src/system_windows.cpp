#include "util/info.h"

// Implementation of the System for Windows systems
#if defined(THOR_HOST_PLATFORM_WINDOWS)

namespace Thor {

static Filesystem::File* filesystem_open_file(System&, StringView, Filesystem::Access) {
	// TODO: implement using CreateFileW
	return nullptr;
}

static void filesystem_close_file(System&, Filesystem::File*) {
	// TODO: implement using CloseHandle
}

static Uint64 filesystem_read_file(System&, Filesystem::File*, Uint64, Slice<Uint8>) {
	// TODO: implement using ReadFile /w OVERLAPPED struct set for offset
	return 0;
}

static Uint64 filesystem_write_file(System&, Filesystem::File*, Uint64, Slice<const Uint8>) {
	// TODO: implement using WriteFile /w OVERLAPPED struct set for offset
	return 0;
}

static Uint64 filesystem_tell_file(System&, Filesystem::File*) {
	// TODO: implement using GetFileInformationByHandle
	return 0;
}

Filesystem::Directory* filesystem_open_dir(System&, StringView) {
	// TODO: implement with FindFirstFileW
	return nullptr;
}

void filesystem_close_dir(System&, Filesystem::Directory*) {
	// TODO: implement with FindClose
}

Bool filesystem_read_dir(System&, Filesystem::Directory*, Filesystem::Item&) {
	// TODO: implement with FindNextFileW
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

static void* heap_allocate(System&, Ulen, Bool) {
	// TODO: implement with VirtualAlloc
	return nullptr;
}

static void heap_deallocate(System&, void *, Ulen) {
	// TODO: implement with VirtualFree
}

extern const Heap STD_HEAP = {
	.allocate   = heap_allocate,
	.deallocate = heap_deallocate,
};

static void console_write(System&, StringView) {
	// TODO: implement with WriteConsole
}

extern const Console STD_CONSOLE = {
	.write = console_write,
};

} // namespace Thor

#endif // THOR_HOST_PLATFORM_WINDOWS