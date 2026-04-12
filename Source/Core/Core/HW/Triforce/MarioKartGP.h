// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/HookableEvent.h"
#include "Core/CPUThreadConfigCallback.h"
#include "Core/HW/Triforce/IOPorts.h"
#include "Core/HW/Triforce/SerialDevice.h"

namespace ciface::Core
{
class SpringEffect;
class FrictionEffect;
}  // namespace ciface::Core

namespace Triforce
{

// FFB wheel used by MarioKartGP and MarioKartGP2.
class MarioKartGPSteeringWheel final : public SerialDevice
{
public:
  MarioKartGPSteeringWheel();
  ~MarioKartGPSteeringWheel() override;

  void Update() override;

  void Reset();

  void DoState(PointerWrap&) override;

private:
  void HandleConfigChange();

  void ProcessRequest(std::span<const u8>);

  u8 m_init_state = 0;

  std::unique_ptr<ciface::Core::SpringEffect> m_spring_effect;
  std::unique_ptr<ciface::Core::FrictionEffect> m_friction_effect;

  const CPUThreadConfigCallback::ConfigChangedCallbackID m_config_changed_callback_id;
  const Common::EventHook m_devices_changed_hook;
};

// Used for both MarioKartGP and MarioKartGP2.
class MarioKartGPCommon_IOAdapter final : public IOAdapter
{
public:
  explicit MarioKartGPCommon_IOAdapter(MarioKartGPSteeringWheel* steering_wheel)
      : m_steering_wheel{steering_wheel}
  {
  }

  void Update() override;

protected:
  void HandleGenericOutputsChanged(std::span<const u8> bits_set,
                                   std::span<const u8> bits_cleared) override;

private:
  MarioKartGPSteeringWheel* const m_steering_wheel;
};

}  // namespace Triforce
