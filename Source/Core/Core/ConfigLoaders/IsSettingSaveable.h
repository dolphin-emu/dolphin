// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

namespace Config
{
struct ConfigLocation;
}

namespace ConfigLoaders
{
// This is a temporary function that allows for both the new and old configuration
// systems to co-exist without trampling on each other while saving.
// This function shall be removed when the old configuration system retires.
bool IsSettingSaveable(const Config::ConfigLocation& config_location);
bool IsSettingSaveableGameINI(const Config::ConfigLocation& config_location);
}  // namespace ConfigLoader
