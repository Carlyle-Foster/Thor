#include "util/file.h"

namespace Thor {

Maybe<File> File::open(System& sys, StringView name, Access access) {
	if (name.is_empty()) {
		return {};
	}
	if (auto file = sys.filesystem.open(sys, name, access)) {
		return File{sys, file};
	}
	return {};
}

Uint64 File::read(Uint64 offset, Slice<Uint8> data) const {
	Uint64 total = 0;
	while (total < data.length()) {
		if (auto rd = sys_.filesystem.read(sys_, file_, offset, data)) {
			total += rd;
			offset += rd;
			data = data.slice(rd);
		} else {
			break;
		}
	}
	return total;
}

Uint64 File::write(Uint64 offset, Slice<const Uint8> data) const {
	Uint64 total = 0;
	while (total < data.length()) {
		if (auto wr = sys_.filesystem.write(sys_, file_, offset, data)) {
			total += wr;
			offset += wr;
			data = data.slice(wr);
		} else {
			break;
		}
	}
	return total;
}

Uint64 File::tell() const {
	return sys_.filesystem.tell(sys_, file_);
}

void File::close() {
	if (file_) {
		sys_.filesystem.close(sys_, file_);
		file_ = nullptr;
	}
}

Array<Uint8> File::map(Allocator& allocator) const {
	Array<Uint8> result{allocator};
	if (!result.resize(tell())) {
		return {allocator};
	}
	if (read(0, result.slice()) != result.length()) {
		return {allocator};
	}
	return result;
}

} // namespace Thor