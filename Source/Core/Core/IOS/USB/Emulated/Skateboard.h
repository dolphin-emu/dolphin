// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/CommonTypes.h"
#include "Core/IOS/USB/Common.h"
#include "InputCommon/ControllerEmu/ControlGroup/Buttons.h"
#include "InputCommon/ControllerEmu/ControlGroup/IMUAccelerometer.h"
#include "InputCommon/ControllerEmu/ControlGroup/Slider.h"
#include "InputCommon/ControllerEmu/ControllerEmu.h"

namespace IOS::HLE::USB
{

#pragma pack(1)
struct SkateboardHidReport
{
  // The default values here are what the dongle sends when no skateboard is connected.
  u8 m_buttons[3] = {0, 0, 0b1111};
  u8 m_80_80_80_80[4] = {0x80, 0x80, 0x80, 0x80};
  u8 m_00_00_00_00[4] = {0, 0, 0, 0};
  u8 m_lidar[4] = {0, 0, 0, 0};  // IR sensors, range: 0x00 (LED off) - 0x38 (close object)
  // The x/y/z axis naming here matches the BMA020 datasheet.
  s8 m_accel0_z = 0;
  s8 m_accel1_z = 0;
  u8 m_00_00[2] = {0, 0};
  // signed 10-bit little-endian, these are encrypted
  u16 m_accel0_x = 0x200;
  u16 m_accel0_y = 0x200;
  u16 m_accel1_x = 0x200;
  u16 m_accel1_y = 0x200;

  bool operator==(const SkateboardHidReport& other) const = default;
  static std::pair<u32, u32> ReorderNibbles(u32 a, u32 b);
  void Encrypt();
  void Decrypt();
  void Dump(const char* prefix = "") const;

  static const u32 sbox[2][16];
};
#pragma pack()
static_assert(sizeof(SkateboardHidReport) == 27);

class SkateboardController : public ControllerEmu::EmulatedController
{
public:
  SkateboardController();
  std::string GetName() const override;
  SkateboardHidReport GetState() const;
  InputConfig* GetConfig() const override;

private:
  ControllerEmu::Buttons* m_buttons[3];
  ControllerEmu::IMUAccelerometer* m_accel[2];
  ControllerEmu::Slider* m_slider[4];
};

class SkateboardUSB final : public Device
{
public:
  explicit SkateboardUSB() = default;
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
  SkateboardController m_inputs;

  static DeviceDescriptor s_device_descriptor;
  static ConfigDescriptor s_config_descriptor;
  static InterfaceDescriptor s_interface_descriptor;
  static std::vector<EndpointDescriptor> s_endpoint_descriptors;
};

}  // namespace IOS::HLE::USB
