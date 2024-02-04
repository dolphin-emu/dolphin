// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Common/JsonUtil.h"

picojson::object ToJsonObject(const Common::Vec3& vec)
{
  picojson::object obj;
  obj.emplace("x", vec.x);
  obj.emplace("y", vec.x);
  obj.emplace("z", vec.x);
  return obj;
}

void FromJson(const picojson::object& obj, Common::Vec3& vec)
{
  const auto x_iter = obj.find("x");
  if (x_iter == obj.end())
  {
    vec.x = 0.0f;
  }
  else
  {
    if (!x_iter->second.is<double>())
    {
      vec.x = 0.0f;
    }
    else
    {
      vec.x = static_cast<float>(x_iter->second.get<double>());
    }
  }

  const auto y_iter = obj.find("y");
  if (y_iter == obj.end())
  {
    vec.y = 0.0f;
  }
  else
  {
    if (!y_iter->second.is<double>())
    {
      vec.y = 0.0f;
    }
    else
    {
      vec.y = static_cast<float>(y_iter->second.get<double>());
    }
  }

  const auto z_iter = obj.find("z");
  if (z_iter == obj.end())
  {
    vec.z = 0.0f;
  }
  else
  {
    if (!z_iter->second.is<double>())
    {
      vec.z = 0.0f;
    }
    else
    {
      vec.z = static_cast<float>(z_iter->second.get<double>());
    }
  }
}
