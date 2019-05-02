// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <string>

#include "Core/HW/EXI/EXI_Device.h"

class PointerWrap;

namespace ExpansionInterface
{
class CEXIIPL : public IEXIDevice
{
public:
  CEXIIPL();
  ~CEXIIPL() override;

  void SetCS(int cs) override;
  bool IsPresent() const override;
  void DoState(PointerWrap& p) override;

  static constexpr u32 UNIX_EPOCH = 0;         // 1970-01-01 00:00:00
  static constexpr u32 GC_EPOCH = 0x386D4380;  // 2000-01-01 00:00:00

  static u32 GetEmulatedTime(u32 epoch);
  static u64 NetPlay_GetEmulatedTime();

  static void Descrambler(u8* data, u32 size);

  static bool HasIPLDump();

private:
  std::unique_ptr<u8[]> m_rom;

  // TODO these ranges are highly suspect
  enum
  {
    ROM_BASE = 0,
    ROM_SIZE = 0x200000,
    SRAM_BASE = 0x800000,
    SRAM_SIZE = 0x44,
    UART_BASE = 0x800400,
    UART_SIZE = 0x50,
    WII_RTC_BASE = 0x840000,
    WII_RTC_SIZE = 0x40,
    EUART_BASE = 0xc00000,
    EUART_SIZE = 8,
  };

  struct
  {
    bool is_write() const { return (value >> 31) & 1; }
    // TODO this is definitely a guess
    // Also, the low 6 bits are completely ignored
    u32 address() const { return (value >> 6) & 0x1ffffff; }
    u32 low_bits() const { return value & 0x3f; }
    u32 value;
  } m_command{};
  u32 m_command_bytes_received{};
  // Technically each device has it's own state, but we assume the selected
  // device will not change without toggling cs, and that each device has at
  // most 1 interesting position to keep track of.
  u32 m_cursor{};

  std::string m_buffer;
  bool m_fonts_loaded{};

  void UpdateRTC();

  void TransferByte(u8& data) override;

  bool LoadFileToIPL(const std::string& filename, u32 offset);
  void LoadFontFile(const std::string& filename, u32 offset);

  static std::string FindIPLDump(const std::string& path_prefix);
};
}  // namespace ExpansionInterface
