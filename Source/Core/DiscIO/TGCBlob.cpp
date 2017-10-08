// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DiscIO/TGCBlob.h"

#include <memory>
#include <string>
#include <utility>

#include "Common/File.h"
#include "Common/Swap.h"

namespace
{
template <typename T>
struct Interval
{
  T start;
  T length;

  T End() const { return start + length; }
  bool IsEmpty() const { return length == 0; }
};

template <typename T>
void SplitInterval(T split_point, Interval<T> interval, Interval<T>* out_1, Interval<T>* out_2)
{
  if (interval.start < split_point)
    *out_1 = {interval.start, std::min(interval.length, split_point - interval.start)};
  else
    *out_1 = {0, 0};

  if (interval.End() > split_point)
  {
    *out_2 = {std::max(interval.start, split_point),
              std::min(interval.length, interval.End() - split_point)};
  }
  else
  {
    *out_2 = {0, 0};
  }
}

u32 SubtractBE32(u32 minuend_be, u32 subtrahend_le)
{
  return Common::swap32(Common::swap32(minuend_be) - subtrahend_le);
}

void Replace8(u64 offset, u64 nbytes, u8* out_ptr, u64 replace_offset, u8 replace_value)
{
  if (offset <= replace_offset && offset + nbytes > replace_offset)
    out_ptr[replace_offset - offset] = replace_value;
}

void Replace32(u64 offset, u64 nbytes, u8* out_ptr, u64 replace_offset, u32 replace_value)
{
  for (size_t i = 0; i < sizeof(u32); ++i)
    Replace8(offset, nbytes, out_ptr, replace_offset + i, reinterpret_cast<u8*>(&replace_value)[i]);
}
}

namespace DiscIO
{
std::unique_ptr<TGCFileReader> TGCFileReader::Create(File::IOFile file)
{
  TGCHeader header;
  if (file.Seek(0, SEEK_SET) && file.ReadArray(&header, 1) && header.magic == TGC_MAGIC)
    return std::unique_ptr<TGCFileReader>(new TGCFileReader(std::move(file)));

  return nullptr;
}

TGCFileReader::TGCFileReader(File::IOFile file) : m_file(std::move(file))
{
  m_file.Seek(0, SEEK_SET);
  m_file.ReadArray(&m_header, 1);
  u32 header_size = Common::swap32(m_header.tgc_header_size);
  m_size = m_file.GetSize();
  m_file_area_shift = static_cast<s64>(Common::swap32(m_header.file_area_virtual_offset)) -
                      Common::swap32(m_header.file_area_real_offset) + header_size;
}

u64 TGCFileReader::GetDataSize() const
{
  return m_size + Common::swap32(m_header.file_area_virtual_offset) -
         Common::swap32(m_header.file_area_real_offset);
}

bool TGCFileReader::Read(u64 offset, u64 nbytes, u8* out_ptr)
{
  Interval<u64> first_part = {0, 0};
  Interval<u64> empty_part = {0, 0};
  Interval<u64> file_part = {0, 0};

  const u32 tgc_header_size = Common::swap32(m_header.tgc_header_size);
  const u64 split_point = Common::swap32(m_header.file_area_real_offset) - tgc_header_size;
  SplitInterval(split_point, Interval<u64>{offset, nbytes}, &first_part, &file_part);
  if (m_file_area_shift > tgc_header_size)
  {
    SplitInterval(static_cast<u64>(m_file_area_shift - tgc_header_size), file_part, &empty_part,
                  &file_part);
  }

  // Offsets in the initial areas of the disc are unshifted
  // (except for InternalRead's constant shift by tgc_header_size).
  if (!first_part.IsEmpty())
  {
    if (!InternalRead(first_part.start, first_part.length, out_ptr + (first_part.start - offset)))
      return false;
  }

  // The data between the file area and the area that precedes it is treated as all zeroes.
  // The game normally won't attempt to access this part of the virtual disc, but let's not return
  // an error if it gets accessed, in case someone wants to copy or hash the whole virtual disc.
  if (!empty_part.IsEmpty())
    std::fill_n(out_ptr + (empty_part.start - offset), empty_part.length, 0);

  // Offsets in the file area are shifted by m_file_area_shift.
  if (!file_part.IsEmpty())
  {
    if (!InternalRead(file_part.start - m_file_area_shift, file_part.length,
                      out_ptr + (file_part.start - offset)))
    {
      return false;
    }
  }

  return true;
}

bool TGCFileReader::InternalRead(u64 offset, u64 nbytes, u8* out_ptr)
{
  const u32 tgc_header_size = Common::swap32(m_header.tgc_header_size);

  if (m_file.Seek(offset + tgc_header_size, SEEK_SET) && m_file.ReadBytes(out_ptr, nbytes))
  {
    Replace32(offset, nbytes, out_ptr, 0x420,
              SubtractBE32(m_header.dol_real_offset, tgc_header_size));
    Replace32(offset, nbytes, out_ptr, 0x424,
              SubtractBE32(m_header.fst_real_offset, tgc_header_size));
    return true;
  }

  m_file.Clear();
  return false;
}

}  // namespace DiscIO
