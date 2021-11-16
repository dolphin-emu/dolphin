// Copyright 2020 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once
#include <DiscIO/VolumeGC.h>
#include <DiscIO/WIABlob.h>
namespace DiscIO
{
void ConvertDisc(
    std::string original_path, std::string dst_path, DiscIO::BlobType format = DiscIO::BlobType::RVZ,
    DiscIO::WIARVZCompressionType compression_type = DiscIO::WIARVZCompressionType::LZMA2,
    int compression_level = 9, int block_size = 0x20000, bool remove_junk = false,
    bool delete_original = false);
}
