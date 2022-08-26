// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/CommonTypes.h"

namespace Common
{
bool SyncSDFolderToSDImage(bool deterministic);
bool SyncSDImageToSDFolder();
}  // namespace Common
