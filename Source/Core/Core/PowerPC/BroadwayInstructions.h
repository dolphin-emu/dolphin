// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <type_traits>

#include "Common/CommonTypes.h"

class BroadwayInstruction
{
public:
  consteval BroadwayInstruction(u32 _OPCD = 0, u32 _SUBOP10 = 0, u32 _hex = 0)
      : hex(_hex), OPCD(_OPCD), SUBOP10(_SUBOP10)
  {
  }
  u32 hex;
  u32 OPCD;
  u32 SUBOP10;
};

constexpr BroadwayInstruction bcctr{0, 0, 0x4e800420};
constexpr BroadwayInstruction bcctrl{0, 0, 0x4e800421};
constexpr BroadwayInstruction bcctrx{19, 528};
constexpr BroadwayInstruction bclrx{19, 16};
constexpr BroadwayInstruction bcx{16};
constexpr BroadwayInstruction bl{18, 0x48000001};
constexpr BroadwayInstruction blr{0, 0, 0x4e800020};
constexpr BroadwayInstruction blrl{0, 0, 0x4e800021};
constexpr BroadwayInstruction bx{18};
constexpr BroadwayInstruction cmp{31};
constexpr BroadwayInstruction cmpi{11};
constexpr BroadwayInstruction cmpl{31, 32};
constexpr BroadwayInstruction cmpli{10};
constexpr BroadwayInstruction cror{19, 449};
constexpr BroadwayInstruction lmw{46};
constexpr BroadwayInstruction mfspr{31, 339};
constexpr BroadwayInstruction mtspr{31, 467};
constexpr BroadwayInstruction rfi{0, 0, 0x4C000064};
constexpr BroadwayInstruction sc{17};
constexpr BroadwayInstruction stmw{47};
constexpr BroadwayInstruction tw{31, 4};
constexpr BroadwayInstruction twi{3};
