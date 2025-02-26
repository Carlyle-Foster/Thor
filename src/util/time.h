#ifndef THOR_TIME_H
#define THOR_TIME_H
#include "util/types.h"

namespace Thor {

struct System;

template<typename T>
struct Time;

struct MonotonicTime;
struct WallTime;

// Type safe unit representing seconds as 64-bit floating-point value.
struct Seconds {
	THOR_FORCEINLINE explicit constexpr Seconds(Float64 value)
		: raw_{value}
	{
	}
	THOR_FORCEINLINE Bool is_inf() const {
		union { Float64 f; Uint64 u; } bits{raw_};
		return (bits.u & -1ull >> 1) == 0x7ffull << 52;
	}
	THOR_FORCEINLINE constexpr Seconds operator+(Seconds other) const {
		return Seconds{raw_ + other.raw_};
	}
	THOR_FORCEINLINE constexpr Seconds operator-(Seconds other) const {
		return Seconds{raw_ - other.raw_};
	}
	THOR_FORCEINLINE constexpr Seconds operator-() const {
		return Seconds{-raw_};
	}
	THOR_FORCEINLINE Seconds& operator+=(Seconds other) {
		return *this = *this + other;
	}
	THOR_FORCEINLINE Seconds& operator-=(Seconds other) {
		return *this = *this - other;
	}
	THOR_FORCEINLINE auto value() const {
		return raw_;
	}
	WallTime operator+(WallTime) const;
	WallTime operator-(WallTime) const;
	MonotonicTime operator+(MonotonicTime) const;
	MonotonicTime operator-(MonotonicTime) const;
private:
	Float64 raw_ = 0.0;
};

// CRTP base for time implementations.
template<typename T>
struct Time {
	THOR_FORCEINLINE static constexpr T from_raw(Float64 seconds) {
		return T{seconds};
	}
	THOR_FORCEINLINE static constexpr T from_now(System& sys, Seconds time_from_now) {
		if (time_from_now.is_inf()) {
			return T::from_raw(time_from_now.raw_);
		}
		return T::now(sys) + time_from_now;
	}
	THOR_FORCEINLINE Bool is_inf() const {
		return Seconds{raw_}.is_inf();
	}
	THOR_FORCEINLINE constexpr Seconds seconds_since_epoch() const {
		return Seconds{raw_};
	}
	THOR_FORCEINLINE constexpr Seconds operator-(T other) const {
		return Seconds{raw_ - other.raw_};
	}
	THOR_FORCEINLINE constexpr T operator+(Seconds other) const {
		return from_raw(raw_ + other.value());
	}
	THOR_FORCEINLINE constexpr T operator-(Seconds other) const {
		return from_raw(raw_ - other.value());
	}
	THOR_FORCEINLINE constexpr T operator-() const {
		return from_raw(-raw_);
	}
	THOR_FORCEINLINE T operator+=(Seconds other) {
		auto derived = static_cast<T*>(this);
		return *derived = *derived + other;
	}
	THOR_FORCEINLINE T operator-=(Seconds other) {
		auto derived = static_cast<T*>(this);
		return *derived = *derived - other;
	}
	THOR_FORCEINLINE Bool operator<(const Time& other) const { return raw_ < other.raw; }
	THOR_FORCEINLINE Bool operator>(const Time& other) const { return raw_ > other.raw; }
	THOR_FORCEINLINE Bool operator<=(const Time& other) const { return raw_ <= other.raw; }
	THOR_FORCEINLINE Bool operator>=(const Time& other) const { return raw_ >= other.raw; }
protected:
	constexpr Time() = default;
	constexpr Time(Float64 raw)
		: raw_{raw}
	{
	}
	Float64 raw_ = 0.0;
};

// We have both MonotonicTime and WallTime implementations.
struct MonotonicTime : Time<MonotonicTime> {
	constexpr MonotonicTime() = default;
	static MonotonicTime now(System& sys);
private:
	friend struct Time<MonotonicTime>;
	constexpr MonotonicTime(Float64 raw) : Time<MonotonicTime>{raw} {}
};

struct WallTime : Time<WallTime> {
	constexpr WallTime() = default;
	static WallTime now(System& sys);
private:
	friend struct Time<WallTime>;
	constexpr WallTime(Float64 raw) : Time<WallTime>{raw} {}
};

} // namespace Thor

#endif // THOR_TIME_H