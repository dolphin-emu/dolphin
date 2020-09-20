// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>
#include <type_traits>

#include "Common/Config/Enums.h"

namespace Config
{
namespace detail
{
// std::underlying_type may only be used with enum types, so make sure T is an enum type first.
template <typename T>
using UnderlyingType = typename std::enable_if_t<std::is_enum<T>{}, std::underlying_type<T>>::type;
}  // namespace detail

struct Location
{
  System system;
  std::string section;
  std::string key;

  bool operator==(const Location& other) const;
  bool operator!=(const Location& other) const;
  bool operator<(const Location& other) const;
};

template <typename T>
class Info
{
public:
  constexpr Info(const Location& location, const T& default_value)
      : m_location{location}, m_default_value{default_value}
  {
  }

  // Make it easy to convert Info<Enum> into Info<UnderlyingType<Enum>>
  // so that enum settings can still easily work with code that doesn't care about the enum values.
  template <typename Enum,
            std::enable_if_t<std::is_same<T, detail::UnderlyingType<Enum>>::value>* = nullptr>
  constexpr Info(const Info<Enum>& other)
      : m_location{other.GetLocation()}, m_default_value{static_cast<detail::UnderlyingType<Enum>>(
                                             other.GetDefaultValue())}
  {
  }

  constexpr const Location& GetLocation() const { return m_location; }
  constexpr const T& GetDefaultValue() const { return m_default_value; }

private:
  Location m_location;
  T m_default_value;
};
}  // namespace Config
