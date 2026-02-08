// SPDX-License-Identifier: CC0-1.0

#pragma once

#include <cstddef>
#include <memory>
#include <type_traits>
#include "Common/CommonTypes.h"
#include "Common/MathUtil.h"

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
struct IsNOf : std::bool_constant<std::conjunction_v<std::is_convertible<Ts, T>...> &&
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

// Lighter than optional<T> but you must manage object lifetime yourself.
// You must call Destroy if you call Construct.
// Useful for containers.
template <typename T>
class ManuallyConstructedValue
{
public:
  template <typename... Args>
  T& Construct(Args&&... args)
  {
    static_assert(sizeof(ManuallyConstructedValue) == sizeof(T));

// TODO: Remove placement-new version when we can require Clang 16.
#if defined(__cpp_aggregate_paren_init) && (__cpp_aggregate_paren_init >= 201902L)
    return *std::construct_at(&m_value.data, std::forward<Args>(args)...);
#else
    return *::new (&m_value.data) T{std::forward<Args>(args)...};
#endif
  }

  void Destroy() { std::destroy_at(&m_value.data); }

  T* Ptr() { return &m_value.data; }
  const T* Ptr() const { return &m_value.data; }
  T& Ref() { return m_value.data; }
  const T& Ref() const { return m_value.data; }

  T* operator->() { return Ptr(); }
  const T* operator->() const { return Ptr(); }
  T& operator*() { return Ref(); }
  const T& operator*() const { return Ref(); }

private:
  union Value
  {
    // The union allows this object's automatic construction to be avoided.
    T data;

    Value() {}
    ~Value() {}

    Value& operator=(const Value&) = delete;
    Value(const Value&) = delete;
    Value& operator=(Value&&) = delete;
    Value(Value&&) = delete;

  } m_value;
};

template <typename T>
concept Enum = std::is_enum_v<T>;

template <typename T, typename Underlying>
concept TypedEnum = std::is_same_v<std::underlying_type_t<T>, Underlying>;

template <typename T>
concept BooleanEnum = TypedEnum<T, bool>;

template <typename T>
requires(sizeof(T) <= sizeof(u64) && std::has_single_bit(sizeof(T)))
using MakeUnsignedSameSize =
    std::tuple_element_t<MathUtil::IntLog2(sizeof(T)), std::tuple<u8, u16, u32, u64>>;

template <std::unsigned_integral T>
using MakeAtLeastU32 = std::conditional_t<std::is_same_v<T, u64>, u64, u32>;

}  // namespace Common
