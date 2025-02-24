#include "util/assert.h"
#include "util/system.h"

namespace Thor {

void assert(System& sys, StringView cond, StringView file, Sint32 line) {
	sys.process.assert(sys, cond, file, line);
	for (;;);
}

} // namespace Thor