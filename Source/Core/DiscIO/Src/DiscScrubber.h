// Copyright (C) 2003 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/


// DiscScrubber removes the garbage data from discs (currently wii only) which
// is on the disc due to encryption

// It could be adapted to gc discs, but the gain is most likely negligible,
// and having 1:1 backups of discs is always nice when they are reasonably sized

// Note: the technique is inspired by Wiiscrubber, but much simpler - intentionally :)

#ifndef DISC_SCRUBBER_H
#define DISC_SCRUBBER_H

#include "Common.h"
#include "Blob.h"


namespace DiscIO
{

namespace DiscScrubber
{

u32 IsScrubbed(const char* filename);

bool Scrub(const char* filename, CompressCB callback = 0, void* arg = 0);

} // namespace DiscScrubber

} // namespace DiscIO

#endif  // DISC_SCRUBBER_H
