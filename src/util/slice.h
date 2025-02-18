#ifndef THOR_SLICE_H
#define THOR_SLICE_H
#include "util/types.h"
#include "util/exchange.h"

namespace Thor {

// Slice is a convenience type around a pointer and a length. Think of it like a
// span or a view. Specialization for T = const char is implicitly provided so
// that Slice<const char> is the same as StringView.
template<typename T>
struct Slice {
	constexpr Slice() = default;
	constexpr Slice(T* data, Ulen length)
		: data_{data}
		, length_{length}
	{
	}
	template<Ulen E>
	constexpr Slice(T (&data)[E])
		: data_{data}
		, length_{E}
	{
		if constexpr (is_same<T, const char>) {
			length_--;
		}
	}
	constexpr Slice(const Slice& other)
		: data_{other.data_}
		, length_{other.length_}
	{
	}
	constexpr Slice& operator=(const Slice&) = default;
	constexpr Slice(Slice&& other)
		: data_{exchange(other.data_, nullptr)}
		, length_{exchange(other.length_, 0)}
	{
	}
	constexpr auto&& operator[](this auto&& self, Ulen index) {
		return self.data_[index];
	}
	constexpr Slice slice(this auto&& self, Ulen offset) {
		return Slice{self.data_ + offset, self.length_ - offset};
	}
	constexpr Slice truncate(this auto&& self, Ulen length) {
		return Slice{self.data_, length};
	}
	[[nodiscard]] constexpr auto length() const { return length_; }
	[[nodiscard]] constexpr auto is_empty() const { return length_ == 0; }
	[[nodiscard]] constexpr auto&& data(this auto&& self) { return self.data_; }
	template<typename U>
	Slice<U> cast(this auto&& self) {
		return Slice<U>{(U*)(self.data_), (self.length_ * sizeof(T)) / sizeof(U)};
	}
	[[nodiscard]] friend constexpr Bool operator==(const Slice& lhs, const Slice& rhs) {
		const auto lhs_len = lhs.length();
		const auto rhs_len = rhs.length();
		if (lhs_len != rhs_len) {
			return false;
		}
		if (lhs.data() == rhs.data()) {
			return true;
		}
		for (Ulen i = 0; i < lhs_len; i++) {
			if (lhs[i] != rhs[i]) {
				return false;
			}
		}
		return true;
	}
private:
	T*   data_   = nullptr;
	Ulen length_ = 0;
};

} // namespace Thor

#endif // THOR_SLICE_H