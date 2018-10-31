// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>

#include "Common/CommonTypes.h"

namespace IOS::HLE
{
struct MemoryValues
{
  u16 ios_number;
  u32 ios_version;
  u32 ios_date;
  u32 mem1_physical_size;
  u32 mem1_simulated_size;
  u32 mem1_end;
  u32 mem1_arena_begin;
  u32 mem1_arena_end;
  u32 mem2_physical_size;
  u32 mem2_simulated_size;
  u32 mem2_end;
  u32 mem2_arena_begin;
  u32 mem2_arena_end;
  u32 ipc_buffer_begin;
  u32 ipc_buffer_end;
  u32 hollywood_revision;
  u32 ram_vendor;
  u32 unknown_begin;
  u32 unknown_end;
  u32 sysmenu_sync;
};

const std::array<MemoryValues, 41>& GetMemoryValues();

enum class Feature
{
  // Kernel, ES, FS, STM, DI, OH0, OH1
  Core = 1 << 0,
  // SDIO
  SDIO = 1 << 1,
  // Network (base support: SO, Ethernet; KD, SSL, NCD, Wi-Fi)
  SO = 1 << 2,
  Ethernet = 1 << 3,
  KD = 1 << 4,
  SSL = 1 << 5,
  NCD = 1 << 6,
  WiFi = 1 << 7,
  // KBD
  USB_KBD = 1 << 8,
  // USB_HID v4
  USB_HIDv4 = 1 << 9,
  // SDv2 support
  SDv2 = 1 << 10,
  // New USB modules (USB, USB_VEN, USB_HUB, USB_MSC, OHCI0, USB_HIDv5)
  NewUSB = 1 << 11,
  // EHCI
  EHCI = 1 << 12,
  // WFS (WFSSRV, WFSI, USB_SHARED)
  WFS = 1 << 13,
};

constexpr Feature operator|(Feature lhs, Feature rhs)
{
  return static_cast<Feature>(static_cast<int>(lhs) | static_cast<int>(rhs));
}

constexpr Feature& operator|=(Feature& lhs, Feature rhs)
{
  lhs = lhs | rhs;
  return lhs;
}

constexpr bool HasFeature(Feature features, Feature feature)
{
  return (static_cast<int>(features) & static_cast<int>(feature)) != 0;
}

bool HasFeature(u32 major_version, Feature feature);
Feature GetFeatures(u32 major_version);
bool IsEmulated(u32 major_version);
bool IsEmulated(u64 title_id);
}  // namespace IOS::HLE
