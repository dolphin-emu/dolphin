// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>
#include <string_view>

#include "Common/CommonPaths.h"

static const inline std::string DOLPHIN_SYSTEM_GRAPHICS_MOD_DIR =
    LOAD_DIR DIR_SEP GRAPHICSMOD_DIR DIR_SEP;

namespace GraphicsModSystem
{
static constexpr std::string_view internal_tag_prefix = "__internal_";
}
