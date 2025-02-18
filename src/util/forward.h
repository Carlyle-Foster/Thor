#ifndef THOR_FORWARD_H
#define THOR_FORWARD_H
#include "util/traits.h"

namespace Thor {

template<typename T>
T&& forward(RemoveReference<T>&& arg) {
	return static_cast<T&&>(arg);
}

template<typename T>
T&& forward(RemoveReference<T>& arg) {
	return static_cast<T&&>(arg);
}

} // namespace Thor

#endif // THOR_FORWARD_H