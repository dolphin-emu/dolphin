// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/Config/Config.h"

namespace Config
{
// Configuration Information
extern const Info<bool> RA_ENABLED;
extern const Info<std::string> RA_USERNAME;
extern const Info<std::string> RA_API_TOKEN;
}  // namespace Config
