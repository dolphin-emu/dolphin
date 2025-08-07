// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/IOS/USB/Emulated/LogitechMic.h"

#ifndef TYPE_CLASS
#define TYPE_CLASS 1
#endif

#include <algorithm>

#include "Core/HW/Memmap.h"
#include "Core/System.h"

namespace IOS::HLE::USB
{
LogitechMic::LogitechMic()
{
  m_id = u64(m_vid) << 32 | u64(m_pid) << 16 | u64(9) << 8 | u64(1);
}

LogitechMic::~LogitechMic() = default;

DeviceDescriptor LogitechMic::GetDeviceDescriptor() const
{
  return m_device_descriptor;
}

std::vector<ConfigDescriptor> LogitechMic::GetConfigurations() const
{
  return m_config_descriptor;
}

std::vector<InterfaceDescriptor> LogitechMic::GetInterfaces(u8 config) const
{
  return m_interface_descriptor;
}

std::vector<EndpointDescriptor> LogitechMic::GetEndpoints(u8 config, u8 interface, u8 alt) const
{
  return m_endpoint_descriptor;
}

bool LogitechMic::Attach()
{
  if (m_device_attached)
    return true;

  DEBUG_LOG_FMT(IOS_USB, "[{:04x}:{:04x}] Opening device", m_vid, m_pid);
  if (!m_microphone)
    m_microphone = std::make_unique<MicrophoneLogitech>(m_sampler);
  m_device_attached = true;
  return true;
}

bool LogitechMic::AttachAndChangeInterface(const u8 interface)
{
  if (!Attach())
    return false;

  if (interface != m_active_interface)
    return ChangeInterface(interface) == 0;

  return true;
}

int LogitechMic::CancelTransfer(const u8 endpoint)
{
  INFO_LOG_FMT(IOS_USB, "[{:04x}:{:04x} {}] Cancelling transfers (endpoint {:#x})", m_vid, m_pid,
               m_active_interface, endpoint);

  return IPC_SUCCESS;
}

int LogitechMic::ChangeInterface(const u8 interface)
{
  DEBUG_LOG_FMT(IOS_USB, "[{:04x}:{:04x} {}] Changing interface to {}", m_vid, m_pid,
                m_active_interface, interface);
  m_active_interface = interface;
  return 0;
}

int LogitechMic::GetNumberOfAltSettings(u8 interface)
{
  return 0;
}

int LogitechMic::SetAltSetting(u8 alt_setting)
{
  return 0;
}

int LogitechMic::SubmitTransfer(std::unique_ptr<CtrlMessage> cmd)
{
  DEBUG_LOG_FMT(IOS_USB,
                "[{:04x}:{:04x} {}] Control: bRequestType={:02x} bRequest={:02x} wValue={:04x}"
                " wIndex={:04x} wLength={:04x}",
                m_vid, m_pid, m_active_interface, cmd->request_type, cmd->request, cmd->value,
                cmd->index, cmd->length);

  switch (cmd->request_type << 8 | cmd->request)
  {
  
  // After some testing, it seems that LEGO Rock Band (and other games too, haven't tested) calls for case 0x02 upon startup
  // It probably needs to respond to case 0x02 with the descriptor information in LogitechMic.h
  
  case USBHDR(DIR_DEVICE2HOST, TYPE_STANDARD, REC_INTERFACE, REQUEST_GET_INTERFACE):
  {
    constexpr u8 data{1};
    cmd->FillBuffer(&data, sizeof(data));
    cmd->ScheduleTransferCompletion(1, 100);
    break;
  }
  case USBHDR(DIR_HOST2DEVICE, TYPE_STANDARD, REC_INTERFACE, REQUEST_SET_INTERFACE):
  {
    INFO_LOG_FMT(IOS_USB, "[{:04x}:{:04x} {}] REQUEST_SET_INTERFACE index={:04x} value={:04x}",
                 m_vid, m_pid, m_active_interface, cmd->index, cmd->value);
    if (static_cast<u8>(cmd->index) != m_active_interface)
    {
      const int ret = ChangeInterface(static_cast<u8>(cmd->index));
      if (ret < 0)
      {
        ERROR_LOG_FMT(IOS_USB, "[{:04x}:{:04x} {}] Failed to change interface to {}", m_vid, m_pid,
                      m_active_interface, cmd->index);
        return ret;
      }
    }
    const int ret = SetAltSetting(static_cast<u8>(cmd->value));
    if (ret == 0)
      cmd->GetEmulationKernel().EnqueueIPCReply(cmd->ios_request, cmd->length);
    return ret;
  }
  case USBHDR(DIR_HOST2DEVICE, TYPE_VENDOR, REC_INTERFACE, 0):
  {
    init = false;
    cmd->GetEmulationKernel().EnqueueIPCReply(cmd->ios_request, IPC_SUCCESS);
    break;
  }
  case USBHDR(DIR_DEVICE2HOST, TYPE_VENDOR, REC_INTERFACE, REQUEST_GET_DESCRIPTOR):
  {
    if (!init)
    {
      constexpr u8 data{0};
      cmd->FillBuffer(&data, sizeof(data));
      cmd->GetEmulationKernel().EnqueueIPCReply(cmd->ios_request, IPC_SUCCESS);
      init = true;
    }
    else
    {
      constexpr u8 data{1};
      cmd->FillBuffer(&data, sizeof(data));
      cmd->GetEmulationKernel().EnqueueIPCReply(cmd->ios_request, IPC_SUCCESS);
    }
    break;
  }
  case USBHDR(DIR_HOST2DEVICE, TYPE_VENDOR, REC_INTERFACE, 1):
    SetRegister(cmd);
    cmd->GetEmulationKernel().EnqueueIPCReply(cmd->ios_request, IPC_SUCCESS);
    break;
  case USBHDR(DIR_DEVICE2HOST, TYPE_VENDOR, REC_INTERFACE, 2):
    GetRegister(cmd);
    cmd->GetEmulationKernel().EnqueueIPCReply(cmd->ios_request, IPC_SUCCESS);
    break;
  default:
    NOTICE_LOG_FMT(IOS_USB, "Unknown command");
    cmd->GetEmulationKernel().EnqueueIPCReply(cmd->ios_request, IPC_SUCCESS);
  }

  return IPC_SUCCESS;
}


int LogitechMic::SubmitTransfer(std::unique_ptr<BulkMessage> cmd)
{
  cmd->GetEmulationKernel().EnqueueIPCReply(cmd->ios_request, IPC_SUCCESS);
  return IPC_SUCCESS;
}

int LogitechMic::SubmitTransfer(std::unique_ptr<IntrMessage> cmd)
{
  cmd->GetEmulationKernel().EnqueueIPCReply(cmd->ios_request, IPC_SUCCESS);
  return IPC_SUCCESS;
}

int LogitechMic::SubmitTransfer(std::unique_ptr<IsoMessage> cmd)
{
  auto& system = cmd->GetEmulationKernel().GetSystem();
  auto& memory = system.GetMemory();

  u8* packets = memory.GetPointerForRange(cmd->data_address, cmd->length);
  if (packets == nullptr)
  {
    ERROR_LOG_FMT(IOS_USB, "Logitech USB Microphone command invalid");
    return IPC_EINVAL;
  }

  switch (cmd->endpoint)
  {
  case ENDPOINT_AUDIO_IN:
  {
    // Transfer: Logitech Mic -> Wii
    u16 size = 0;
    if (m_microphone && m_microphone->HasData(cmd->length / sizeof(s16)))
      size = m_microphone->ReadIntoBuffer(packets, cmd->length);
    for (std::size_t i = 0; i < cmd->num_packets; i++)
    {
      cmd->SetPacketReturnValue(i, std::min(size, cmd->packet_sizes[i]));
      size = (size > cmd->packet_sizes[i]) ? (size - cmd->packet_sizes[i]) : 0;
    }
    break;
  }
}

  // Transferring too slow causes the visual cue to not appear,
  // while transferring too fast results in more choppy audio.
  DEBUG_LOG_FMT(IOS_USB,
                "Logitech Mic isochronous transfer: length={:04x} endpoint={:02x} num_packets={:02x}",
                cmd->length, cmd->endpoint, cmd->num_packets);

  // According to the Wii Speak specs on wiibrew, it's "USB 2.0 Full-speed Device Module",
  // so the length of a single frame should be 1 ms.
  //
  // Monster Hunter 3 and the Wii Speak Channel use cmd->length=0x100, allowing 256/2 samples
  // (i.e. 128 samples in 16-bit mono) per frame transfer. The Microphone class is using cubeb
  // configured with a sample rate of 8000.
  //
  // Based on USB sniffing using Wireshark + Dolphin USB passthrough:
  //  - 125 frames are received per second (i.e. timing 8 ms per frame)
  //  - however, the cmd->length=0x80 which doesn't match the HLE emulation
  //  - each frame having 8 packets of 0x10 bytes
  //
  // Let's sample at a reasonable speed.
  
  // (keeping this comment for reference when/if I can test the Logitech mic through Wireshark)
  
  const u32 transfer_timing = 2000;
  cmd->ScheduleTransferCompletion(IPC_SUCCESS, transfer_timing);
  return IPC_SUCCESS;
}

void LogitechMic::SetRegister(const std::unique_ptr<CtrlMessage>& cmd)
{
  auto& system = cmd->GetEmulationKernel().GetSystem();
  auto& memory = system.GetMemory();
  const u8 reg = memory.Read_U8(cmd->data_address + 1) & ~1;
  const u16 arg1 = memory.Read_U16(cmd->data_address + 2);
  const u16 arg2 = memory.Read_U16(cmd->data_address + 4);

  DEBUG_LOG_FMT(IOS_USB, "Logitech Mic register set (reg={:02x}, arg1={:04x}, arg2={:04x})", reg, arg1,
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
                   "Logitech Mic unsupported SAMPLER_FREQ set (arg1={:04x}, arg2={:04x}) defaulting "
                   "to FREQ_22KHZ",
                   arg1, arg2);
      [[fallthrough]];
    case FREQ_22KHZ:
      m_sampler.freq = 22050;
      break;
    }
    if (m_microphone)
      m_microphone->SetSamplingRate(m_sampler.freq);
    break;
  case SAMPLER_GAIN:
    WARN_LOG_FMT(IOS_USB, "Logitech Mic SAMPLER_GAIN set (arg1={:04x}, arg2={:04x}) not implemented",
                 arg1, arg2);
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
                   "Logitech Mic unsupported SAMPLER_GAIN set (arg1={:04x}, arg2={:04x}) defaulting "
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
      WARN_LOG_FMT(IOS_USB, "Logitech Mic unsupported SP_STATE set (arg1={:04x}, arg2={:04x})", arg1,
                   arg2);
      break;
    }
    break;
  case SAMPLER_MUTE:
    m_sampler.mute = !!arg1;
    break;
  default:
    WARN_LOG_FMT(IOS_USB,
                 "Logitech Mic unsupported register set (reg={:02x}, arg1={:04x}, arg2={:04x})", reg,
                 arg1, arg2);
    break;
  }
}

void LogitechMic::GetRegister(const std::unique_ptr<CtrlMessage>& cmd) const
{
  auto& system = cmd->GetEmulationKernel().GetSystem();
  auto& memory = system.GetMemory();
  const u8 reg = memory.Read_U8(cmd->data_address + 1) & ~1;
  const u32 arg1 = cmd->data_address + 2;
  const u32 arg2 = cmd->data_address + 4;

  DEBUG_LOG_FMT(IOS_USB, "Logitech Mic register get (reg={:02x}, arg1={:08x}, arg2={:08x})", reg, arg1,
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
                   "Logitech Mic unsupported SAMPLER_FREQ get (arg1={:04x}, arg2={:04x}) defaulting "
                   "to FREQ_22KHZ",
                   arg1, arg2);
      [[fallthrough]];
    case 22050:
      memory.Write_U16(FREQ_22KHZ, arg1);
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
                   "Logitech Mic unsupported SAMPLER_GAIN get (arg1={:04x}, arg2={:04x}) defaulting "
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
      // TODO: Find how it was measured and how accurate it was
      // memory.Write_U16(0x39B0, arg2);  // 6dB
      memory.Write_U16(m_microphone->GetLoudnessLevel(), arg2);
      break;
    case SP_RIN:
      break;
    default:
      WARN_LOG_FMT(IOS_USB, "Logitech Mic unsupported SP_STATE get (arg1={:04x}, arg2={:04x})", arg1,
                   arg2);
      break;
    }
    break;
  case SAMPLER_MUTE:
    memory.Write_U16(m_sampler.mute ? 1 : 0, arg1);
    break;
  default:
    WARN_LOG_FMT(IOS_USB,
                 "Logitech Mic unsupported register get (reg={:02x}, arg1={:08x}, arg2={:08x})", reg,
                 arg1, arg2);
    break;
  }
}
}  // namespace IOS::HLE::USB
