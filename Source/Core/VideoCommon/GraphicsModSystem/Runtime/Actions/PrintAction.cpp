// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/GraphicsModSystem/Runtime/Actions/PrintAction.h"

#include "Common/Logging/Log.h"

void PrintAction::OnDrawStarted(bool*)
{
  INFO_LOG_FMT(VIDEO, "OnDrawStarted Called");
}

void PrintAction::OnEFB(bool*, u32 texture_width, u32 texture_height, u32* scaled_width,
                        u32* scaled_height)
{
  if (!scaled_width || !scaled_height)
    return;

  INFO_LOG_FMT(VIDEO, "OnEFB Called. Original [{}, {}], Scaled [{}, {}]", texture_width,
               texture_height, *scaled_width, *scaled_height);
}

void PrintAction::OnProjection(Common::Matrix44*)
{
  INFO_LOG_FMT(VIDEO, "OnProjection Called");
}

void PrintAction::OnProjectionAndTexture(Common::Matrix44*)
{
  INFO_LOG_FMT(VIDEO, "OnProjectionAndTexture Called");
}

void PrintAction::OnTextureLoad()
{
  INFO_LOG_FMT(VIDEO, "OnTextureLoad Called");
}
