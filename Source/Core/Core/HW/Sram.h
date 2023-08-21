// Copyright 2009 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

// Modified code taken from libogc
/*-------------------------------------------------------------

system.h -- OS functions and initialization

Copyright (C) 2004
Michael Wiedenbauer (shagkur)
Dave Murphy (WinterMute)

This software is provided 'as-is', without any express or implied
warranty.  In no event will the authors be held liable for any
damages arising from the use of this software.

Permission is granted to anyone to use this software for any
purpose, including commercial applications, and to alter it and
redistribute it freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented; you
must not claim that you wrote the original software. If you use
this software in a product, an acknowledgment in the product
documentation would be appreciated but is not required.

2. Altered source versions must be plainly marked as such, and
must not be misrepresented as being the original software.

3. This notice may not be removed or altered from any source
distribution.


-------------------------------------------------------------*/
#pragma once

#include <array>
#include <string>

#include "Common/CommonTypes.h"
#include "Common/Swap.h"

namespace ExpansionInterface
{
enum class Slot : int;
};

using CardFlashId = std::array<u8, 12>;

#pragma pack(push, 1)

// Note: UnlockSram does:
// if ((flags & 3) == 3) flags &= ~3;
// It also checks and can reset gbs_mode

struct SramFlags
{
  enum : u8
  {
    // 0 = Mono, 1 = Stereo
    kStereo = 1 << 2,
    // If unset, IPL will ask user to configure settings
    kOobeDone = 1 << 3,
    // Always display IPL menu on boot, even if disc is inserted
    kBootToMenu = 1 << 6,
    // Display Progressive Scan prompt if the game supports it
    kProgressiveScan = 1 << 7,
  };
  bool stereo() const { return value & kStereo; }
  bool oobe_done() const { return value & kOobeDone; }
  bool boot_to_menu() const { return value & kBootToMenu; }
  bool progressive_scan() const { return value & kProgressiveScan; }
  void set_flag(bool enable, u8 flag)
  {
    if (enable)
      value |= flag;
    else
      value &= ~flag;
  }
  void stereo(bool enable) { set_flag(enable, kStereo); }
  void oobe_done(bool enable) { set_flag(enable, kOobeDone); }
  void boot_to_menu(bool enable) { set_flag(enable, kBootToMenu); }
  void progressive_scan(bool enable) { set_flag(enable, kProgressiveScan); }
  u8 value;
};

struct SramSettings
{
  // checksum covers [rtc_bias, flags]
  Common::BigEndianValue<u16> checksum;
  Common::BigEndianValue<u16> checksum_inv;

  // Unknown attributes
  u32 ead0;
  u32 ead1;

  u32 rtc_bias;

  // Pixel offset for VI
  s8 vi_horizontal_offset;

  // Unknown attribute
  u8 ntd;

  u8 language;
  SramFlags flags;
};

struct SramSettingsEx
{
  // Memorycard unlock flash ID
  CardFlashId flash_id[2];
  // Device IDs of last connected wireless devices
  u32 wireless_kbd_id;
  u16 wireless_pad_id[4];
  // Last non-recoverable error from DI
  u8 di_error_code;
  u8 field_25;
  u8 flash_id_checksum[2];
  u16 gbs_mode;
  u8 field_3e[2];
};

struct Sram
{
  Common::BigEndianValue<u32> rtc{};
  SramSettings settings{};
  SramSettingsEx settings_ex{};
  // Allow access to this entire structure as a raw blob
  // Typical union-with-byte-array method can't be used here on GCC
  u8& operator[](size_t offset) { return reinterpret_cast<u8*>(&rtc)[offset]; }
};
// TODO determine real full sram size for gc/wii
static_assert(sizeof(Sram) == 0x44);

#pragma pack(pop)

void InitSRAM(Sram* sram, const std::string& filename);
void SetCardFlashID(Sram* sram, const u8* buffer, ExpansionInterface::Slot card_slot);
void FixSRAMChecksums(Sram* sram);
