// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

class PointerWrap;

namespace HW
{
void Init();
void Shutdown();
void DoState(PointerWrap& p);
}  // namespace HW
