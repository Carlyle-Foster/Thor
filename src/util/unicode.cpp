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

Bool Rune::is_digit(Sint32 base) const {
	if (v_ < 0x80) {
		Sint32 value = 16; // Base should never be more than this... assert?
		switch (v_) {
		case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9':
			value = v_ - '0';
		case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
			value = v_ - 'a' + 10;
		case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
			value = v_ - 'A' + 10;
		}

		return value < base;
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
