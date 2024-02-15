// Copyright 2016 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DiscIO/TGCBlob.h"

#include <algorithm>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "Common/IOFile.h"
#include "Common/Swap.h"

namespace
{
u32 SubtractBE32(u32 minuend_be, u32 subtrahend_le)
{
  return Common::swap32(Common::swap32(minuend_be) - subtrahend_le);
}

void Replace(u64 offset, u64 size, u8* out_ptr, u64 replace_offset, u64 replace_size,
             const u8* replace_ptr)
{
  const u64 replace_start = std::max(offset, replace_offset);
  const u64 replace_end = std::min(offset + size, replace_offset + replace_size);

  if (replace_end > replace_start)
  {
    std::copy(replace_ptr + (replace_start - replace_offset),
              replace_ptr + (replace_end - replace_offset), out_ptr + (replace_start - offset));
  }
}

template <typename T>
void Replace(u64 offset, u64 size, u8* out_ptr, u64 replace_offset, const T& replace_value)
{
  static_assert(std::is_trivially_copyable_v<T>);

  const u8* replace_ptr = reinterpret_cast<const u8*>(&replace_value);
  Replace(offset, size, out_ptr, replace_offset, sizeof(T), replace_ptr);
}
}  // namespace

namespace DiscIO
{
std::unique_ptr<TGCFileReader> TGCFileReader::Create(File::IOFile file)
{
  TGCHeader header;
  if (file.Seek(0, File::SeekOrigin::Begin) && file.ReadArray(&header, 1) &&
      header.magic == TGC_MAGIC)
  {
    return std::unique_ptr<TGCFileReader>(new TGCFileReader(std::move(file)));
  }

  return nullptr;
}

TGCFileReader::TGCFileReader(File::IOFile file) : m_file(std::move(file))
{
  m_file.Seek(0, File::SeekOrigin::Begin);
  m_file.ReadArray(&m_header, 1);

  m_size = m_file.GetSize();

  const u32 fst_offset = Common::swap32(m_header.fst_real_offset);
  const u32 fst_size = Common::swap32(m_header.fst_size);
  m_fst.resize(fst_size);
  if (!m_file.Seek(fst_offset, File::SeekOrigin::Begin) ||
      !m_file.ReadBytes(m_fst.data(), m_fst.size()))
  {
    m_fst.clear();
  }

  constexpr size_t FST_ENTRY_SIZE = 12;
  if (m_fst.size() < FST_ENTRY_SIZE)
    return;

  // This calculation can overflow, but this is not a problem, because in that case
  // the old_offset + file_area_shift calculation later also overflows, cancelling it out
  const u32 file_area_shift = Common::swap32(m_header.file_area_real_offset) -
                              Common::swap32(m_header.file_area_virtual_offset) -
                              Common::swap32(m_header.tgc_header_size);

  const size_t claimed_fst_entries = Common::swap32(m_fst.data() + 8);
  const size_t fst_entries = std::min(claimed_fst_entries, m_fst.size() / FST_ENTRY_SIZE);
  for (size_t i = 0; i < fst_entries; ++i)
  {
    // If this is a file (as opposed to a directory)...
    if (m_fst[i * FST_ENTRY_SIZE] == 0)
    {
      // ...change its offset
      const u32 old_offset = Common::swap32(m_fst.data() + i * FST_ENTRY_SIZE + 4);
      const u32 new_offset = Common::swap32(old_offset + file_area_shift);
      Replace<u32>(0, m_fst.size(), m_fst.data(), i * FST_ENTRY_SIZE + 4, new_offset);
    }
  }
}

std::unique_ptr<BlobReader> TGCFileReader::CopyReader() const
{
  return Create(m_file.Duplicate("rb"));
}

u64 TGCFileReader::GetDataSize() const
{
  return m_size - Common::swap32(m_header.tgc_header_size);
}

bool TGCFileReader::Read(u64 offset, u64 nbytes, u8* out_ptr)
{
  const u32 tgc_header_size = Common::swap32(m_header.tgc_header_size);

  if (m_file.Seek(offset + tgc_header_size, File::SeekOrigin::Begin) &&
      m_file.ReadBytes(out_ptr, nbytes))
  {
    const u32 replacement_dol_offset = SubtractBE32(m_header.dol_real_offset, tgc_header_size);
    const u32 replacement_fst_offset = SubtractBE32(m_header.fst_real_offset, tgc_header_size);

    Replace<u32>(offset, nbytes, out_ptr, 0x0420, replacement_dol_offset);
    Replace<u32>(offset, nbytes, out_ptr, 0x0424, replacement_fst_offset);
    Replace(offset, nbytes, out_ptr, Common::swap32(replacement_fst_offset), m_fst.size(),
            m_fst.data());

    return true;
  }

  m_file.ClearError();
  return false;
}

}  // namespace DiscIO
