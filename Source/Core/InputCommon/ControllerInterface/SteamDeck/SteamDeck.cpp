// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "InputCommon/ControllerInterface/SteamDeck/SteamDeck.h"

#include <type_traits>

#include <hidapi.h>

#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/MathUtil.h"
#include "InputCommon/ControllerInterface/ControllerInterface.h"

namespace ciface::SteamDeck
{
constexpr std::string_view STEAMDECK_SOURCE_NAME = "SteamDeck";

struct DeckInputReport
{
  // +0
  u8 major_ver;
  u8 minor_ver;
  u8 report_type;
  u8 report_sz;

  u32 frame;

  // +8
  u32 buttons0;
  u32 buttons1;

  // +16
  s16 l_pad_x;
  s16 l_pad_y;
  s16 r_pad_x;
  s16 r_pad_y;

  // +24
  s16 accel_x;
  s16 accel_y;
  s16 accel_z;
  s16 gyro_pitch;
  s16 gyro_roll;
  s16 gyro_yaw;
  s16 pose_quat_w;
  s16 pose_quat_x;
  s16 pose_quat_y;
  s16 pose_quat_z;

  // +44
  u16 l_trig;
  u16 r_trig;

  // +48
  s16 l_stick_x;
  s16 l_stick_y;
  s16 r_stick_x;
  s16 r_stick_y;

  // +56
  u16 l_pad_force;
  u16 r_pad_force;
  s16 l_stick_capa;
  s16 r_stick_capa;
};
static_assert(sizeof(DeckInputReport) == 64);

class Device final : public Core::Device
{
private:
  class Button final : public Input
  {
  public:
    Button(const char* name, const u32& buttons, u32 mask)
        : m_name(name), m_buttons(buttons), m_mask(mask)
    {
    }
    std::string GetName() const override { return m_name; }
    ControlState GetState() const override { return (m_buttons & m_mask) != 0; }

  private:
    const char* const m_name;
    const u32& m_buttons;
    const u32 m_mask;
  };

  template <class T>
  class AnalogInput : public Input
  {
  public:
    AnalogInput(const char* name, const T& input, ControlState range)
        : m_name(name), m_input(input), m_range(range)
    {
    }
    std::string GetName() const final override { return m_name; }
    ControlState GetState() const final override { return ControlState(m_input) / m_range; }

  private:
    const char* m_name;
    const T& m_input;
    const ControlState m_range;
  };

  class MotionInput final : public AnalogInput<s16>
  {
  public:
    using AnalogInput::AnalogInput;
    bool IsDetectable() const override { return false; }
  };

public:
  Device(hid_device* device);
  std::string GetName() const final override;
  std::string GetSource() const final override;
  Core::DeviceRemoval UpdateInput() override;

private:
  hid_device* m_device;
  DeckInputReport m_latest_input;
  int m_gyro_reenable = 0;
};

class InputBackend final : public ciface::InputBackend
{
public:
  InputBackend(ControllerInterface* controller_interface);
  void PopulateDevices() override;
};

std::unique_ptr<ciface::InputBackend> CreateInputBackend(ControllerInterface* controller_interface)
{
  return std::make_unique<InputBackend>(controller_interface);
}

InputBackend::InputBackend(ControllerInterface* controller_interface)
    : ciface::InputBackend(controller_interface)
{
}

void InputBackend::PopulateDevices()
{
  INFO_LOG_FMT(CONTROLLERINTERFACE, "SteamDeck PopulateDevices");

  auto possible_devices = hid_enumerate(0x28de, 0x1205);
  bool was_found = false;
  std::string found_path;
  auto this_device = possible_devices;

  while (this_device)
  {
    INFO_LOG_FMT(CONTROLLERINTERFACE, "Found {} (interface {})", this_device->path,
                 this_device->interface_number);

    if (this_device->interface_number == 2)
    {
      was_found = true;
      found_path = this_device->path;
      break;
    }

    this_device = this_device->next;
  }

  hid_free_enumeration(possible_devices);

  if (was_found)
  {
    INFO_LOG_FMT(CONTROLLERINTERFACE, "Found Steam Deck controls at {}", found_path);
  }
  else
  {
    INFO_LOG_FMT(CONTROLLERINTERFACE, "No Steam Deck interface found");
    return;
  }

  auto deck_dev = hid_open_path(found_path.c_str());
  if (!deck_dev)
  {
    ERROR_LOG_FMT(CONTROLLERINTERFACE, "Steam Deck controls could not be opened");
    return;
  }
  if (hid_set_nonblocking(deck_dev, 1) != 0)
  {
    ERROR_LOG_FMT(CONTROLLERINTERFACE, "Steam Deck controls could not be set nonblocking");
    return;
  }

  GetControllerInterface().RemoveDevice(
      [](const auto* dev) { return dev->GetSource() == STEAMDECK_SOURCE_NAME; });

  GetControllerInterface().AddDevice(std::make_shared<Device>(deck_dev));
}

Device::Device(hid_device* device) : m_device{device}
{
  // clang-format off
  AddInput(new Button("R2 Full Pull",           m_latest_input.buttons0, 0x00000001));
  AddInput(new Button("L2 Full Pull",           m_latest_input.buttons0, 0x00000002));
  AddInput(new Button("R1",                     m_latest_input.buttons0, 0x00000004));
  AddInput(new Button("L1",                     m_latest_input.buttons0, 0x00000008));
  AddInput(new Button("Y",                      m_latest_input.buttons0, 0x00000010));
  AddInput(new Button("B",                      m_latest_input.buttons0, 0x00000020));
  AddInput(new Button("X",                      m_latest_input.buttons0, 0x00000040));
  AddInput(new Button("A",                      m_latest_input.buttons0, 0x00000080));
  AddInput(new Button("D-Pad Up",               m_latest_input.buttons0, 0x00000100));
  AddInput(new Button("D-Pad Right",            m_latest_input.buttons0, 0x00000200));
  AddInput(new Button("D-Pad Left",             m_latest_input.buttons0, 0x00000400));
  AddInput(new Button("D-Pad Down",             m_latest_input.buttons0, 0x00000800));
  AddInput(new Button("View",                   m_latest_input.buttons0, 0x00001000));
  AddInput(new Button("Steam",                  m_latest_input.buttons0, 0x00002000));
  AddInput(new Button("Menu",                   m_latest_input.buttons0, 0x00004000));
  AddInput(new Button("L5",                     m_latest_input.buttons0, 0x00008000));
  AddInput(new Button("R5",                     m_latest_input.buttons0, 0x00010000));
  AddInput(new Button("Left Trackpad Click",    m_latest_input.buttons0, 0x00020000));
  AddInput(new Button("Right Trackpad Click",   m_latest_input.buttons0, 0x00040000));
  AddInput(new Button("Left Trackpad Touch",    m_latest_input.buttons0, 0x00080000));
  AddInput(new Button("Right Trackpad Touch",   m_latest_input.buttons0, 0x00100000));
  AddInput(new Button("L3",                     m_latest_input.buttons0, 0x00400000));
  AddInput(new Button("R3",                     m_latest_input.buttons0, 0x04000000));
  AddInput(new Button("L4",                     m_latest_input.buttons1, 0x00000200));
  AddInput(new Button("R4",                     m_latest_input.buttons1, 0x00000400));
  AddInput(new Button("Left Stick Touch",       m_latest_input.buttons1, 0x00004000));
  AddInput(new Button("Right Stick Touch",      m_latest_input.buttons1, 0x00008000));
  AddInput(new Button("Quick Access",           m_latest_input.buttons1, 0x00040000));

  AddInput(new AnalogInput("L2",                      m_latest_input.l_trig,       32767));
  AddInput(new AnalogInput("R2",                      m_latest_input.r_trig,       32767));
  AddInput(new AnalogInput("Left Stick X-",           m_latest_input.l_stick_x,    -32767));
  AddInput(new AnalogInput("Left Stick X+",           m_latest_input.l_stick_x,    32767));
  AddInput(new AnalogInput("Left Stick Y-",           m_latest_input.l_stick_y,    -32767));
  AddInput(new AnalogInput("Left Stick Y+",           m_latest_input.l_stick_y,    32767));
  AddInput(new AnalogInput("Right Stick X-",          m_latest_input.r_stick_x,    -32767));
  AddInput(new AnalogInput("Right Stick X+",          m_latest_input.r_stick_x,    32767));
  AddInput(new AnalogInput("Right Stick Y-",          m_latest_input.r_stick_y,    -32767));
  AddInput(new AnalogInput("Right Stick Y+",          m_latest_input.r_stick_y,    32767));
  AddInput(new AnalogInput("Left Trackpad X-",        m_latest_input.l_pad_x,      -32767));
  AddInput(new AnalogInput("Left Trackpad X+",        m_latest_input.l_pad_x,      32767));
  AddInput(new AnalogInput("Left Trackpad Y-",        m_latest_input.l_pad_y,      -32767));
  AddInput(new AnalogInput("Left Trackpad Y+",        m_latest_input.l_pad_y,      32767));
  AddInput(new AnalogInput("Right Trackpad X-",       m_latest_input.r_pad_x,      -32767));
  AddInput(new AnalogInput("Right Trackpad X+",       m_latest_input.r_pad_x,      32767));
  AddInput(new AnalogInput("Right Trackpad Y-",       m_latest_input.r_pad_y,      -32767));
  AddInput(new AnalogInput("Right Trackpad Y+",       m_latest_input.r_pad_y,      32767));
  AddInput(new AnalogInput("Left Trackpad Pressure",  m_latest_input.l_pad_force,  32767));
  AddInput(new AnalogInput("Right Trackpad Pressure", m_latest_input.r_pad_force,  32767));
  AddInput(new AnalogInput("Left Stick Capacitance",  m_latest_input.l_stick_capa, 32767));
  AddInput(new AnalogInput("Right Stick Capacitance", m_latest_input.r_stick_capa, 32767));

  // 0x4000 LSBs = 1 g
  // final output in m/s^2
  constexpr auto accel_scale = 16384.0 / MathUtil::GRAVITY_ACCELERATION;
  AddInput(new MotionInput("Accel Up",        m_latest_input.accel_z, -accel_scale));
  AddInput(new MotionInput("Accel Down",      m_latest_input.accel_z, accel_scale));
  AddInput(new MotionInput("Accel Left",      m_latest_input.accel_x, accel_scale));
  AddInput(new MotionInput("Accel Right",     m_latest_input.accel_x, -accel_scale));
  AddInput(new MotionInput("Accel Forward",   m_latest_input.accel_y, -accel_scale));
  AddInput(new MotionInput("Accel Backward",  m_latest_input.accel_y, accel_scale));

  // 16.384 (?) LSBs = 1 deg / s
  // final output in rads / s
  constexpr auto gyro_scale = 16.384 * 360.0 / MathUtil::TAU;
  AddInput(new MotionInput("Gyro Pitch Up",   m_latest_input.gyro_pitch, gyro_scale));
  AddInput(new MotionInput("Gyro Pitch Down", m_latest_input.gyro_pitch, -gyro_scale));
  AddInput(new MotionInput("Gyro Roll Left",  m_latest_input.gyro_roll,  -gyro_scale));
  AddInput(new MotionInput("Gyro Roll Right", m_latest_input.gyro_roll,  gyro_scale));
  AddInput(new MotionInput("Gyro Yaw Left",   m_latest_input.gyro_yaw,   gyro_scale));
  AddInput(new MotionInput("Gyro Yaw Right",  m_latest_input.gyro_yaw,   -gyro_scale));
  // clang-format on
}

std::string Device::GetName() const
{
  return "Steam Deck";
}

std::string Device::GetSource() const
{
  return std::string(STEAMDECK_SOURCE_NAME);
}

Core::DeviceRemoval Device::UpdateInput()
{
  // As of a certain mid-2023 update to the Steam client,
  // Steam will disable gyro data if gyro is not mapped in Steam Input.
  // This disabling will happen every time the Steam overlay is closed.
  // This command turns gyro data back on periodically.
  if (++m_gyro_reenable == 250)
  {
    m_gyro_reenable = 0;
    // Using names from Valve's contribution to SDL this packet decodes as:
    // 0x00 = report ID
    // 0x87 = ID_SET_SETTINGS_VALUES
    // 0x03 = payload length
    // 0x30 = SETTING_IMU_MODE
    // 0x18 0x00 = SETTING_GYRO_MODE_SEND_RAW_ACCEL | SETTING_GYRO_MODE_SEND_RAW_GYRO
    const unsigned char pkt[65] = {0x00, 0x87, 0x03, 0x30, 0x18, 0x00};
    hid_send_feature_report(m_device, pkt, sizeof(pkt));
  }

  DeckInputReport rpt;
  bool got_anything = false;
  // Read all available input reports (processing only the most recent one).
  while (hid_read(m_device, reinterpret_cast<u8*>(&rpt), sizeof(rpt)) > 0)
  {
    got_anything = true;
  }
  // In case there were no reports available to be read, bail early.
  if (!got_anything)
    return Core::DeviceRemoval::Keep;

  if (rpt.major_ver != 0x01 || rpt.minor_ver != 0x00 || rpt.report_type != 0x09 ||
      rpt.report_sz != sizeof(rpt))
  {
    ERROR_LOG_FMT(CONTROLLERINTERFACE, "Steam Deck bad report");
    return Core::DeviceRemoval::Keep;
  }

  m_latest_input = rpt;

  return Core::DeviceRemoval::Keep;
}

}  // namespace ciface::SteamDeck
