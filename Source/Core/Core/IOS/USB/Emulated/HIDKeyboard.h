// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <map>
#include <memory>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/Keyboard.h"
#include "Core/IOS/USB/Common.h"

namespace IOS::HLE::USB
{
class HIDKeyboard final : public Device
{
public:
  HIDKeyboard();
  ~HIDKeyboard();

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
  HIDProtocol m_current_protocol = HIDProtocol::Report;
  Common::HIDPressedState m_last_state;
  std::shared_ptr<Common::KeyboardContext> m_keyboard_context;

  // Apple Extended Keyboard [Mitsumi]
  // - Model A1058 / USB 1.1
  const u16 m_vid = 0x05ac;
  const u16 m_pid = 0x020c;
  u8 m_active_interface = 0;
  bool m_device_attached = false;
  const DeviceDescriptor m_device_descriptor{0x12,   0x01,   0x0110, 0x00, 0x00, 0x00, 0x08,
                                             0x05AC, 0x020C, 0x0395, 0x01, 0x03, 0x00, 0x01};
  const std::vector<ConfigDescriptor> m_config_descriptor{
      {0x09, 0x02, 0x003B, 0x02, 0x01, 0x00, 0xA0, 0x19}};
  static constexpr u8 INTERFACE_0 = 0;
  static constexpr u8 INTERFACE_1 = 1;
  const std::vector<InterfaceDescriptor> m_interface_descriptor{
      {0x09, 0x04, INTERFACE_0, 0x00, 0x01, 0x03, 0x01, 0x01, 0x00},
      {0x09, 0x04, INTERFACE_1, 0x00, 0x01, 0x03, 0x00, 0x00, 0x00}};
  const std::map<u8, std::vector<EndpointDescriptor>> m_endpoint_descriptor{
      {INTERFACE_0, std::vector<EndpointDescriptor>{{0x07, 0x05, 0x81, 0x03, 0x0008, 0x0A}}},
      {INTERFACE_1, std::vector<EndpointDescriptor>{{0x07, 0x05, 0x82, 0x03, 0x0004, 0x0A}}},
  };
};
}  // namespace IOS::HLE::USB
