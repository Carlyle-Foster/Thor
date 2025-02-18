#ifndef THOR_TYPES_H
#define THOR_TYPES_H

// Work out the compiler being used.
#if defined(__clang__)
	#define THOR_COMPILER_CLANG
#elif defined(__GNUC__) || defined(__GNUG__)
	#define THOR_COMPILER_GCC
#elif defined(_MSC_VER)
	#define THOR_COMPILER_MSVC
#endif

struct Nat {};
inline void *operator new(decltype(sizeof 0), void* ptr, Nat) {
	return ptr;
}

namespace Thor {

using Uint8 = unsigned char;
using Sint8 = signed char;
using Uint16 = unsigned short;
using Sint16 = signed short;
using Uint32 = unsigned int;
using Sint32 = signed int;
using Uint64 = unsigned long long;
using Sint64 = signed long long;
using Ulen = decltype(sizeof 0);
using Bool = bool;
using Address = unsigned long long;

constexpr Uint8 operator""_u8(unsigned long long int v) { return v; }
constexpr Uint16 operator""_u16(unsigned long long int v) { return v; }
constexpr Uint32 operator""_u32(unsigned long long int v) { return v; }
constexpr Uint64 operator""_u64(unsigned long long int v) { return v; }
constexpr Ulen operator""_ulen(unsigned long long int v) { return v; }

template<typename T>
struct Identity { using Type = T; };

} // namespace Thor

#endif // THOR_TYPES_H