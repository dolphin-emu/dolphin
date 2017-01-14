// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/IPC_HLE/ESFormats.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "Common/ChunkFile.h"
#include "Common/CommonFuncs.h"
#include "Common/CommonTypes.h"

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
  if (m_bytes.size() < 0x1E4)
  {
    // TMD is too small to contain its base fields.
    return false;
  }

  if (m_bytes.size() < 0x1E4 + GetNumContents() * 36u)
  {
    // TMD is too small to contain all its expected content entries.
    return false;
  }

  return true;
}

u64 TMDReader::GetTitleId() const
{
  return Common::swap64(m_bytes.data() + 0x18C);
}

u16 TMDReader::GetNumContents() const
{
  return Common::swap16(m_bytes.data() + 0x1DE);
}

bool TMDReader::GetContent(u16 index, Content* content) const
{
  if (index >= GetNumContents())
  {
    return false;
  }

  const u8* content_base = m_bytes.data() + 0x1E4 + index * 36;
  content->id = Common::swap32(content_base);
  content->index = Common::swap16(content_base + 4);
  content->type = Common::swap16(content_base + 6);
  content->size = Common::swap64(content_base + 8);
  std::copy(content_base + 16, content_base + 36, content->sha1.begin());

  return true;
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
