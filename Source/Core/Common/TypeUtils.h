// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cstddef>
#include <type_traits>

namespace Common
{
template <typename>
struct MemberPointerInfo;
// Helper to get information about a pointer to a data member.
// See https://en.cppreference.com/w/cpp/language/pointer#Pointers_to_members
// This template takes the type for a member pointer.
template <typename M, typename O>
struct MemberPointerInfo<M O::*>
{
  using MemberType = M;
  using ObjectType = O;
};

// This template takes a specific member pointer.
template <auto member_pointer>
using MemberType = typename MemberPointerInfo<decltype(member_pointer)>::MemberType;

// This template takes a specific member pointer.
template <auto member_pointer>
using ObjectType = typename MemberPointerInfo<decltype(member_pointer)>::ObjectType;

// Template for checking if Types is count occurrences of T.
template <typename T, size_t count, typename... Ts>
struct IsNOf : std::integral_constant<bool, std::conjunction_v<std::is_convertible<Ts, T>...> &&
                                                sizeof...(Ts) == count>
{
};
}  // namespace Common
