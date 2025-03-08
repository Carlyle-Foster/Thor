#ifndef THOR_TYPES_H
#define THOR_TYPES_H

#include "util/info.h"

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
using Float64 = double;
using Float32 = float;
using Ulen = decltype(sizeof 0);
using Bool = bool;
using Address = unsigned long long;
using Hash = Uint64;
using Unit = struct {};

constexpr Uint8 operator""_u8(unsigned long long int v) { return v; }
constexpr Uint16 operator""_u16(unsigned long long int v) { return v; }
constexpr Uint32 operator""_u32(unsigned long long int v) { return v; }
constexpr Uint64 operator""_u64(unsigned long long int v) { return v; }
constexpr Ulen operator""_ulen(unsigned long long int v) { return v; }

// Helpers for working with lo and hi parts of integer types.
constexpr auto lo(Uint16 v) -> Uint8  { return v; }
constexpr auto hi(Uint16 v) -> Uint8  { return v >> 8; }
constexpr auto lo(Uint32 v) -> Uint16 { return v; }
constexpr auto hi(Uint32 v) -> Uint16 { return v >> 16; }
constexpr auto lo(Uint64 v) -> Uint32 { return v; }
constexpr auto hi(Uint64 v) -> Uint32 { return v >> 32; }

constexpr auto lo(Sint16 v) -> Uint8  { return lo(Uint16(v)); }
constexpr auto hi(Sint16 v) -> Uint8  { return hi(Uint16(v)); }
constexpr auto lo(Sint32 v) -> Uint16 { return lo(Uint32(v)); }
constexpr auto hi(Sint32 v) -> Uint16 { return hi(Uint32(v)); }
constexpr auto lo(Sint64 v) -> Uint32 { return lo(Uint64(v)); }
constexpr auto hi(Sint64 v) -> Uint32 { return hi(Uint64(v)); }

template<typename T>
struct Identity { using Type = T; };

} // namespace Thor

#endif // THOR_TYPES_H
