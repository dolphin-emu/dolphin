// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Common/JsonUtil.h"

picojson::object ToJsonObject(const Common::Vec3& vec)
{
  picojson::object obj;
  obj.emplace("x", vec.x);
  obj.emplace("y", vec.y);
  obj.emplace("z", vec.z);
  return obj;
}

void FromJson(const picojson::object& obj, Common::Vec3& vec)
{
  vec.x = ReadNumericFromJson<float>(obj, "x").value_or(0.0f);
  vec.y = ReadNumericFromJson<float>(obj, "y").value_or(0.0f);
  vec.z = ReadNumericFromJson<float>(obj, "z").value_or(0.0f);
}
