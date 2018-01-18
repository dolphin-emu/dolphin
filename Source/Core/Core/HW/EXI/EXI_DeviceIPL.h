// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>

#include "Core/HW/EXI/EXI_Device.h"

class PointerWrap;

namespace ExpansionInterface
{
class CEXIIPL : public IEXIDevice
{
public:
  CEXIIPL();
  virtual ~CEXIIPL();

  void SetCS(int _iCS) override;
  bool IsPresent() const override;
  void DoState(PointerWrap& p) override;

#if defined(_MSC_VER) && _MSC_VER <= 1800
  enum {UNIX_EPOCH = 0, GC_EPOCH = 0x386D4380, WII_EPOCH = 0x477E5826};
#else
  static constexpr u32 UNIX_EPOCH = 0;          // 1970-01-01 00:00:00
  static constexpr u32 GC_EPOCH = 0x386D4380;   // 2000-01-01 00:00:00
  static constexpr u32 WII_EPOCH = 0x477E5826;  // 2008-01-04 16:00:38
#endif
  // The Wii epoch is suspiciously random, and the Wii was even
  // released before it, but apparently it works anyway?

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
  u8* m_pIPL;

  // STATE_TO_SAVE
  //! RealTimeClock
  u8 m_RTC[4];

  //! Helper
  u32 m_uPosition;
  u32 m_uAddress;
  u32 m_uRWOffset;

  std::string m_buffer;
  bool m_FontsLoaded;

  void UpdateRTC();

  void TransferByte(u8& _uByte) override;
  bool IsWriteCommand() const { return !!(m_uAddress & (1 << 31)); }
  u32 CommandRegion() const { return (m_uAddress & ~(1 << 31)) >> 8; }
  bool LoadFileToIPL(const std::string& filename, u32 offset);
  void LoadFontFile(const std::string& filename, u32 offset);
  std::string FindIPLDump(const std::string& path_prefix);
};
}  // namespace ExpansionInterface
