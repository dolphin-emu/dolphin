// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <optional>
#include <string>

#include "Common/HookableEvent.h"
#include "InputCommon/ControllerInterface/InputBackend.h"

namespace ciface::Core
{
class Device;
}

namespace ciface::Android
{
std::unique_ptr<ciface::InputBackend> CreateInputBackend(ControllerInterface* controller_interface);

// One physical controller is reported by Android as several input devices: the controller
// itself plus, for instance, its touchpad and motion sensors. These properties tell the real
// controller apart from the rest.
struct DeviceProperties
{
  // Advertises a gamepad or joystick source and was given a positive controller number,
  // which Android grants to no other kind of device.
  bool is_gamepad = false;

  // Identifies the physical controller this device belongs to. Android keeps it the same
  // for as long as the controller exists, unlike the name and the controller number, which
  // change as controllers come and go.
  std::string descriptor;

  // The position at which this device first delivered gamepad input, counting from zero, or
  // nothing if it never has. This is what identifies a controller somebody is playing on:
  // the devices Android reports alongside it never deliver any.
  std::optional<int> input_order;
};

// Returns the properties of a device provided by this backend, or nothing for other devices.
std::optional<DeviceProperties> GetDeviceProperties(const ciface::Core::Device& device);

// Forgets which devices have delivered input, so that the next session starts over.
void ForgetDeliveredInput();

// Forgets that the given device delivered input, for when it disconnects.
void ForgetDeliveredInput(int device_id);

// Registers a listener to be called when a device delivers its first gamepad input.
[[nodiscard]] Common::EventHook
RegisterFirstInputCallback(Common::HookableEvent<>::CallbackType callback);

}  // namespace ciface::Android
