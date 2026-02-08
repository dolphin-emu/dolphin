// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <mutex>
#include <string>
#include <utility>

#include "Common/CommonTypes.h"
#include "Common/Config/Enums.h"
#include "Common/Mutex.h"
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
  constexpr Info(Location location, T default_value)
      : m_location{std::move(location)}, m_default_value{default_value},
        m_cached_value{std::move(default_value), 0}
  {
  }

  Info(const Info<T>& other)
      : m_location{other.m_location}, m_default_value{other.m_default_value},
        m_cached_value(other.GetCachedValue())
  {
  }

  // Make it easy to convert Info<Enum> into Info<UnderlyingType<Enum>>
  // so that enum settings can still easily work with code that doesn't care about the enum values.
  template <Common::TypedEnum<T> Enum>
  Info(const Info<Enum>& other)
      : m_location{other.GetLocation()}, m_default_value{static_cast<T>(other.GetDefaultValue())},
        m_cached_value(other.template GetCachedValueCasted<T>())
  {
  }

  ~Info() = default;

  // Assignments after construction would require more locking to be thread safe.
  // It seems unnecessary to have this functionality anyways.
  Info& operator=(const Info&) = delete;
  Info& operator=(Info&&) = delete;
  // Moves are also unnecessary and would be thread unsafe without additional locking.
  Info(Info&&) = delete;

  constexpr const Location& GetLocation() const { return m_location; }
  constexpr const T& GetDefaultValue() const { return m_default_value; }

  CachedValue<T> GetCachedValue() const
  {
    std::lock_guard lk{m_cached_value_mutex};
    return m_cached_value;
  }

  template <typename U>
  CachedValue<U> GetCachedValueCasted() const
  {
    std::lock_guard lk{m_cached_value_mutex};
    return {static_cast<U>(m_cached_value.value), m_cached_value.config_version};
  }

  // Only updates if the provided config_version is newer.
  void TryToSetCachedValue(CachedValue<T> new_value) const
  {
    std::lock_guard lk{m_cached_value_mutex};
    if (new_value.config_version > m_cached_value.config_version)
      m_cached_value = std::move(new_value);
  }

private:
  Location m_location;
  T m_default_value;

  mutable CachedValue<T> m_cached_value;

  // In testing, this mutex is effectively never contested.
  // The lock durations are brief and each `Info` object is mostly relevant to one thread.
  // Common::SpinMutex is ~3x faster than std::shared_mutex when uncontested.
  mutable Common::SpinMutex m_cached_value_mutex;
};
}  // namespace Config
