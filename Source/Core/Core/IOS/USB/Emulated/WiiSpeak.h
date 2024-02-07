// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <thread>

#include "Common/CommonTypes.h"
#include "Common/Event.h"
#include "Common/WorkQueueThread.h"
#include "Core/HW/Memmap.h"
#include "Core/IOS/USB/Common.h"
#include "Core/IOS/USB/Emulated/Microphone.h"
#include "Core/System.h"

namespace IOS::HLE::USB
{
class WiiSpeak final : public Device
{
public:
  WiiSpeak(EmulationKernel& ios, const std::string& device_name);
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
  struct WSState
  {
    bool sample_on;
    bool mute;
    int freq;
    int gain;
    bool ec_reset;
    bool sp_on;
  };

  WSState sampler{};

  enum Registers
  {
    SAMPLER_STATE = 0,
    SAMPLER_MUTE = 0xc0,

    SAMPLER_FREQ = 2,
    FREQ_8KHZ = 0,
    FREQ_11KHZ = 1,
    FREQ_RESERVED = 2,
    FREQ_16KHZ = 3,

    SAMPLER_GAIN = 4,
    GAIN_00dB = 0,
    GAIN_15dB = 1,
    GAIN_30dB = 2,
    GAIN_36dB = 3,

    EC_STATE = 0x14,

    SP_STATE = 0x38,
    SP_ENABLE = 0x1010,
    SP_SIN = 0x2001,
    SP_SOUT = 0x2004,
    SP_RIN = 0x200d
  };

  void GetRegister(std::unique_ptr<CtrlMessage>& cmd);
  void SetRegister(std::unique_ptr<CtrlMessage>& cmd);

  EmulationKernel& m_ios;
  u16 m_vid = 0;
  u16 m_pid = 0;
  u8 m_active_interface = 0;
  bool m_device_attached = false;
  bool init = false;
  std::unique_ptr<Microphone> m_microphone;
  DeviceDescriptor m_device_descriptor{};
  std::vector<ConfigDescriptor> m_config_descriptor;
  std::vector<InterfaceDescriptor> m_interface_descriptor;
  std::vector<EndpointDescriptor> m_endpoint_descriptor;
  std::mutex m_mutex;
  Common::Event m_shutdown_event;
};
}  // namespace IOS::HLE::USB
