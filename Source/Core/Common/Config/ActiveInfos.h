// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <optional>
#include <unordered_set>
#include <vector>

#include "Common/Config/Config.h"
#include "Common/Config/Layer.h"
#include "Common/Config/Location.h"

namespace Config
{
template <typename T>
class Info;

class ActiveInfosBase
{
public:
  static void InvalidateCache();

protected:
  // unique_ptr is used to avoid problems with initialization order.
  // We should be able to stop using unique_ptr once we are using C++20 everywhere,
  // since C++20 makes std::vector's default constructor constexpr.
  static std::unique_ptr<std::vector<void (*)()>> s_invalidate_cache_functions;

  static bool s_program_is_exiting;
};

template <typename T>
class ActiveInfos : public ActiveInfosBase
{
public:
  static void Add(Info<T>* info)
  {
    if (!s_active_infos)
    {
      if (!s_invalidate_cache_functions)
        s_invalidate_cache_functions = std::make_unique<std::vector<void (*)()>>();
      s_invalidate_cache_functions->push_back(&InvalidateCache);

      s_active_infos = std::make_unique<InfoSet>();
    }
    s_active_infos->set.insert(info);
  }

  static void Remove(Info<T>* info)
  {
    if (!s_program_is_exiting)
      s_active_infos->set.erase(info);
  }

  static void InvalidateCache()
  {
    for (Info<T>* info : s_active_infos->set)
      Update(info);
  }

  static void Update(Info<T>* info)
  {
    info->m_cached_value = Get(info->GetLocation()).value_or(info->GetDefaultValue());
  }

private:
  static std::optional<T> Get(const Location& location)
  {
    const std::shared_ptr<Layer> layer = GetLayer(GetActiveLayerForConfig(location));
    return layer ? layer->Get<T>(location) : std::nullopt;
  }

  struct InfoSet
  {
    // This destructor exists to avoid a crash when the program exits.
    ~InfoSet() { s_program_is_exiting = true; }

    std::unordered_set<Info<T>*> set;
  };

  // unique_ptr is used to avoid problems with initialization order.
  static inline std::unique_ptr<InfoSet> s_active_infos;
};

}  // namespace Config
