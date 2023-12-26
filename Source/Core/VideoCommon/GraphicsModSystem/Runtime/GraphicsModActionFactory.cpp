// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/GraphicsModSystem/Runtime/GraphicsModActionFactory.h"

#include "Common/Logging/Log.h"

#include "VideoCommon/Constants.h"
#include "VideoCommon/GraphicsModSystem/Runtime/Actions/CustomPipelineAction.h"
#include "VideoCommon/GraphicsModSystem/Runtime/Actions/MoveAction.h"
#include "VideoCommon/GraphicsModSystem/Runtime/Actions/PrintAction.h"
#include "VideoCommon/GraphicsModSystem/Runtime/Actions/ScaleAction.h"
#include "VideoCommon/GraphicsModSystem/Runtime/Actions/SkipAction.h"

namespace GraphicsModActionFactory
{
std::unique_ptr<GraphicsModAction> Create(std::string_view name, const picojson::value& json_data,
                                          std::shared_ptr<VideoCommon::CustomAssetLibrary> library)
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
  else if (name == "custom_pipeline")
  {
#ifdef ANDROID
    // Custom shaders currently need more than 8 pixel samplers
    // to be used with textures, rather than make things complicated
    // just disable the feature for Android which has issues
    ERROR_LOG_FMT(VIDEO, "Android needs more than 8 pixel samplers to function, {} provided",
                  VideoCommon::MAX_PIXEL_SHADER_SAMPLERS);
    return nullptr;
#else
    return CustomPipelineAction::Create(json_data, std::move(library));
#endif
  }

  return nullptr;
}
}  // namespace GraphicsModActionFactory
