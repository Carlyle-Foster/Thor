// Implementation of the System in terms of STD C
#include <stdio.h> // fopen, fclose, fread, fwrite, ftell
#include <stdlib.h> // calloc, malloc, free
#include <string.h> // memcpy

#include "util/system.h"

namespace Thor {

static Filesystem::File* filesystem_open(const System& sys, StringView name, Filesystem::Access access) {
	const char* mode = "";
	switch (access) {
	case Filesystem::Access::RD:
		mode = "rb";
		break;
	case Filesystem::Access::WR:
		mode = "wb";
		break;
	}
	auto path = sys.heap.allocate(sys, name.length() + 1, true);
	if (!path) {
		// Out of memory.
		return nullptr;
	}
	memcpy(path, name.data(), name.length());
	auto fp = fopen(reinterpret_cast<const char *>(path), mode);
	sys.heap.deallocate(sys, path, name.length() + 1);
	return reinterpret_cast<Filesystem::File*>(fp);
}

static void filesystem_close(const System&, Filesystem::File* file) {
	fclose(reinterpret_cast<FILE*>(file));
}

static Uint64 filesystem_read(const System&, Filesystem::File* file, Uint64 offset, Slice<Uint8> data) {
	const auto fp = reinterpret_cast<FILE*>(file);
	if (fseek(fp, offset, SEEK_SET) != 0) {
		return 0;
	}
	if (fread(data.data(), data.length(), 1, fp) != 1) {
		return 0;
	}
	return data.length();
}

static Uint64 filesystem_write(const System&, Filesystem::File* file, Uint64 offset, Slice<const Uint8> data) {
	const auto fp = reinterpret_cast<FILE*>(file);
	if (fseek(fp, offset, SEEK_SET) != 0) {
		return 0;
	}
	if (fwrite(data.data(), data.length(), 1, fp) != 1) {
		return 0;
	}
	return data.length();
}

static Uint64 filesystem_tell(const System&, Filesystem::File* file) {
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

const Filesystem STD_FILESYSTEM = {
	.open  = filesystem_open,
	.close = filesystem_close,
	.read  = filesystem_read,
	.write = filesystem_write,
	.tell  = filesystem_tell,
};

static void* heap_allocate(const System&, Ulen length, Bool zero) {
	if (zero) {
		return calloc(1, length);
	}
	return malloc(length);
}

static void heap_deallocate(const System&, void *addr, Ulen) {
	free(addr);
}

const Heap STD_HEAP = {
	.allocate   = heap_allocate,
	.deallocate = heap_deallocate,
};

static void console_write(const System&, StringView data) {
	printf("%.*s", Sint32(data.length()), data.data());
}
static void console_flush(const System&) {
	fflush(stdout);
}

const Console STD_CONSOLE = {
	.write = console_write,
	.flush = console_flush,
};

extern const System STD_SYSTEM = {
	.filesystem = STD_FILESYSTEM,
	.heap       = STD_HEAP,
	.console    = STD_CONSOLE,
};

} // namespace Thor