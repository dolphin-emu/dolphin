// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <type_traits>
#include <utility>

#include "Common/Config/ActiveInfos.h"
#include "Common/Config/Location.h"

namespace Config
{
namespace detail
{
// std::underlying_type may only be used with enum types, so make sure T is an enum type first.
template <typename T>
using UnderlyingType = typename std::enable_if_t<std::is_enum<T>{}, std::underlying_type<T>>::type;
}  // namespace detail

template <typename T>
class Info
{
  friend class ActiveInfos<T>;

public:
  Info(const Location& location, const T& default_value)
      : m_location{location}, m_default_value{default_value}
  {
    ActiveInfos<T>::Update(this);
    ActiveInfos<T>::Add(this);
  }

  Info(Location&& location, T&& default_value)
      : m_location{std::move(location)}, m_default_value{std::move(default_value)}
  {
    ActiveInfos<T>::Update(this);
    ActiveInfos<T>::Add(this);
  }

  // Make it easy to convert Info<Enum> into Info<UnderlyingType<Enum>>
  // so that enum settings can still easily work with code that doesn't care about the enum values.
  template <typename Enum,
            std::enable_if_t<std::is_same<T, detail::UnderlyingType<Enum>>::value>* = nullptr>
  Info(const Info<Enum>& other)
      : m_location{other.GetLocation()}, m_default_value{static_cast<detail::UnderlyingType<Enum>>(
                                             other.GetDefaultValue())},
        m_cached_value{static_cast<detail::UnderlyingType<Enum>>(other.Get())}
  {
    ActiveInfos<T>::Add(this);
  }

  Info(const Info<T>& other)
      : m_location{other.m_location}, m_default_value{other.m_default_value},
        m_cached_value{other.m_cached_value}
  {
    ActiveInfos<T>::Add(this);
  }

  Info(Info<T>&& other)
      : m_location{std::move(other.location)}, m_default_value{std::move(other.m_default_value)},
        m_cached_value{std::move(other.m_cached_value)}
  {
    ActiveInfos<T>::Add(this);
  }

  Info<T>& operator=(const Info<T>& other) = default;
  Info<T>& operator=(Info<T>&& other) = default;

  ~Info() { ActiveInfos<T>::Remove(this); }

  const Location& GetLocation() const { return m_location; }
  const T& GetDefaultValue() const { return m_default_value; }
  const T& Get() const { return m_cached_value; }

private:
  Location m_location;
  T m_default_value;
  T m_cached_value;
};
}  // namespace Config
