// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include "Common/Common.h"

#define UDPWM_B1 (1 << 0)
#define UDPWM_B2 (1 << 1)
#define UDPWM_BA (1 << 2)
#define UDPWM_BB (1 << 3)
#define UDPWM_BP (1 << 4)
#define UDPWM_BM (1 << 5)
#define UDPWM_BH (1 << 6)
#define UDPWM_BU (1 << 7)
#define UDPWM_BD (1 << 8)
#define UDPWM_BL (1 << 9)
#define UDPWM_BR (1 << 10)
#define UDPWM_SK (1 << 11)
#define UDPWM_NC (1 << 0)
#define UDPWM_NZ (1 << 1)

enum
{
  CC_B_A = (1 << 0),
  CC_B_B = (1 << 1),
  CC_B_X = (1 << 2),
  CC_B_Y = (1 << 3),
  CC_B_PLUS = (1 << 4),
  CC_B_MINUS = (1 << 5),
  CC_B_HOME = (1 << 6),
  CC_B_L = (1 << 7),
  CC_B_R = (1 << 8),
  CC_B_ZL = (1 << 9),
  CC_B_ZR = (1 << 10),
  CC_B_UP = (1 << 11),
  CC_B_DOWN = (1 << 12),
  CC_B_LEFT = (1 << 13),
  CC_B_RIGHT = (1 << 14),
};

enum
{
  GC_B_A = (1 << 0),
  GC_B_B = (1 << 1),
  GC_B_X = (1 << 2),
  GC_B_Y = (1 << 3),
  GC_B_START = (1 << 4),
  GC_B_L = (1 << 7),
  GC_B_R = (1 << 8),
  GC_B_Z = (1 << 9),
  GC_B_UP = (1 << 11),
  GC_B_DOWN = (1 << 12),
  GC_B_LEFT = (1 << 13),
  GC_B_RIGHT = (1 << 14),
};

namespace RazerHydra
{
bool getAccel(int index, bool sideways, bool has_extension, float* gx, float* gy, float* gz);
bool getButtons(int index, bool sideways, bool has_extension, u32* mask, bool* cycle_extension);
bool getNunchuk(int index, float* jx, float* jy, u8* mask);
void getIR(int index, double* x, double* y, double* z);
bool getNunchuckAccel(int index, float* gx, float* gy, float* gz);
bool getClassic(int index, float* lx, float* ly, float* rx, float* ry, float* l, float* r,
                u32* mask);
bool getGameCube(int index, float* lx, float* ly, float* rx, float* ry, float* l, float* r,
                 u32* mask);
};
