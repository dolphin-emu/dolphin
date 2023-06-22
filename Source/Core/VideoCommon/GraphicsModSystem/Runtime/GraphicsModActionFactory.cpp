// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/GraphicsModSystem/Runtime/GraphicsModActionFactory.h"

#include "VideoCommon/GraphicsModSystem/Runtime/Actions/MoveAction.h"
#include "VideoCommon/GraphicsModSystem/Runtime/Actions/PrintAction.h"
#include "VideoCommon/GraphicsModSystem/Runtime/Actions/ScaleAction.h"
#include "VideoCommon/GraphicsModSystem/Runtime/Actions/SkipAction.h"

namespace GraphicsModActionFactory
{
std::unique_ptr<GraphicsModAction> Create(std::string_view name, const picojson::value& json_data,
                                          std::string_view path)
{
  if (name == "print")
  {
    return std::make_unique<PrintAction>();
  }
  else if (name == "skip")
  {
    return std::make_unique<SkipAction>();
  }
  else if (name == "move")
  {
    return MoveAction::Create(json_data);
  }
  else if (name == "scale")
  {
    return ScaleAction::Create(json_data);
  }

  return nullptr;
}
}  // namespace GraphicsModActionFactory
