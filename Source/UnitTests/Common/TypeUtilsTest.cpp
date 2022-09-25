// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>

#include "Common/TypeUtils.h"

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

TEST(TypeUtils, MemberPointerInfo)
{
  EXPECT_TRUE((std::is_same_v<Common::MemberType<&Foo::a>, Data<1>>));
  EXPECT_TRUE((Common::MemberType<&Foo::a>::GetX() == 1));
  EXPECT_TRUE((std::is_same_v<Common::MemberType<&Foo::b>, Data<2>>));
  EXPECT_TRUE((Common::MemberType<&Foo::b>::GetX() == 2));
  EXPECT_TRUE((std::is_same_v<Common::MemberType<&Foo::c>, int>));

  EXPECT_TRUE((std::is_same_v<Common::ObjectType<&Foo::a>, Foo>));
  EXPECT_TRUE((std::is_same_v<Common::ObjectType<&Foo::b>, Foo>));
  EXPECT_TRUE((std::is_same_v<Common::ObjectType<&Foo::c>, Foo>));

  EXPECT_TRUE((std::is_same_v<Common::MemberType<&Foo::c>,
                              Common::MemberPointerInfo<int Foo::*>::MemberType>));
  EXPECT_TRUE((std::is_same_v<Common::ObjectType<&Foo::c>,
                              Common::MemberPointerInfo<int Foo::*>::ObjectType>));

  EXPECT_TRUE((std::is_same_v<Common::MemberType<&Bar::c>, int>));
  EXPECT_TRUE((std::is_same_v<Common::MemberType<&Bar::d>, int>));

  EXPECT_TRUE((std::is_same_v<Common::ObjectType<&Bar::d>, Bar>));
  // Somewhat unexpected behavior:
  EXPECT_TRUE((std::is_same_v<Common::ObjectType<&Bar::c>, Foo>));
  EXPECT_FALSE((std::is_same_v<Common::ObjectType<&Bar::c>, Bar>));
}

TEST(TypeUtils, IsNOf)
{
  EXPECT_TRUE((Common::IsNOf<int, 0>::value));
  EXPECT_FALSE((Common::IsNOf<int, 0, int>::value));
  EXPECT_TRUE((Common::IsNOf<int, 1, int>::value));
  EXPECT_FALSE((Common::IsNOf<int, 1>::value));
  EXPECT_FALSE((Common::IsNOf<int, 1, int, int>::value));
  EXPECT_TRUE((Common::IsNOf<int, 2, int, int>::value));
  EXPECT_TRUE((Common::IsNOf<int, 2, int, short>::value));  // Type conversions ARE allowed
  EXPECT_FALSE((Common::IsNOf<int, 2, int, char*>::value));
}
