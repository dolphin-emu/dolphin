// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DiscIO/NANDImporter.h"

#include <array>
#include <cinttypes>
#include <cstring>

#include "Common/Crypto/AES.h"
#include "Common/File.h"
#include "Common/FileUtil.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Common/StringUtil.h"
#include "Common/Swap.h"
#include "DiscIO/NANDContentLoader.h"

namespace DiscIO
{
constexpr size_t NAND_SIZE = 0x20000000;
constexpr size_t NAND_KEYS_SIZE = 0x400;

NANDImporter::NANDImporter() = default;
NANDImporter::~NANDImporter() = default;

void NANDImporter::ImportNANDBin(const std::string& path_to_bin,
                                 std::function<void()> update_callback)
{
  m_update_callback = std::move(update_callback);

  if (!ReadNANDBin(path_to_bin))
    return;

  const std::string nand_root = File::GetUserPath(D_WIIROOT_IDX);
  m_nand_root_length = nand_root.length();
  if (nand_root.back() == '/')
    m_nand_root_length++;

  FindSuperblock();
  ProcessEntry(0, nand_root);
  ExportKeys(nand_root);
  ExtractCertificates(nand_root);

  // We have to clear the cache so the new NAND takes effect
  DiscIO::NANDContentManager::Access().ClearCache();
}

bool NANDImporter::ReadNANDBin(const std::string& path_to_bin)
{
  constexpr size_t NAND_TOTAL_BLOCKS = 0x40000;
  constexpr size_t NAND_BLOCK_SIZE = 0x800;
  constexpr size_t NAND_ECC_BLOCK_SIZE = 0x40;
  constexpr size_t NAND_BIN_SIZE =
      (NAND_BLOCK_SIZE + NAND_ECC_BLOCK_SIZE) * NAND_TOTAL_BLOCKS + NAND_KEYS_SIZE;  // 0x21000400

  File::IOFile file(path_to_bin, "rb");
  if (file.GetSize() != NAND_BIN_SIZE)
  {
    PanicAlertT("This file does not look like a BootMii NAND backup. (0x%" PRIx64
                " does not equal 0x%zx)",
                file.GetSize(), NAND_BIN_SIZE);
    return false;
  }

  m_nand.resize(NAND_SIZE);

  for (size_t i = 0; i < NAND_TOTAL_BLOCKS; i++)
  {
    // Instead of updating on every cycle, we only update every 1000 cycles for a balance between
    // not updating fast enough vs updating too fast
    if (i % 1000 == 0)
      m_update_callback();

    file.ReadBytes(&m_nand[i * NAND_BLOCK_SIZE], NAND_BLOCK_SIZE);
    file.Seek(NAND_ECC_BLOCK_SIZE, SEEK_CUR);  // We don't care about the ECC blocks
  }

  m_nand_keys.resize(NAND_KEYS_SIZE);
  file.ReadBytes(m_nand_keys.data(), NAND_KEYS_SIZE);
  return true;
}

void NANDImporter::FindSuperblock()
{
  constexpr size_t NAND_SUPERBLOCK_START = 0x1fc00000;
  constexpr size_t NAND_SUPERBLOCK_SIZE = 0x40000;

  size_t superblock = 0;
  u32 newest_version = 0;
  for (size_t pos = NAND_SUPERBLOCK_START; pos < NAND_SIZE; pos += NAND_SUPERBLOCK_SIZE)
  {
    if (!memcmp(m_nand.data() + pos, "SFFS", 4))
    {
      u32 version = Common::swap32(&m_nand[pos + 4]);
      INFO_LOG(DISCIO, "Found superblock at 0x%zx with version 0x%x", pos, version);
      if (superblock == 0 || version > newest_version)
      {
        superblock = pos;
        newest_version = version;
      }
    }
  }

  m_nand_fat_offset = superblock + 0xC;
  m_nand_fst_offset = m_nand_fat_offset + 0x10000;
  INFO_LOG(DISCIO, "Using superblock version 0x%x at position 0x%zx. FAT/FST offset: 0x%zx/0x%zx",
           newest_version, superblock, m_nand_fat_offset, m_nand_fst_offset);
}

std::string NANDImporter::GetPath(const NANDFSTEntry& entry, const std::string& parent_path)
{
  std::string name(reinterpret_cast<const char*>(&entry.name), sizeof(NANDFSTEntry::name));
  // Get rid of any extra null characters
  while (name.back() == '\0')
    name.pop_back();

  if (name.front() == '/' || parent_path.back() == '/')
    return parent_path + name;

  return parent_path + '/' + name;
}

std::string NANDImporter::FormatDebugString(const NANDFSTEntry& entry)
{
  return StringFromFormat("%12.12s 0x%02x 0x%02x 0x%04x 0x%04x 0x%08x 0x%04x 0x%04x 0x%04x 0x%08x",
                          entry.name, entry.mode, entry.attr, entry.sub, entry.sib, entry.size,
                          entry.x1, entry.uid, entry.gid, entry.x3);
}

void NANDImporter::ProcessEntry(u16 entry_number, const std::string& parent_path)
{
  NANDFSTEntry entry;
  memcpy(&entry, &m_nand[m_nand_fst_offset + sizeof(NANDFSTEntry) * Common::swap16(entry_number)],
         sizeof(NANDFSTEntry));

  if (entry.sib != 0xffff)
    ProcessEntry(entry.sib, parent_path);

  if ((entry.mode & 3) == 1)
    ProcessFile(entry, parent_path);
  else if ((entry.mode & 3) == 2)
    ProcessDirectory(entry, parent_path);
  else
    ERROR_LOG(DISCIO, "Unknown mode: %s", FormatDebugString(entry).c_str());
}

void NANDImporter::ProcessDirectory(const NANDFSTEntry& entry, const std::string& parent_path)
{
  m_update_callback();
  INFO_LOG(DISCIO, "Path: %s", FormatDebugString(entry).c_str());

  const std::string path = GetPath(entry, parent_path);
  File::CreateDir(path);

  if (entry.sub != 0xffff)
    ProcessEntry(entry.sub, path);

  INFO_LOG(DISCIO, "Path: %s", parent_path.c_str() + m_nand_root_length);
}

void NANDImporter::ProcessFile(const NANDFSTEntry& entry, const std::string& parent_path)
{
  constexpr size_t NAND_AES_KEY_OFFSET = 0x158;
  constexpr size_t NAND_FAT_BLOCK_SIZE = 0x4000;

  m_update_callback();
  INFO_LOG(DISCIO, "File: %s", FormatDebugString(entry).c_str());

  const std::string path = GetPath(entry, parent_path);
  File::IOFile file(path, "wb");
  std::array<u8, 16> key{};
  std::copy(&m_nand_keys[NAND_AES_KEY_OFFSET], &m_nand_keys[NAND_AES_KEY_OFFSET + key.size()],
            key.begin());
  u16 sub = Common::swap16(entry.sub);
  u32 remaining_bytes = Common::swap32(entry.size);

  while (remaining_bytes > 0)
  {
    std::array<u8, 16> iv{};
    std::vector<u8> block = Common::AES::Decrypt(
        key.data(), iv.data(), &m_nand[NAND_FAT_BLOCK_SIZE * sub], NAND_FAT_BLOCK_SIZE);
    u32 size = remaining_bytes < NAND_FAT_BLOCK_SIZE ? remaining_bytes : NAND_FAT_BLOCK_SIZE;
    file.WriteBytes(block.data(), size);
    remaining_bytes -= size;
    sub = Common::swap16(&m_nand[m_nand_fat_offset + 2 * sub]);
  }
}

void NANDImporter::ExtractCertificates(const std::string& nand_root)
{
  const std::string title_contents_path =
      nand_root + "/title/00000001/0000000d/content/00000011.app";
  File::IOFile file(title_contents_path, "rb");
  if (!file)
  {
    PanicAlertT("Unable to open %s! Refer to "
                "https://dolphin-emu.org/docs/guides/wii-network-guide/ to set up "
                "certificates.",
                title_contents_path.c_str());
    return;
  }

  struct Certificate
  {
    u32 offset;
    u32 size;
    std::string filename;
  };
  std::array<Certificate, 3> certificates = {{{0x92834, 1005, "/clientca.pem"},
                                              {0x92d38, 609, "/clientcakey.pem"},
                                              {0x92440, 897, "/rootca.pem"}}};
  for (const Certificate& cert : certificates)
  {
    file.Seek(cert.offset, SEEK_SET);
    std::vector<u8> pem_cert(cert.size);
    file.ReadBytes(pem_cert.data(), pem_cert.size());

    const std::string pem_file_path = nand_root + cert.filename;
    File::IOFile pem_file(pem_file_path, "wb");
    if (!pem_file.WriteBytes(pem_cert.data(), pem_cert.size()))
      PanicAlertT("Unable to write to file %s", pem_file_path.c_str());
  }
}

void NANDImporter::ExportKeys(const std::string& nand_root)
{
  const std::string file_path = nand_root + "/keys.bin";
  File::IOFile file(file_path, "wb");
  if (!file.WriteBytes(m_nand_keys.data(), NAND_KEYS_SIZE))
    PanicAlertT("Unable to write to file %s", file_path.c_str());
}
}
