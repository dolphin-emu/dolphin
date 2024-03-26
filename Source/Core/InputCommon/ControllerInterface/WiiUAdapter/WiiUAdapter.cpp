// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "InputCommon/ControllerInterface/WiiUAdapter/WiiUAdapter.h"

#include <array>

#include "Core/HW/SI/SI.h"
#include "InputCommon/ControllerInterface/ControllerInterface.h"
#include "InputCommon/GCAdapter.h"
#include "InputCommon/GCPadStatus.h"

// TODO: name?
namespace ciface::WiiUAdapter
{
// TODO: name?
constexpr auto SOURCE_NAME = "WiiUAdapter";

class GCController;

class InputBackend final : public ciface::InputBackend
{
public:
  InputBackend(ControllerInterface* controller_interface);
  void PopulateDevices() override;
  void UpdateInput(std::vector<std::weak_ptr<ciface::Core::Device>>& devices_to_remove) override;

private:
  std::array<std::weak_ptr<GCController>, SerialInterface::MAX_SI_CHANNELS> m_devices;
};

std::unique_ptr<ciface::InputBackend> CreateInputBackend(ControllerInterface* controller_interface)
{
  return std::make_unique<InputBackend>(controller_interface);
}

struct DigitalInputProps
{
  u16 value;
  const char* name;
};

constexpr DigitalInputProps digital_inputs[] = {
    {PAD_BUTTON_UP, "Up"},
    {PAD_BUTTON_DOWN, "Down"},
    {PAD_BUTTON_LEFT, "Left"},
    {PAD_BUTTON_RIGHT, "Right"},
    {PAD_BUTTON_A, "A"},
    {PAD_BUTTON_B, "B"},
    {PAD_BUTTON_X, "X"},
    {PAD_BUTTON_Y, "Y"},
    {PAD_TRIGGER_Z, "Z"},
    {PAD_TRIGGER_L, "L"},
    {PAD_TRIGGER_R, "R"},
    // TODO: "START" or "Start"?
    {PAD_BUTTON_START, "Start"},
};

struct AnalogInputProps
{
  u8 GCPadStatus::*ptr;
  const char* name;
};

constexpr AnalogInputProps stick_inputs[] = {
    {&GCPadStatus::stickX, "Main X"},
    {&GCPadStatus::stickY, "Main Y"},
    {&GCPadStatus::substickX, "C X"},
    {&GCPadStatus::substickY, "C Y"},
};

constexpr AnalogInputProps trigger_inputs[] = {
    {&GCPadStatus::triggerLeft, "L-Analog"},
    {&GCPadStatus::triggerRight, "R-Analog"},
};

class DigitalInput : public Core::Device::Input
{
public:
  DigitalInput(const u16* button_state, u32 index) : m_button_state{*button_state}, m_index{index}
  {
  }

  std::string GetName() const override { return digital_inputs[m_index].name; }

  ControlState GetState() const override
  {
    return (digital_inputs[m_index].value & m_button_state) != 0;
  }

private:
  const u16& m_button_state;
  const u32 m_index;
};

class AnalogInput : public Core::Device::Input
{
public:
  AnalogInput(const GCPadStatus* pad_status, const AnalogInputProps& props, u8 neutral_value,
              s32 range)
      : m_base_name{props.name}, m_neutral_value{neutral_value}, m_range{range},
        m_state{pad_status->*props.ptr}
  {
  }

  ControlState GetState() const override
  {
    return ControlState(m_state - m_neutral_value) / m_range;
  }

protected:
  const char* const m_base_name;
  const u8 m_neutral_value;
  const s32 m_range;

private:
  const u8& m_state;
};

class StickInput : public AnalogInput
{
public:
  using AnalogInput::AnalogInput;

  std::string GetName() const override
  {
    return std::string(m_base_name) + (m_range > 0 ? '+' : '-');
  }
};

class TriggerInput : public AnalogInput
{
public:
  using AnalogInput::AnalogInput;

  std::string GetName() const override { return m_base_name; }
};

class Motor : public Core::Device::Output
{
public:
  Motor(u32 index) : m_index{index} {}

  void SetState(ControlState value) override
  {
    const bool new_state = std::lround(value);

    if (new_state == m_last_state)
      return;

    m_last_state = new_state;

    GCAdapter::Output(m_index, new_state);
  }

  std::string GetName() const override { return "Motor"; }

private:
  u32 m_index;
  bool m_last_state = false;
};

class GCController : public Core::Device
{
public:
  GCController(u32 index) : m_index{index}
  {
    // Add buttons.
    for (u32 i = 0; i != std::size(digital_inputs); ++i)
      AddInput(new DigitalInput{&m_pad_status.button, i});

    // TODO: use origin values.
    const auto origin = GCAdapter::GetOrigin(index);

    // Add sticks.
    for (auto& props : stick_inputs)
    {
      // Add separate -/+ inputs.
      AddInput(new StickInput{&m_pad_status, props, GCPadStatus::MAIN_STICK_CENTER_X,
                              -GCPadStatus::MAIN_STICK_RADIUS});
      AddInput(new StickInput{&m_pad_status, props, GCPadStatus::MAIN_STICK_CENTER_X,
                              GCPadStatus::MAIN_STICK_RADIUS});
    }

    // Add triggers.
    for (auto& props : trigger_inputs)
      AddInput(new TriggerInput{&m_pad_status, props, 0, std::numeric_limits<u8>::max()});

    // Rumble.
    AddOutput(new Motor{index});
  }

  std::optional<int> GetPreferredId() const override { return m_index; }

  std::string GetName() const override
  {
    // TODO: name?
    return "GCPad";
  }

  std::string GetSource() const override { return SOURCE_NAME; }

  int GetSortPriority() const override { return -3; }

  Core::DeviceRemoval UpdateInput() override
  {
    m_pad_status = GCAdapter::PeekInput(m_index);
    return Core::DeviceRemoval::Keep;
  }

private:
  GCPadStatus m_pad_status;
  const u32 m_index;
};

InputBackend::InputBackend(ControllerInterface* controller_interface)
    : ciface::InputBackend(controller_interface)
{
  GCAdapter::Init();
}

void InputBackend::PopulateDevices()
{
  for (int i = 0; i != SerialInterface::MAX_SI_CHANNELS; ++i)
  {
    if (GCAdapter::DeviceConnected(i))
    {
      auto new_device = std::make_shared<GCController>(i);
      m_devices[i] = new_device;
      GetControllerInterface().AddDevice(std::move(new_device));
    }
  }
}

void InputBackend::UpdateInput(std::vector<std::weak_ptr<ciface::Core::Device>>& devices_to_remove)
{
  // "Hotplug" is handled here.
  for (int i = 0; i != SerialInterface::MAX_SI_CHANNELS; ++i)
  {
    const bool is_device_connected = GCAdapter::DeviceConnected(i);
    const auto device = m_devices[i].lock();

    if (is_device_connected == (device != nullptr))
      continue;

    if (is_device_connected)
    {
      auto new_device = std::make_shared<GCController>(i);
      m_devices[i] = new_device;
      GetControllerInterface().AddDevice(std::move(new_device));
    }
    else
    {
      devices_to_remove.emplace_back(device);
    }
  }
}

}  // namespace ciface::WiiUAdapter
