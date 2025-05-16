// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <mutex>
#include <shared_mutex>
#include <string>
#include <utility>

#include "Common/CommonTypes.h"
#include "Common/Config/Enums.h"
#include "Common/TypeUtils.h"

namespace Config
{
struct Location
{
  System system{};
  std::string section;
  std::string key;

  bool operator==(const Location& other) const;
  bool operator<(const Location& other) const;
};

template <typename T>
struct CachedValue
{
  T value;
  u64 config_version;
};

template <typename T>
class Info
{
public:
  constexpr Info(const Location& location, const T& default_value)
      : m_location{location}, m_default_value{default_value}, m_cached_value{default_value, 0}
  {
  }

  Info(const Info<T>& other) { *this = other; }

  // Not thread-safe
  Info(Info<T>&& other) { *this = std::move(other); }

  // Make it easy to convert Info<Enum> into Info<UnderlyingType<Enum>>
  // so that enum settings can still easily work with code that doesn't care about the enum values.
  template <Common::TypedEnum<T> Enum>
  Info(const Info<Enum>& other)
  {
    *this = other;
  }

  Info<T>& operator=(const Info<T>& other)
  {
    m_location = other.GetLocation();
    m_default_value = other.GetDefaultValue();
    m_cached_value = other.GetCachedValue();
    return *this;
  }

  // Not thread-safe
  Info<T>& operator=(Info<T>&& other)
  {
    m_location = std::move(other.m_location);
    m_default_value = std::move(other.m_default_value);
    m_cached_value = std::move(other.m_cached_value);
    return *this;
  }

  // Make it easy to convert Info<Enum> into Info<UnderlyingType<Enum>>
  // so that enum settings can still easily work with code that doesn't care about the enum values.
  template <Common::TypedEnum<T> Enum>
  Info<T>& operator=(const Info<Enum>& other)
  {
    m_location = other.GetLocation();
    m_default_value = static_cast<T>(other.GetDefaultValue());
    m_cached_value = other.template GetCachedValueCasted<T>();
    return *this;
  }

  constexpr const Location& GetLocation() const { return m_location; }
  constexpr const T& GetDefaultValue() const { return m_default_value; }

  CachedValue<T> GetCachedValue() const
  {
    std::shared_lock lock(m_cached_value_mutex);
    return m_cached_value;
  }

  template <typename U>
  CachedValue<U> GetCachedValueCasted() const
  {
    std::shared_lock lock(m_cached_value_mutex);
    return CachedValue<U>{static_cast<U>(m_cached_value.value), m_cached_value.config_version};
  }

  void SetCachedValue(const CachedValue<T>& cached_value) const
  {
    std::unique_lock lock(m_cached_value_mutex);
    if (m_cached_value.config_version < cached_value.config_version)
      m_cached_value = cached_value;
  }

private:
  Location m_location;
  T m_default_value;

  mutable CachedValue<T> m_cached_value;
  mutable std::shared_mutex m_cached_value_mutex;
};
}  // namespace Config
