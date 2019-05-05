// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <memory>
#include <string>
#include <utility>

#include "DiscIO/FileBlob.h"

namespace DiscIO
{
PlainFileReader::PlainFileReader(File::IOFile file) : m_file(std::move(file))
{
  m_size = m_file.GetSize();
}

std::unique_ptr<PlainFileReader> PlainFileReader::Create(File::IOFile file)
{
  if (file)
    return std::unique_ptr<PlainFileReader>(new PlainFileReader(std::move(file)));

  return nullptr;
}

bool PlainFileReader::Read(u64 offset, u64 nbytes, u8* out_ptr)
{
  if (m_file.Seek(offset, SEEK_SET) && m_file.ReadBytes(out_ptr, nbytes))
  {
    return true;
  }
  else
  {
    m_file.Clear();
    return false;
  }
}

}  // namespace DiscIO
