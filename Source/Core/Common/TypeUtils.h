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

// Template for checking if Types is count occurrences of T.
template <typename T, size_t count, typename... Ts>
struct IsNOf : std::integral_constant<bool, std::conjunction_v<std::is_convertible<Ts, T>...> &&
                                                sizeof...(Ts) == count>
{
};

static_assert(IsNOf<int, 0>::value);
static_assert(!IsNOf<int, 0, int>::value);
static_assert(IsNOf<int, 1, int>::value);
static_assert(!IsNOf<int, 1>::value);
static_assert(!IsNOf<int, 1, int, int>::value);
static_assert(IsNOf<int, 2, int, int>::value);
static_assert(IsNOf<int, 2, int, short>::value);  // Type conversions ARE allowed
static_assert(!IsNOf<int, 2, int, char*>::value);

// TODO: This can be replaced with std::array's fill() once C++20 is fully supported.
// Prior to C++20, std::array's fill() function is, unfortunately, not constexpr.
// Ditto for <algorithm>'s std::fill. Although Dolphin targets C++20, Android doesn't
// seem to properly support constexpr fill(), so we need this for now.
template <typename T1, size_t N, typename T2>
constexpr void Fill(std::array<T1, N>& array, const T2& value)
{
  for (auto& entry : array)
  {
    entry = value;
  }
}
}  // namespace Common
