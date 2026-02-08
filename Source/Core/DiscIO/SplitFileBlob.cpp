// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DiscIO/SplitFileBlob.h"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <fmt/format.h>

#include "Common/Assert.h"
#include "Common/FileUtil.h"
#include "Common/IOFile.h"
#include "Common/MsgHandler.h"

namespace DiscIO
{
SplitPlainFileReader::SplitPlainFileReader(std::vector<SingleFile> files)
    : m_files(std::move(files))
{
  m_size = 0;
  for (const auto& f : m_files)
    m_size += f.size;
}

std::unique_ptr<SplitPlainFileReader> SplitPlainFileReader::Create(std::string_view first_file_path)
{
  constexpr std::string_view part0_iso = ".part0.iso";
  if (!first_file_path.ends_with(part0_iso))
    return nullptr;

  const std::string_view base_path =
      first_file_path.substr(0, first_file_path.size() - part0_iso.size());
  std::vector<SingleFile> files;
  size_t index = 0;
  u64 offset = 0;
  while (true)
  {
    File::IOFile f(fmt::format("{}.part{}.iso", base_path, index), "rb");
    if (!f.IsOpen())
      break;
    const u64 size = f.GetSize();
    if (size == 0)
      return nullptr;
    files.emplace_back(SingleFile{std::move(f), offset, size});
    offset += size;
    ++index;
  }

  if (files.size() < 2)
    return nullptr;

  files.shrink_to_fit();
  return std::unique_ptr<SplitPlainFileReader>(new SplitPlainFileReader(std::move(files)));
}

std::unique_ptr<BlobReader> SplitPlainFileReader::CopyReader() const
{
  std::vector<SingleFile> new_files{};
  for (const SingleFile& file : m_files)
  {
    new_files.push_back(
        {.file = file.file.Duplicate("rb"), .offset = file.offset, .size = file.size});
  }
  return std::unique_ptr<SplitPlainFileReader>(new SplitPlainFileReader(std::move(new_files)));
}

bool SplitPlainFileReader::Read(u64 offset, u64 nbytes, u8* out_ptr)
{
  if (offset >= m_size)
    return false;

  u64 current_offset = offset;
  u64 rest = nbytes;
  u8* out = out_ptr;
  for (auto& file : m_files)
  {
    if (current_offset >= file.offset && current_offset < file.offset + file.size)
    {
      auto& f = file.file;
      const u64 seek_offset = current_offset - file.offset;
      const u64 current_read = std::min(file.size - seek_offset, rest);
      if (!f.Seek(seek_offset, File::SeekOrigin::Begin) || !f.ReadBytes(out, current_read))
      {
        f.ClearError();
        return false;
      }

      rest -= current_read;
      if (rest == 0)
        return true;
      current_offset += current_read;
      out += current_read;
    }
  }

  return rest == 0;
}
}  // namespace DiscIO
