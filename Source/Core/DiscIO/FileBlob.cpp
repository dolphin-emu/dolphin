// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DiscIO/FileBlob.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "Common/Align.h"
#include "Common/Assert.h"
#include "Common/Buffer.h"
#include "Common/FileUtil.h"
#include "Common/MsgHandler.h"

#include "DiscIO/MultithreadedCompressor.h"

namespace DiscIO
{
PlainFileReader::PlainFileReader(File::DirectIOFile file)
    : m_file(std::move(file)), m_size{m_file.GetSize()}
{
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

ConversionResultCode ConvertToPlain(std::unique_ptr<BlobReader> infile, File::DirectIOFile& outfile,
                                    const CompressCB& callback)
{
  ASSERT(infile->GetDataSizeType() == DataSizeType::Accurate);

  constexpr size_t MINIMUM_BUFFER_SIZE = 0x80000;
  u64 buffer_size = infile->GetBlockSize();
  if (buffer_size == 0)
  {
    buffer_size = MINIMUM_BUFFER_SIZE;
  }
  else
  {
    while (buffer_size < MINIMUM_BUFFER_SIZE)
      buffer_size *= 2;
  }

  const u64 total_size = infile->GetDataSize();

  // Avoid fragmentation.
  if (!Resize(outfile, total_size))
    return ConversionResultCode::WriteFailed;

  Common::UniqueBuffer<u8> buffer(buffer_size);

  const u64 progress_interval = Common::AlignUp(std::max<u64>(total_size / 100, 1), buffer_size);
  for (u64 read_pos = 0; read_pos != total_size;)
  {
    if (read_pos % progress_interval == 0)
    {
      const bool was_cancelled =
          !callback(Common::GetStringT("Unpacking"), float(read_pos) / float(total_size));
      if (was_cancelled)
        return ConversionResultCode::Canceled;
    }

    const u64 read_size = std::min(buffer_size, total_size - read_pos);
    if (!infile->Read(read_pos, read_size, buffer.data()))
      return ConversionResultCode::ReadFailed;

    if (!outfile.Write(buffer.data(), read_size))
      return ConversionResultCode::WriteFailed;

    read_pos += read_size;
  }

  return ConversionResultCode::Success;
}

}  // namespace DiscIO
