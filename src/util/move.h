#ifndef THOR_MOVE_H
#define THOR_MOVE_H
#include "util/traits.h"

namespace Thor {

template<typename T>
constexpr RemoveReference<T>&& move(T&& arg) {
	return static_cast<RemoveReference<T>&&>(arg);
}

} // namespace Thor

#endif // THOR_MOVE_H