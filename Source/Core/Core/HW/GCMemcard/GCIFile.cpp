// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/HW/GCMemcard/GCIFile.h"

#include <cinttypes>

#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "Common/File.h"
#include "Common/Logging/Log.h"

bool GCIFile::LoadHeader()
{
  if (m_filename.empty())
    return false;

  File::IOFile save_file(m_filename, "rb");
  if (!save_file)
    return false;

  INFO_LOG(EXPANSIONINTERFACE, "Reading header from disk for %s", m_filename.c_str());
  if (!save_file.ReadBytes(&m_gci_header, sizeof(m_gci_header)))
  {
    ERROR_LOG(EXPANSIONINTERFACE, "Failed to read header for %s", m_filename.c_str());
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

    INFO_LOG(EXPANSIONINTERFACE, "Reading savedata from disk for %s", m_filename.c_str());
    u16 num_blocks = m_gci_header.m_block_count;

    const u32 size = num_blocks * BLOCK_SIZE;
    u64 file_size = save_file.GetSize();
    if (file_size != size + DENTRY_SIZE)
    {
      ERROR_LOG(EXPANSIONINTERFACE,
                "%s\nwas not loaded because it is an invalid GCI.\n File size (0x%" PRIx64
                ") does not match the size recorded in the header (0x%x)",
                m_filename.c_str(), file_size, size + DENTRY_SIZE);
      return false;
    }

    m_save_data.resize(num_blocks);
    save_file.Seek(DENTRY_SIZE, SEEK_SET);
    if (!save_file.ReadBytes(m_save_data.data(), size))
    {
      ERROR_LOG(EXPANSIONINTERFACE, "Failed to read data from GCI file %s", m_filename.c_str());
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
  p.DoPOD<DEntry>(m_gci_header);
  p.Do(m_dirty);
  p.Do(m_filename);
  int num_blocks = (int)m_save_data.size();
  p.Do(num_blocks);
  m_save_data.resize(num_blocks);
  for (auto itr = m_save_data.begin(); itr != m_save_data.end(); ++itr)
  {
    p.DoPOD<GCMBlock>(*itr);
  }
  p.Do(m_used_blocks);
}
