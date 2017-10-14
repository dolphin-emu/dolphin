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
#include "Common/File.h"
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
NANDContentData::~NANDContentData() = default;

NANDContentDataFile::NANDContentDataFile(const std::string& filename) : m_filename{filename}
{
}

NANDContentDataFile::~NANDContentDataFile() = default;

void NANDContentDataFile::EnsureOpen()
{
  if (!m_file)
    m_file = std::make_unique<File::IOFile>(m_filename, "rb");
  else if (!m_file->IsOpen())
    m_file->Open(m_filename, "rb");
}
void NANDContentDataFile::Open()
{
  EnsureOpen();
}
std::vector<u8> NANDContentDataFile::Get()
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

bool NANDContentDataFile::GetRange(u32 start, u32 size, u8* buffer)
{
  EnsureOpen();
  if (!m_file->IsGood())
    return false;

  if (!m_file->Seek(start, SEEK_SET))
    return false;

  return m_file->ReadBytes(buffer, static_cast<size_t>(size));
}
void NANDContentDataFile::Close()
{
  if (m_file && m_file->IsOpen())
    m_file->Close();
}

bool NANDContentDataBuffer::GetRange(u32 start, u32 size, u8* buffer)
{
  if (start + size > m_buffer.size())
    return false;

  std::copy_n(&m_buffer[start], size, buffer);
  return true;
}

NANDContentLoader::NANDContentLoader(const std::string& content_name, Common::FromWhichRoot from)
    : m_root(from)
{
  m_Valid = Initialize(content_name);
}

NANDContentLoader::~NANDContentLoader()
{
}

bool NANDContentLoader::IsValid() const
{
  return m_Valid;
}

const NANDContent* NANDContentLoader::GetContentByID(u32 id) const
{
  const auto iterator = std::find_if(m_Content.begin(), m_Content.end(), [id](const auto& content) {
    return content.m_metadata.id == id;
  });
  return iterator != m_Content.end() ? &*iterator : nullptr;
}

const NANDContent* NANDContentLoader::GetContentByIndex(int index) const
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

bool NANDContentLoader::Initialize(const std::string& name)
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

    std::vector<u8> bytes(tmd_file.GetSize());
    tmd_file.ReadBytes(bytes.data(), bytes.size());
    m_tmd.SetBytes(std::move(bytes));

    m_ticket = FindSignedTicket(m_tmd.GetTitleId());
  }

  InitializeContentEntries(data_app);
  return true;
}

void NANDContentLoader::InitializeContentEntries(const std::vector<u8>& data_app)
{
  if (!m_ticket.IsValid())
  {
    ERROR_LOG(IOS_ES, "No valid ticket for title %016" PRIx64, m_tmd.GetTitleId());
    return;
  }

  const std::vector<IOS::ES::Content> contents = m_tmd.GetContents();
  m_Content.resize(contents.size());

  u32 data_app_offset = 0;
  const std::array<u8, 16> title_key = m_ticket.GetTitleKey();
  IOS::ES::SharedContentMap shared_content{m_root};

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

      m_Content[i].m_Data = std::make_unique<NANDContentDataBuffer>(Common::AES::Decrypt(
          title_key.data(), iv.data(), &data_app[data_app_offset], rounded_size));
      data_app_offset += rounded_size;
    }
    else
    {
      std::string filename;
      if (content.IsShared())
        filename = *shared_content.GetFilenameFromSHA1(content.sha1);
      else
        filename = StringFromFormat("%s/%08x.app", m_Path.c_str(), content.id);

      m_Content[i].m_Data = std::make_unique<NANDContentDataFile>(filename);
    }

    m_Content[i].m_metadata = std::move(content);
  }
}

NANDContentManager::~NANDContentManager()
{
}

const NANDContentLoader& NANDContentManager::GetNANDLoader(const std::string& content_path,
                                                           Common::FromWhichRoot from)
{
  auto it = m_map.find(content_path);
  if (it != m_map.end())
    return *it->second;
  return *m_map
              .emplace_hint(it, std::make_pair(content_path, std::make_unique<NANDContentLoader>(
                                                                 content_path, from)))
              ->second;
}

const NANDContentLoader& NANDContentManager::GetNANDLoader(u64 title_id, Common::FromWhichRoot from)
{
  std::string path = Common::GetTitleContentPath(title_id, from);
  return GetNANDLoader(path, from);
}

void NANDContentManager::ClearCache()
{
  m_map.clear();
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
