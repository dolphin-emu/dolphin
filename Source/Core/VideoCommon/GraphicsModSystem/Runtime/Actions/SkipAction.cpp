// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/GraphicsModSystem/Runtime/Actions/SkipAction.h"

void SkipAction::OnDrawStarted(bool* skip)
{
  if (!skip)
    return;

  *skip = true;
}

void SkipAction::OnEFB(bool* skip, u32, u32, u32*, u32*)
{
  if (!skip)
    return;

  *skip = true;
}
