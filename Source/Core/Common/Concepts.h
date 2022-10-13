// 2022 Dolphin Emulator Project
// SPDX-License-Identifier: CC0-1.0

#pragma once

#include <cstddef>
#include <type_traits>

#include "Common/TypeUtils.h"

#include "Common/Future/CppLibConcepts.h"

namespace Common
{
template <class T>
concept TriviallyCopyable = std::is_trivially_copyable<T>::value;

template <class T>
concept NotTriviallyCopyable = !TriviallyCopyable<T>;

template <class T>
concept Const = std::is_const<T>::value;

template <class T>
concept NotConst = !Const<T>;

template <class T>
concept Enumerated = std::is_enum<T>::value;

template <class T>
concept NotEnumerated = !Enumerated<T>;

template <class T>
concept ScopedEnum = std::is_scoped_enum<T>::value;

template <class T>
concept UnscopedEnum = Common::IsUnscopedEnum<T>::value;

template <class T>
concept Class = std::is_class<T>::value;

template <class T>
concept Union = std::is_union<T>::value;

template <class T>
concept Arithmetic = std::is_arithmetic<T>::value;

template <class T>
concept Pointer = std::is_pointer<T>::value;

template <class T>
concept Function = std::is_function<T>::value;

template <class T>
concept FunctionPointer = Function<std::remove_pointer_t<T>>;

template <class T>
concept IntegralOrEnum = std::integral<T> || Enumerated<T>;

template <class T, class U>
concept UnderlyingSameAs = std::same_as<std::underlying_type_t<T>, U>;

template <class T, class U>
concept SameAsOrUnderlyingSameAs = std::same_as<T, U> || UnderlyingSameAs<T, U>;

template <size_t N, class... Ts>
concept CountOfTypes = IsCountOfTypes<N, Ts...>::value;

template <class T, class... Ts>
concept ConvertibleFromAllOf = IsConvertibleFromAllOf<T, Ts...>::value;

template <class T, class... Ts>
concept SameAsAnyOf = IsSameAsAnyOf<T, Ts...>::value;
};  // namespace Common
