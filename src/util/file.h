#ifndef THOR_FILE_H
#define THOR_FILE_H
#include "util/array.h"
#include "util/maybe.h"
#include "util/system.h"

namespace Thor {

struct File {
	using Access = Filesystem::Access;

	static Maybe<File> open(System& sys, StringView name, Access access);

	constexpr File(File&& other)
		: sys_{other.sys_}
		, file_{exchange(other.file_, nullptr)}
	{
	}

	~File() { close(); }

	File& operator=(File&& other) {
		return *new (drop(), Nat{}) File{move(other)};
	}

	[[nodiscard]] Uint64 read(Uint64 offset, Slice<Uint8> data) const;
	[[nodiscard]] Uint64 write(Uint64 offset, Slice<const Uint8> data) const;
	[[nodiscard]] Uint64 tell() const;

	void close();

	Array<Uint8> map(Allocator& allocator) const;

private:
	File* drop() {
		close();
		return this;
	}
	constexpr File(System& sys, Filesystem::File* file)
		: sys_{sys}
		, file_{file}
	{
	}
	System&           sys_;
	Filesystem::File* file_;
};

} // namespace Thor

#endif // THOR_FILE_H