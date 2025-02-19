#ifndef THOR_HASH_H
#define THOR_HASH_H

#include "util/traits.h"

namespace Thor {

template<typename K>
concept Hashable = requires(const K& key) {
	{ key.hash(0_u64) } -> Same<Hash>;
};

constexpr const Hash FNV_OFFSET = 14695981039346656037_u64;
constexpr const Hash FNV_PRIME  = 1099511628211_u64;

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
constexpr Hash hash(T v, Hash h = FNV_OFFSET) {
	return hash(hi(v), hash(lo(v), h));
}
template<>
constexpr Hash hash<Uint8>(Uint8 v, Hash h) {
	return (h ^ Hash(v)) * FNV_PRIME;
}
constexpr Hash hash(Hashable auto v, Hash h = FNV_OFFSET) {
	return v.hash(h);
}

} // namespace Thor

#endif // THOR_HASH_H