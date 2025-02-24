#ifndef THOR_ASSERT_H
#define THOR_ASSERT_H
#include "util/string.h"

namespace Thor {

[[noreturn]] void assert(System& sys, StringView cond, StringView file, Sint32 line);

} // namespace Thor

#define THOR_ASSERT(sys, cond) \
  ((cond) ? (void)0 : ::Thor::assert((sys), #cond, __FILE__, __LINE__))

#endif // THOR_ASSERT_H