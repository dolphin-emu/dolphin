// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

namespace Config
{
struct Location;
}

namespace ConfigLoaders
{
// This is a temporary function that allows for both the new and old configuration
// systems to co-exist without trampling on each other while saving.
// This function shall be removed when the old configuration system retires.
bool IsSettingSaveable(const Config::Location& config_location);
}  // namespace ConfigLoaders
