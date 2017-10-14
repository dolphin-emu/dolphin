// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/IOS/ES/Formats.h"

#include <algorithm>
#include <array>
#include <cinttypes>
#include <cstddef>
#include <cstring>
#include <locale>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "Common/Assert.h"
#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "Common/File.h"
#include "Common/FileUtil.h"
#include "Common/Logging/Log.h"
#include "Common/StringUtil.h"
#include "Common/Swap.h"
#include "Core/CommonTitles.h"
#include "Core/IOS/Device.h"
#include "Core/IOS/IOS.h"
#include "Core/IOS/IOSC.h"

namespace IOS
{
namespace ES
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

SignedBlobReader::SignedBlobReader(const std::vector<u8>& bytes) : m_bytes(bytes)
{
}

SignedBlobReader::SignedBlobReader(std::vector<u8>&& bytes) : m_bytes(std::move(bytes))
{
}

const std::vector<u8>& SignedBlobReader::GetBytes() const
{
  return m_bytes;
}

void SignedBlobReader::SetBytes(const std::vector<u8>& bytes)
{
  m_bytes = bytes;
}

void SignedBlobReader::SetBytes(std::vector<u8>&& bytes)
{
  m_bytes = std::move(bytes);
}

bool SignedBlobReader::IsSignatureValid() const
{
  // Too small for the certificate type.
  if (m_bytes.size() < sizeof(Cert::type))
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

std::vector<u8> SignedBlobReader::GetSignatureData() const
{
  switch (GetSignatureType())
  {
  case SignatureType::RSA4096:
  {
    const auto signature_begin = m_bytes.begin() + offsetof(SignatureRSA4096, sig);
    return std::vector<u8>(signature_begin, signature_begin + sizeof(SignatureRSA4096::sig));
  }
  case SignatureType::RSA2048:
  {
    const auto signature_begin = m_bytes.begin() + offsetof(SignatureRSA2048, sig);
    return std::vector<u8>(signature_begin, signature_begin + sizeof(SignatureRSA2048::sig));
  }
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
  default:
    return 0;
  }
}

std::string SignedBlobReader::GetIssuer() const
{
  switch (GetSignatureType())
  {
  case SignatureType::RSA4096:
  {
    const char* issuer =
        reinterpret_cast<const char*>(m_bytes.data() + offsetof(SignatureRSA4096, issuer));
    return std::string(issuer, strnlen(issuer, sizeof(SignatureRSA4096::issuer)));
  }
  case SignatureType::RSA2048:
  {
    const char* issuer =
        reinterpret_cast<const char*>(m_bytes.data() + offsetof(SignatureRSA2048, issuer));
    return std::string(issuer, strnlen(issuer, sizeof(SignatureRSA2048::issuer)));
  }
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

TMDReader::TMDReader(const std::vector<u8>& bytes) : SignedBlobReader(bytes)
{
}

TMDReader::TMDReader(std::vector<u8>&& bytes) : SignedBlobReader(std::move(bytes))
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
  if (GetTitleId() == Titles::SYSTEM_MENU)
    return DiscIO::GetSysMenuRegion(GetTitleVersion());

  return DiscIO::RegionSwitchWii(static_cast<u8>(GetTitleId() & 0xff));
}

std::string TMDReader::GetGameID() const
{
  char game_id[6];
  std::memcpy(game_id, m_bytes.data() + offsetof(TMDHeader, title_id) + 4, 4);
  std::memcpy(game_id + 4, m_bytes.data() + offsetof(TMDHeader, group_id), 2);

  const bool all_printable = std::all_of(std::begin(game_id), std::end(game_id), [](char c) {
    return std::isprint(c, std::locale::classic());
  });

  if (all_printable)
    return std::string(game_id, sizeof(game_id));

  return StringFromFormat("%016" PRIx64, GetTitleId());
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

TicketReader::TicketReader(const std::vector<u8>& bytes) : SignedBlobReader(bytes)
{
}

TicketReader::TicketReader(std::vector<u8>&& bytes) : SignedBlobReader(std::move(bytes))
{
}

bool TicketReader::IsValid() const
{
  return IsSignatureValid() && !m_bytes.empty() && m_bytes.size() % sizeof(Ticket) == 0;
}

size_t TicketReader::GetNumberOfTickets() const
{
  return m_bytes.size() / sizeof(Ticket);
}

std::vector<u8> TicketReader::GetRawTicket(u64 ticket_id_to_find) const
{
  for (size_t i = 0; i < GetNumberOfTickets(); ++i)
  {
    const auto ticket_begin = m_bytes.begin() + sizeof(IOS::ES::Ticket) * i;
    const u64 ticket_id = Common::swap64(&*ticket_begin + offsetof(IOS::ES::Ticket, ticket_id));
    if (ticket_id == ticket_id_to_find)
      return std::vector<u8>(ticket_begin, ticket_begin + sizeof(IOS::ES::Ticket));
  }
  return {};
}

std::vector<u8> TicketReader::GetRawTicketView(u32 ticket_num) const
{
  // A ticket view is composed of a version + part of a ticket starting from the ticket_id field.
  const auto ticket_start = m_bytes.cbegin() + sizeof(Ticket) * ticket_num;
  const auto view_start = ticket_start + offsetof(Ticket, ticket_id);

  // Copy the ticket version to the buffer (a single byte extended to 4).
  std::vector<u8> view(sizeof(TicketView::version));
  const u32 version = Common::swap32(m_bytes.at(offsetof(Ticket, version)));
  std::memcpy(view.data(), &version, sizeof(version));

  // Copy the rest of the ticket view structure from the ticket.
  view.insert(view.end(), view_start, view_start + (sizeof(TicketView) - sizeof(version)));
  _assert_(view.size() == sizeof(TicketView));

  return view;
}

u32 TicketReader::GetDeviceId() const
{
  return Common::swap32(m_bytes.data() + offsetof(Ticket, device_id));
}

u64 TicketReader::GetTitleId() const
{
  return Common::swap64(m_bytes.data() + offsetof(Ticket, title_id));
}

std::array<u8, 16> TicketReader::GetTitleKey(const HLE::IOSC& iosc) const
{
  u8 iv[16] = {};
  std::copy_n(&m_bytes[offsetof(Ticket, title_id)], sizeof(Ticket::title_id), iv);

  const u8 index = m_bytes.at(offsetof(Ticket, common_key_index));
  auto common_key_handle =
      index != 1 ? HLE::IOSC::HANDLE_COMMON_KEY : HLE::IOSC::HANDLE_NEW_COMMON_KEY;
  if (index != 0 && index != 1)
  {
    WARN_LOG(IOS_ES, "Bad common key index for title %016" PRIx64 ": %u -- using common key 0",
             GetTitleId(), index);
  }

  std::array<u8, 16> key;
  iosc.Decrypt(common_key_handle, iv, &m_bytes[offsetof(Ticket, title_key)], 16, key.data(),
               HLE::PID_ES);
  return key;
}

std::array<u8, 16> TicketReader::GetTitleKey() const
{
  const bool is_rvt = (GetIssuer() == "Root-CA00000002-XS00000006");
  const HLE::IOSC::ConsoleType console_type =
      is_rvt ? HLE::IOSC::ConsoleType::RVT : HLE::IOSC::ConsoleType::Retail;
  return GetTitleKey(HLE::IOSC{console_type});
}

void TicketReader::DeleteTicket(u64 ticket_id_to_delete)
{
  std::vector<u8> new_ticket;
  const size_t num_tickets = GetNumberOfTickets();
  for (size_t i = 0; i < num_tickets; ++i)
  {
    const auto ticket_start = m_bytes.cbegin() + sizeof(Ticket) * i;
    const u64 ticket_id = Common::swap64(&*ticket_start + offsetof(Ticket, ticket_id));
    if (ticket_id != ticket_id_to_delete)
      new_ticket.insert(new_ticket.end(), ticket_start, ticket_start + sizeof(Ticket));
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
      iosc.CreateObject(&public_handle, IOSC::TYPE_PUBLIC_KEY, IOSC::SUBTYPE_ECC233, PID_ES);
  if (ret != IPC_SUCCESS)
    return ret;

  const auto public_key_iter = ticket_begin + offsetof(Ticket, server_public_key);
  ret = iosc.ImportPublicKey(public_handle, &*public_key_iter, nullptr, PID_ES);
  if (ret != IPC_SUCCESS)
    return ret;

  IOSC::Handle key_handle;
  ret = iosc.CreateObject(&key_handle, IOSC::TYPE_SECRET_KEY, IOSC::SUBTYPE_AES128, PID_ES);
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

void TicketReader::FixCommonKeyIndex()
{
  u8& index = m_bytes[offsetof(Ticket, common_key_index)];
  // Assume the ticket is using the normal common key if it's an invalid value.
  index = index <= 1 ? index : 0;
}

struct SharedContentMap::Entry
{
  // ID string
  std::array<u8, 8> id;
  // Binary SHA1 hash
  std::array<u8, 20> sha1;
};

SharedContentMap::SharedContentMap(Common::FromWhichRoot root) : m_root(root)
{
  static_assert(sizeof(Entry) == 28, "SharedContentMap::Entry has the wrong size");

  m_file_path = Common::RootUserPath(root) + "/shared1/content.map";

  File::IOFile file(m_file_path, "rb");
  Entry entry;
  while (file.ReadArray(&entry, 1))
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
  return Common::RootUserPath(m_root) + StringFromFormat("/shared1/%s.app", id_string.c_str());
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

  const std::string id = StringFromFormat("%08x", m_last_id);
  Entry entry;
  std::copy(id.cbegin(), id.cend(), entry.id.begin());
  entry.sha1 = sha1;
  m_entries.push_back(entry);

  WriteEntries();
  filename = Common::RootUserPath(m_root) + StringFromFormat("/shared1/%s.app", id.c_str());
  m_last_id++;
  return *filename;
}

bool SharedContentMap::DeleteSharedContent(const std::array<u8, 20>& sha1)
{
  m_entries.erase(std::remove_if(m_entries.begin(), m_entries.end(),
                                 [&sha1](const auto& entry) { return entry.sha1 == sha1; }),
                  m_entries.end());
  return WriteEntries();
}

bool SharedContentMap::WriteEntries() const
{
  // Temporary files in ES are only 12 characters long (excluding /tmp/).
  const std::string temp_path = Common::RootUserPath(m_root) + "/tmp/shared1/cont";
  File::CreateFullPath(temp_path);

  // Atomically write the new content map.
  {
    File::IOFile file(temp_path, "w+b");
    if (!file.WriteArray(m_entries.data(), m_entries.size()))
      return false;
    File::CreateFullPath(m_file_path);
  }
  return File::RenameSync(temp_path, m_file_path);
}

static std::pair<u32, u64> ReadUidSysEntry(File::IOFile& file)
{
  u64 title_id = 0;
  if (!file.ReadBytes(&title_id, sizeof(title_id)))
    return {};

  u32 uid = 0;
  if (!file.ReadBytes(&uid, sizeof(uid)))
    return {};

  return {Common::swap32(uid), Common::swap64(title_id)};
}

UIDSys::UIDSys(Common::FromWhichRoot root)
{
  m_file_path = Common::RootUserPath(root) + "/sys/uid.sys";

  File::IOFile file(m_file_path, "rb");
  while (true)
  {
    const std::pair<u32, u64> entry = ReadUidSysEntry(file);
    if (!entry.first && !entry.second)
      break;

    m_entries.insert(std::move(entry));
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
    return 0x00001000;
  return m_entries.rbegin()->first + 1;
}

u32 UIDSys::GetOrInsertUIDForTitle(const u64 title_id)
{
  const u32 current_uid = GetUIDFromTitle(title_id);
  if (current_uid)
  {
    INFO_LOG(IOS_ES, "Title %016" PRIx64 " already exists in uid.sys", title_id);
    return current_uid;
  }

  const u32 uid = GetNextUID();
  m_entries.insert({uid, title_id});

  // Byte swap before writing.
  const u64 swapped_title_id = Common::swap64(title_id);
  const u32 swapped_uid = Common::swap32(uid);

  File::CreateFullPath(m_file_path);
  File::IOFile file(m_file_path, "ab");

  if (!file.WriteBytes(&swapped_title_id, sizeof(title_id)) ||
      !file.WriteBytes(&swapped_uid, sizeof(uid)))
  {
    ERROR_LOG(IOS_ES, "Failed to write to /sys/uid.sys");
    return 0;
  }

  return uid;
}

CertReader::CertReader(std::vector<u8>&& bytes) : SignedBlobReader(std::move(bytes))
{
  if (!IsSignatureValid())
    return;

  switch (GetSignatureType())
  {
  case SignatureType::RSA4096:
    if (m_bytes.size() < sizeof(CertRSA4096))
      return;
    m_bytes.resize(sizeof(CertRSA4096));
    break;

  case SignatureType::RSA2048:
    if (m_bytes.size() < sizeof(CertRSA2048))
      return;
    m_bytes.resize(sizeof(CertRSA2048));
    break;

  default:
    return;
  }

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

std::vector<u8> CertReader::GetPublicKey() const
{
  static const std::map<SignatureType, std::pair<size_t, size_t>> type_to_key_info = {{
      {SignatureType::RSA4096,
       {offsetof(CertRSA4096, public_key),
        sizeof(CertRSA4096::public_key) + sizeof(CertRSA4096::exponent)}},
      {SignatureType::RSA2048,
       {offsetof(CertRSA2048, public_key),
        sizeof(CertRSA2048::public_key) + sizeof(CertRSA2048::exponent)}},
  }};

  const auto info = type_to_key_info.at(GetSignatureType());
  const auto key_begin = m_bytes.begin() + info.first;
  return std::vector<u8>(key_begin, key_begin + info.second);
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
    const std::string name = cert_reader.GetName();
    certs.emplace(std::move(name), std::move(cert_reader));
  }
  return certs;
}
}  // namespace ES
}  // namespace IOS
