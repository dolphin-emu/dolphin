// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/GraphicsModSystem/Runtime/Actions/SkipAction.h"

void SkipAction::OnDrawStarted(GraphicsModActionData::DrawStarted* draw_started)
{
  if (!draw_started) [[unlikely]]
    return;

  if (!draw_started->skip) [[unlikely]]
    return;

  *draw_started->skip = true;
}

void SkipAction::OnEFB(GraphicsModActionData::EFB* efb)
{
  if (!efb) [[unlikely]]
    return;

  if (!efb->skip) [[unlikely]]
    return;

  *efb->skip = true;
}
