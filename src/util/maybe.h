#ifndef THOR_MAYBE_H
#define THOR_MAYBE_H
#include "util/exchange.h"

namespace Thor {

// Simple optional type, use like Maybe<T>, check if valid with if (maybe) or
// if (maybe.is_valid()). The underlying T can be extracted (unboxed) with the
// use of the -> or * operators, i.e maybe->thing or (*maybe).thing, where thing
// is a field of T.
template<typename T>
struct Maybe {
	constexpr Maybe()
		: as_nat_{}
	{
	}
	constexpr Maybe(T&& value)
		: as_value_{move(value)}
		, valid_{true}
	{
	}
	constexpr Maybe(Maybe&& other)
		: valid_{exchange(other.valid_, false)}
	{
		if (valid_) {
			new (&as_value_, Nat{}) T{move(other.as_value_)};
			other.as_value_.~T();
		}
	}
	constexpr Maybe& operator=(T&& value) {
		return *new(drop(), Nat{}) Maybe{move(value)};
	}
	constexpr Maybe& operator=(Maybe&& value) {
		return *new(drop(), Nat{}) Maybe{move(value)};
	}
	[[nodiscard]] constexpr auto is_valid() const { return valid_; }
	[[nodiscard]] constexpr operator Bool() const { return is_valid(); }
	[[nodiscard]] constexpr T& value() { return as_value_; }
	[[nodiscard]] constexpr const T& value() const { return as_value_; }
	[[nodiscard]] constexpr T& operator*() { return as_value_; }
	[[nodiscard]] constexpr const T& operator*() const { return as_value_; }
	[[nodiscard]] constexpr T* operator->() { return &as_value_; }
	[[nodiscard]] constexpr const T* operator->() const { return &as_value_; }
	~Maybe() { drop(); }
	void reset() {
		drop();
		valid_ = false;
	}
	template<typename... Ts>
	void emplace(Ts&&... args) {
		new (drop(), Nat{}) Maybe{T{forward<Ts>(args)...}};
	}
private:
	constexpr Maybe* drop() {
		if (is_valid()) as_value_.~T();
		return this;
	}
	union {
		Nat as_nat_;
		T   as_value_;
	};
	Bool valid_ = false;
};

} // namespace Thor

#endif // THOR_MAYBE_H