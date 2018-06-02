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

private:
  enum
  {
    ROM_SIZE = 1024 * 1024 * 2,
    ROM_MASK = (ROM_SIZE - 1)
  };

  enum
  {
    REGION_RTC = 0x200000,
    REGION_SRAM = 0x200001,
    REGION_UART = 0x200100,
    REGION_UART_UNK = 0x200103,
    REGION_BARNACLE = 0x200113,
    REGION_WRTC0 = 0x210000,
    REGION_WRTC1 = 0x210001,
    REGION_WRTC2 = 0x210008,
    REGION_EUART_UNK = 0x300000,
    REGION_EUART = 0x300001
  };

  //! IPL
  u8* m_ipl;

  // STATE_TO_SAVE
  //! RealTimeClock
  std::array<u8, 4> m_rtc{};

  //! Helper
  u32 m_position = 0;
  u32 m_address = 0;
  u32 m_rw_offset = 0;

  std::string m_buffer;
  bool m_fonts_loaded = false;

  void UpdateRTC();

  void TransferByte(u8& byte) override;
  bool IsWriteCommand() const { return !!(m_address & (1 << 31)); }
  u32 CommandRegion() const { return (m_address & ~(1 << 31)) >> 8; }
  bool LoadFileToIPL(const std::string& filename, u32 offset);
  void LoadFontFile(const std::string& filename, u32 offset);
  std::string FindIPLDump(const std::string& path_prefix);
};
}  // namespace ExpansionInterface
