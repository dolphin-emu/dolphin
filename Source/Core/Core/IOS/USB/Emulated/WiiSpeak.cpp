// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/IOS/USB/Emulated/WiiSpeak.h"

#include "Core/Config/MainSettings.h"
#include "Core/HW/Memmap.h"

namespace IOS::HLE::USB
{
WiiSpeak::WiiSpeak(IOS::HLE::EmulationKernel& ios) : m_ios(ios)
{
  m_id = u64(m_vid) << 32 | u64(m_pid) << 16 | u64(9) << 8 | u64(1);
}

WiiSpeak::~WiiSpeak() = default;

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
  if (!m_microphone)
    m_microphone = std::make_unique<Microphone>();
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

  // Without a proper way to reconnect the emulated Wii Speak,
  // this error after being raised prevents some games to use the microphone later.
  //
  // if (!IsMicrophoneConnected())
  //  return IPC_ENOENT;

  switch (cmd->request_type << 8 | cmd->request)
  {
  case USBHDR(DIR_DEVICE2HOST, TYPE_STANDARD, REC_INTERFACE, REQUEST_GET_INTERFACE):
  {
    const u8 data{1};
    cmd->FillBuffer(&data, sizeof(data));
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
      const u8 data{0};
      cmd->FillBuffer(&data, sizeof(data));
      m_ios.EnqueueIPCReply(cmd->ios_request, IPC_SUCCESS);
      init = true;
    }
    else
    {
      const u8 data{1};
      cmd->FillBuffer(&data, sizeof(data));
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
}

int WiiSpeak::SubmitTransfer(std::unique_ptr<BulkMessage> cmd)
{
  m_ios.EnqueueIPCReply(cmd->ios_request, IPC_SUCCESS);
  return IPC_SUCCESS;
}

int WiiSpeak::SubmitTransfer(std::unique_ptr<IntrMessage> cmd)
{
  m_ios.EnqueueIPCReply(cmd->ios_request, IPC_SUCCESS);
  return IPC_SUCCESS;
}

int WiiSpeak::SubmitTransfer(std::unique_ptr<IsoMessage> cmd)
{
  // if (!IsMicrophoneConnected())
  //   return IPC_ENOENT;

  auto& system = m_ios.GetSystem();
  auto& memory = system.GetMemory();

  u8* packets = memory.GetPointerForRange(cmd->data_address, cmd->length);
  if (packets == nullptr)
  {
    ERROR_LOG_FMT(IOS_USB, "Wii Speak command invalid");
    return IPC_EINVAL;
  }

  switch (cmd->endpoint)
  {
  case ENDPOINT_AUDIO_IN:
    // Transfer: Wii Speak -> Wii
    if (m_microphone && m_microphone->HasData())
      m_microphone->ReadIntoBuffer(packets, cmd->length);
    break;
  case ENDPOINT_AUDIO_OUT:
    // Transfer: Wii -> Wii Speak
    break;
  default:
    WARN_LOG_FMT(IOS_USB, "Wii Speak unsupported isochronous transfer (endpoint={:02x})",
                 cmd->endpoint);
    break;
  }

  // Anything more causes the visual cue to not appear.
  // Anything less is more choppy audio.
  DEBUG_LOG_FMT(IOS_USB,
                "Wii Speak isochronous transfer: length={:04x} endpoint={:02x} num_packets={:02x}",
                cmd->length, cmd->endpoint, cmd->num_packets);

  // According to the Wii Speak specs on wiibrew, it's "USB 2.0 Full-speed Device Module",
  // so the length of a single frame should be 1 ms.
  // TODO: Find a proper way to compute the transfer timing.
  const u32 transfer_timing = 2500;  // 2.5 ms
  cmd->ScheduleTransferCompletion(IPC_SUCCESS, transfer_timing);
  return IPC_SUCCESS;
}

void WiiSpeak::SetRegister(const std::unique_ptr<CtrlMessage>& cmd)
{
  auto& system = m_ios.GetSystem();
  auto& memory = system.GetMemory();
  const u8 reg = memory.Read_U8(cmd->data_address + 1) & ~1;
  const u16 arg1 = memory.Read_U16(cmd->data_address + 2);
  const u16 arg2 = memory.Read_U16(cmd->data_address + 4);

  DEBUG_LOG_FMT(IOS_USB, "Wii Speak register set (reg={:02x}, arg1={:04x}, arg2={:04x})", reg, arg1,
                arg2);

  switch (reg)
  {
  case SAMPLER_STATE:
    m_sampler.sample_on = !!arg1;
    break;
  case SAMPLER_FREQ:
    switch (arg1)
    {
    case FREQ_8KHZ:
      m_sampler.freq = 8000;
      break;
    case FREQ_11KHZ:
      m_sampler.freq = 11025;
      break;
    case FREQ_RESERVED:
    default:
      WARN_LOG_FMT(IOS_USB,
                   "Wii Speak unsupported SAMPLER_FREQ set (arg1={:04x}, arg2={:04x}) defaulting "
                   "to FREQ_16KHZ",
                   arg1, arg2);
      [[fallthrough]];
    case FREQ_16KHZ:
      m_sampler.freq = 16000;
      break;
    }
    break;
  case SAMPLER_GAIN:
    switch (arg1 & ~0x300)
    {
    case GAIN_00dB:
      m_sampler.gain = 0;
      break;
    case GAIN_15dB:
      m_sampler.gain = 15;
      break;
    case GAIN_30dB:
      m_sampler.gain = 30;
      break;
    default:
      WARN_LOG_FMT(IOS_USB,
                   "Wii Speak unsupported SAMPLER_GAIN set (arg1={:04x}, arg2={:04x}) defaulting "
                   "to GAIN_36dB",
                   arg1, arg2);
      [[fallthrough]];
    case GAIN_36dB:
      m_sampler.gain = 36;
      break;
    }
    break;
  case EC_STATE:
    m_sampler.ec_reset = !!arg1;
    break;
  case SP_STATE:
    switch (arg1)
    {
    case SP_ENABLE:
      m_sampler.sp_on = arg2 == 0;
      break;
    case SP_SIN:
    case SP_SOUT:
    case SP_RIN:
      break;
    default:
      WARN_LOG_FMT(IOS_USB, "Wii Speak unsupported SP_STATE set (arg1={:04x}, arg2={:04x})", arg1,
                   arg2);
      break;
    }
    break;
  case SAMPLER_MUTE:
    m_sampler.mute = !!arg1;
    break;
  default:
    WARN_LOG_FMT(IOS_USB,
                 "Wii Speak unsupported register set (reg={:02x}, arg1={:04x}, arg2={:04x})", reg,
                 arg1, arg2);
    break;
  }
}

void WiiSpeak::GetRegister(const std::unique_ptr<CtrlMessage>& cmd) const
{
  auto& system = m_ios.GetSystem();
  auto& memory = system.GetMemory();
  const u8 reg = memory.Read_U8(cmd->data_address + 1) & ~1;
  const u32 arg1 = cmd->data_address + 2;
  const u32 arg2 = cmd->data_address + 4;

  DEBUG_LOG_FMT(IOS_USB, "Wii Speak register get (reg={:02x}, arg1={:08x}, arg2={:08x})", reg, arg1,
                arg2);

  switch (reg)
  {
  case SAMPLER_STATE:
    memory.Write_U16(m_sampler.sample_on ? 1 : 0, arg1);
    break;
  case SAMPLER_FREQ:
    switch (m_sampler.freq)
    {
    case 8000:
      memory.Write_U16(FREQ_8KHZ, arg1);
      break;
    case 11025:
      memory.Write_U16(FREQ_11KHZ, arg1);
      break;
    default:
      WARN_LOG_FMT(IOS_USB,
                   "Wii Speak unsupported SAMPLER_FREQ get (arg1={:04x}, arg2={:04x}) defaulting "
                   "to FREQ_16KHZ",
                   arg1, arg2);
      [[fallthrough]];
    case 16000:
      memory.Write_U16(FREQ_16KHZ, arg1);
      break;
    }
    break;
  case SAMPLER_GAIN:
    switch (m_sampler.gain)
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
    default:
      WARN_LOG_FMT(IOS_USB,
                   "Wii Speak unsupported SAMPLER_GAIN get (arg1={:04x}, arg2={:04x}) defaulting "
                   "to GAIN_36dB",
                   arg1, arg2);
      [[fallthrough]];
    case 36:
      memory.Write_U16(0x300 | GAIN_36dB, arg1);
      break;
    }
    break;
  case EC_STATE:
    memory.Write_U16(m_sampler.ec_reset ? 1 : 0, arg1);
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
    default:
      WARN_LOG_FMT(IOS_USB, "Wii Speak unsupported SP_STATE get (arg1={:04x}, arg2={:04x})", arg1,
                   arg2);
      break;
    }
    break;
  case SAMPLER_MUTE:
    memory.Write_U16(m_sampler.mute ? 1 : 0, arg1);
    break;
  default:
    WARN_LOG_FMT(IOS_USB,
                 "Wii Speak unsupported register get (reg={:02x}, arg1={:08x}, arg2={:08x})", reg,
                 arg1, arg2);
    break;
  }
}

bool WiiSpeak::IsMicrophoneConnected() const
{
  return Config::Get(Config::MAIN_WII_SPEAK_CONNECTED);
}
}  // namespace IOS::HLE::USB
