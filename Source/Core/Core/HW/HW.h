// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

class PointerWrap;
struct Sram;
namespace Core
{
class System;
}

namespace HW
{
void Init(Core::System& system, const Sram* override_sram);
void Shutdown(Core::System& system);
void DoState(Core::System& system, PointerWrap& p, bool delta);
}  // namespace HW
