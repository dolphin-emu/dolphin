// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HW/SI/SI_DeviceGCAdapter.h"

#include <cstring>

#include "Common/CommonTypes.h"
#include "Common/Swap.h"
#include "Core/Config/MainSettings.h"
#include "Core/Core.h"
#include "Core/HW/GCPad.h"
#include "Core/NetPlayProto.h"
#include "Core/System.h"
#include "InputCommon/GCAdapter.h"

namespace SerialInterface
{
CSIDevice_GCAdapter::CSIDevice_GCAdapter(Core::System& system, SIDevices device, int device_number)
    : CSIDevice_GCController(system, device, device_number)
{
  // Make sure PAD_GET_ORIGIN gets set due to a newly connected device.
  GCAdapter::ResetDeviceType(m_device_number);

  // get the correct pad number that should rumble locally when using netplay
  const int pad_num = NetPlay_InGamePadToLocalPad(m_device_number);
  if (pad_num < 4)
    m_simulate_konga = Config::Get(Config::GetInfoForSimulateKonga(pad_num));
}

GCPadStatus CSIDevice_GCAdapter::GetPadStatus()
{
  GCPadStatus pad_status = {};

  // For netplay, the local controllers are polled in GetNetPads(), and
  // the remote controllers receive their status there as well
  if (!NetPlay::IsNetPlayRunning())
  {
    pad_status = GCAdapter::Input(m_device_number);
  }

  HandleMoviePadStatus(m_system.GetMovie(), m_device_number, &pad_status);

  // Our GCAdapter code sets PAD_GET_ORIGIN when a new device has been connected.
  // Watch for this to calibrate real controllers on connection.
  if (pad_status.button & PAD_GET_ORIGIN)
    SetOrigin(pad_status);

  return pad_status;
}

int CSIDevice_GCAdapter::RunBuffer(u8* buffer, int request_length)
{
  if (!Core::WantsDeterminism())
  {
    // The previous check is a hack to prevent a desync due to SI devices
    // being different and returning different values on RunBuffer();
    // the corresponding code in GCAdapter.cpp has the same check.

    // This returns an error value if there is no controller plugged
    // into this port on the hardware gc adapter, exposing it to the game.
    if (!GCAdapter::DeviceConnected(m_device_number))
    {
      u32 device = Common::swap32(SI_NONE);
      memcpy(buffer, &device, sizeof(device));
      return 4;
    }
  }
  return CSIDevice_GCController::RunBuffer(buffer, request_length);
}

bool CSIDevice_GCAdapter::GetData(u32& hi, u32& low)
{
  CSIDevice_GCController::GetData(hi, low);

  if (m_simulate_konga)
  {
    hi &= CSIDevice_TaruKonga::HI_BUTTON_MASK;
  }

  return true;
}

void CSIDevice_GCController::Rumble(int pad_num, ControlState strength, SIDevices device)
{
  if (device == SIDEVICE_WIIU_ADAPTER)
    GCAdapter::Output(pad_num, static_cast<u8>(strength));
  else if (SIDevice_IsGCController(device))
    Pad::Rumble(pad_num, strength);
}
}  // namespace SerialInterface
