// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/Boot/DolReader.h"

#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include "Common/Align.h"
#include "Common/BitUtils.h"
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

bool DolReader::Initialize(std::span<const u8> buffer)
{
  if (buffer.size() < sizeof(DolHeader) || buffer.size() > UINT32_MAX)
    return false;

  std::memcpy(&m_dolheader, buffer.data(), sizeof(DolHeader));

  // swap memory
  u32* p = (u32*)&m_dolheader;
  for (std::size_t i = 0; i < (sizeof(DolHeader) / sizeof(u32)); i++)
    p[i] = Common::swap32(p[i]);

  const u32 HID4_pattern = Common::swap32(0x7c13fba6);
  const u32 HID4_mask = Common::swap32(0xfc1fffff);

  m_is_wii = false;
  for (int i = 0; i < DOL_NUM_TEXT; ++i)
  {
    if (m_dolheader.m_text_size[i] == 0)
      continue;

    // apploaders/IOS align the size and work from there.
    // we will do the same, and prepare data to be read later with aligned sizes
    // yes it can read too much, but thats how apploaders/IOS work
    // it will also cause the effect of the dol not loading once it is to be read out of bounds
    const u64 section_size = Common::AlignUp(static_cast<u64>(m_dolheader.m_text_size[i]), 32);
    if (section_size != m_dolheader.m_text_size[i])
    {
      WARN_LOG_FMT(BOOT,
                   "DolReader: Text section {} size is not aligned to 32 bytes and can cause "
                   "issues when loaded",
                   i);
    }

    if (buffer.size() < m_dolheader.m_text_offset[i] + section_size)
      return false;

    auto& section = m_sections.emplace_back();
    section.m_address = m_dolheader.m_text_address[i];
    section.m_header_section_size = m_dolheader.m_text_size[i];
    section.m_data = buffer.subspan(m_dolheader.m_text_offset[i], section_size);

    const u8* const begin = section.m_data.data();
    const u8* const end = begin + section_size;
    for (const u8* ptr = begin; ptr < end; ptr += 4)
    {
      if ((Common::BitCastPtr<u32>(ptr) & HID4_mask) != HID4_pattern)
        continue;

      m_is_wii = true;
      break;
    }
  }

  for (int i = 0; i < DOL_NUM_DATA; ++i)
  {
    if (m_dolheader.m_data_size[i] == 0)
      continue;

    const u64 section_size = Common::AlignUp(static_cast<u64>(m_dolheader.m_data_size[i]), 32);
    const u32 section_offset = m_dolheader.m_data_offset[i];
    if (section_size != m_dolheader.m_data_size[i])
    {
      WARN_LOG_FMT(BOOT,
                   "DolReader: Data section {} size is not aligned to 32 bytes and can cause "
                   "issues when loaded",
                   i);
    }

    if (buffer.size() < section_offset + section_size)
      return false;

    auto& section = m_sections.emplace_back();
    section.m_address = m_dolheader.m_data_address[i];
    section.m_header_section_size = m_dolheader.m_data_size[i];
    section.m_data = buffer.subspan(section_offset, section_size);

    // Check if this dol contains an ancast image
    // The ancast image will always be in the first data section
    if (i == 0 && section.m_header_section_size > sizeof(EspressoAncastHeader) &&
        section.m_address == ESPRESSO_ANCAST_LOCATION_VIRT &&
        Common::swap32(section.m_data.data()) == ANCAST_MAGIC)
    {
      m_ancast_index = i;
    }
  }

  return true;
}

bool DolReader::LoadIntoMemory(Core::System& system, bool only_in_mem1) const
{
  if (!m_is_valid)
    return false;

  if (IsAncast())
    return LoadAncastIntoMemory(system);

  auto& memory = system.GetMemory();

  // load all loadable sections
  for (auto& section : m_sections)
  {
    if (only_in_mem1 && section.m_address + section.m_data.size() >= memory.GetRamSizeReal())
      continue;

    memory.CopyToEmu(section.m_address, section.m_data.data(), section.m_data.size());
  }

  return true;
}

// On a real console this would be done in the Espresso bootrom
bool DolReader::LoadAncastIntoMemory(Core::System& system) const
{
  const LoadableSection& section = m_sections[m_ancast_index.value()];
  const u32 section_address = m_dolheader.m_data_address[0];
  const EspressoAncastHeader header =
      Common::BitCastPtr<EspressoAncastHeader>(section.m_data.data());

  // Verify header block size
  if (Common::swap32(header.header_block.header_block_size) != sizeof(AncastHeaderBlock))
  {
    ERROR_LOG_FMT(BOOT, "Ancast: Invalid header block size: 0x{:x}",
                  Common::swap32(header.header_block.header_block_size));
    return false;
  }

  // Make sure this is a PPC ancast image
  if (Common::swap32(header.signature_block.signature_type) != 0x01)
  {
    ERROR_LOG_FMT(BOOT, "Ancast: Invalid signature type: 0x{:x}",
                  Common::swap32(header.signature_block.signature_type));
    return false;
  }

  // Make sure this is a Wii-Mode ancast image
  if (Common::swap32(header.info_block.image_type) != ANCAST_IMAGE_TYPE_ESPRESSO_WII)
  {
    ERROR_LOG_FMT(BOOT, "Ancast: Invalid image type: 0x{:x}",
                  Common::swap32(header.info_block.image_type));
    return false;
  }

  // Verify the body size
  const u32 body_size = Common::swap32(header.info_block.body_size);
  if (body_size + sizeof(EspressoAncastHeader) > section.m_header_section_size)
  {
    ERROR_LOG_FMT(BOOT, "Ancast: Invalid body size: 0x{:x}", body_size);
    return false;
  }

  // Verify the body hash
  const auto digest = Common::SHA1::CalculateDigest(
      section.m_data.data() + sizeof(EspressoAncastHeader), body_size);
  if (digest != header.info_block.body_hash)
  {
    ERROR_LOG_FMT(BOOT, "Ancast: Body hash mismatch");
    return false;
  }

  // Check if this is a retail or dev image
  bool is_dev = false;
  if (Common::swap32(header.info_block.console_type) == ANCAST_CONSOLE_TYPE_DEV)
  {
    is_dev = true;
  }
  else if (Common::swap32(header.info_block.console_type) != ANCAST_CONSOLE_TYPE_RETAIL)
  {
    ERROR_LOG_FMT(BOOT, "Ancast: Invalid console type: 0x{:x}",
                  Common::swap32(header.info_block.console_type));
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
  if (!ctx->Crypt(vwii_ancast_iv, section.m_data.data() + sizeof(EspressoAncastHeader),
                  decrypted.data(), body_size))
    return false;

  auto& memory = system.GetMemory();

  // Copy the Ancast header to the emu
  memory.CopyToEmu(section_address, &header, sizeof(EspressoAncastHeader));

  // Copy the decrypted body to the emu
  memory.CopyToEmu(section_address + sizeof(EspressoAncastHeader), decrypted.data(), body_size);

  return true;
}
