// Copyright 2019 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HW/GCMemcard/GCIFile.h"

#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "Common/IOFile.h"
#include "Common/Logging/Log.h"

namespace Memcard
{
bool GCIFile::LoadHeader()
{
  if (m_filename.empty())
    return false;

  File::IOFile save_file(m_filename, "rb");
  if (!save_file)
    return false;

  INFO_LOG_FMT(EXPANSIONINTERFACE, "Reading header from disk for {}", m_filename);
  if (!save_file.ReadBytes(&m_gci_header, sizeof(m_gci_header)))
  {
    ERROR_LOG_FMT(EXPANSIONINTERFACE, "Failed to read header for {}", m_filename);
    return false;
  }

  return true;
}

bool GCIFile::LoadSaveBlocks()
{
  if (m_save_data.empty())
  {
    if (m_filename.empty())
      return false;

    File::IOFile save_file(m_filename, "rb");
    if (!save_file)
      return false;

    INFO_LOG_FMT(EXPANSIONINTERFACE, "Reading savedata from disk for {}", m_filename);
    const u16 num_blocks = m_gci_header.m_block_count;

    const u32 size = num_blocks * BLOCK_SIZE;
    const u64 file_size = save_file.GetSize();
    if (file_size != size + DENTRY_SIZE)
    {
      ERROR_LOG_FMT(EXPANSIONINTERFACE,
                    "{}\nwas not loaded because it is an invalid GCI.\n File size ({:#x}) does not "
                    "match the size recorded in the header ({:#x})",
                    m_filename.c_str(), file_size, size + DENTRY_SIZE);
      return false;
    }

    m_save_data.resize(num_blocks);
    save_file.Seek(DENTRY_SIZE, File::SeekOrigin::Begin);
    if (!save_file.ReadBytes(m_save_data.data(), size))
    {
      ERROR_LOG_FMT(EXPANSIONINTERFACE, "Failed to read data from GCI file {}", m_filename);
      m_save_data.clear();
      return false;
    }
  }
  return true;
}

bool GCIFile::HasCopyProtection() const
{
  if ((strcmp(reinterpret_cast<const char*>(m_gci_header.m_filename.data()), "PSO_SYSTEM") == 0) ||
      (strcmp(reinterpret_cast<const char*>(m_gci_header.m_filename.data()), "PSO3_SYSTEM") == 0) ||
      (strcmp(reinterpret_cast<const char*>(m_gci_header.m_filename.data()), "f_zero.dat") == 0))
    return true;
  return false;
}

int GCIFile::UsesBlock(u16 block_num)
{
  for (u16 i = 0; i < m_used_blocks.size(); ++i)
  {
    if (m_used_blocks[i] == block_num)
      return i;
  }
  return -1;
}

void GCIFile::DoState(PointerWrap& p)
{
  p.Do(m_gci_header);
  p.Do(m_dirty);
  p.Do(m_filename);
  p.Do(m_save_data);
  p.Do(m_used_blocks);
}
}  // namespace Memcard
