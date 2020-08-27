// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/MD5.h"

#include <fstream>
#include <functional>
#include <mbedtls/md5.h>
#include <string>

#include <fmt/format.h>

#include "Common/StringUtil.h"
#include "DiscIO/Blob.h"

namespace MD5
{
std::string MD5Sum(const std::string& file_path, std::function<bool(int)> report_progress)
{
  std::string output_string;
  std::vector<u8> data(8 * 1024 * 1024);
  u64 read_offset = 0;
  mbedtls_md5_context ctx;

  std::unique_ptr<DiscIO::BlobReader> file(DiscIO::CreateBlobReader(file_path));
  u64 game_size = file->GetDataSize();

  mbedtls_md5_starts_ret(&ctx);

  while (read_offset < game_size)
  {
    size_t read_size = std::min(static_cast<u64>(data.size()), game_size - read_offset);
    if (!file->Read(read_offset, read_size, data.data()))
      return output_string;

    mbedtls_md5_update_ret(&ctx, data.data(), read_size);
    read_offset += read_size;

    int progress =
        static_cast<int>(static_cast<float>(read_offset) / static_cast<float>(game_size) * 100);
    if (!report_progress(progress))
      return output_string;
  }

  std::array<u8, 16> output;
  mbedtls_md5_finish_ret(&ctx, output.data());

  // Convert to hex
  for (u8 n : output)
    output_string += fmt::format("{:02x}", n);

  return output_string;
}
}  // namespace MD5
