#include "util/stream.h"

namespace Thor {

Maybe<FileStream> FileStream::open(System& sys, StringView name, File::Access access) {
	auto file = File::open(sys, name, access);
	if (!file) {
		return {};
	}
	return FileStream { move(*file) };
}

Bool FileStream::write(Slice<const Uint8> data) {
	const auto nb = file_.write(offset_, data);
	offset_ += nb;
	return nb == data.length();
}

Bool FileStream::read(Slice<Uint8> data) {
	const auto nb = file_.read(offset_, data);
	offset_ += nb;
	return nb == data.length();
}

Uint64 FileStream::tell() const {
	return offset_;
}

} // namespace Thor