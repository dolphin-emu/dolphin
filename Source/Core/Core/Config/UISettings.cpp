// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/Config/UISettings.h"

namespace Config
{
// UI.General

const Info<bool> MAIN_USE_DISCORD_PRESENCE{{System::Main, "General", "UseDiscordPresence"}, true};
#ifdef ANDROID
const Info<bool> MAIN_USE_GAME_COVERS{{System::Main, "General", "UseGameCovers"}, true};
#else
const Info<bool> MAIN_USE_GAME_COVERS{{System::Main, "General", "UseGameCovers"}, false};
#endif
const Info<HotkeyFocusPolicy> MAIN_HOTKEY_FOCUS_POLICY{
    {System::Main, "General", "HotkeysRequireFocus"}, HotkeyFocusPolicy::Dolphin};
const Info<bool> MAIN_RECURSIVE_ISO_PATHS{{System::Main, "General", "RecursiveISOPaths"}, false};
const Info<std::string> MAIN_CURRENT_STATE_PATH{{System::Main, "General", "CurrentStatePath"}, ""};

}  // namespace Config
