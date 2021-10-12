// Copyright 2019 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <cstdio>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"

// Refer to docs/autoupdate_overview.md for a detailed overview of the autoupdate process

bool RunUpdater(std::vector<std::string> args);
