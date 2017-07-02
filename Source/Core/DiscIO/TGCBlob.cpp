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
    *out_2 = {std::max(interval.start, split_point),
              std::min(interval.length, interval.End() - split_point)};
  else
    *out_2 = {0, 0};
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
  u32 header_size = Common::swap32(m_header.header_size);
  m_size = m_file.GetSize();
  m_file_offset = Common::swap32(m_header.unknown_important_2) -
                  Common::swap32(m_header.unknown_important_1) + header_size;
}

u64 TGCFileReader::GetDataSize() const
{
  return m_size + Common::swap32(m_header.unknown_important_2) -
         Common::swap32(m_header.unknown_important_1);
}

bool TGCFileReader::Read(u64 offset, u64 nbytes, u8* out_ptr)
{
  Interval<u64> first_part;
  Interval<u64> empty_part;
  Interval<u64> last_part;

  u32 header_size = Common::swap32(m_header.header_size);
  SplitInterval(m_file_offset, Interval<u64>{offset, nbytes}, &first_part, &last_part);
  SplitInterval(m_size - header_size, first_part, &first_part, &empty_part);

  // Offsets before m_file_offset are read as usual
  if (!first_part.IsEmpty())
  {
    if (!InternalRead(first_part.start, first_part.length, out_ptr + (first_part.start - offset)))
      return false;
  }

  // If any offset before m_file_offset isn't actually in the file,
  // treat it as 0x00 bytes (so e.g. MD5 calculation of the whole disc won't fail)
  if (!empty_part.IsEmpty())
    std::fill_n(out_ptr + (empty_part.start - offset), empty_part.length, 0);

  // Offsets after m_file_offset are read as if they are (offset - m_file_offset)
  if (!last_part.IsEmpty())
  {
    if (!InternalRead(last_part.start - m_file_offset, last_part.length,
                      out_ptr + (last_part.start - offset)))
      return false;
  }

  return true;
}

bool TGCFileReader::InternalRead(u64 offset, u64 nbytes, u8* out_ptr)
{
  u32 header_size = Common::swap32(m_header.header_size);

  if (m_file.Seek(offset + header_size, SEEK_SET) && m_file.ReadBytes(out_ptr, nbytes))
  {
    Replace32(offset, nbytes, out_ptr, 0x420, SubtractBE32(m_header.dol_offset, header_size));
    Replace32(offset, nbytes, out_ptr, 0x424, SubtractBE32(m_header.fst_offset, header_size));
    return true;
  }

  m_file.Clear();
  return false;
}

}  // namespace DiscIO
