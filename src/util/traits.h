#ifndef THOR_TRAITS_H
#define THOR_TRAITS_H
#include "util/types.h"

namespace Thor {

template<typename T> struct RemoveReference_      { using Type = T; };
template<typename T> struct RemoveReference_<T&>  { using Type = T; };
template<typename T> struct RemoveReference_<T&&> { using Type = T; };
template<typename T>
using RemoveReference = typename RemoveReference_<T>::Type;

template<typename T>
concept Referenceable = requires {
	typename Identity<T&>;
};

template<typename T, auto = Referenceable<T>>
struct AddLValueReference_ { using Type = T; };
template<typename T, auto = Referenceable<T>>
struct AddRValueReference_ { using Type = T; };

template<typename T> struct AddLValueReference_<T, true> { using Type = T&; };
template<typename T> struct AddRValueReference_<T, true> { using Type = T&&; };

template<typename T> using AddLValueReference = typename AddLValueReference_<T>::Type;
template<typename T> using AddRValueReference = typename AddRValueReference_<T>::Type;

template<typename T>
inline constexpr auto is_copy_constructible =
	__is_constructible(T, AddLValueReference<const T>);

template<typename T>
inline constexpr auto is_move_constructible =
	__is_constructible(T, AddRValueReference<T>);

template<typename T>
concept CopyConstructible = is_copy_constructible<T>;

template<typename T>
concept MoveConstructible = is_move_constructible<T>;

template<typename T1, typename T2>
inline constexpr auto is_same = false;

template<typename T>
inline constexpr auto is_same<T, T> = true;

template<typename T1, typename T2>
concept Same = is_same<T1, T2>;

template<typename B, typename D>
inline constexpr auto is_base_of = __is_base_of(B, D);

template<typename D, typename B>
concept DerivedFrom = is_base_of<B, D>;

template<typename T>
inline constexpr bool is_polymorphic = __is_polymorphic(T);

} // namespace Thor

#endif // THOR_TRAITS_H