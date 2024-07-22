// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DiscIO/NANDImporter.h"

#include <algorithm>
#include <cstring>

#include "Common/Crypto/AES.h"
#include "Common/FileUtil.h"
#include "Common/IOFile.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Core/IOS/ES/Formats.h"

namespace DiscIO
{
constexpr size_t NAND_SIZE = 0x20000000;
constexpr size_t NAND_KEYS_SIZE = 0x400;

NANDImporter::NANDImporter() : m_nand_root(File::GetUserPath(D_WIIROOT_IDX))
{
}
NANDImporter::~NANDImporter() = default;

void NANDImporter::ImportNANDBin(const std::string& path_to_bin,
                                 std::function<void()> update_callback,
                                 std::function<std::string()> get_otp_dump_path)
{
  m_update_callback = std::move(update_callback);

  if (!ReadNANDBin(path_to_bin, get_otp_dump_path))
    return;
  if (!FindSuperblock())
    return;

  ExportKeys();
  ProcessEntry(0, "");
  ExtractCertificates();
}

bool NANDImporter::ReadNANDBin(const std::string& path_to_bin,
                               std::function<std::string()> get_otp_dump_path)
{
  constexpr size_t NAND_TOTAL_BLOCKS = 0x40000;
  constexpr size_t NAND_BLOCK_SIZE = 0x800;
  constexpr size_t NAND_ECC_BLOCK_SIZE = 0x40;
  constexpr size_t NAND_BIN_SIZE =
      (NAND_BLOCK_SIZE + NAND_ECC_BLOCK_SIZE) * NAND_TOTAL_BLOCKS;  // 0x21000000

  File::IOFile file(path_to_bin, "rb");
  const u64 image_size = file.GetSize();
  if (image_size != NAND_BIN_SIZE + NAND_KEYS_SIZE && image_size != NAND_BIN_SIZE)
  {
    PanicAlertFmtT("This file does not look like a BootMii NAND backup.");
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

    // We don't care about the ECC blocks
    file.Seek(NAND_ECC_BLOCK_SIZE, File::SeekOrigin::Current);
  }

  m_nand_keys.resize(NAND_KEYS_SIZE);

  // Read the OTP/SEEPROM dump.
  // If it is not included in the NAND image, get a path to the dump and read key data from it.
  if (image_size == NAND_BIN_SIZE)
  {
    const std::string otp_dump_path = get_otp_dump_path();
    if (otp_dump_path.empty())
      return false;
    File::IOFile keys_file{otp_dump_path, "rb"};
    return keys_file.ReadBytes(m_nand_keys.data(), NAND_KEYS_SIZE);
  }

  // Otherwise, just read the key data from the NAND image.
  return file.ReadBytes(m_nand_keys.data(), NAND_KEYS_SIZE);
}

bool NANDImporter::FindSuperblock()
{
  constexpr size_t NAND_SUPERBLOCK_START = 0x1fc00000;

  // There are 16 superblocks, choose the highest/newest version
  for (int i = 0; i < 16; i++)
  {
    auto superblock = std::make_unique<NANDSuperblock>();
    std::memcpy(superblock.get(), &m_nand[NAND_SUPERBLOCK_START + i * sizeof(NANDSuperblock)],
                sizeof(NANDSuperblock));

    if (std::memcmp(superblock->magic.data(), "SFFS", 4) != 0)
    {
      ERROR_LOG_FMT(DISCIO, "Superblock #{} does not exist", i);
      continue;
    }

    INFO_LOG_FMT(DISCIO, "Superblock #{} has version {:#x}", i, superblock->version);

    if (!m_superblock || superblock->version > m_superblock->version)
      m_superblock = std::move(superblock);
  }

  if (!m_superblock)
  {
    PanicAlertFmtT("This file does not contain a valid Wii filesystem.");
    return false;
  }

  INFO_LOG_FMT(DISCIO, "Using superblock version {:#x}", m_superblock->version);
  return true;
}

std::string NANDImporter::GetPath(const NANDFSTEntry& entry, const std::string& parent_path)
{
  std::string name(entry.name, strnlen(entry.name, sizeof(NANDFSTEntry::name)));

  if (name.front() == '/' || parent_path.back() == '/')
    return parent_path + name;

  return parent_path + '/' + name;
}

void NANDImporter::ProcessEntry(u16 entry_number, const std::string& parent_path)
{
  while (entry_number != 0xffff)
  {
    if (entry_number >= m_superblock->fst.size())
    {
      ERROR_LOG_FMT(DISCIO, "FST entry number {} out of range", entry_number);
      return;
    }

    const NANDFSTEntry entry = m_superblock->fst[entry_number];

    const std::string path = GetPath(entry, parent_path);
    INFO_LOG_FMT(DISCIO, "Entry: {} Path: {}", entry, path);
    m_update_callback();

    Type type = static_cast<Type>(entry.mode & 3);
    if (type == Type::File)
    {
      std::vector<u8> data = GetEntryData(entry);
      File::IOFile file(m_nand_root + path, "wb");
      file.WriteBytes(data.data(), data.size());
    }
    else if (type == Type::Directory)
    {
      File::CreateDir(m_nand_root + path);
      ProcessEntry(entry.sub, path);
    }
    else
    {
      ERROR_LOG_FMT(DISCIO, "Ignoring unknown entry type for {}", entry);
    }

    entry_number = entry.sib;
  }
}

std::vector<u8> NANDImporter::GetEntryData(const NANDFSTEntry& entry)
{
  constexpr size_t NAND_FAT_BLOCK_SIZE = 0x4000;

  u16 sub = entry.sub;
  size_t remaining_bytes = entry.size;
  std::vector<u8> data{};
  data.reserve(remaining_bytes);

  auto block = std::make_unique<u8[]>(NAND_FAT_BLOCK_SIZE);
  while (remaining_bytes > 0)
  {
    if (sub >= m_superblock->fat.size())
    {
      ERROR_LOG_FMT(DISCIO, "FAT block index {} out of range", sub);
      return {};
    }

    m_aes_ctx->CryptIvZero(&m_nand[NAND_FAT_BLOCK_SIZE * sub], block.get(), NAND_FAT_BLOCK_SIZE);

    size_t size = std::min(remaining_bytes, NAND_FAT_BLOCK_SIZE);
    data.insert(data.end(), block.get(), block.get() + size);
    remaining_bytes -= size;

    sub = m_superblock->fat[sub];
  }

  return data;
}

bool NANDImporter::ExtractCertificates()
{
  const std::string content_dir = m_nand_root + "/title/00000001/0000000d/content/";

  File::IOFile tmd_file(content_dir + "title.tmd", "rb");
  std::vector<u8> tmd_bytes(tmd_file.GetSize());
  if (!tmd_file.ReadBytes(tmd_bytes.data(), tmd_bytes.size()))
  {
    ERROR_LOG_FMT(DISCIO, "ExtractCertificates: Could not read IOS13 TMD");
    return false;
  }

  IOS::ES::TMDReader tmd(std::move(tmd_bytes));
  IOS::ES::Content content_metadata;
  if (!tmd.GetContent(tmd.GetBootIndex(), &content_metadata))
  {
    ERROR_LOG_FMT(DISCIO, "ExtractCertificates: Could not get content ID from TMD");
    return false;
  }

  File::IOFile content_file(content_dir + fmt::format("{:08x}.app", content_metadata.id), "rb");
  std::vector<u8> content_bytes(content_file.GetSize());
  if (!content_file.ReadBytes(content_bytes.data(), content_bytes.size()))
  {
    ERROR_LOG_FMT(DISCIO, "ExtractCertificates: Could not read IOS13 contents");
    return false;
  }

  struct PEMCertificate
  {
    std::string_view filename;
    std::array<u8, 4> search_bytes;
  };

  static constexpr std::array<PEMCertificate, 3> certificates{{
      {"/clientca.pem", {{0x30, 0x82, 0x03, 0xE9}}},
      {"/clientcakey.pem", {{0x30, 0x82, 0x02, 0x5D}}},
      {"/rootca.pem", {{0x30, 0x82, 0x03, 0x7D}}},
  }};

  for (const PEMCertificate& certificate : certificates)
  {
    const auto search_result = std::ranges::search(content_bytes, certificate.search_bytes).begin();

    if (search_result == content_bytes.end())
    {
      ERROR_LOG_FMT(DISCIO, "ExtractCertificates: Could not find offset for certficate '{}'",
                    certificate.filename);
      return false;
    }

    const std::string pem_file_path = m_nand_root + std::string(certificate.filename);
    const ptrdiff_t certificate_offset = std::distance(content_bytes.begin(), search_result);
    constexpr int min_offset = 2;
    if (certificate_offset < min_offset)
    {
      ERROR_LOG_FMT(
          DISCIO,
          "ExtractCertificates: Invalid certificate offset {:#x}, must be between {:#x} and {:#x}",
          certificate_offset, min_offset, content_bytes.size());
      return false;
    }
    const u16 certificate_size = Common::swap16(&content_bytes[certificate_offset - min_offset]);
    const size_t available_size = content_bytes.size() - static_cast<size_t>(certificate_offset);
    if (certificate_size > available_size)
    {
      ERROR_LOG_FMT(
          DISCIO,
          "ExtractCertificates: Invalid certificate size {:#x}, must be {:#x} bytes or smaller",
          certificate_size, available_size);
      return false;
    }
    INFO_LOG_FMT(DISCIO, "ExtractCertificates: '{}' offset: {:#x} size: {:#x}",
                 certificate.filename, certificate_offset, certificate_size);

    File::IOFile pem_file(pem_file_path, "wb");
    if (!pem_file.WriteBytes(&content_bytes[certificate_offset], certificate_size))
    {
      ERROR_LOG_FMT(DISCIO, "ExtractCertificates: Unable to write to file {}", pem_file_path);
      return false;
    }
  }
  return true;
}

void NANDImporter::ExportKeys()
{
  constexpr size_t NAND_AES_KEY_OFFSET = 0x158;

  m_aes_ctx = Common::AES::CreateContextDecrypt(&m_nand_keys[NAND_AES_KEY_OFFSET]);

  const std::string file_path = m_nand_root + "/keys.bin";
  File::IOFile file(file_path, "wb");
  if (!file.WriteBytes(m_nand_keys.data(), NAND_KEYS_SIZE))
    PanicAlertFmtT("Unable to write to file {0}", file_path);
}
}  // namespace DiscIO
