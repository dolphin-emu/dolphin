// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/Config/SessionSettings.h"

#include "Common/CPUDetect.h"
#include "Common/CommonTypes.h"
#include "Common/Config/Config.h"

namespace Config
{
const Info<bool> SESSION_USE_FMA{{System::Session, "Core", "UseFMA"}, CPUInfo().bFMA};
const Info<bool> SESSION_LOAD_IPL_DUMP{{System::Session, "Core", "LoadIPLDump"}, true};
const Info<bool> SESSION_GCI_FOLDER_CURRENT_GAME_ONLY{
    {System::Session, "Core", "GCIFolderCurrentGameOnly"}, false};
const Info<bool> SESSION_CODE_SYNC_OVERRIDE{{System::Session, "Core", "CheatSyncOverride"}, false};
const Info<bool> SESSION_SAVE_DATA_WRITABLE{{System::Session, "Core", "SaveDataWritable"}, true};
const Info<bool> SESSION_SHOULD_FAKE_ERROR_001{{System::Session, "Core", "ShouldFakeError001"},
                                               false};
}  // namespace Config
