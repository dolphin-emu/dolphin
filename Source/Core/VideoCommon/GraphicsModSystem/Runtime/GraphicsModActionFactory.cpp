// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/GraphicsModSystem/Runtime/GraphicsModActionFactory.h"

#include "VideoCommon/GraphicsModSystem/Runtime/Actions/CustomPipelineAction.h"
#include "VideoCommon/GraphicsModSystem/Runtime/Actions/MoveAction.h"
#include "VideoCommon/GraphicsModSystem/Runtime/Actions/PrintAction.h"
#include "VideoCommon/GraphicsModSystem/Runtime/Actions/ScaleAction.h"
#include "VideoCommon/GraphicsModSystem/Runtime/Actions/SkipAction.h"
#include "VideoCommon/GraphicsModSystem/Runtime/Actions/TransformAction.h"

namespace GraphicsModActionFactory
{
std::unique_ptr<GraphicsModAction> Create(std::string_view name, const picojson::value& json_data,
                                          std::shared_ptr<VideoCommon::CustomAssetLibrary> library)
{
  if (name == PrintAction::factory_name)
  {
    return std::make_unique<PrintAction>();
  }
  else if (name == SkipAction::factory_name)
  {
    return std::make_unique<SkipAction>();
  }
  else if (name == MoveAction::factory_name)
  {
    return MoveAction::Create(json_data);
  }
  else if (name == ScaleAction::factory_name)
  {
    return ScaleAction::Create(json_data);
  }
  else if (name == CustomPipelineAction::factory_name)
  {
    return CustomPipelineAction::Create(json_data, std::move(library));
  }
  else if (name == TransformAction::factory_name)
  {
    return TransformAction::Create(json_data);
  }

  return nullptr;
}
}  // namespace GraphicsModActionFactory
