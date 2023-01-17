// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinNoGUI/Platform.h"
#include "Core/HW/ProcessorInterface.h"
#include "Core/IOS/IOS.h"
#include "Core/IOS/STM/STM.h"
#include "Core/State.h"
#include "Core/System.h"

Platform::~Platform() = default;

bool Platform::Init()
{
  return true;
}

void Platform::SetTitle(const std::string& title)
{
}

void Platform::UpdateRunningFlag()
{
  if (m_shutdown_requested.TestAndClear())
  {
    const auto ios = IOS::HLE::GetIOS();
    const auto stm = ios ? ios->GetDeviceByName("/dev/stm/eventhook") : nullptr;
    if (!m_tried_graceful_shutdown.IsSet() && stm &&
        std::static_pointer_cast<IOS::HLE::STMEventHookDevice>(stm)->HasHookInstalled())
    {
      auto& system = Core::System::GetInstance();
      system.GetProcessorInterface().PowerButton_Tap();
      m_tried_graceful_shutdown.Set();
    }
    else
    {
      m_running.Clear();
    }
  }
}

void Platform::Stop()
{
  m_running.Clear();
}

void Platform::RequestShutdown()
{
  m_shutdown_requested.Set();
}
