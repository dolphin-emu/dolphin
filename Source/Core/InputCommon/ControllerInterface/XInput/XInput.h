// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

// XInput suffers a similar issue as XAudio2. Since Win8, it is part of the OS.
// However, unlike XAudio2 they have not made the API incompatible - so we just
// compile against the latest version and fall back to dynamically loading the
// old DLL.

#pragma once

#include <windows.h>
#include <XInput.h>

#include "InputCommon/ControllerInterface/ControllerInterface.h"

#ifndef XINPUT_DEVSUBTYPE_FLIGHT_STICK
#error You are building this module against the wrong version of DirectX. You probably need to remove DXSDK_DIR from your include path and/or _WIN32_WINNT is wrong.
#endif

namespace ciface::XInput
{
void Init();
void PopulateDevices();
void DeInit();

class Device : public Core::Device
{
private:
  class Button : public Core::Device::Input
  {
  public:
    Button(u8 index, const WORD& buttons) : m_buttons(buttons), m_index(index) {}
    std::string GetName() const override;
    ControlState GetState() const override;

  private:
    const WORD& m_buttons;
    u8 m_index;
  };

  class Axis : public Core::Device::Input
  {
  public:
    Axis(u8 index, const SHORT& axis, SHORT range) : m_axis(axis), m_range(range), m_index(index) {}
    std::string GetName() const override;
    ControlState GetState() const override;

  private:
    const SHORT& m_axis;
    const SHORT m_range;
    const u8 m_index;
  };

  class Trigger : public Core::Device::Input
  {
  public:
    Trigger(u8 index, const BYTE& trigger, BYTE range)
        : m_trigger(trigger), m_range(range), m_index(index)
    {
    }
    std::string GetName() const override;
    ControlState GetState() const override;

  private:
    const BYTE& m_trigger;
    const BYTE m_range;
    const u8 m_index;
  };

  class Motor : public Core::Device::Output
  {
  public:
    Motor(u8 index, Device* parent, WORD& motor, WORD range)
        : m_motor(motor), m_range(range), m_index(index), m_parent(parent)
    {
    }
    std::string GetName() const override;
    void SetState(ControlState state) override;

  private:
    WORD& m_motor;
    const WORD m_range;
    const u8 m_index;
    Device* m_parent;
  };

public:
  void UpdateInput() override;

  Device(const XINPUT_CAPABILITIES& capabilities, u8 index);

  std::string GetName() const final override;
  std::string GetSource() const final override;
  std::optional<int> GetPreferredId() const final override;

  void UpdateMotors();

private:
  XINPUT_STATE m_state_in;
  XINPUT_VIBRATION m_state_out{};
  XINPUT_VIBRATION m_current_state_out{};
  const BYTE m_subtype;
  const u8 m_index;
};
}  // namespace ciface::XInput
