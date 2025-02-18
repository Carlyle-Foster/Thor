#ifndef THOR_UNICODE_H
#define THOR_UNICODE_H
#include "util/types.h"

namespace Thor {

struct Rune {
	constexpr Rune(Uint32 v) : v_{v} {}
	[[nodiscard]] Bool is_char() const;
	[[nodiscard]] Bool is_digit() const;
	[[nodiscard]] Bool is_alpha() const;
	[[nodiscard]] Bool is_white() const;
	operator Uint32() const { return v_; }
private:
	Uint32 v_;
};

} // namespace Thor

#endif // THOR_UNICODE