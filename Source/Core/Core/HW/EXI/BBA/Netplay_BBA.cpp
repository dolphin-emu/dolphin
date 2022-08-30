// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Common/Assert.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Common/StringUtil.h"
#include "Core/HW/EXI/EXI_Device.h"
#include "Core/HW/EXI/EXI_DeviceEthernet.h"

#include "VideoCommon/OnScreenDisplay.h"

#include <SFML/Network.hpp>

#include <cstring>

/*netplay BBA code will go here.
networkinterface  */

namespace ExpansionInterface
{

bool CEXIETHERNET::NetplayBBAnetworkinterface::Activate()
{
  if (IsActivated())
    return true;

  if (!m_bba_failure_notifiedtwo)
  {
    OSD::AddMessage("Netplay BBA started.", 30000);
    m_bba_failure_notifiedtwo = true;
  }
  return false;
}

bool CEXIETHERNET::NetplayBBAnetworkinterface::IsActivated()
{
  return false;
}

}  // namespace ExpansionInterfacex
