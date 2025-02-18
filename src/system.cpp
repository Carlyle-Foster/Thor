// Implementation of the System in terms of STD C
#include <stdio.h> // fopen, fclose, fread, fwrite, ftell
#include <stdlib.h> // calloc, malloc, free
#include <string.h> // memcpy

#include "util/system.h"

namespace Thor {

static Filesystem::File* filesystem_open(System& sys, StringView name, Filesystem::Access access) {
	const char* mode = nullptr;
	switch (access) {
	case Filesystem::Access::RD: mode = "rb"; break;
	case Filesystem::Access::WR: mode = "wb"; break;
	}
	ScratchAllocator<1024> scratch{sys.allocator};
	auto path = scratch.allocate<char>(name.length() + 1, false);
	if (!path) {
		// Out of memory.
		return nullptr;
	}
	memcpy(path, name.data(), name.length());
	path[name.length()] = '\0';
	auto fp = fopen(path, mode);
	return reinterpret_cast<Filesystem::File*>(fp);
}

static void filesystem_close(System&, Filesystem::File* file) {
	fclose(reinterpret_cast<FILE*>(file));
}

static Uint64 filesystem_read(System&, Filesystem::File* file, Uint64 offset, Slice<Uint8> data) {
	const auto fp = reinterpret_cast<FILE*>(file);
	if (fseek(fp, offset, SEEK_SET) != 0) {
		return 0;
	}
	if (fread(data.data(), data.length(), 1, fp) != 1) {
		return 0;
	}
	return data.length();
}

static Uint64 filesystem_write(System&, Filesystem::File* file, Uint64 offset, Slice<const Uint8> data) {
	const auto fp = reinterpret_cast<FILE*>(file);
	if (fseek(fp, offset, SEEK_SET) != 0) {
		return 0;
	}
	if (fwrite(data.data(), data.length(), 1, fp) != 1) {
		return 0;
	}
	return data.length();
}

static Uint64 filesystem_tell(System&, Filesystem::File* file) {
	const auto fp = reinterpret_cast<FILE*>(file);
	if (fseek(fp, 0, SEEK_END) != 0) {
		return 0;
	}
	const auto len = ftell(fp);
	if (len < 0) {
		return 0;
	}
	return len;
}

extern const Filesystem STD_FILESYSTEM = {
	.open  = filesystem_open,
	.close = filesystem_close,
	.read  = filesystem_read,
	.write = filesystem_write,
	.tell  = filesystem_tell,
};

static void* heap_allocate(System&, Ulen length, Bool zero) {
	if (zero) {
		return calloc(1, length);
	}
	return malloc(length);
}

static void heap_deallocate(System&, void *addr, Ulen) {
	free(addr);
}

extern const Heap STD_HEAP = {
	.allocate   = heap_allocate,
	.deallocate = heap_deallocate,
};

static void console_write(System&, StringView data) {
	printf("%.*s", Sint32(data.length()), data.data());
}
static void console_flush(System&) {
	fflush(stdout);
}

extern const Console STD_CONSOLE = {
	.write = console_write,
	.flush = console_flush,
};

} // namespace Thor