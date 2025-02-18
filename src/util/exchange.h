#ifndef THOR_EXCHANGE_H
#define THOR_EXCHANGE_H
#include "util/move.h"
#include "util/forward.h"

namespace Thor {

template<typename T, typename U = T>
constexpr T exchange(T& obj, U&& new_value) {
	T old_value = move(obj);
	obj = forward<U>(new_value);
	return old_value;
}

} // namespace Thor

#endif // THOR_EXCHANGE_H