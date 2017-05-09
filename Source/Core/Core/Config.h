// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>
#include <vector>

#include "Common/Config/Config.h"
#include "Common/Config/Enums.h"
#include "Common/Config/SettingInfo.h"

namespace Config {

template <typename T>
T Get(const SettingInfo<T>& info)
{
  auto meta_layer = GetLayer(LayerType::Meta);
  return meta_layer->GetOrCreateSection(info.system, info.section_name)
      ->template Get<T>(info.key, info.default_value);
}

template <typename T>
void Set(LayerType layer, const SettingInfo<T>& info, const T& value)
{
  GetLayer(layer)->GetOrCreateSection(info.system, info.section_name)
      ->Set(info.key, value);
}

constexpr SettingInfo<bool> WII {System::Main, "Runtime", "bWii", false};

}
