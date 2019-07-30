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

struct ConfigLocation
{
  System system;
  std::string section;
  std::string key;

  bool operator==(const ConfigLocation& other) const;
  bool operator!=(const ConfigLocation& other) const;
  bool operator<(const ConfigLocation& other) const;
};

template <typename T>
struct ConfigInfo
{
  ConfigInfo(const ConfigLocation& location_, const T& default_value_)
      : location{location_}, default_value{default_value_}
  {
  }

  // Make it easy to convert ConfigInfo<Enum> into ConfigInfo<UnderlyingType<Enum>>
  // so that enum settings can still easily work with code that doesn't care about the enum values.
  template <typename Enum,
            std::enable_if_t<std::is_same<T, detail::UnderlyingType<Enum>>::value>* = nullptr>
  ConfigInfo(const ConfigInfo<Enum>& other)
      : location{other.location}, default_value{static_cast<detail::UnderlyingType<Enum>>(
                                      other.default_value)}
  {
  }

  ConfigLocation location;
  T default_value;
};
}  // namespace Config
