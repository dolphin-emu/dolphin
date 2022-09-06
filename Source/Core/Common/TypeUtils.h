// 2022 Dolphin Emulator Project
// SPDX-License-Identifier: CC0-1.0

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

namespace detail
{
template <int x>
struct Data
{
  static constexpr int GetX() { return x; }
};
struct Foo
{
  Data<1> a;
  Data<2> b;
  int c;
};
struct Bar : Foo
{
  int d;
};

static_assert(std::is_same_v<MemberType<&Foo::a>, Data<1>>);
static_assert(MemberType<&Foo::a>::GetX() == 1);
static_assert(std::is_same_v<MemberType<&Foo::b>, Data<2>>);
static_assert(MemberType<&Foo::b>::GetX() == 2);
static_assert(std::is_same_v<MemberType<&Foo::c>, int>);

static_assert(std::is_same_v<ObjectType<&Foo::a>, Foo>);
static_assert(std::is_same_v<ObjectType<&Foo::b>, Foo>);
static_assert(std::is_same_v<ObjectType<&Foo::c>, Foo>);

static_assert(std::is_same_v<MemberType<&Foo::c>, MemberPointerInfo<int Foo::*>::MemberType>);
static_assert(std::is_same_v<ObjectType<&Foo::c>, MemberPointerInfo<int Foo::*>::ObjectType>);

static_assert(std::is_same_v<MemberType<&Bar::c>, int>);
static_assert(std::is_same_v<MemberType<&Bar::d>, int>);

static_assert(std::is_same_v<ObjectType<&Bar::d>, Bar>);
// Somewhat unexpected behavior:
static_assert(std::is_same_v<ObjectType<&Bar::c>, Foo>);
static_assert(!std::is_same_v<ObjectType<&Bar::c>, Bar>);
}  // namespace detail

// Type trait for checking if the size of Types is equal to N
template <std::size_t N, typename... Ts>
struct IsCountOfTypes : std::bool_constant<sizeof...(Ts) == N>
{
};

// Type trait for checking if all of the given Types are convertible to T
template <typename T, typename... Ts>
struct IsConvertibleFromAllOf : std::conjunction<std::is_convertible<Ts, T>...>
{
};

// Type trait for checking if any of the given Types are the same as T
template <typename T, typename... Ts>
struct IsSameAsAnyOf : std::disjunction<std::is_same<Ts, T>...>
{
};
}  // namespace Common
