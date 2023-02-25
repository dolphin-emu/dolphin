// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/GraphicsModSystem/Runtime/Actions/PrintAction.h"

#include "Common/Logging/Log.h"

void PrintAction::OnDrawStarted(GraphicsModActionData::DrawStarted*)
{
  INFO_LOG_FMT(VIDEO, "OnDrawStarted Called");
}

void PrintAction::OnEFB(GraphicsModActionData::EFB* efb)
{
  if (!efb) [[unlikely]]
    return;

  if (!efb->scaled_width) [[unlikely]]
    return;

  if (!efb->scaled_height) [[unlikely]]
    return;

  INFO_LOG_FMT(VIDEO, "OnEFB Called. Original [{}, {}], Scaled [{}, {}]", efb->texture_width,
               efb->texture_height, *efb->scaled_width, *efb->scaled_height);
}

void PrintAction::OnProjection(GraphicsModActionData::Projection*)
{
  INFO_LOG_FMT(VIDEO, "OnProjection Called");
}

void PrintAction::OnProjectionAndTexture(GraphicsModActionData::Projection*)
{
  INFO_LOG_FMT(VIDEO, "OnProjectionAndTexture Called");
}

void PrintAction::OnTextureLoad(GraphicsModActionData::TextureLoad* texture_load)
{
  if (!texture_load) [[unlikely]]
    return;

  INFO_LOG_FMT(VIDEO, "OnTextureLoad Called.  Texture: {}", texture_load->texture_name);
}
