// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/BitUtils.h"
#include "Common/CommonTypes.h"

class PointerWrap;
namespace MMIO
{
class Mapping;
}

namespace IOS
{
enum StarletInterruptCause
{
  INT_CAUSE_TIMER = 0x1,
  INT_CAUSE_NAND = 0x2,
  INT_CAUSE_AES = 0x4,
  INT_CAUSE_SHA1 = 0x8,
  INT_CAUSE_EHCI = 0x10,
  INT_CAUSE_OHCI0 = 0x20,
  INT_CAUSE_OHCI1 = 0x40,
  INT_CAUSE_SD = 0x80,
  INT_CAUSE_WIFI = 0x100,

  INT_CAUSE_GPIO_BROADWAY = 0x400,
  INT_CAUSE_GPIO_STARLET = 0x800,

  INT_CAUSE_RST_BUTTON = 0x40000,

  INT_CAUSE_IPC_BROADWAY = 0x40000000,
  INT_CAUSE_IPC_STARLET = 0x80000000
};

enum class GPIO : u32
{
  POWER = 0x1,
  SHUTDOWN = 0x2,
  FAN = 0x4,
  DC_DC = 0x8,
  DI_SPIN = 0x10,
  SLOT_LED = 0x20,
  EJECT_BTN = 0x40,
  SLOT_IN = 0x80,
  SENSOR_BAR = 0x100,
  DO_EJECT = 0x200,
  EEP_CS = 0x400,
  EEP_CLK = 0x800,
  EEP_MOSI = 0x1000,
  EEP_MISO = 0x2000,
  AVE_SCL = 0x4000,
  AVE_SDA = 0x8000,
  DEBUG0 = 0x10000,
  DEBUG1 = 0x20000,
  DEBUG2 = 0x40000,
  DEBUG3 = 0x80000,
  DEBUG4 = 0x100000,
  DEBUG5 = 0x200000,
  DEBUG6 = 0x400000,
  DEBUG7 = 0x800000,
};

extern Common::Flags<GPIO> g_gpio_out;

void Init();
void Reset();
void Shutdown();
void DoState(PointerWrap& p);

void RegisterMMIO(MMIO::Mapping* mmio, u32 base);

void ClearX1();
void GenerateAck(u32 address);
void GenerateReply(u32 address);

bool IsReady();
}  // namespace IOS
