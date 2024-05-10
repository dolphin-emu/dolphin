// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>
#include <vector>

#include "Common/CommonTypes.h"
#include "Core/IOS/USB/Common.h"
#include "Core/IOS/USB/Emulated/Microphone.h"
#include "Core/System.h"

namespace IOS::HLE::USB
{
class WiiSpeak final : public Device
{
public:
  WiiSpeak(EmulationKernel& ios);
  ~WiiSpeak();

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

  WSState m_sampler{};

  enum Registers
  {
    SAMPLER_STATE = 0,
    SAMPLER_MUTE = 0x0c,

    SAMPLER_FREQ = 2,
    FREQ_8KHZ = 0,
    FREQ_11KHZ = 1,
    FREQ_RESERVED = 2,
    FREQ_16KHZ = 3,  // default

    SAMPLER_GAIN = 4,
    GAIN_00dB = 0,
    GAIN_15dB = 1,
    GAIN_30dB = 2,
    GAIN_36dB = 3,  // default

    EC_STATE = 0x14,

    SP_STATE = 0x38,
    SP_ENABLE = 0x1010,
    SP_SIN = 0x2001,
    SP_SOUT = 0x2004,
    SP_RIN = 0x200d
  };

  void GetRegister(const std::unique_ptr<CtrlMessage>& cmd) const;
  void SetRegister(const std::unique_ptr<CtrlMessage>& cmd);
  bool IsMicrophoneConnected() const;

  EmulationKernel& m_ios;
  const u16 m_vid = 0x057E;
  const u16 m_pid = 0x0308;
  u8 m_active_interface = 0;
  bool m_device_attached = false;
  bool init = false;
  std::unique_ptr<Microphone> m_microphone{};
  const DeviceDescriptor m_device_descriptor{0x12,  0x1,    0x200,  0,   0,   0,   0x10,
                                             0x57E, 0x0308, 0x0214, 0x1, 0x2, 0x0, 0x1};
  const std::vector<ConfigDescriptor> m_config_descriptor{
      {0x9, 0x2, 0x0030, 0x1, 0x1, 0x0, 0x80, 0x32}};
  const std::vector<InterfaceDescriptor> m_interface_descriptor{
      {0x9, 0x4, 0x0, 0x0, 0x0, 0xFF, 0xFF, 0xFF, 0x0},
      {0x9, 0x4, 0x0, 0x01, 0x03, 0xFF, 0xFF, 0xFF, 0x0}};
  static constexpr u8 ENDPOINT_AUDIO_IN = 0x81;
  static constexpr u8 ENDPOINT_AUDIO_OUT = 0x3;
  static constexpr u8 ENDPOINT_DATA_OUT = 0x2;
  const std::vector<EndpointDescriptor> m_endpoint_descriptor{
      {0x7, 0x5, ENDPOINT_AUDIO_IN, 0x1, 0x0020, 0x1},
      {0x7, 0x5, ENDPOINT_DATA_OUT, 0x2, 0x0020, 0},
      {0x7, 0x5, ENDPOINT_AUDIO_OUT, 0x1, 0x0040, 0x1}};
};
}  // namespace IOS::HLE::USB
