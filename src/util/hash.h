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