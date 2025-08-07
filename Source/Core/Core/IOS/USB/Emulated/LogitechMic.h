// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

// As I've already stated, I copied the Wii Speak emulation code and I tried to modify it to fit the Logitech USB Microphone

#pragma once

#include <atomic>
#include <memory>
#include <vector>

#include "Common/CommonTypes.h"
#include "Core/IOS/USB/Common.h"
#include "Core/IOS/USB/Emulated/Microphone-Logitech.h"

namespace IOS::HLE::USB
{
struct LogitechMicState
{
  // Use atomic for members concurrently used by the data callback
  std::atomic<bool> sample_on;
  std::atomic<bool> mute;
  int freq;
  int gain;
  bool ec_reset;
  bool sp_on;

  static constexpr u32 DEFAULT_SAMPLING_RATE = 22050;
};

class LogitechMic final : public Device
{
public:
  LogitechMic();
  ~LogitechMic() override;

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
  LogitechMicState m_sampler{};

  enum Registers
  {
    SAMPLER_STATE = 0,
    SAMPLER_MUTE = 0x0c,

    SAMPLER_FREQ = 2,
    FREQ_8KHZ = 0,
    FREQ_11KHZ = 1,
    FREQ_RESERVED = 2,
    FREQ_22KHZ = 3,  // default

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

  const u16 m_vid = 0x046D;
  const u16 m_pid = 0x0A03;
  u8 m_active_interface = 0;
  bool m_device_attached = false;
  bool init = false;
  std::unique_ptr<MicrophoneLogitech> m_microphone{};
  
  // These are the descriptors I'm not too sure I got right
  const DeviceDescriptor m_device_descriptor{0x12,  0x1,    0x200,  0,   0,   0,   0x08,
                                             0x46D, 0x0A03, 0x0102, 0x1, 0x2, 0x0, 0x1};
  const std::vector<ConfigDescriptor> m_config_descriptor{
      {0x09, 0x02, 0x0079, 0x02, 0x01, 0x03, 0x80, 0x60}};
  const std::vector<InterfaceDescriptor> m_interface_descriptor{
      {0x09, 0x04, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00},
      {0x09, 0x04, 0x01, 0x00, 0x00, 0x01, 0x02, 0x00, 0x00},
      {0x09, 0x04, 0x01, 0x01, 0x01, 0x01, 0x02, 0x00, 0x00}
  };
  static constexpr u8 ENDPOINT_AUDIO_IN = 0x81;
  const std::vector<EndpointDescriptor> m_endpoint_descriptor{
      {0x07, 0x05, ENDPOINT_AUDIO_IN, 0x01, 0x0040, 0x01}
  };
};
}  // namespace IOS::HLE::USB

