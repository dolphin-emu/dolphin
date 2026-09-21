// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>
#include <vector>

#include "Common/Config/Config.h"
#include "Common/HookableEvent.h"

class InputConfig;

namespace ciface::Core
{
class DeviceContainer;
}

namespace ControllerEmu
{
class EmulatedController;
}

// Automatically assigns SDL controllers to player slots based on connection order.
// When a controller is connected, it's assigned to the first available player slot.
// When disconnected, that slot becomes inactive.
// Nintendo controllers (Wiimotes) are detected and handled specially.
class AutoControllerAssignment
{
public:
  AutoControllerAssignment();
  ~AutoControllerAssignment();

  // Initialize auto-assignment, registering for device change events.
  // Must be called after Pad::Initialize() and Wiimote::Initialize().
  void Initialize();

  // Shutdown and unregister callbacks.
  void Shutdown();

  // Perform assignment now (e.g., on initial startup).
  void PerformAssignment();

private:
  // Checks if a device name indicates a Nintendo/Wiimote controller.
  static bool IsNintendoController(const std::string& device_name);

  // Extracts the human-readable device name from a qualified name "SDL/{id}/{name}".
  static std::string ExtractDeviceName(const std::string& qualified_name);

  // Loads a profile onto a controller. First tries a controller-specific profile
  // ("SDL Auto Assignment <device_name>.ini"), then falls back to the stock profile.
  // The controller's device is forced to force_device regardless of any "Device"
  // entry inside the profile.
  static void LoadProfileForController(InputConfig* config,
                                       ControllerEmu::EmulatedController* controller,
                                       const std::string& device_name,
                                       const std::string& force_device,
                                       const std::string& stock_profile_name);

  // Get list of currently connected SDL device qualified names, in order.
  std::vector<std::string> GetConnectedSDLDevices() const;

  // Get the qualified names of the live internal Bluetooth Wii Remote devices, in id order.
  std::vector<std::string> GetConnectedBluetoothWiimotes() const;

  // Assign SDL devices to GCPad player slots.
  void AssignGCPadSlots(const std::vector<std::string>& sdl_devices);

  // Assign SDL devices to Wiimote emulation slots.
  void AssignWiimoteSlots(const std::vector<std::string>& sdl_devices);

  Common::EventHook m_devices_changed_hook;
  Config::ConfigChangedCallbackID m_config_changed_callback_id{};
  bool m_was_enabled = false;
};
