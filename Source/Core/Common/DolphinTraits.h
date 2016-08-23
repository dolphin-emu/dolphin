// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.
#pragma once

#include <type_traits>

// is_exportable
// True for types that can be exported on a c API

// all Fundamental types (int, char, float, nullptr_t) can be exported
template <typename T>
struct is_exportable : std::is_fundamental<T>
{
};

// pointers to standard layout structs/classes can be exported
template <typename T>
struct is_exportable<T*> : std::is_standard_layout<std::remove_pointer_t<T>>
{
};
template <typename T>
struct is_exportable<T* const> : is_exportable<std::remove_const_t<T>>
{
};

// c style arrays (to otherwise exportable things) can be exported too.
template <typename T>
struct is_exportable<T[]> : is_exportable<T>
{
};
template <typename T, size_t N>
struct is_exportable<T[N]> : is_exportable<T>
{
};

// is all exportable - All arguments is_exportable?
template <typename... Args>
struct is_all_exportable : std::true_type
{
};
template <typename T, typename... Args>
struct is_all_exportable<T, Args...>
    : std::conditional_t<is_exportable<T>::value, is_all_exportable<Args...>, std::false_type>
{
};

// is_pure_arg
// False for types which would allow the callee to modify the caller's state

// Fundamental types are always pure when used in arguments
// Anything passed by value is pure too
template <typename T>
struct is_pure_arg
    : std::conditional_t<
          std::is_fundamental<T>::value, std::true_type,
          std::conditional_t<!std::is_array<T>::value && !std::is_pointer<T>::value &&
                                 !std::is_reference<T>::value,
                             std::true_type, std::false_type>>
{
};

// Arrays are pure if they point to constant types
template <typename T>
struct is_pure_arg<T[]> : public std::is_const<std::remove_all_extents_t<T>>
{
};
template <typename T, size_t N>
struct is_pure_arg<T[N]> : public std::is_const<std::remove_all_extents_t<T>>
{
};

// Pointer are pure if they point to constant types
template <typename T>
struct is_pure_arg<T*> : public std::is_const<std::remove_pointer_t<T>>
{
};

// References are pure if they point to constant types
template <typename T>
struct is_pure_arg<T&> : public std::is_const<std::remove_reference_t<T>>
{
};

// is_all_pure_arg
template <typename... Args>
struct is_all_pure_arg : public std::true_type
{
};
template <typename T, typename... Args>
struct is_all_pure_arg<T, Args...>
    : public std::conditional_t<is_pure_arg<T>::value, is_all_pure_arg<Args...>, std::false_type>
{
};
