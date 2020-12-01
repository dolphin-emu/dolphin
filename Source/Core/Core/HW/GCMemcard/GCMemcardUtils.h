// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

namespace Memcard
{
struct DEntry;

bool HasSameIdentity(const DEntry& lhs, const DEntry& rhs);
}  // namespace Memcard
