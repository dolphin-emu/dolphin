// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "Common/Assert.h"
#include "Common/FileUtil.h"
#include "Common/MsgHandler.h"
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

bool ConvertToPlain(BlobReader* infile, const std::string& infile_path,
                    const std::string& outfile_path, CompressCB callback, void* arg)
{
  ASSERT(infile->IsDataSizeAccurate());

  File::IOFile outfile(outfile_path, "wb");
  if (!outfile)
  {
    PanicAlertT("Failed to open the output file \"%s\".\n"
                "Check that you have permissions to write the target folder and that the media can "
                "be written.",
                outfile_path.c_str());
    return false;
  }

  constexpr size_t DESIRED_BUFFER_SIZE = 0x80000;
  u64 buffer_size = infile->GetBlockSize();
  if (buffer_size == 0)
  {
    buffer_size = DESIRED_BUFFER_SIZE;
  }
  else
  {
    while (buffer_size < DESIRED_BUFFER_SIZE)
      buffer_size *= 2;
  }

  std::vector<u8> buffer(buffer_size);
  const u64 num_buffers = (infile->GetDataSize() + buffer_size - 1) / buffer_size;
  int progress_monitor = std::max<int>(1, num_buffers / 100);
  bool success = true;

  for (u64 i = 0; i < num_buffers; i++)
  {
    if (i % progress_monitor == 0)
    {
      const bool was_cancelled =
          !callback(Common::GetStringT("Unpacking"), (float)i / (float)num_buffers, arg);
      if (was_cancelled)
      {
        success = false;
        break;
      }
    }
    const u64 inpos = i * buffer_size;
    const u64 sz = std::min(buffer_size, infile->GetDataSize() - inpos);
    if (!infile->Read(inpos, sz, buffer.data()))
    {
      PanicAlertT("Failed to read from the input file \"%s\".", infile_path.c_str());
      success = false;
      break;
    }
    if (!outfile.WriteBytes(buffer.data(), sz))
    {
      PanicAlertT("Failed to write the output file \"%s\".\n"
                  "Check that you have enough space available on the target drive.",
                  outfile_path.c_str());
      success = false;
      break;
    }
  }

  if (!success)
  {
    // Remove the incomplete output file.
    outfile.Close();
    File::Delete(outfile_path);
  }

  return success;
}

}  // namespace DiscIO
