// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/Boot/DolReader.h"

#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include "Common/File.h"
#include "Common/FileUtil.h"
#include "Common/Swap.h"
#include "Core/HW/Memmap.h"

DolReader::DolReader(const std::vector<u8>& buffer) : BootExecutableReader(buffer)
{
  m_is_valid = Initialize(buffer);
}

DolReader::DolReader(File::IOFile file) : BootExecutableReader(std::move(file))
{
  m_is_valid = Initialize(m_bytes);
}

DolReader::DolReader(const std::string& filename) : BootExecutableReader(filename)
{
  m_is_valid = Initialize(m_bytes);
}

DolReader::~DolReader() = default;

bool DolReader::Initialize(const std::vector<u8>& buffer)
{
  if (buffer.size() < sizeof(SDolHeader))
    return false;

  memcpy(&m_dolheader, buffer.data(), sizeof(SDolHeader));

  // swap memory
  u32* p = (u32*)&m_dolheader;
  for (size_t i = 0; i < (sizeof(SDolHeader) / sizeof(u32)); i++)
    p[i] = Common::swap32(p[i]);

  const u32 HID4_pattern = Common::swap32(0x7c13fba6);
  const u32 HID4_mask = Common::swap32(0xfc1fffff);

  m_is_wii = false;

  m_text_sections.reserve(DOL_NUM_TEXT);
  for (int i = 0; i < DOL_NUM_TEXT; ++i)
  {
    if (m_dolheader.textSize[i] != 0)
    {
      if (buffer.size() < m_dolheader.textOffset[i] + m_dolheader.textSize[i])
        return false;

      const u8* text_start = &buffer[m_dolheader.textOffset[i]];
      m_text_sections.emplace_back(text_start, &text_start[m_dolheader.textSize[i]]);

      for (unsigned int j = 0; !m_is_wii && j < (m_dolheader.textSize[i] / sizeof(u32)); ++j)
      {
        u32 word = ((u32*)text_start)[j];
        if ((word & HID4_mask) == HID4_pattern)
          m_is_wii = true;
      }
    }
    else
    {
      // Make sure that m_text_sections indexes match header indexes
      m_text_sections.emplace_back();
    }
  }

  m_data_sections.reserve(DOL_NUM_DATA);
  for (int i = 0; i < DOL_NUM_DATA; ++i)
  {
    if (m_dolheader.dataSize[i] != 0)
    {
      if (buffer.size() < m_dolheader.dataOffset[i] + m_dolheader.dataSize[i])
        return false;

      const u8* data_start = &buffer[m_dolheader.dataOffset[i]];
      m_data_sections.emplace_back(data_start, &data_start[m_dolheader.dataSize[i]]);
    }
    else
    {
      // Make sure that m_data_sections indexes match header indexes
      m_data_sections.emplace_back();
    }
  }

  return true;
}

bool DolReader::LoadIntoMemory(bool only_in_mem1) const
{
  if (!m_is_valid)
    return false;

  // load all text (code) sections
  for (size_t i = 0; i < m_text_sections.size(); ++i)
    if (!m_text_sections[i].empty() &&
        !(only_in_mem1 &&
          m_dolheader.textAddress[i] + m_text_sections[i].size() >= Memory::REALRAM_SIZE))
    {
      Memory::CopyToEmu(m_dolheader.textAddress[i], m_text_sections[i].data(),
                        m_text_sections[i].size());
    }

  // load all data sections
  for (size_t i = 0; i < m_data_sections.size(); ++i)
    if (!m_data_sections[i].empty() &&
        !(only_in_mem1 &&
          m_dolheader.dataAddress[i] + m_data_sections[i].size() >= Memory::REALRAM_SIZE))
    {
      Memory::CopyToEmu(m_dolheader.dataAddress[i], m_data_sections[i].data(),
                        m_data_sections[i].size());
    }

  return true;
}
