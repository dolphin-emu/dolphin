// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinWX/WxEventUtils.h"

#include <wx/event.h>
#include "Core/Core.h"
#include "Core/NetPlayProto.h"

namespace WxEventUtils
{
void OnEnableIfCoreNotRunning(wxUpdateUIEvent& event)
{
  event.Enable(!Core::IsRunning());
}

void OnEnableIfNetplayNotRunning(wxUpdateUIEvent& event)
{
  event.Enable(!NetPlay::IsNetPlayRunning());
}
}  // namespace WxEventUtils
