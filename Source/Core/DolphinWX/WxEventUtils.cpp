// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinWX/WxEventUtils.h"

#include <wx/event.h>
#include "Core/Core.h"
#include "Core/HW/CPU.h"
#include "Core/NetPlayProto.h"

namespace WxEventUtils
{
void OnEnableIfCoreInitialized(wxUpdateUIEvent& event)
{
  event.Enable(Core::GetState() != Core::CORE_UNINITIALIZED);
}

void OnEnableIfCoreUninitialized(wxUpdateUIEvent& event)
{
  event.Enable(Core::GetState() == Core::CORE_UNINITIALIZED);
}

void OnEnableIfCoreRunning(wxUpdateUIEvent& event)
{
  event.Enable(Core::IsRunning());
}

void OnEnableIfCoreNotRunning(wxUpdateUIEvent& event)
{
  event.Enable(!Core::IsRunning());
}

void OnEnableIfCorePaused(wxUpdateUIEvent& event)
{
  event.Enable(Core::GetState() == Core::CORE_PAUSE);
}

void OnEnableIfCoreRunningOrPaused(wxUpdateUIEvent& event)
{
  const auto state = Core::GetState();

  event.Enable(state == Core::CORE_RUN || state == Core::CORE_PAUSE);
}

void OnEnableIfCPUCanStep(wxUpdateUIEvent& event)
{
  event.Enable(Core::IsRunning() && CPU::IsStepping());
}

void OnEnableIfNetplayNotRunning(wxUpdateUIEvent& event)
{
  event.Enable(!NetPlay::IsNetPlayRunning());
}
}  // namespace WxEventUtils
