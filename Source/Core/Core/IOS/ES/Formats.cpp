// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/IOS/ES/Formats.h"

#include <algorithm>
#include <cinttypes>
#include <cstddef>
#include <cstring>
#include <locale>
#include <string>
#include <utility>
#include <vector>

#include "Common/Assert.h"
#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "Common/Crypto/AES.h"
#include "Common/FileUtil.h"
#include "Common/Logging/Log.h"
#include "Common/StringUtil.h"
#include "Common/Swap.h"
#include "Core/ec_wii.h"

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
  return IsTitleType(title_id, TitleType::Channel) ||
         IsTitleType(title_id, TitleType::SystemChannel) ||
         IsTitleType(title_id, TitleType::GameWithChannel);
}

bool Content::IsShared() const
{
  return (type & 0x8000) != 0;
}

TMDReader::TMDReader(const std::vector<u8>& bytes) : m_bytes(bytes)
{
}

TMDReader::TMDReader(std::vector<u8>&& bytes) : m_bytes(std::move(bytes))
{
}

void TMDReader::SetBytes(const std::vector<u8>& bytes)
{
  m_bytes = bytes;
}

void TMDReader::SetBytes(std::vector<u8>&& bytes)
{
  m_bytes = std::move(bytes);
}

bool TMDReader::IsValid() const
{
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

const std::vector<u8>& TMDReader::GetRawTMD() const
{
  return m_bytes;
}

std::vector<u8> TMDReader::GetRawHeader() const
{
  return std::vector<u8>(m_bytes.begin(), m_bytes.begin() + sizeof(TMDHeader));
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

DiscIO::Region TMDReader::GetRegion() const
{
  if (GetTitleId() == 0x0000000100000002)
    return DiscIO::GetSysMenuRegion(GetTitleVersion());

  return DiscIO::RegionSwitchWii(static_cast<u8>(GetTitleId() & 0xff));
}

u64 TMDReader::GetTitleId() const
{
  return Common::swap64(m_bytes.data() + offsetof(TMDHeader, title_id));
}

u16 TMDReader::GetTitleVersion() const
{
  return Common::swap16(m_bytes.data() + offsetof(TMDHeader, title_version));
}

u16 TMDReader::GetGroupId() const
{
  return Common::swap16(m_bytes.data() + offsetof(TMDHeader, group_id));
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

void TMDReader::DoState(PointerWrap& p)
{
  p.Do(m_bytes);
}

TicketReader::TicketReader(const std::vector<u8>& bytes) : m_bytes(bytes)
{
}

TicketReader::TicketReader(std::vector<u8>&& bytes) : m_bytes(std::move(bytes))
{
}

void TicketReader::SetBytes(const std::vector<u8>& bytes)
{
  m_bytes = bytes;
}

void TicketReader::SetBytes(std::vector<u8>&& bytes)
{
  m_bytes = std::move(bytes);
}

bool TicketReader::IsValid() const
{
  // Too small for the signature type.
  if (m_bytes.size() < sizeof(u32))
    return false;

  u32 ticket_offset = GetOffset();
  if (ticket_offset == 0)
    return false;

  // Too small for the ticket itself.
  if (m_bytes.size() < ticket_offset + sizeof(Ticket))
    return false;

  return true;
}

void TicketReader::DoState(PointerWrap& p)
{
  p.Do(m_bytes);
}

u32 TicketReader::GetNumberOfTickets() const
{
  return static_cast<u32>(m_bytes.size() / (GetOffset() + sizeof(Ticket)));
}

u32 TicketReader::GetOffset() const
{
  u32 signature_type = Common::swap32(m_bytes.data());
  if (signature_type == 0x10000)  // RSA4096
    return 576;
  if (signature_type == 0x10001)  // RSA2048
    return 320;
  if (signature_type == 0x10002)  // ECDSA
    return 128;

  ERROR_LOG(COMMON, "Invalid ticket signature type: %08x", signature_type);
  return 0;
}

const std::vector<u8>& TicketReader::GetRawTicket() const
{
  return m_bytes;
}

std::vector<u8> TicketReader::GetRawTicketView(u32 ticket_num) const
{
  // A ticket view is composed of a view ID + part of a ticket starting from the ticket_id field.
  const auto ticket_start = m_bytes.cbegin() + GetOffset() + sizeof(Ticket) * ticket_num;
  const auto view_start = ticket_start + offsetof(Ticket, ticket_id);

  // Copy the view ID to the buffer.
  std::vector<u8> view(sizeof(TicketView::view));
  const u32 view_id = Common::swap32(ticket_num);
  std::memcpy(view.data(), &view_id, sizeof(view_id));

  // Copy the rest of the ticket view structure from the ticket.
  view.insert(view.end(), view_start, view_start + (sizeof(TicketView) - sizeof(view_id)));
  _assert_(view.size() == sizeof(TicketView));

  return view;
}

u32 TicketReader::GetDeviceId() const
{
  return Common::swap32(m_bytes.data() + GetOffset() + offsetof(Ticket, device_id));
}

u64 TicketReader::GetTitleId() const
{
  return Common::swap64(m_bytes.data() + GetOffset() + offsetof(Ticket, title_id));
}

std::vector<u8> TicketReader::GetTitleKey() const
{
  const u8 common_key[16] = {0xeb, 0xe4, 0x2a, 0x22, 0x5e, 0x85, 0x93, 0xe4,
                             0x48, 0xd9, 0xc5, 0x45, 0x73, 0x81, 0xaa, 0xf7};
  u8 iv[16] = {};
  std::copy_n(&m_bytes[GetOffset() + offsetof(Ticket, title_id)], sizeof(Ticket::title_id), iv);
  return Common::AES::Decrypt(common_key, iv, &m_bytes[GetOffset() + offsetof(Ticket, title_key)],
                              16);
}

constexpr s32 IOSC_OK = 0;

s32 TicketReader::Unpersonalise()
{
  const auto ticket_begin = m_bytes.begin() + GetOffset();

  // IOS uses IOSC to compute an AES key from the peer public key and the device's private ECC key,
  // which is used the decrypt the title key. The IV is the ticket ID (8 bytes), zero extended.

  const auto public_key_iter = ticket_begin + offsetof(Ticket, server_public_key);
  EcWii::ECCKey public_key;
  std::copy_n(public_key_iter, sizeof(Ticket::server_public_key), public_key.begin());

  const EcWii& ec = EcWii::GetInstance();
  const std::array<u8, 16> shared_secret = ec.GetSharedSecret(public_key);

  std::array<u8, 16> iv{};
  std::copy_n(ticket_begin + offsetof(Ticket, ticket_id), sizeof(Ticket::ticket_id), iv.begin());

  const std::vector<u8> key =
      Common::AES::Decrypt(shared_secret.data(), iv.data(),
                           &*ticket_begin + offsetof(Ticket, title_key), sizeof(Ticket::title_key));

  // Finally, IOS copies the decrypted title key back to the ticket buffer.
  std::copy(key.cbegin(), key.cend(), ticket_begin + offsetof(Ticket, title_key));
  return IOSC_OK;
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

std::string SharedContentMap::GetFilenameFromSHA1(const std::array<u8, 20>& sha1) const
{
  const auto it = std::find_if(m_entries.begin(), m_entries.end(),
                               [&sha1](const auto& entry) { return entry.sha1 == sha1; });
  if (it == m_entries.end())
    return "unk";

  const std::string id_string(it->id.begin(), it->id.end());
  return Common::RootUserPath(m_root) + StringFromFormat("/shared1/%s.app", id_string.c_str());
}

std::string SharedContentMap::AddSharedContent(const std::array<u8, 20>& sha1)
{
  std::string filename = GetFilenameFromSHA1(sha1);
  if (filename != "unk")
    return filename;

  const std::string id = StringFromFormat("%08x", m_last_id);
  Entry entry;
  std::copy(id.cbegin(), id.cend(), entry.id.begin());
  entry.sha1 = sha1;
  m_entries.push_back(entry);

  File::CreateFullPath(m_file_path);

  File::IOFile file(m_file_path, "ab");
  file.WriteArray(&entry, 1);

  filename = Common::RootUserPath(m_root) + StringFromFormat("/shared1/%s.app", id.c_str());
  m_last_id++;
  return filename;
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
    AddTitle(TITLEID_SYSMENU);
  }
}

u32 UIDSys::GetUIDFromTitle(u64 title_id)
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

void UIDSys::AddTitle(u64 title_id)
{
  if (GetUIDFromTitle(title_id))
  {
    INFO_LOG(IOS_ES, "Title %016" PRIx64 " already exists in uid.sys", title_id);
    return;
  }

  u32 uid = GetNextUID();
  m_entries.insert({uid, title_id});

  // Byte swap before writing.
  title_id = Common::swap64(title_id);
  uid = Common::swap32(uid);

  File::CreateFullPath(m_file_path);
  File::IOFile file(m_file_path, "ab");

  if (!file.WriteBytes(&title_id, sizeof(title_id)) || !file.WriteBytes(&uid, sizeof(uid)))
    ERROR_LOG(IOS_ES, "Failed to write to /sys/uid.sys");
}
}  // namespace ES
}  // namespace IOS
