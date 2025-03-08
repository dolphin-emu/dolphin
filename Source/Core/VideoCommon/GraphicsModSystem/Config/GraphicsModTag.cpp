// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/GraphicsModSystem/Config/GraphicsModTag.h"

#include "Common/JsonUtil.h"
#include "Common/Logging/Log.h"

namespace GraphicsModSystem::Config
{
void GraphicsModTag::Serialize(picojson::object& obj) const
{
  obj.emplace("name", m_name);
  obj.emplace("description", m_description);
}

bool GraphicsModTag::Deserialize(const picojson::object& obj)
{
  const auto name = ReadStringFromJson(obj, "name");
  if (!name)
  {
    ERROR_LOG_FMT(VIDEO,
                  "Failed to load mod configuration file, specified tag has an invalid name");
    return false;
  }
  m_name = *name;
  m_description = ReadStringFromJson(obj, "description").value_or("");

  return true;
}
}  // namespace GraphicsModSystem::Config
