// Copyright 2019 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>
#include <vector>

#include "Common/CommonTypes.h"
#include "Core/HW/GCMemcard/GCMemcard.h"

class PointerWrap;

namespace Memcard
{
class GCIFile
{
public:
  bool LoadHeader();
  bool LoadSaveBlocks();
  bool HasCopyProtection() const;
  void DoState(PointerWrap& p);
  int UsesBlock(u16 blocknum);

  DEntry m_gci_header;
  std::vector<GCMBlock> m_save_data;
  std::vector<u16> m_used_blocks;
  bool m_dirty = false;
  std::string m_filename;
};
}  // namespace Memcard
