// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/CommonTypes.h"
#include "Common/Config/Config.h"

namespace Config
{
extern const Info<bool> SESSION_USE_FMA;
extern const Info<bool> SESSION_LOAD_IPL_DUMP;
extern const Info<bool> SESSION_GCI_FOLDER_CURRENT_GAME_ONLY;
extern const Info<bool> SESSION_CODE_SYNC_OVERRIDE;
extern const Info<bool> SESSION_SAVE_DATA_WRITABLE;
extern const Info<bool> SESSION_SHOULD_FAKE_ERROR_001;
}  // namespace Config
