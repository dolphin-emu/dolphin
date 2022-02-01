// Copyright 2021 Dolphin MMJR
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "AndroidHotkey.h"
#include <Core/Config/MainSettings.h>
#include "Core/Config/GraphicsSettings.h"
#include "VideoCommon/OnScreenDisplay.h"

namespace AndroidHotkey
{
#define SHOW_MESSAGE(x) if (showMessage) { OSD::AddMessage(x); }
bool onHotkeyEvent(int id, bool showMessage)
{
    bool newValue;
    switch (id)
    {
        case Hotkey::HK_TOGGLE_SKIP_EFB_ACCESS:
            newValue = !Config::Get(Config::GFX_HACK_EFB_ACCESS_ENABLE);
            Config::SetCurrent(Config::GFX_HACK_EFB_ACCESS_ENABLE, newValue);
            SHOW_MESSAGE(StringFromFormat("Skip EFB Access from CPU: %s", newValue ? "OFF" : "ON"))
            //java code treats this setting as inverted, so invert it before it's returned
            newValue = !newValue;
            break;
        case Hotkey::HK_TOGGLE_EFBCOPIES:
            newValue = !Config::Get(Config::GFX_HACK_SKIP_EFB_COPY_TO_RAM);
            Config::SetCurrent(Config::GFX_HACK_SKIP_EFB_COPY_TO_RAM, newValue);
            SHOW_MESSAGE(StringFromFormat("Store EFB Copies to Texture: %s", newValue ? "ON" : "OFF"))
            break;
        default:
            return false;
    }
    return newValue;
}
#undef SHOW_MESSAGE

bool getHotkeyState(int id) {
    switch (id)
    {
        case Hotkey::HK_TOGGLE_SKIP_EFB_ACCESS:
            return !Config::Get(Config::GFX_HACK_EFB_ACCESS_ENABLE);
        case Hotkey::HK_TOGGLE_EFBCOPIES:
            return Config::Get(Config::GFX_HACK_SKIP_EFB_COPY_TO_RAM);
        default:
            return false;
    }
}
} // namespace AndroidHotkey
