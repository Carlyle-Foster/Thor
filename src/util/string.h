#ifndef THOR_STRING_H
#define THOR_STRING_H
#include "util/array.h"
#include "util/maybe.h"

namespace Thor {

using StringView = Slice<const char>;

// Utility for building a string incrementally.
struct StringBuilder {
	constexpr StringBuilder(Allocator& allocator)
		: build_{allocator}
	{
	}
	void put(char ch);
	void put(StringView view);
	void lpad(Ulen n, char ch, char pad = ' ');
	void lpad(Ulen n, StringView view, char pad = ' ');
	void rpad(Ulen n, char ch, char pad = ' ');
	void rpad(Ulen n, StringView view, char pad = ' ');
	Maybe<StringView> result() const;
private:
	Array<char> build_;
	Bool        error_ = false;
};

} // namespace Thor

#endif // THOR_STRING_H