// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

namespace Core
{
class System;
}

class PointerWrap;

void VideoCommon_DoState(Core::System& system, PointerWrap& p);
