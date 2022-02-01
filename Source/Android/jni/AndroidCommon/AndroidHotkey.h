// Copyright 2021 Dolphin MMJR
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

namespace AndroidHotkey
{
enum Hotkey
{
    HOTKEY = 900,
    HK_TOGGLE_SKIP_EFB_ACCESS = 901,
    HK_TOGGLE_EFBCOPIES = 902,
};

/*
 * Handles hotkeys events
 *
 * @param id        ID of the hotkey to toggle.
 * @param action    Action to be performed.
 * @return          The state of the toggled hotkey.
 */
bool onHotkeyEvent(int id, bool showMessage);

/*
 * Retrieves hotkey's state
 *
 * @param HotkeyId  ID of the hotkey to get the state of.
 * @return          The state of the hotkey.
 */
bool getHotkeyState(int id);

} // namespace AndroidHotkey
