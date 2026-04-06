// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>

#include "DiscIO/Blob.h"

namespace DiscIO
{

std::unique_ptr<BlobReader> CreateCachedBlobReader(std::unique_ptr<BlobReader> reader);
std::unique_ptr<BlobReader> CreateScrubbingCachedBlobReader(std::unique_ptr<BlobReader> reader);

}  // namespace DiscIO
