// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/Boot/DolReader.h"

#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include "Common/IOFile.h"
#include "Common/Swap.h"
#include "Core/Boot/AncastTypes.h"
#include "Core/HW/Memmap.h"
#include "Core/System.h"

DolReader::DolReader(std::vector<u8> buffer) : BootExecutableReader(std::move(buffer))
{
  m_is_valid = Initialize(m_bytes);
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
  if (buffer.size() < sizeof(SDolHeader) || buffer.size() > UINT32_MAX)
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
      u32 section_size = m_dolheader.dataSize[i];
      u32 section_offset = m_dolheader.dataOffset[i];
      if (buffer.size() < section_offset)
        return false;

      std::vector<u8> data(section_size);
      const u8* data_start = &buffer[section_offset];
      std::memcpy(&data[0], data_start,
                  std::min((size_t)section_size, buffer.size() - section_offset));
      m_data_sections.emplace_back(data);
    }
    else
    {
      // Make sure that m_data_sections indexes match header indexes
      m_data_sections.emplace_back();
    }
  }

  // Check if this dol contains an ancast image
  // The ancast image will always be in the first data section
  m_is_ancast = false;
  if (m_data_sections[0].size() > sizeof(EspressoAncastHeader) &&
      m_dolheader.dataAddress[0] == ESPRESSO_ANCAST_LOCATION_VIRT)
  {
    // Check for the ancast magic
    if (Common::swap32(m_data_sections[0].data()) == ANCAST_MAGIC)
      m_is_ancast = true;
  }

  return true;
}

bool DolReader::LoadIntoMemory(Core::System& system, bool only_in_mem1) const
{
  if (!m_is_valid)
    return false;

  if (m_is_ancast)
    return LoadAncastIntoMemory(system);

  auto& memory = system.GetMemory();

  // load all text (code) sections
  for (size_t i = 0; i < m_text_sections.size(); ++i)
  {
    if (!m_text_sections[i].empty() &&
        !(only_in_mem1 &&
          m_dolheader.textAddress[i] + m_text_sections[i].size() >= memory.GetRamSizeReal()))
    {
      memory.CopyToEmu(m_dolheader.textAddress[i], m_text_sections[i].data(),
                       m_text_sections[i].size());
    }
  }

  // load all data sections
  for (size_t i = 0; i < m_data_sections.size(); ++i)
  {
    if (!m_data_sections[i].empty() &&
        !(only_in_mem1 &&
          m_dolheader.dataAddress[i] + m_data_sections[i].size() >= memory.GetRamSizeReal()))
    {
      memory.CopyToEmu(m_dolheader.dataAddress[i], m_data_sections[i].data(),
                       m_data_sections[i].size());
    }
  }

  return true;
}

// On a real console this would be done in the Espresso bootrom
bool DolReader::LoadAncastIntoMemory(Core::System& system) const
{
  // The ancast image will always be in data section 0
  const auto& section = m_data_sections[0];
  const u32 section_address = m_dolheader.dataAddress[0];

  const auto* header = reinterpret_cast<const EspressoAncastHeader*>(section.data());

  // Verify header block size
  if (Common::swap32(header->header_block.header_block_size) != sizeof(AncastHeaderBlock))
  {
    ERROR_LOG_FMT(BOOT, "Ancast: Invalid header block size: 0x{:x}",
                  Common::swap32(header->header_block.header_block_size));
    return false;
  }

  // Make sure this is a PPC ancast image
  if (Common::swap32(header->signature_block.signature_type) != 0x01)
  {
    ERROR_LOG_FMT(BOOT, "Ancast: Invalid signature type: 0x{:x}",
                  Common::swap32(header->signature_block.signature_type));
    return false;
  }

  // Make sure this is a Wii-Mode ancast image
  if (Common::swap32(header->info_block.image_type) != ANCAST_IMAGE_TYPE_ESPRESSO_WII)
  {
    ERROR_LOG_FMT(BOOT, "Ancast: Invalid image type: 0x{:x}",
                  Common::swap32(header->info_block.image_type));
    return false;
  }

  // Verify the body size
  const u32 body_size = Common::swap32(header->info_block.body_size);
  if (body_size + sizeof(EspressoAncastHeader) > section.size())
  {
    ERROR_LOG_FMT(BOOT, "Ancast: Invalid body size: 0x{:x}", body_size);
    return false;
  }

  // Verify the body hash
  const auto digest =
      Common::SHA1::CalculateDigest(section.data() + sizeof(EspressoAncastHeader), body_size);
  if (digest != header->info_block.body_hash)
  {
    ERROR_LOG_FMT(BOOT, "Ancast: Body hash mismatch");
    return false;
  }

  // Check if this is a retail or dev image
  bool is_dev = false;
  if (Common::swap32(header->info_block.console_type) == ANCAST_CONSOLE_TYPE_DEV)
  {
    is_dev = true;
  }
  else if (Common::swap32(header->info_block.console_type) != ANCAST_CONSOLE_TYPE_RETAIL)
  {
    ERROR_LOG_FMT(BOOT, "Ancast: Invalid console type: 0x{:x}",
                  Common::swap32(header->info_block.console_type));
    return false;
  }

  // Decrypt the Ancast image
  static constexpr u8 vwii_ancast_retail_key[0x10] = {0x2e, 0xfe, 0x8a, 0xbc, 0xed, 0xbb,
                                                      0x7b, 0xaa, 0xe3, 0xc0, 0xed, 0x92,
                                                      0xfa, 0x29, 0xf8, 0x66};
  static constexpr u8 vwii_ancast_dev_key[0x10] = {0x26, 0xaf, 0xf4, 0xbb, 0xac, 0x88, 0xbb, 0x76,
                                                   0x9d, 0xfc, 0x54, 0xdd, 0x56, 0xd8, 0xef, 0xbd};
  std::unique_ptr<Common::AES::Context> ctx =
      Common::AES::CreateContextDecrypt(is_dev ? vwii_ancast_dev_key : vwii_ancast_retail_key);

  static constexpr u8 vwii_ancast_iv[0x10] = {0x59, 0x6d, 0x5a, 0x9a, 0xd7, 0x05, 0xf9, 0x4f,
                                              0xe1, 0x58, 0x02, 0x6f, 0xea, 0xa7, 0xb8, 0x87};
  std::vector<u8> decrypted(body_size);
  if (!ctx->Crypt(vwii_ancast_iv, section.data() + sizeof(EspressoAncastHeader), decrypted.data(),
                  body_size))
    return false;

  auto& memory = system.GetMemory();

  // Copy the Ancast header to the emu
  memory.CopyToEmu(section_address, header, sizeof(EspressoAncastHeader));

  // Copy the decrypted body to the emu
  memory.CopyToEmu(section_address + sizeof(EspressoAncastHeader), decrypted.data(), body_size);

  return true;
}
