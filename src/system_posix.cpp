#include <stdlib.h> // exit needed from libc sadly
#include "util/info.h"

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

static void* heap_allocate(System&, Ulen length, Bool) {
	auto addr = mmap(nullptr,
	                 length,
	                 PROT_READ | PROT_WRITE,
	                 MAP_PRIVATE | MAP_ANONYMOUS,
	                 0,
	                 0);
	if (addr == MAP_FAILED) {
		return nullptr;
	}
	return addr;
}

static void heap_deallocate(System&, void *addr, Ulen length) {
	munmap(addr, length);
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

} // namespace Thor

#endif // THOR_HOST_PLATFORM_POSIX