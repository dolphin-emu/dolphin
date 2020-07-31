// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <filesystem>
#include <functional>

namespace PyScripting
{

void Init(std::filesystem::path script_filepath);
void Shutdown();

}  // namespace PyScripting
