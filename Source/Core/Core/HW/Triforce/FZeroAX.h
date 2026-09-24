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
}

namespace Triforce
{

// FFB wheel used by FZeroAX and FZeroAXMonster.
class FZeroAXSteeringWheel final : public SerialDevice
{
public:
  FZeroAXSteeringWheel();
  ~FZeroAXSteeringWheel() override;

  void Update() override;

  bool IsInitializing() const;

  s8 GetServoPosition() const { return m_servo_position; }

  void DoState(PointerWrap&) override;

private:
  void HandleConfigChange();

  void ProcessRequest(std::span<const u8>);

  u8 m_init_state = 0;

  s8 m_servo_position = 0;

  std::unique_ptr<ciface::Core::SpringEffect> m_spring_effect;

  const CPUThreadConfigCallback::ConfigChangedCallbackID m_config_changed_callback_id;
  const Common::EventHook m_devices_changed_hook;
};

// Used for both FZeroAX and FZeroAXMonster.
class FZeroAXCommon_IOAdapter final : public IOAdapter
{
public:
  explicit FZeroAXCommon_IOAdapter(FZeroAXSteeringWheel* steering_wheel)
      : m_steering_wheel{steering_wheel}
  {
  }

  void Update() override;

protected:
  void HandleGenericOutputsChanged(std::span<const u8> bits_set,
                                   std::span<const u8> bits_cleared) override;

private:
  FZeroAXSteeringWheel* const m_steering_wheel;
};

// Includes seat motion handling.
class FZeroAXDeluxe_IOAdapter final : public IOAdapter
{
public:
  void Update() override;

  void DoState(PointerWrap&) override;

private:
  u32 m_delay = 0;
  u8 m_rx_reply = 0xF0;
};

class FZeroAXMonster_IOAdapter final : public IOAdapter
{
public:
  void Update() override;
};

}  // namespace Triforce
