// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>
#include <vector>

#include "Core/HW/EXI/EXI_Device.h"

class PointerWrap;

namespace ExpansionInterface
{
class CEXIAgp : public IEXIDevice
{
public:
  CEXIAgp(const int index);
  virtual ~CEXIAgp() override;
  bool IsPresent() const override { return true; }
  void ImmWrite(u32 _uData, u32 _uSize) override;
  u32 ImmRead(u32 _uSize) override;
  void DoState(PointerWrap& p) override;

private:
  enum
  {
    EE_IGNORE_BITS = 0x4,
    EE_DATA_BITS = 0x40,
    EE_READ_FALSE = 0xA,
    EE_READ_TRUE = 0xB,
  };

  int m_slot;

  //! ROM
  u32 m_rom_size = 0;
  u32 m_rom_mask = 0;
  u32 m_eeprom_size = 0;
  u32 m_eeprom_mask = 0;
  std::vector<u8> m_rom;
  std::vector<u8> m_eeprom;

  //! Helper
  u32 m_position = 0;
  u32 m_address = 0;
  u32 m_rw_offset = 0;
  u64 m_eeprom_data = 0;
  u16 m_eeprom_pos = 0;
  u32 m_eeprom_cmd = 0;
  u16 m_eeprom_add_end = 0;
  u16 m_eeprom_add_mask = 0;
  u16 m_eeprom_read_mask = 0;
  u32 m_eeprom_status_mask = 0;
  bool m_eeprom_write_status = false;

  void LoadFileToROM(const std::string& filename);
  void LoadFileToEEPROM(const std::string& filename);
  void SaveFileFromEEPROM(const std::string& filename);
  void LoadRom();
  void CRC8(const u8* data, u32 size);

  u8 m_hash = 0;
  u32 m_current_cmd = 0;
  u32 m_return_pos = 0;
};
}  // namespace ExpansionInterface
