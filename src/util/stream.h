#ifndef THOR_STREAM_H
#define THOR_STREAM_H
#include "util/file.h"

namespace Thor {

struct Stream {
	virtual ~Stream() {}
	virtual Bool write(Slice<const Uint8> data) = 0;
	virtual Bool read(Slice<Uint8> data) = 0;
	virtual Uint64 tell() const = 0;
};

struct FileStream : Stream {
	FileStream(FileStream&& other)
		: Stream{move(other)}
		, file_{move(other.file_)}
		, offset_{exchange(other.offset_, 0)}
	{
	}
	static Maybe<FileStream> open(System& sys, StringView name, File::Access access);
	virtual Bool write(Slice<const Uint8> data);
	virtual Bool read(Slice<Uint8> data);
	virtual Uint64 tell() const;
private:
	FileStream(File&& file)
		: file_{move(file)}
	{
	}
	File   file_;
	Uint64 offset_ = 0;
};

} // namespace Thor

#endif // THOR_STREAM_H