// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>
#include <vector>

#include "Common/CommonTypes.h"
#include "Core/Boot/Boot.h"

namespace File
{
class IOFile;
}

class DolReader final : public BootExecutableReader
{
public:
  explicit DolReader(const std::string& filename);
  explicit DolReader(File::IOFile file);
  explicit DolReader(std::vector<u8> buffer);
  ~DolReader();

  bool IsValid() const override { return m_is_valid; }
  bool IsWii() const override { return m_is_wii; }
  bool IsAncast() const { return m_is_ancast; };
  u32 GetEntryPoint() const override { return m_dolheader.entryPoint; }
  bool LoadIntoMemory(Core::System& system, bool only_in_mem1 = false) const override;
  bool LoadSymbols(const Core::CPUThreadGuard& guard, PPCSymbolDB& ppc_symbol_db) const override
  {
    return false;
  }

private:
  enum
  {
    DOL_NUM_TEXT = 7,
    DOL_NUM_DATA = 11
  };

  struct SDolHeader
  {
    u32 textOffset[DOL_NUM_TEXT];
    u32 dataOffset[DOL_NUM_DATA];

    u32 textAddress[DOL_NUM_TEXT];
    u32 dataAddress[DOL_NUM_DATA];

    u32 textSize[DOL_NUM_TEXT];
    u32 dataSize[DOL_NUM_DATA];

    u32 bssAddress;
    u32 bssSize;
    u32 entryPoint;
  };
  SDolHeader m_dolheader;

  std::vector<std::vector<u8>> m_data_sections;
  std::vector<std::vector<u8>> m_text_sections;

  bool m_is_valid;
  bool m_is_wii;
  bool m_is_ancast;

  // Copy sections to internal buffers
  bool Initialize(const std::vector<u8>& buffer);

  bool LoadAncastIntoMemory(Core::System& system) const;
};
