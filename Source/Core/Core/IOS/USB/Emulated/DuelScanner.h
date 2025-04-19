// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <vector>
#include "Core/IOS/USB/Common.h"
#include "Core/IOS/USB/Emulated/CameraBase.h"

namespace IOS::HLE::USB
{

class DuelScanner final : public Device
{
public:
  DuelScanner(EmulationKernel& ios);
  ~DuelScanner() override;
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
  void ScheduleTransfer(std::unique_ptr<TransferCommand> command, const std::vector<u8>& data,
                        u64 expected_time_us);

  static DeviceDescriptor s_device_descriptor;
  static std::vector<ConfigDescriptor> s_config_descriptor;
  static std::vector<InterfaceDescriptor> s_interface_descriptor;
  static std::vector<EndpointDescriptor> s_endpoint_descriptor;

  EmulationKernel& m_ios;
  const u16 m_vid = 0x057e;
  const u16 m_pid = 0x030d;
  u8 m_active_interface = 0;
  u8 m_active_altsetting = 0;
  const struct UVCImageSize m_supported_sizes[5] = {{640, 480}, {320, 240}, {160, 120}, {176, 144}, {352, 288}};
  struct UVCImageSize m_active_size;
  u32 m_delay = 0;
  u32 m_image_size = 0;
  u32 m_image_pos = 0;
  u8 *m_image_data = nullptr;
  bool m_frame_id = 0;
};
}  // namespace IOS::HLE::USB
