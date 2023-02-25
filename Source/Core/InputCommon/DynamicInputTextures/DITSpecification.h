// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>
#include <vector>

#include <picojson.h>

#include "InputCommon/DynamicInputTextures/DITData.h"

namespace InputCommon::DynamicInputTextures
{
bool ProcessSpecificationV1(picojson::value& root, std::vector<Data>& input_textures,
                            const std::string& base_path, const std::string& json_file);
}
