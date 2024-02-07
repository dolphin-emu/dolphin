// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/IOS/USB/Emulated/WiiSpeak.h"

#include "Core/HW/Memmap.h"


namespace IOS::HLE::USB
{
WiiSpeak::WiiSpeak(IOS::HLE::EmulationKernel& ios, const std::string& device_name) : m_ios(ios)
{
  m_vid = 0x57E;
  m_pid = 0x0308;
  m_id = (u64(m_vid) << 32 | u64(m_pid) << 16 | u64(9) << 8 | u64(1));
  m_device_descriptor =
      DeviceDescriptor{0x12, 0x1, 0x200, 0, 0, 0, 0x10, 0x57E, 0x0308, 0x0214, 0x1, 0x2, 0x0, 0x1};
  m_config_descriptor.emplace_back(ConfigDescriptor{0x9, 0x2, 0x0030, 0x1, 0x1, 0x0, 0x80, 0x32});
  m_interface_descriptor.emplace_back(
      InterfaceDescriptor{0x9, 0x4, 0x0, 0x0, 0x0, 0xFF, 0xFF, 0xFF, 0x0});
  m_interface_descriptor.emplace_back(
      InterfaceDescriptor{0x9, 0x4, 0x0, 0x01, 0x03, 0xFF, 0xFF, 0xFF, 0x0});
  m_endpoint_descriptor.emplace_back(EndpointDescriptor{0x7, 0x5, 0x81, 0x1, 0x0020, 0x1});
  m_endpoint_descriptor.emplace_back(EndpointDescriptor{0x7, 0x5, 0x2, 0x2, 0x0020, 0});
  m_endpoint_descriptor.emplace_back(EndpointDescriptor{0x7, 0x5, 0x3, 0x1, 0x0040, 1});

  m_microphone = Microphone();
  if (m_microphone.OpenMicrophone() != 0)
  {
    ERROR_LOG_FMT(IOS_USB, "Error opening the microphone.");
    b_is_mic_connected = false;
    return;
  }

  if (m_microphone.StartCapture() != 0)
  {
    ERROR_LOG_FMT(IOS_USB, "Error starting captures.");
    b_is_mic_connected = false;
    return;
  }

  m_microphone_thread = std::thread([this] {
    u64 timeout{};
    constexpr u64 TIMESTEP = 256ull * 1'000'000ull / 48000ull;
    while (true)
    {
      if (m_shutdown_event.WaitFor(std::chrono::microseconds{timeout}))
        return;

      std::lock_guard lg(m_mutex);
      timeout = TIMESTEP - (std::chrono::duration_cast<std::chrono::microseconds>(
                                std::chrono::steady_clock::now().time_since_epoch())
                                .count() %
                            TIMESTEP);
      m_microphone.PerformAudioCapture();
      m_microphone.GetSoundData();
    }
  });
}

WiiSpeak::~WiiSpeak()
{
  {
    std::lock_guard lg(m_mutex);
    if (!m_microphone_thread.joinable())
      return;

    m_shutdown_event.Set();
  }

  m_microphone_thread.join();
  m_microphone.StopCapture();
}

DeviceDescriptor WiiSpeak::GetDeviceDescriptor() const
{
  return m_device_descriptor;
}

std::vector<ConfigDescriptor> WiiSpeak::GetConfigurations() const
{
  return m_config_descriptor;
}

std::vector<InterfaceDescriptor> WiiSpeak::GetInterfaces(u8 config) const
{
  return m_interface_descriptor;
}

std::vector<EndpointDescriptor> WiiSpeak::GetEndpoints(u8 config, u8 interface, u8 alt) const
{
  return m_endpoint_descriptor;
}

bool WiiSpeak::Attach()
{
  if (m_device_attached)
    return true;

  DEBUG_LOG_FMT(IOS_USB, "[{:04x}:{:04x}] Opening device", m_vid, m_pid);
  m_device_attached = true;
  return true;
}

bool WiiSpeak::AttachAndChangeInterface(const u8 interface)
{
  if (!Attach())
    return false;

  if (interface != m_active_interface)
    return ChangeInterface(interface) == 0;

  return true;
}

int WiiSpeak::CancelTransfer(const u8 endpoint)
{
  INFO_LOG_FMT(IOS_USB, "[{:04x}:{:04x} {}] Cancelling transfers (endpoint {:#x})", m_vid, m_pid,
               m_active_interface, endpoint);

  return IPC_SUCCESS;
}

int WiiSpeak::ChangeInterface(const u8 interface)
{
  DEBUG_LOG_FMT(IOS_USB, "[{:04x}:{:04x} {}] Changing interface to {}", m_vid, m_pid,
                m_active_interface, interface);
  m_active_interface = interface;
  return 0;
}

int WiiSpeak::GetNumberOfAltSettings(u8 interface)
{
  return 0;
}

int WiiSpeak::SetAltSetting(u8 alt_setting)
{
  return 0;
}

int WiiSpeak::SubmitTransfer(std::unique_ptr<CtrlMessage> cmd)
{
  DEBUG_LOG_FMT(IOS_USB,
                "[{:04x}:{:04x} {}] Control: bRequestType={:02x} bRequest={:02x} wValue={:04x}"
                " wIndex={:04x} wLength={:04x}",
                m_vid, m_pid, m_active_interface, cmd->request_type, cmd->request, cmd->value,
                cmd->index, cmd->length);

  if (!b_is_mic_connected)
    return IPC_ENOENT;

  switch (cmd->request_type << 8 | cmd->request)
  {
  case USBHDR(DIR_DEVICE2HOST, TYPE_STANDARD, REC_INTERFACE, REQUEST_GET_INTERFACE):
  {
    std::array<u8, 1> data{1};
    cmd->FillBuffer(data.data(), 1);
    cmd->ScheduleTransferCompletion(1, 100);
    break;
  }
  case USBHDR(DIR_HOST2DEVICE, TYPE_VENDOR, REC_INTERFACE, 0):
  {
    init = false;
    m_ios.EnqueueIPCReply(cmd->ios_request, IPC_SUCCESS);
    break;
  }
  case USBHDR(DIR_DEVICE2HOST, TYPE_VENDOR, REC_INTERFACE, REQUEST_GET_DESCRIPTOR):
  {
    if (!init)
    {
      std::array<u8, 1> data{0};
      cmd->FillBuffer(data.data(), 1);
      m_ios.EnqueueIPCReply(cmd->ios_request, IPC_SUCCESS);
      init = true;
    }
    else
    {
      std::array<u8, 1> data{1};
      cmd->FillBuffer(data.data(), 1);
      m_ios.EnqueueIPCReply(cmd->ios_request, IPC_SUCCESS);
    }
    break;
  }
  case USBHDR(DIR_HOST2DEVICE, TYPE_VENDOR, REC_INTERFACE, 1):
    SetRegister(cmd);
    m_ios.EnqueueIPCReply(cmd->ios_request, IPC_SUCCESS);
    break;
  case USBHDR(DIR_DEVICE2HOST, TYPE_VENDOR, REC_INTERFACE, 2):
    GetRegister(cmd);
    m_ios.EnqueueIPCReply(cmd->ios_request, IPC_SUCCESS);
    break;
  default:
    NOTICE_LOG_FMT(IOS_USB, "Unknown command");
    m_ios.EnqueueIPCReply(cmd->ios_request, IPC_SUCCESS);
  }

  return IPC_SUCCESS;
};
int WiiSpeak::SubmitTransfer(std::unique_ptr<BulkMessage> cmd)
{
  m_ios.EnqueueIPCReply(cmd->ios_request, IPC_SUCCESS);
  return IPC_SUCCESS;
};
int WiiSpeak::SubmitTransfer(std::unique_ptr<IntrMessage> cmd)
{
  m_ios.EnqueueIPCReply(cmd->ios_request, IPC_SUCCESS);
  return IPC_SUCCESS;
};

int WiiSpeak::SubmitTransfer(std::unique_ptr<IsoMessage> cmd)
{
  if (!b_is_mic_connected)
    return IPC_ENOENT;

  auto& system = m_ios.GetSystem();
  auto& memory = system.GetMemory();

  u8* packets = memory.GetPointerForRange(cmd->data_address, cmd->length);
  if (packets == nullptr)
  {
    ERROR_LOG_FMT(IOS_USB, "Wii Speak command invalid");
    return IPC_EINVAL;
  }

  if (cmd->endpoint == 0x81 && m_microphone.HasData())
    m_microphone.ReadIntoBuffer(packets, cmd->length);

  // Anything more causes the visual cue to not appear.
  // Anything less is more choppy audio.
  cmd->ScheduleTransferCompletion(IPC_SUCCESS, 20000);
  return IPC_SUCCESS;
};

void WiiSpeak::SetRegister(std::unique_ptr<CtrlMessage>& cmd)
{
  auto& system = m_ios.GetSystem();
  auto& memory = system.GetMemory();
  const u8 reg = memory.Read_U8(cmd->data_address + 1) & ~1;
  const u16 arg1 = memory.Read_U16(cmd->data_address + 2);
  const u16 arg2 = memory.Read_U16(cmd->data_address + 4);

  switch (reg)
  {
  case SAMPLER_STATE:
    sampler.sample_on = !!arg1;
    break;
  case SAMPLER_FREQ:
    switch (arg1)
    {
    case FREQ_8KHZ:
      sampler.freq = 8000;
      break;
    case FREQ_11KHZ:
      sampler.freq = 11025;
      break;
    case FREQ_RESERVED:
    case FREQ_16KHZ:
    default:
      sampler.freq = 16000;
      break;
    }
    break;
  case SAMPLER_GAIN:
    switch (arg1 & ~0x300)
    {
    case GAIN_00dB:
      sampler.gain = 0;
      break;
    case GAIN_15dB:
      sampler.gain = 15;
      break;
    case GAIN_30dB:
      sampler.gain = 30;
      break;
    case GAIN_36dB:
    default:
      sampler.gain = 36;
      break;
    }
    break;
  case EC_STATE:
    sampler.ec_reset = !!arg1;
    break;
  case SP_STATE:
    switch (arg1)
    {
    case SP_ENABLE:
      sampler.sp_on = arg2 == 0;
      break;
    case SP_SIN:
    case SP_SOUT:
    case SP_RIN:
      break;
    }
    break;
  case SAMPLER_MUTE:
    sampler.mute = !!arg1;
    break;
  }
}

void WiiSpeak::GetRegister(std::unique_ptr<CtrlMessage>& cmd)
{
  auto& system = m_ios.GetSystem();
  auto& memory = system.GetMemory();
  const u8 reg = memory.Read_U8(cmd->data_address + 1) & ~1;
  const u32 arg1 = cmd->data_address + 2;
  const u32 arg2 = cmd->data_address + 4;

  switch (reg)
  {
  case SAMPLER_STATE:
    memory.Write_U16(sampler.sample_on ? 1 : 0, arg1);
    break;
  case SAMPLER_FREQ:
    switch (sampler.freq)
    {
    case 8000:
      memory.Write_U16(FREQ_8KHZ, arg1);
      break;
    case 11025:
      memory.Write_U16(FREQ_11KHZ, arg1);
      break;
    case 16000:
    default:
      memory.Write_U16(FREQ_16KHZ, arg1);
      break;
    }
    break;
  case SAMPLER_GAIN:
    switch (sampler.gain)
    {
    case 0:
      memory.Write_U16(0x300 | GAIN_00dB, arg1);
      break;
    case 15:
      memory.Write_U16(0x300 | GAIN_15dB, arg1);
      break;
    case 30:
      memory.Write_U16(0x300 | GAIN_30dB, arg1);
      break;
    case 36:
    default:
      memory.Write_U16(0x300 | GAIN_36dB, arg1);
      break;
    }
    break;
  case EC_STATE:
    memory.Write_U16(sampler.ec_reset ? 1 : 0, arg1);
    break;
  case SP_STATE:
    switch (memory.Read_U16(arg1))
    {
    case SP_ENABLE:
      memory.Write_U16(1, arg2);
      break;
    case SP_SIN:
      break;
    case SP_SOUT:
      memory.Write_U16(0x39B0, arg2);  // 6dB
      break;
    case SP_RIN:
      break;
    }
    break;
  case SAMPLER_MUTE:
    memory.Write_U16(sampler.mute ? 1 : 0, arg1);
    break;
  }
}
}  // namespace IOS::HLE::USB
