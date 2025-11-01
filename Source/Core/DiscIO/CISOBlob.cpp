// Copyright 2010 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DiscIO/CISOBlob.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "Common/BitUtils.h"
#include "Common/CommonTypes.h"

namespace DiscIO
{
CISOFileReader::CISOFileReader(File::DirectIOFile file) : m_file(std::move(file))
{
  m_size = m_file.GetSize();

  CISOHeader header;
  m_file.OffsetRead(0, Common::AsWritableU8Span(header));

  m_block_size = header.block_size;

  MapType count = 0;
  for (u32 idx = 0; idx < CISO_MAP_SIZE; ++idx)
    m_ciso_map[idx] = (1 == header.map[idx]) ? count++ : UNUSED_BLOCK_ID;
}

std::unique_ptr<CISOFileReader> CISOFileReader::Create(File::DirectIOFile file)
{
  CISOHeader header;
  if (file.OffsetRead(0, Common::AsWritableU8Span(header)) && header.magic == CISO_MAGIC)
  {
    return std::unique_ptr<CISOFileReader>(new CISOFileReader(std::move(file)));
  }

  return nullptr;
}

std::unique_ptr<BlobReader> CISOFileReader::CopyReader() const
{
  return Create(m_file);
}

u64 CISOFileReader::GetDataSize() const
{
  return static_cast<u64>(CISO_MAP_SIZE) * m_block_size;
}

u64 CISOFileReader::GetRawSize() const
{
  return m_size;
}

bool CISOFileReader::Read(u64 offset, u64 nbytes, u8* out_ptr)
{
  if (offset + nbytes > GetDataSize())
    return false;

  while (nbytes != 0)
  {
    u64 const block = offset / m_block_size;
    u64 const data_offset = offset % m_block_size;
    u64 const bytes_to_read = std::min(m_block_size - data_offset, nbytes);

    if (block < CISO_MAP_SIZE && UNUSED_BLOCK_ID != m_ciso_map[block])
    {
      // calculate the base address
      u64 const file_off = CISO_HEADER_SIZE + m_ciso_map[block] * (u64)m_block_size + data_offset;

      if (!m_file.OffsetRead(file_off, out_ptr, bytes_to_read))
        return false;
    }
    else
    {
      std::fill_n(out_ptr, bytes_to_read, 0);
    }

    out_ptr += bytes_to_read;
    offset += bytes_to_read;
    nbytes -= bytes_to_read;
  }

  return true;
}

}  // namespace DiscIO
