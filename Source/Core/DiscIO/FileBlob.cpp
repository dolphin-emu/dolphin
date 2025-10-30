// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DiscIO/FileBlob.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "Common/Assert.h"
#include "Common/FileUtil.h"
#include "Common/MsgHandler.h"

namespace DiscIO
{
PlainFileReader::PlainFileReader(File::DirectIOFile file) : m_file(std::move(file))
{
  m_size = m_file.GetSize();
}

std::unique_ptr<PlainFileReader> PlainFileReader::Create(File::DirectIOFile file)
{
  if (file.IsOpen())
    return std::unique_ptr<PlainFileReader>(new PlainFileReader(std::move(file)));

  return nullptr;
}

std::unique_ptr<BlobReader> PlainFileReader::CopyReader() const
{
  return Create(m_file);
}

bool PlainFileReader::Read(u64 offset, u64 nbytes, u8* out_ptr)
{
  return m_file.OffsetRead(offset, out_ptr, nbytes);
}

bool ConvertToPlain(BlobReader* infile, const std::string& infile_path,
                    const std::string& outfile_path, const CompressCB& callback)
{
  ASSERT(infile->GetDataSizeType() == DataSizeType::Accurate);

  File::DirectIOFile outfile(outfile_path, File::AccessMode::Write);
  if (!outfile.IsOpen())
  {
    PanicAlertFmtT(
        "Failed to open the output file \"{0}\".\n"
        "Check that you have permissions to write the target folder and that the media can "
        "be written.",
        outfile_path);
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
          !callback(Common::GetStringT("Unpacking"), (float)i / (float)num_buffers);
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
      PanicAlertFmtT("Failed to read from the input file \"{0}\".", infile_path);
      success = false;
      break;
    }
    if (!outfile.Write(buffer.data(), sz))
    {
      PanicAlertFmtT("Failed to write the output file \"{0}\".\n"
                     "Check that you have enough space available on the target drive.",
                     outfile_path);
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
