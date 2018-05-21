// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <cstddef>

#include "Common/CommonTypes.h"

namespace Common::Random
{
/// Fill `buffer` with random bytes using a cryptographically secure pseudo-random number generator.
void Generate(void* buffer, std::size_t size);
}  // namespace Common::Random
