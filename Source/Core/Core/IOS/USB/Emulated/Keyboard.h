// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <memory>
#include <span>
#include <vector>

#include "Common/CommonTypes.h"
#include "Core/IOS/USB/Common.h"
#include "Core/System.h"

namespace IOS::HLE::USB
{
using HIDPressedKeys = std::array<u8, 6>;

#pragma pack(push, 1)
struct HIDKeyboardReport
{
  u8 modifiers = 0;
  u8 oem = 0;
  HIDPressedKeys pressed_keys{};
};
#pragma pack(pop)

enum class HIDProtocol : u16
{
  Boot = 0,
  Report = 1,
};

class Keyboard final : public Device
{
public:
  Keyboard(EmulationKernel& ios);
  ~Keyboard();

  DeviceDescriptor GetDeviceDescriptor() const override;
  std::vector<ConfigDescriptor> GetConfigurations() const override;
  std::vector<InterfaceDescriptor> GetInterfaces(u8 config) const override;
  std::vector<EndpointDescriptor> GetEndpoints(u8 config, u8 interface, u8 alt) const override;
  bool Attach() override;
  bool AttachAndChangeInterface(u8 interface) override;
  int CancelTransfer(u8 endpoint) override;
  int ChangeInterface(u8 interface) override;
  int GetNumberOfAltSettings(u8 interface) override;
  int SetAltSetting(u8 alt_setting) override;
  int SubmitTransfer(std::unique_ptr<CtrlMessage> message) override;
  int SubmitTransfer(std::unique_ptr<BulkMessage> message) override;
  int SubmitTransfer(std::unique_ptr<IntrMessage> message) override;
  int SubmitTransfer(std::unique_ptr<IsoMessage> message) override;

private:
  void ScheduleTransfer(std::unique_ptr<TransferCommand> command, std::span<const u8> data,
                        u64 expected_time_us);
  bool IsKeyPressed(int virtual_key) const;
  HIDPressedKeys PollHIDPressedKeys();
  u8 PollHIDModifiers();
  HIDKeyboardReport PollInputs();

  EmulationKernel& m_ios;
  HIDKeyboardReport m_last_report;
  HIDProtocol m_current_protocol = HIDProtocol::Report;

  // Apple Extended Keyboard [Mitsumi]
  // - Model A1058 / USB 1.1
  const u16 m_vid = 0x05ac;
  const u16 m_pid = 0x020c;
  u8 m_active_interface = 0;
  bool m_device_attached = false;
  bool init = false;
  const DeviceDescriptor m_device_descriptor{0x12,   0x01,   0x0110, 0x00, 0x00, 0x00, 0x08,
                                             0x05AC, 0x020C, 0x0395, 0x01, 0x03, 0x00, 0x01};
  // Uncommenting these lines crashes MH3
  // Note to self: this keyboard was working on real hardware, though :v/
  const std::vector<ConfigDescriptor> m_config_descriptor{
      {0x09, 0x02, 0x003B, /*0x02*/ 0x01, 0x01, 0x00, 0xA0}};
  const std::vector<InterfaceDescriptor> m_interface_descriptor{
      {0x09, 0x04, 0x00, 0x00, 0x01, 0x03, 0x01, 0x01, 0x00},
      // {0x09, 0x04, 0x01, 0x00, 0x01, 0x03, 0x00, 0x00, 0x00}
  };
  const std::vector<EndpointDescriptor> m_endpoint_descriptor{
      {0x07, 0x05, 0x81, 0x03, 0x0008, 0x0A},
      // {0x07, 0x05, 0x82, 0x03, 0x0004, 0x0A}
  };
};
}  // namespace IOS::HLE::USB
