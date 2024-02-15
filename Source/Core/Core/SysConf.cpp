// Copyright 2009 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/SysConf.h"

#include <algorithm>
#include <array>
#include <cstdio>

#include "Common/CommonPaths.h"
#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/IOFile.h"
#include "Common/Logging/Log.h"
#include "Common/Swap.h"
#include "Core/IOS/FS/FileSystem.h"
#include "Core/IOS/Uids.h"

constexpr size_t SYSCONF_SIZE = 0x4000;

static size_t GetNonArrayEntrySize(SysConf::Entry::Type type)
{
  switch (type)
  {
  case SysConf::Entry::Type::Byte:
  case SysConf::Entry::Type::ByteBool:
    return 1;
  case SysConf::Entry::Type::Short:
    return 2;
  case SysConf::Entry::Type::Long:
    return 4;
  case SysConf::Entry::Type::LongLong:
    return 8;
  default:
    ASSERT(false);
    return 0;
  }
}
SysConf::SysConf(std::shared_ptr<IOS::HLE::FS::FileSystem> fs) : m_fs{std::move(fs)}
{
  Load();
}

SysConf::~SysConf()
{
  Save();
}

void SysConf::Clear()
{
  m_entries.clear();
}

void SysConf::Load()
{
  Clear();

  const auto file = m_fs->OpenFile(IOS::HLE::FS::Uid{0}, IOS::HLE::FS::Gid{0},
                                   "/shared2/sys/SYSCONF", IOS::HLE::FS::Mode::Read);
  if (!file || file->GetStatus()->size != SYSCONF_SIZE || !LoadFromFile(*file))
  {
    WARN_LOG_FMT(CORE, "No valid SYSCONF detected. Creating a new one.");
    InsertDefaultEntries();
  }
}

bool SysConf::LoadFromFile(const IOS::HLE::FS::FileHandle& file)
{
  file.Seek(4, IOS::HLE::FS::SeekMode::Set);
  u16 number_of_entries;
  file.Read(&number_of_entries, 1);
  number_of_entries = Common::swap16(number_of_entries);

  std::vector<u16> offsets(number_of_entries);
  for (u16& offset : offsets)
  {
    file.Read(&offset, 1);
    offset = Common::swap16(offset);
  }

  for (const u16 offset : offsets)
  {
    file.Seek(offset, IOS::HLE::FS::SeekMode::Set);

    // Metadata
    u8 description = 0;
    file.Read(&description, 1);
    const Entry::Type type = static_cast<Entry::Type>((description & 0xe0) >> 5);
    const u8 name_length = (description & 0x1f) + 1;
    std::string name(name_length, '\0');
    file.Read(&name[0], name.size());

    // Data
    std::vector<u8> data;
    switch (type)
    {
    case Entry::Type::BigArray:
    {
      u16 data_length = 0;
      file.Read(&data_length, 1);
      // The stored u16 is length - 1, not length.
      data.resize(Common::swap16(data_length) + 1);
      break;
    }
    case Entry::Type::SmallArray:
    {
      u8 data_length = 0;
      file.Read(&data_length, 1);
      data.resize(data_length + 1);
      break;
    }
    case Entry::Type::Byte:
    case Entry::Type::ByteBool:
    case Entry::Type::Short:
    case Entry::Type::Long:
    case Entry::Type::LongLong:
      data.resize(GetNonArrayEntrySize(type));
      break;
    default:
      ERROR_LOG_FMT(CORE, "Unknown entry type {} in SYSCONF for {} (offset {})",
                    static_cast<u8>(type), name, offset);
      return false;
    }

    file.Read(data.data(), data.size());
    AddEntry({type, name, std::move(data)});
  }
  return true;
}

template <typename T>
static void AppendToBuffer(std::vector<u8>* vector, T value)
{
  const T swapped_value = Common::FromBigEndian(value);
  vector->resize(vector->size() + sizeof(T));
  std::memcpy(&*(vector->end() - sizeof(T)), &swapped_value, sizeof(T));
}

bool SysConf::Save() const
{
  std::vector<u8> buffer;
  buffer.reserve(SYSCONF_SIZE);

  // Header
  constexpr std::array<u8, 4> version{{'S', 'C', 'v', '0'}};
  buffer.insert(buffer.end(), version.cbegin(), version.cend());
  AppendToBuffer<u16>(&buffer, static_cast<u16>(m_entries.size()));

  const size_t entries_begin_offset = buffer.size() + sizeof(u16) * (m_entries.size() + 1);
  std::vector<u8> entries;
  for (const auto& item : m_entries)
  {
    // Offset
    AppendToBuffer<u16>(&buffer, static_cast<u16>(entries_begin_offset + entries.size()));

    // Entry metadata (type and name)
    entries.insert(entries.end(),
                   (static_cast<u8>(item.type) << 5) | (static_cast<u8>(item.name.size()) - 1));
    entries.insert(entries.end(), item.name.cbegin(), item.name.cend());

    // Entry data
    switch (item.type)
    {
    case Entry::Type::BigArray:
    {
      const u16 data_size = static_cast<u16>(item.bytes.size());
      // length - 1 is stored, not length.
      AppendToBuffer<u16>(&entries, data_size - 1);
      entries.insert(entries.end(), item.bytes.cbegin(), item.bytes.cbegin() + data_size);
      break;
    }

    case Entry::Type::SmallArray:
    {
      const u8 data_size = static_cast<u8>(item.bytes.size());
      AppendToBuffer<u8>(&entries, data_size - 1);
      entries.insert(entries.end(), item.bytes.cbegin(), item.bytes.cbegin() + data_size);
      break;
    }

    default:
      entries.insert(entries.end(), item.bytes.cbegin(), item.bytes.cend());
      break;
    }
  }
  // Offset for the dummy past-the-end entry.
  AppendToBuffer<u16>(&buffer, static_cast<u16>(entries_begin_offset + entries.size()));

  // Main data.
  buffer.insert(buffer.end(), entries.cbegin(), entries.cend());

  // Make sure the buffer size is 0x4000 bytes now and write the footer.
  buffer.resize(SYSCONF_SIZE);
  constexpr std::array<u8, 4> footer = {{'S', 'C', 'e', 'd'}};
  std::copy(footer.cbegin(), footer.cend(), buffer.end() - footer.size());

  // Write the new data.
  const std::string temp_file = "/tmp/SYSCONF";
  constexpr auto rw_mode = IOS::HLE::FS::Mode::ReadWrite;
  {
    auto file = m_fs->CreateAndOpenFile(IOS::SYSMENU_UID, IOS::SYSMENU_GID, temp_file,
                                        {rw_mode, rw_mode, rw_mode});
    if (!file || !file->Write(buffer.data(), buffer.size()))
      return false;
  }
  m_fs->CreateDirectory(IOS::SYSMENU_UID, IOS::SYSMENU_GID, "/shared2/sys", 0,
                        {rw_mode, rw_mode, rw_mode});
  const auto result =
      m_fs->Rename(IOS::SYSMENU_UID, IOS::SYSMENU_GID, temp_file, "/shared2/sys/SYSCONF");
  return result == IOS::HLE::FS::ResultCode::Success;
}

SysConf::Entry::Entry(Type type_, std::string name_) : type(type_), name(std::move(name_))
{
  if (type != Type::SmallArray && type != Type::BigArray)
    bytes.resize(GetNonArrayEntrySize(type));
}

SysConf::Entry::Entry(Type type_, std::string name_, std::vector<u8> bytes_)
    : type(type_), name(std::move(name_)), bytes(std::move(bytes_))
{
}

SysConf::Entry& SysConf::AddEntry(Entry&& entry)
{
  return m_entries.emplace_back(std::move(entry));
}

SysConf::Entry* SysConf::GetEntry(std::string_view key)
{
  const auto iterator = std::find_if(m_entries.begin(), m_entries.end(),
                                     [&key](const auto& entry) { return entry.name == key; });
  return iterator != m_entries.end() ? &*iterator : nullptr;
}

const SysConf::Entry* SysConf::GetEntry(std::string_view key) const
{
  const auto iterator = std::find_if(m_entries.begin(), m_entries.end(),
                                     [&key](const auto& entry) { return entry.name == key; });
  return iterator != m_entries.end() ? &*iterator : nullptr;
}

SysConf::Entry* SysConf::GetOrAddEntry(std::string_view key, Entry::Type type)
{
  if (Entry* entry = GetEntry(key))
    return entry;

  return &AddEntry({type, std::string(key)});
}

void SysConf::RemoveEntry(std::string_view key)
{
  std::erase_if(m_entries, [&key](const auto& entry) { return entry.name == key; });
}

void SysConf::InsertDefaultEntries()
{
  AddEntry({Entry::Type::BigArray, "BT.DINF", std::vector<u8>(0x460 + 1)});
  AddEntry({Entry::Type::BigArray, "BT.CDIF", std::vector<u8>(0x204 + 1)});
  AddEntry({Entry::Type::Long, "BT.SENS", {0, 0, 0, 3}});
  AddEntry({Entry::Type::Byte, "BT.BAR", {1}});
  AddEntry({Entry::Type::Byte, "BT.SPKV", {0x58}});
  AddEntry({Entry::Type::Byte, "BT.MOT", {1}});

  std::vector<u8> console_nick = {0, 'd', 0, 'o', 0, 'l', 0, 'p', 0, 'h', 0, 'i', 0, 'n'};
  // 22 bytes: 2 bytes per character (10 characters maximum),
  // 1 for a null terminating character, 1 for the string length
  console_nick.resize(22);
  console_nick[21] = static_cast<u8>(strlen("dolphin"));
  AddEntry({Entry::Type::SmallArray, "IPL.NIK", std::move(console_nick)});

  AddEntry({Entry::Type::Byte, "IPL.LNG", {1}});
  std::vector<u8> ipl_sadr(0x1007 + 1);
  ipl_sadr[0] = 0x6c;
  AddEntry({Entry::Type::BigArray, "IPL.SADR", std::move(ipl_sadr)});

  std::vector<u8> ipl_pc(0x49 + 1);
  ipl_pc[1] = 0x04;
  ipl_pc[2] = 0x14;
  AddEntry({Entry::Type::SmallArray, "IPL.PC", std::move(ipl_pc)});

  AddEntry({Entry::Type::Long, "IPL.CB", {0x00, 0x00, 0x00, 0x00}});
  AddEntry({Entry::Type::Byte, "IPL.AR", {1}});
  AddEntry({Entry::Type::Byte, "IPL.SSV", {1}});

  AddEntry({Entry::Type::ByteBool, "IPL.CD", {0}});
  AddEntry({Entry::Type::ByteBool, "IPL.CD2", {0}});
  AddEntry({Entry::Type::ByteBool, "IPL.EULA", {1}});
  AddEntry({Entry::Type::Byte, "IPL.UPT", {2}});
  AddEntry({Entry::Type::Byte, "IPL.PGS", {0}});
  AddEntry({Entry::Type::Byte, "IPL.E60", {1}});
  AddEntry({Entry::Type::Byte, "IPL.DH", {0}});
  AddEntry({Entry::Type::Long, "IPL.INC", {0, 0, 0, 8}});
  AddEntry({Entry::Type::Long, "IPL.FRC", {0, 0, 0, 0x28}});
  AddEntry({Entry::Type::SmallArray, "IPL.IDL", {0, 1}});

  AddEntry({Entry::Type::Long, "NET.WCFG", {0, 0, 0, 1}});
  AddEntry({Entry::Type::Long, "NET.CTPC", std::vector<u8>(4)});
  AddEntry({Entry::Type::Byte, "WWW.RST", {0}});

  AddEntry({Entry::Type::ByteBool, "MPLS.MOVIE", {1}});
}
