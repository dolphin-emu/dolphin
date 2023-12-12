// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/IOS/ES/Formats.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstring>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <fmt/format.h>

#include "Common/Assert.h"
#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "Common/Crypto/SHA1.h"
#include "Common/Logging/Log.h"
#include "Common/NandPaths.h"
#include "Common/StringUtil.h"
#include "Common/Swap.h"
#include "Core/CommonTitles.h"
#include "Core/IOS/Device.h"
#include "Core/IOS/FS/FileSystem.h"
#include "Core/IOS/FS/FileSystemProxy.h"
#include "Core/IOS/IOS.h"
#include "Core/IOS/IOSC.h"
#include "Core/IOS/Uids.h"

namespace IOS::ES
{
constexpr size_t CONTENT_VIEW_SIZE = 0x10;

bool IsTitleType(u64 title_id, TitleType title_type)
{
  return static_cast<u32>(title_id >> 32) == static_cast<u32>(title_type);
}

bool IsDiscTitle(u64 title_id)
{
  return IsTitleType(title_id, TitleType::Game) ||
         IsTitleType(title_id, TitleType::GameWithChannel);
}

bool IsChannel(u64 title_id)
{
  if (title_id == Titles::SYSTEM_MENU)
    return true;

  return IsTitleType(title_id, TitleType::Channel) ||
         IsTitleType(title_id, TitleType::SystemChannel) ||
         IsTitleType(title_id, TitleType::GameWithChannel) ||
         IsTitleType(title_id, TitleType::HiddenChannel);
}

bool Content::IsShared() const
{
  return (type & 0x8000) != 0;
}

bool Content::IsOptional() const
{
  return (type & 0x4000) != 0;
}

bool operator==(const Content& lhs, const Content& rhs)
{
  auto fields = [](const Content& c) { return std::tie(c.id, c.index, c.type, c.size, c.sha1); };
  return fields(lhs) == fields(rhs);
}

bool operator!=(const Content& lhs, const Content& rhs)
{
  return !operator==(lhs, rhs);
}

SignedBlobReader::SignedBlobReader(std::vector<u8> bytes) : m_bytes(std::move(bytes))
{
}

const std::vector<u8>& SignedBlobReader::GetBytes() const
{
  return m_bytes;
}

void SignedBlobReader::SetBytes(std::vector<u8> bytes)
{
  m_bytes = std::move(bytes);
}

static size_t GetIssuerOffset(SignatureType signature_type)
{
  switch (signature_type)
  {
  case SignatureType::RSA2048:
    return offsetof(SignatureRSA2048, issuer);
  case SignatureType::RSA4096:
    return offsetof(SignatureRSA4096, issuer);
  case SignatureType::ECC:
    return offsetof(SignatureECC, issuer);
  default:
    return 0;
  }
}

std::array<u8, 20> SignedBlobReader::GetSha1() const
{
  const size_t skip = GetIssuerOffset(GetSignatureType());
  return Common::SHA1::CalculateDigest(m_bytes.data() + skip, m_bytes.size() - skip);
}

bool SignedBlobReader::IsSignatureValid() const
{
  // Too small for the certificate type.
  if (m_bytes.size() < sizeof(SignatureType))
    return false;

  // Too small to contain the whole signature data.
  const size_t signature_size = GetSignatureSize();
  if (signature_size == 0 || m_bytes.size() < signature_size)
    return false;

  return true;
}

SignatureType SignedBlobReader::GetSignatureType() const
{
  return static_cast<SignatureType>(Common::swap32(m_bytes.data()));
}

template <typename T, typename It>
static std::vector<u8> DetailGetSignatureData(It begin)
{
  const auto signature_begin = begin + offsetof(T, sig);
  return std::vector<u8>(signature_begin, signature_begin + sizeof(T::sig));
}

std::vector<u8> SignedBlobReader::GetSignatureData() const
{
  switch (GetSignatureType())
  {
  case SignatureType::RSA4096:
    return DetailGetSignatureData<SignatureRSA4096>(m_bytes.cbegin());
  case SignatureType::RSA2048:
    return DetailGetSignatureData<SignatureRSA2048>(m_bytes.cbegin());
  case SignatureType::ECC:
    return DetailGetSignatureData<SignatureECC>(m_bytes.cbegin());
  default:
    return {};
  }
}

size_t SignedBlobReader::GetSignatureSize() const
{
  switch (GetSignatureType())
  {
  case SignatureType::RSA4096:
    return sizeof(SignatureRSA4096);
  case SignatureType::RSA2048:
    return sizeof(SignatureRSA2048);
  case SignatureType::ECC:
    return sizeof(SignatureECC);
  default:
    return 0;
  }
}

template <typename T>
static std::string DetailGetIssuer(const u8* bytes)
{
  const char* issuer = reinterpret_cast<const char*>(bytes + offsetof(T, issuer));
  return {issuer, strnlen(issuer, sizeof(T::issuer))};
}

std::string SignedBlobReader::GetIssuer() const
{
  switch (GetSignatureType())
  {
  case SignatureType::RSA4096:
    return DetailGetIssuer<SignatureRSA4096>(m_bytes.data());
  case SignatureType::RSA2048:
    return DetailGetIssuer<SignatureRSA2048>(m_bytes.data());
  case SignatureType::ECC:
    return DetailGetIssuer<SignatureECC>(m_bytes.data());
  default:
    return "";
  }
}

void SignedBlobReader::DoState(PointerWrap& p)
{
  p.Do(m_bytes);
}

bool IsValidTMDSize(size_t size)
{
  return size <= 0x49e4;
}

TMDReader::TMDReader(std::vector<u8> bytes) : SignedBlobReader(std::move(bytes))
{
}

bool TMDReader::IsValid() const
{
  if (!IsSignatureValid())
    return false;

  if (m_bytes.size() < sizeof(TMDHeader))
  {
    // TMD is too small to contain its base fields.
    return false;
  }

  if (m_bytes.size() < sizeof(TMDHeader) + GetNumContents() * sizeof(Content))
  {
    // TMD is too small to contain all its expected content entries.
    return false;
  }

  return true;
}

std::vector<u8> TMDReader::GetRawView() const
{
  // Base fields
  std::vector<u8> view(m_bytes.cbegin() + offsetof(TMDHeader, tmd_version),
                       m_bytes.cbegin() + offsetof(TMDHeader, access_rights));

  const auto version = m_bytes.cbegin() + offsetof(TMDHeader, title_version);
  view.insert(view.end(), version, version + sizeof(TMDHeader::title_version));

  const auto num_contents = m_bytes.cbegin() + offsetof(TMDHeader, num_contents);
  view.insert(view.end(), num_contents, num_contents + sizeof(TMDHeader::num_contents));

  // Content views (same as Content, but without the hash)
  for (size_t i = 0; i < GetNumContents(); ++i)
  {
    const auto content_iterator = m_bytes.cbegin() + sizeof(TMDHeader) + i * sizeof(Content);
    view.insert(view.end(), content_iterator, content_iterator + CONTENT_VIEW_SIZE);
  }

  return view;
}

u16 TMDReader::GetBootIndex() const
{
  return Common::swap16(m_bytes.data() + offsetof(TMDHeader, boot_index));
}

u64 TMDReader::GetIOSId() const
{
  return Common::swap64(m_bytes.data() + offsetof(TMDHeader, ios_id));
}

u64 TMDReader::GetTitleId() const
{
  return Common::swap64(m_bytes.data() + offsetof(TMDHeader, title_id));
}

u32 TMDReader::GetTitleFlags() const
{
  return Common::swap32(m_bytes.data() + offsetof(TMDHeader, title_flags));
}

u16 TMDReader::GetTitleVersion() const
{
  return Common::swap16(m_bytes.data() + offsetof(TMDHeader, title_version));
}

u16 TMDReader::GetGroupId() const
{
  return Common::swap16(m_bytes.data() + offsetof(TMDHeader, group_id));
}

DiscIO::Region TMDReader::GetRegion() const
{
  if (!IsChannel(GetTitleId()))
    return DiscIO::Region::Unknown;

  if (GetTitleId() == Titles::SYSTEM_MENU)
    return DiscIO::GetSysMenuRegion(GetTitleVersion());

  const DiscIO::Region region =
      static_cast<DiscIO::Region>(Common::swap16(m_bytes.data() + offsetof(TMDHeader, region)));

  return region <= DiscIO::Region::NTSC_K ? region : DiscIO::Region::Unknown;
}

bool TMDReader::IsvWii() const
{
  return *(m_bytes.data() + offsetof(TMDHeader, is_vwii));
}

std::string TMDReader::GetGameID() const
{
  char game_id[6];
  std::memcpy(game_id, m_bytes.data() + offsetof(TMDHeader, title_id) + 4, 4);
  std::memcpy(game_id + 4, m_bytes.data() + offsetof(TMDHeader, group_id), 2);

  if (std::all_of(std::begin(game_id), std::end(game_id), Common::IsPrintableCharacter))
    return std::string(game_id, sizeof(game_id));

  return fmt::format("{:016x}", GetTitleId());
}

std::string TMDReader::GetGameTDBID() const
{
  const u8* begin = m_bytes.data() + offsetof(TMDHeader, title_id) + 4;
  const u8* end = begin + 4;

  if (std::all_of(begin, end, Common::IsPrintableCharacter))
    return std::string(begin, end);

  return fmt::format("{:016x}", GetTitleId());
}

u16 TMDReader::GetNumContents() const
{
  return Common::swap16(m_bytes.data() + offsetof(TMDHeader, num_contents));
}

bool TMDReader::GetContent(u16 index, Content* content) const
{
  if (index >= GetNumContents())
  {
    return false;
  }

  const u8* content_base = m_bytes.data() + sizeof(TMDHeader) + index * sizeof(Content);
  content->id = Common::swap32(content_base + offsetof(Content, id));
  content->index = Common::swap16(content_base + offsetof(Content, index));
  content->type = Common::swap16(content_base + offsetof(Content, type));
  content->size = Common::swap64(content_base + offsetof(Content, size));
  std::copy_n(content_base + offsetof(Content, sha1), content->sha1.size(), content->sha1.begin());

  return true;
}

std::vector<Content> TMDReader::GetContents() const
{
  std::vector<Content> contents(GetNumContents());
  for (size_t i = 0; i < contents.size(); ++i)
    GetContent(static_cast<u16>(i), &contents[i]);
  return contents;
}

bool TMDReader::FindContentById(u32 id, Content* content) const
{
  for (u16 index = 0; index < GetNumContents(); ++index)
  {
    if (!GetContent(index, content))
    {
      return false;
    }
    if (content->id == id)
    {
      return true;
    }
  }
  return false;
}

TicketReader::TicketReader(std::vector<u8> bytes) : SignedBlobReader(std::move(bytes))
{
}

bool TicketReader::IsValid() const
{
  if (!IsSignatureValid() || m_bytes.empty())
    return false;

  if (IsV1Ticket())
    return m_bytes.size() == GetTicketSize();

  return m_bytes.size() % sizeof(Ticket) == 0;
}

bool TicketReader::IsV1Ticket() const
{
  // Version can only be 0 or 1.
  return GetVersion() == 1;
}

size_t TicketReader::GetNumberOfTickets() const
{
  if (IsV1Ticket())
    return 1;

  return m_bytes.size() / sizeof(Ticket);
}

u32 TicketReader::GetTicketSize() const
{
  if (IsV1Ticket())
  {
    return Common::swap32(m_bytes.data() + sizeof(Ticket) +
                          offsetof(V1TicketHeader, v1_ticket_size)) +
           sizeof(Ticket);
  }

  return sizeof(Ticket);
}

std::vector<u8> TicketReader::GetRawTicket(u64 ticket_id_to_find) const
{
  for (size_t i = 0; i < GetNumberOfTickets(); ++i)
  {
    const auto ticket_begin = m_bytes.begin() + GetTicketSize() * i;
    const u64 ticket_id = Common::swap64(&*ticket_begin + offsetof(ES::Ticket, ticket_id));
    if (ticket_id == ticket_id_to_find)
      return {ticket_begin, ticket_begin + GetTicketSize()};
  }
  return {};
}

std::vector<u8> TicketReader::GetRawTicketView(u32 ticket_num) const
{
  // A ticket view is composed of a version + part of a ticket starting from the ticket_id field.
  const auto ticket_start = m_bytes.cbegin() + sizeof(Ticket) * ticket_num;
  const auto view_start = ticket_start + offsetof(Ticket, ticket_id);

  std::vector<u8> view(sizeof(u32));
  view[0] = GetVersion();

  // Copy the rest of the ticket view structure from the ticket.
  view.insert(view.end(), view_start, view_start + (sizeof(TicketView) - sizeof(u32)));
  ASSERT(view.size() == sizeof(TicketView));

  return view;
}

u8 TicketReader::GetVersion() const
{
  return m_bytes[offsetof(Ticket, version)];
}

u32 TicketReader::GetDeviceId() const
{
  return Common::swap32(m_bytes.data() + offsetof(Ticket, device_id));
}

u64 TicketReader::GetTitleId() const
{
  return Common::swap64(m_bytes.data() + offsetof(Ticket, title_id));
}

u8 TicketReader::GetCommonKeyIndex() const
{
  return m_bytes[offsetof(Ticket, common_key_index)];
}

std::array<u8, 16> TicketReader::GetTitleKey(const HLE::IOSC& iosc) const
{
  u8 iv[16] = {};
  std::copy_n(&m_bytes[offsetof(Ticket, title_id)], sizeof(Ticket::title_id), iv);

  u8 index = m_bytes.at(offsetof(Ticket, common_key_index));
  if (index >= HLE::IOSC::COMMON_KEY_HANDLES.size())
  {
    PanicAlertFmt("Bad common key index for title {:016x}: {} -- using common key 0", GetTitleId(),
                  index);
    index = 0;
  }
  auto common_key_handle = HLE::IOSC::COMMON_KEY_HANDLES[index];

  std::array<u8, 16> key;
  iosc.Decrypt(common_key_handle, iv, &m_bytes[offsetof(Ticket, title_key)], 16, key.data(),
               PID_ES);
  return key;
}

std::array<u8, 16> TicketReader::GetTitleKey() const
{
  return GetTitleKey(HLE::IOSC{GetConsoleType()});
}

HLE::IOSC::ConsoleType TicketReader::GetConsoleType() const
{
  const bool is_rvt = GetIssuer() == "Root-CA00000002-XS00000006";
  return is_rvt ? HLE::IOSC::ConsoleType::RVT : HLE::IOSC::ConsoleType::Retail;
}

void TicketReader::DeleteTicket(u64 ticket_id_to_delete)
{
  std::vector<u8> new_ticket;
  const size_t num_tickets = GetNumberOfTickets();
  for (size_t i = 0; i < num_tickets; ++i)
  {
    const auto ticket_start = m_bytes.cbegin() + GetTicketSize() * i;
    const u64 ticket_id = Common::swap64(&*ticket_start + offsetof(Ticket, ticket_id));
    if (ticket_id != ticket_id_to_delete)
      new_ticket.insert(new_ticket.end(), ticket_start, ticket_start + GetTicketSize());
  }

  m_bytes = std::move(new_ticket);
}

HLE::ReturnCode TicketReader::Unpersonalise(HLE::IOSC& iosc)
{
  const auto ticket_begin = m_bytes.begin();

  // IOS uses IOSC to compute an AES key from the peer public key and the device's private ECC key,
  // which is used the decrypt the title key. The IV is the ticket ID (8 bytes), zero extended.
  using namespace HLE;
  IOSC::Handle public_handle;
  ReturnCode ret =
      iosc.CreateObject(&public_handle, IOSC::TYPE_PUBLIC_KEY, IOSC::ObjectSubType::ECC233, PID_ES);
  if (ret != IPC_SUCCESS)
    return ret;

  const auto public_key_iter = ticket_begin + offsetof(Ticket, server_public_key);
  ret = iosc.ImportPublicKey(public_handle, &*public_key_iter, nullptr, PID_ES);
  if (ret != IPC_SUCCESS)
    return ret;

  IOSC::Handle key_handle;
  ret = iosc.CreateObject(&key_handle, IOSC::TYPE_SECRET_KEY, IOSC::ObjectSubType::AES128, PID_ES);
  if (ret != IPC_SUCCESS)
    return ret;

  ret = iosc.ComputeSharedKey(key_handle, IOSC::HANDLE_CONSOLE_KEY, public_handle, PID_ES);
  if (ret != IPC_SUCCESS)
    return ret;

  std::array<u8, 16> iv{};
  std::copy_n(ticket_begin + offsetof(Ticket, ticket_id), sizeof(Ticket::ticket_id), iv.begin());

  std::array<u8, 16> key{};
  ret = iosc.Decrypt(key_handle, iv.data(), &*ticket_begin + offsetof(Ticket, title_key),
                     sizeof(Ticket::title_key), key.data(), PID_ES);
  // Finally, IOS copies the decrypted title key back to the ticket buffer.
  if (ret == IPC_SUCCESS)
    std::copy(key.cbegin(), key.cend(), ticket_begin + offsetof(Ticket, title_key));

  return ret;
}

void TicketReader::OverwriteCommonKeyIndex(u8 index)
{
  m_bytes[offsetof(Ticket, common_key_index)] = index;
}

struct SharedContentMap::Entry
{
  // ID string
  std::array<u8, 8> id;
  // Binary SHA1 hash
  std::array<u8, 20> sha1;
};

constexpr char CONTENT_MAP_PATH[] = "/shared1/content.map";
SharedContentMap::SharedContentMap(HLE::FSCore& fs_core) : m_fs{fs_core.GetFS()}
{
  static_assert(sizeof(Entry) == 28, "SharedContentMap::Entry has the wrong size");

  Entry entry;
  const auto fd =
      fs_core.Open(PID_KERNEL, PID_KERNEL, CONTENT_MAP_PATH, HLE::FS::Mode::Read, {}, &m_ticks);
  if (fd.Get() < 0)
    return;
  while (fs_core.Read(fd.Get(), &entry, 1, &m_ticks) == sizeof(entry))
  {
    m_entries.push_back(entry);
    m_last_id++;
  }
}

SharedContentMap::~SharedContentMap() = default;

std::optional<std::string>
SharedContentMap::GetFilenameFromSHA1(const std::array<u8, 20>& sha1) const
{
  const auto it = std::find_if(m_entries.begin(), m_entries.end(),
                               [&sha1](const auto& entry) { return entry.sha1 == sha1; });
  if (it == m_entries.end())
    return {};

  const std::string id_string(it->id.begin(), it->id.end());
  return fmt::format("/shared1/{}.app", id_string);
}

std::vector<std::array<u8, 20>> SharedContentMap::GetHashes() const
{
  std::vector<std::array<u8, 20>> hashes;
  hashes.reserve(m_entries.size());
  for (const auto& content_entry : m_entries)
    hashes.emplace_back(content_entry.sha1);

  return hashes;
}

std::string SharedContentMap::AddSharedContent(const std::array<u8, 20>& sha1)
{
  auto filename = GetFilenameFromSHA1(sha1);
  if (filename)
    return *filename;

  const std::string id = fmt::format("{:08x}", m_last_id);
  Entry entry;
  std::copy(id.cbegin(), id.cend(), entry.id.begin());
  entry.sha1 = sha1;
  m_entries.push_back(entry);

  WriteEntries();
  filename = fmt::format("/shared1/{}.app", id);
  m_last_id++;
  return *filename;
}

bool SharedContentMap::DeleteSharedContent(const std::array<u8, 20>& sha1)
{
  std::erase_if(m_entries, [&sha1](const auto& entry) { return entry.sha1 == sha1; });
  return WriteEntries();
}

bool SharedContentMap::WriteEntries() const
{
  // Temporary files are only 12 characters long and must match the final file name
  const std::string temp_path = "/tmp/content.map";
  // Atomically write the new content map.
  {
    constexpr HLE::FS::Modes modes{HLE::FS::Mode::ReadWrite, HLE::FS::Mode::ReadWrite,
                                   HLE::FS::Mode::None};
    const auto file = m_fs->CreateAndOpenFile(PID_KERNEL, PID_KERNEL, temp_path, modes);
    if (!file || !file->Write(m_entries.data(), m_entries.size()))
      return false;
  }
  return m_fs->Rename(PID_KERNEL, PID_KERNEL, temp_path, CONTENT_MAP_PATH) ==
         HLE::FS::ResultCode::Success;
}

static std::pair<u32, u64> ReadUidSysEntry(HLE::FSCore& fs, u64 fd, u64* ticks)
{
  u64 title_id = 0;
  if (fs.Read(fd, &title_id, 1, ticks) != sizeof(title_id))
    return {};

  u32 uid = 0;
  if (fs.Read(fd, &uid, 1, ticks) != sizeof(uid))
    return {};

  return {Common::swap32(uid), Common::swap64(title_id)};
}

constexpr char UID_MAP_PATH[] = "/sys/uid.sys";
UIDSys::UIDSys(HLE::FSCore& fs_core) : m_fs{fs_core.GetFS()}
{
  if (const auto fd =
          fs_core.Open(PID_KERNEL, PID_KERNEL, UID_MAP_PATH, HLE::FS::Mode::Read, {}, &m_ticks);
      fd.Get() >= 0)
  {
    while (true)
    {
      std::pair<u32, u64> entry = ReadUidSysEntry(fs_core, fd.Get(), &m_ticks);
      if (!entry.first && !entry.second)
        break;

      m_entries.insert(std::move(entry));
    }
  }

  if (m_entries.empty())
  {
    GetOrInsertUIDForTitle(Titles::SYSTEM_MENU);
  }
}

u32 UIDSys::GetUIDFromTitle(u64 title_id) const
{
  const auto it = std::find_if(m_entries.begin(), m_entries.end(),
                               [title_id](const auto& entry) { return entry.second == title_id; });
  return (it == m_entries.end()) ? 0 : it->first;
}

u32 UIDSys::GetNextUID() const
{
  if (m_entries.empty())
    return FIRST_PPC_UID;
  return m_entries.rbegin()->first + 1;
}

u32 UIDSys::GetOrInsertUIDForTitle(const u64 title_id)
{
  const u32 current_uid = GetUIDFromTitle(title_id);
  if (current_uid)
  {
    INFO_LOG_FMT(IOS_ES, "Title {:016x} already exists in uid.sys", title_id);
    return current_uid;
  }

  const u32 uid = GetNextUID();
  m_entries.insert({uid, title_id});

  // Byte swap before writing.
  const u64 swapped_title_id = Common::swap64(title_id);
  const u32 swapped_uid = Common::swap32(uid);

  constexpr HLE::FS::Modes modes{HLE::FS::Mode::ReadWrite, HLE::FS::Mode::ReadWrite,
                                 HLE::FS::Mode::None};
  const auto file = m_fs->CreateAndOpenFile(PID_KERNEL, PID_KERNEL, UID_MAP_PATH, modes);
  if (!file || !file->Seek(0, HLE::FS::SeekMode::End) || !file->Write(&swapped_title_id, 1) ||
      !file->Write(&swapped_uid, 1))
  {
    ERROR_LOG_FMT(IOS_ES, "Failed to write to /sys/uid.sys");
    return 0;
  }

  return uid;
}

CertReader::CertReader(std::vector<u8>&& bytes) : SignedBlobReader(std::move(bytes))
{
  if (!IsSignatureValid())
    return;

  using CertStructInfo = std::tuple<SignatureType, PublicKeyType, size_t>;
  static constexpr std::array<CertStructInfo, 4> types{{
      {SignatureType::RSA4096, PublicKeyType::RSA2048, sizeof(CertRSA4096RSA2048)},
      {SignatureType::RSA2048, PublicKeyType::RSA2048, sizeof(CertRSA2048RSA2048)},
      {SignatureType::RSA2048, PublicKeyType::ECC, sizeof(CertRSA2048ECC)},
      {SignatureType::ECC, PublicKeyType::ECC, sizeof(CertECC)},
  }};

  const auto info = std::find_if(types.cbegin(), types.cend(), [this](const CertStructInfo& entry) {
    return m_bytes.size() >= std::get<2>(entry) && std::get<0>(entry) == GetSignatureType() &&
           std::get<1>(entry) == GetPublicKeyType();
  });

  if (info == types.cend())
    return;

  m_bytes.resize(std::get<2>(*info));
  m_is_valid = true;
}

bool CertReader::IsValid() const
{
  return m_is_valid;
}

u32 CertReader::GetId() const
{
  const size_t offset = GetSignatureSize() + offsetof(CertHeader, id);
  return Common::swap32(m_bytes.data() + offset);
}

std::string CertReader::GetName() const
{
  const char* name = reinterpret_cast<const char*>(m_bytes.data() + GetSignatureSize() +
                                                   offsetof(CertHeader, name));
  return std::string(name, strnlen(name, sizeof(CertHeader::name)));
}

PublicKeyType CertReader::GetPublicKeyType() const
{
  const size_t offset = GetSignatureSize() + offsetof(CertHeader, public_key_type);
  return static_cast<PublicKeyType>(Common::swap32(m_bytes.data() + offset));
}

template <typename T, typename It>
std::vector<u8> DetailGetPublicKey(It begin, size_t extra_data = 0)
{
  const auto key_begin = begin + offsetof(T, public_key);
  return {key_begin, key_begin + sizeof(T::public_key) + extra_data};
}

std::vector<u8> CertReader::GetPublicKey() const
{
  switch (GetSignatureType())
  {
  case SignatureType::RSA4096:
    return DetailGetPublicKey<CertRSA4096RSA2048>(m_bytes.begin(), 4);
  case SignatureType::RSA2048:
    if (GetPublicKeyType() == PublicKeyType::RSA2048)
      return DetailGetPublicKey<CertRSA2048RSA2048>(m_bytes.begin(), 4);
    return DetailGetPublicKey<CertRSA2048ECC>(m_bytes.begin());
  case SignatureType::ECC:
    return DetailGetPublicKey<CertECC>(m_bytes.begin());
  default:
    return {};
  }
}

std::map<std::string, CertReader> ParseCertChain(const std::vector<u8>& chain)
{
  std::map<std::string, CertReader> certs;

  size_t processed = 0;
  while (processed != chain.size())
  {
    CertReader cert_reader{std::vector<u8>(chain.begin() + processed, chain.end())};
    if (!cert_reader.IsValid())
      return certs;

    processed += cert_reader.GetBytes().size();
    std::string name = cert_reader.GetName();
    certs.emplace(std::move(name), std::move(cert_reader));
  }
  return certs;
}
}  // namespace IOS::ES
