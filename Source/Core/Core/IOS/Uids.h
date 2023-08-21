// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/CommonTypes.h"

namespace IOS
{
// IOS processes have their UID and GID set to their (hardcoded) PID.
enum IOSUid : u32
{
  PID_KERNEL = 0,
  PID_ES = 1,
  PID_FS = 2,
  PID_DI = 3,
  PID_OH0 = 4,
  PID_OH1 = 5,
  PID_EHCI = 6,
  PID_SDI = 7,
  PID_USBETH = 8,
  PID_NET = 9,
  PID_WD = 10,
  PID_WL = 11,
  PID_KD = 12,
  PID_NCD = 13,
  PID_STM = 14,
  PID_PPCBOOT = 15,
  PID_SSL = 16,
  PID_USB = 17,
  PID_P2P = 18,
  PID_UNKNOWN = 19,
};

constexpr u32 FIRST_PPC_UID = 0x1000;

constexpr u32 SYSMENU_UID = FIRST_PPC_UID;
constexpr u16 SYSMENU_GID = 1;

}  // namespace IOS
