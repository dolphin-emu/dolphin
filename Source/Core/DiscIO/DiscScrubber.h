// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.


// DiscScrubber removes the garbage data from discs (currently Wii only) which
// is on the disc due to encryption

// It could be adapted to GameCube discs, but the gain is most likely negligible,
// and having 1:1 backups of discs is always nice when they are reasonably sized

// Note: the technique is inspired by Wiiscrubber, but much simpler - intentionally :)

#pragma once

#include <string>
#include "Common/CommonTypes.h"

namespace File { class IOFile; }

namespace DiscIO
{

namespace DiscScrubber
{

bool SetupScrub(const std::string& filename, int block_size);
size_t GetNextBlock(File::IOFile& in, u8* buffer);
void Cleanup();

} // namespace DiscScrubber

} // namespace DiscIO
