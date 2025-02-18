#include "util/unicode.h"

namespace Thor {

Bool Rune::is_char() const {
	if (v_ < 0x80) {
		if (v_ == '_') {
			return true;
		}
		return ((v_ | 0x20) - 0x61) < 26;
	}
	// TODO(dweiler): LU, LL, LT, LM, LO => true
	return false;
}

Bool Rune::is_digit() const {
	if (v_ < 0x80) {
		return (v_ - '0') < 10;
	}
	// TODO(dweiler): ND => true
	return false;
}

Bool Rune::is_alpha() const {
	return is_char() || is_digit();
}

Bool Rune::is_white() const {
	return v_ == ' ' || v_ == '\t' || v_ == '\n' || v_ == '\r';
}

} // namespace Thor