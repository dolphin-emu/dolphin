// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DiscIO/NANDContentLoader.h"

#include <algorithm>
#include <array>
#include <cinttypes>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <functional>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "Common/Align.h"
#include "Common/CommonTypes.h"
#include "Common/Crypto/AES.h"
#include "Common/FileUtil.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Common/NandPaths.h"
#include "Common/StringUtil.h"
#include "Common/Swap.h"
// TODO: kill this dependency.
#include "Core/IOS/ES/Formats.h"

#include "DiscIO/WiiWad.h"

namespace DiscIO
{
CNANDContentData::~CNANDContentData() = default;

CNANDContentDataFile::CNANDContentDataFile(const std::string& filename) : m_filename{filename}
{
}

CNANDContentDataFile::~CNANDContentDataFile() = default;

void CNANDContentDataFile::EnsureOpen()
{
  if (!m_file)
    m_file = std::make_unique<File::IOFile>(m_filename, "rb");
  else if (!m_file->IsOpen())
    m_file->Open(m_filename, "rb");
}
void CNANDContentDataFile::Open()
{
  EnsureOpen();
}
std::vector<u8> CNANDContentDataFile::Get()
{
  EnsureOpen();

  if (!m_file->IsGood())
    return {};

  u64 size = m_file->GetSize();
  if (size == 0)
    return {};

  std::vector<u8> result(size);
  m_file->ReadBytes(result.data(), result.size());

  return result;
}

bool CNANDContentDataFile::GetRange(u32 start, u32 size, u8* buffer)
{
  EnsureOpen();
  if (!m_file->IsGood())
    return false;

  if (!m_file->Seek(start, SEEK_SET))
    return false;

  return m_file->ReadBytes(buffer, static_cast<size_t>(size));
}
void CNANDContentDataFile::Close()
{
  if (m_file && m_file->IsOpen())
    m_file->Close();
}

bool CNANDContentDataBuffer::GetRange(u32 start, u32 size, u8* buffer)
{
  if (start + size > m_buffer.size())
    return false;

  std::copy_n(&m_buffer[start], size, buffer);
  return true;
}

CNANDContentLoader::CNANDContentLoader(const std::string& content_name)
{
  m_Valid = Initialize(content_name);
}

CNANDContentLoader::~CNANDContentLoader()
{
}

bool CNANDContentLoader::IsValid() const
{
  return m_Valid;
}

const SNANDContent* CNANDContentLoader::GetContentByID(u32 id) const
{
  const auto iterator = std::find_if(m_Content.begin(), m_Content.end(), [id](const auto& content) {
    return content.m_metadata.id == id;
  });
  return iterator != m_Content.end() ? &*iterator : nullptr;
}

const SNANDContent* CNANDContentLoader::GetContentByIndex(int index) const
{
  for (auto& Content : m_Content)
  {
    if (Content.m_metadata.index == index)
    {
      return &Content;
    }
  }
  return nullptr;
}

bool CNANDContentLoader::Initialize(const std::string& name)
{
  if (name.empty())
    return false;

  m_Path = name;

  WiiWAD wad(name);
  std::vector<u8> data_app;

  if (wad.IsValid())
  {
    m_IsWAD = true;
    m_ticket = wad.GetTicket();
    m_tmd = wad.GetTMD();
    data_app = wad.GetDataApp();
  }
  else
  {
    std::string tmd_filename(m_Path);

    if (tmd_filename.back() == '/')
      tmd_filename += "title.tmd";
    else
      m_Path = tmd_filename.substr(0, tmd_filename.find("title.tmd"));

    File::IOFile tmd_file(tmd_filename, "rb");
    if (!tmd_file)
    {
      WARN_LOG(DISCIO, "CreateFromDirectory: error opening %s", tmd_filename.c_str());
      return false;
    }

    std::vector<u8> bytes(File::GetSize(tmd_filename));
    tmd_file.ReadBytes(bytes.data(), bytes.size());
    m_tmd.SetBytes(std::move(bytes));

    m_ticket = FindSignedTicket(m_tmd.GetTitleId());
  }

  InitializeContentEntries(data_app);
  return true;
}

void CNANDContentLoader::InitializeContentEntries(const std::vector<u8>& data_app)
{
  if (!m_ticket.IsValid())
  {
    ERROR_LOG(IOS_ES, "No valid ticket for title %016" PRIx64, m_tmd.GetTitleId());
    return;
  }

  const std::vector<IOS::ES::Content> contents = m_tmd.GetContents();
  m_Content.resize(contents.size());

  u32 data_app_offset = 0;
  const std::vector<u8> title_key = m_ticket.GetTitleKey();
  IOS::ES::SharedContentMap shared_content{Common::FromWhichRoot::FROM_SESSION_ROOT};

  for (size_t i = 0; i < contents.size(); ++i)
  {
    const auto& content = contents.at(i);

    if (m_IsWAD)
    {
      // The content index is used as IV (2 bytes); the remaining 14 bytes are zeroes.
      std::array<u8, 16> iv{};
      iv[0] = static_cast<u8>(content.index >> 8) & 0xFF;
      iv[1] = static_cast<u8>(content.index) & 0xFF;

      u32 rounded_size = Common::AlignUp(static_cast<u32>(content.size), 0x40);

      m_Content[i].m_Data = std::make_unique<CNANDContentDataBuffer>(Common::AES::Decrypt(
          title_key.data(), iv.data(), &data_app[data_app_offset], rounded_size));
      data_app_offset += rounded_size;
    }
    else
    {
      std::string filename;
      if (content.IsShared())
        filename = shared_content.GetFilenameFromSHA1(content.sha1);
      else
        filename = StringFromFormat("%s/%08x.app", m_Path.c_str(), content.id);

      m_Content[i].m_Data = std::make_unique<CNANDContentDataFile>(filename);
    }

    m_Content[i].m_metadata = std::move(content);
  }
}

CNANDContentManager::~CNANDContentManager()
{
}

const CNANDContentLoader& CNANDContentManager::GetNANDLoader(const std::string& content_path)
{
  auto it = m_map.find(content_path);
  if (it != m_map.end())
    return *it->second;
  return *m_map
              .emplace_hint(it, std::make_pair(content_path,
                                               std::make_unique<CNANDContentLoader>(content_path)))
              ->second;
}

const CNANDContentLoader& CNANDContentManager::GetNANDLoader(u64 title_id,
                                                             Common::FromWhichRoot from)
{
  std::string path = Common::GetTitleContentPath(title_id, from);
  return GetNANDLoader(path);
}

bool CNANDContentManager::RemoveTitle(u64 title_id, Common::FromWhichRoot from)
{
  auto& loader = GetNANDLoader(title_id, from);
  if (!loader.IsValid())
    return false;
  loader.RemoveTitle();
  return GetNANDLoader(title_id, from).IsValid();
}

void CNANDContentManager::ClearCache()
{
  m_map.clear();
}

void CNANDContentLoader::RemoveTitle() const
{
  const u64 title_id = m_tmd.GetTitleId();
  INFO_LOG(DISCIO, "RemoveTitle %08x/%08x", (u32)(title_id >> 32), (u32)title_id);
  if (IsValid())
  {
    // remove TMD?
    for (const auto& content : m_Content)
    {
      if (!content.m_metadata.IsShared())
      {
        std::string path = StringFromFormat("%s/%08x.app", m_Path.c_str(), content.m_metadata.id);
        INFO_LOG(DISCIO, "Delete %s", path.c_str());
        File::Delete(path);
      }
    }
    CNANDContentManager::Access().ClearCache();  // deletes 'this'
  }
}

u64 CNANDContentManager::Install_WiiWAD(const std::string& filename)
{
  if (filename.find(".wad") == std::string::npos)
    return 0;
  const CNANDContentLoader& content_loader = GetNANDLoader(filename);
  if (content_loader.IsValid() == false)
    return 0;

  const u64 title_id = content_loader.GetTMD().GetTitleId();

  // copy WAD's TMD header and contents to content directory

  std::string content_path(Common::GetTitleContentPath(title_id, Common::FROM_CONFIGURED_ROOT));
  std::string tmd_filename(Common::GetTMDFileName(title_id, Common::FROM_CONFIGURED_ROOT));
  File::CreateFullPath(tmd_filename);

  File::IOFile tmd_file(tmd_filename, "wb");
  if (!tmd_file)
  {
    PanicAlertT("WAD installation failed: error creating %s", tmd_filename.c_str());
    return 0;
  }

  const auto& raw_tmd = content_loader.GetTMD().GetRawTMD();
  tmd_file.WriteBytes(raw_tmd.data(), raw_tmd.size());

  IOS::ES::SharedContentMap shared_content{Common::FromWhichRoot::FROM_CONFIGURED_ROOT};
  for (const auto& content : content_loader.GetContent())
  {
    std::string app_filename;
    if (content.m_metadata.IsShared())
      app_filename = shared_content.AddSharedContent(content.m_metadata.sha1);
    else
      app_filename = StringFromFormat("%s%08x.app", content_path.c_str(), content.m_metadata.id);

    if (!File::Exists(app_filename))
    {
      File::CreateFullPath(app_filename);
      File::IOFile app_file(app_filename, "wb");
      if (!app_file)
      {
        PanicAlertT("WAD installation failed: error creating %s", app_filename.c_str());
        return 0;
      }

      app_file.WriteBytes(content.m_Data->Get().data(), content.m_metadata.size);
    }
    else
    {
      INFO_LOG(DISCIO, "Content %s already exists.", app_filename.c_str());
    }
  }

  // Extract and copy WAD's ticket to ticket directory
  if (!AddTicket(content_loader.GetTicket()))
  {
    PanicAlertT("WAD installation failed: error creating ticket");
    return 0;
  }

  IOS::ES::UIDSys uid_sys{Common::FromWhichRoot::FROM_CONFIGURED_ROOT};
  uid_sys.AddTitle(title_id);

  ClearCache();

  return title_id;
}

bool AddTicket(const IOS::ES::TicketReader& signed_ticket)
{
  if (!signed_ticket.IsValid())
  {
    return false;
  }

  u64 title_id = signed_ticket.GetTitleId();

  std::string ticket_filename = Common::GetTicketFileName(title_id, Common::FROM_CONFIGURED_ROOT);
  File::CreateFullPath(ticket_filename);

  File::IOFile ticket_file(ticket_filename, "wb");
  if (!ticket_file)
    return false;

  const std::vector<u8>& raw_ticket = signed_ticket.GetRawTicket();
  return ticket_file.WriteBytes(raw_ticket.data(), raw_ticket.size());
}

IOS::ES::TicketReader FindSignedTicket(u64 title_id)
{
  std::string ticket_filename = Common::GetTicketFileName(title_id, Common::FROM_CONFIGURED_ROOT);
  File::IOFile ticket_file(ticket_filename, "rb");
  if (!ticket_file)
  {
    return IOS::ES::TicketReader{};
  }

  std::vector<u8> signed_ticket(ticket_file.GetSize());
  if (!ticket_file.ReadBytes(signed_ticket.data(), signed_ticket.size()))
  {
    return IOS::ES::TicketReader{};
  }

  return IOS::ES::TicketReader{std::move(signed_ticket)};
}
}  // namespace end
