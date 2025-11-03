// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/IOS/USB/Emulated/LogitechMic.h"

#include <algorithm>

#include "Core/Config/MainSettings.h"
#include "Core/HW/Memmap.h"
#include "Core/System.h"

namespace IOS::HLE::USB
{
enum class RequestCode : u8
{
  SetCur = 0x01,
  GetCur = 0x81,
  SetMin = 0x02,
  GetMin = 0x82,
  SetMax = 0x03,
  GetMax = 0x83,
  SetRes = 0x04,
  GetRes = 0x84,
  SetMem = 0x05,
  GetMem = 0x85,
  GetStat = 0xff
};

enum class FeatureUnitControlSelector : u8
{
  Mute = 0x01,
  Volume = 0x02,
  Bass = 0x03,
  Midi = 0x04,
  Treble = 0x05,
  GraphicEqualizer = 0x06,
  AutomaticGain = 0x07,
  Delay = 0x08,
  BassBoost = 0x09,
  Loudness = 0x0a
};

enum class EndpointControlSelector : u8
{
  SamplingFreq = 0x01,
  Pitch = 0x02
};

bool LogitechMicState::IsSampleOn() const
{
  return true;
}

bool LogitechMicState::IsMuted() const
{
  return mute;
}

u32 LogitechMicState::GetDefaultSamplingRate() const
{
  return DEFAULT_SAMPLING_RATE;
}

namespace
{
class MicrophoneLogitech final : public Microphone
{
public:
  explicit MicrophoneLogitech(const LogitechMicState& sampler, u8 index)
      : Microphone(sampler, fmt::format("Logitech Mic {}", index)), m_sampler(sampler),
        m_index(index)
  {
  }

private:
#ifdef HAVE_CUBEB
  std::string GetInputDeviceId() const override
  {
    return Config::Get(Config::MAIN_LOGITECH_MIC_MICROPHONE[m_index]);
  }
  std::string GetCubebStreamName() const override
  {
    return "Dolphin Emulated Logitech USB Microphone " + std::to_string(m_index);
  }
  s16 GetVolumeModifier() const override
  {
    return Config::Get(Config::MAIN_LOGITECH_MIC_VOLUME_MODIFIER[m_index]);
  }
  bool AreSamplesByteSwapped() const override { return false; }
#endif

  bool IsMicrophoneMuted() const override
  {
    return Config::Get(Config::MAIN_LOGITECH_MIC_MUTED[m_index]);
  }
  u32 GetStreamSize() const override { return BUFF_SIZE_SAMPLES * m_sampler.sample_rate / 250; }

  const LogitechMicState& m_sampler;
  const u8 m_index;
};
}  // namespace

LogitechMic::LogitechMic(u8 index) : m_index(index)
{
  assert(index >= 0 && index <= 3);
  m_id = u64(m_vid) << 32 | u64(m_pid) << 16 | u64(9) << 8 | u64(index + 1);
}

static const DeviceDescriptor DEVICE_DESCRIPTOR{0x12,   0x01,   0x0200, 0x00, 0x00, 0x00, 0x08,
                                                0x046d, 0x0a03, 0x0001, 0x01, 0x02, 0x00, 0x01};

DeviceDescriptor LogitechMic::GetDeviceDescriptor() const
{
  return DEVICE_DESCRIPTOR;
}

static const std::vector<ConfigDescriptor> CONFIG_DESCRIPTOR{
    ConfigDescriptor{0x09, 0x02, 0x0079, 0x02, 0x01, 0x03, 0x80, 0x3c},
};

std::vector<ConfigDescriptor> LogitechMic::GetConfigurations() const
{
  return CONFIG_DESCRIPTOR;
}

static const std::vector<InterfaceDescriptor> INTERFACE_DESCRIPTORS{
    InterfaceDescriptor{0x09, 0x04, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00},
    InterfaceDescriptor{0x09, 0x04, 0x01, 0x00, 0x00, 0x01, 0x02, 0x00, 0x00},
    InterfaceDescriptor{0x09, 0x04, 0x01, 0x01, 0x01, 0x01, 0x02, 0x00, 0x00}};

std::vector<InterfaceDescriptor> LogitechMic::GetInterfaces(u8 /*config*/) const
{
  return INTERFACE_DESCRIPTORS;
}

static constexpr u8 ENDPOINT_AUDIO_IN = 0x84;
static const std::vector<EndpointDescriptor> ENDPOINT_DESCRIPTORS{
    EndpointDescriptor{0x09, 0x05, ENDPOINT_AUDIO_IN, 0x0d, 0x0060, 0x01},
};

std::vector<EndpointDescriptor> LogitechMic::GetEndpoints(u8 /*config*/, u8 interface, u8 alt) const
{
  if (interface == 1 && alt == 1)
    return ENDPOINT_DESCRIPTORS;

  return std::vector<EndpointDescriptor>{};
}

bool LogitechMic::Attach()
{
  if (m_device_attached)
    return true;

  DEBUG_LOG_FMT(IOS_USB, "[{:04x}:{:04x} {}] Opening device", m_vid, m_pid, m_index);
  if (!m_microphone)
  {
    m_microphone = std::make_unique<MicrophoneLogitech>(m_sampler, m_index);
    m_microphone->Initialize();
  }
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
  DEBUG_LOG_FMT(IOS_USB, "[{:04x}:{:04x} {}:{}] Cancelling transfers (endpoint {:#x})", m_vid,
                m_pid, m_index, m_active_interface, endpoint);

  return IPC_SUCCESS;
}

int LogitechMic::ChangeInterface(const u8 interface)
{
  DEBUG_LOG_FMT(IOS_USB, "[{:04x}:{:04x} {}:{}] Changing interface to {}", m_vid, m_pid, m_index,
                m_active_interface, interface);
  m_active_interface = interface;
  return 0;
}

int LogitechMic::GetNumberOfAltSettings(u8 interface)
{
  if (interface == 1)
    return 2;

  return 0;
}

int LogitechMic::SetAltSetting(u8 /*alt_setting*/)
{
  return 0;
}

static constexpr u32 USBGETAID(u8 cs, u8 request, u16 index)
{
  return static_cast<u32>((cs << 24) | (request << 16) | index);
}

static constexpr u32 USBGETAID(FeatureUnitControlSelector cs, RequestCode request, u16 index)
{
  return USBGETAID(Common::ToUnderlying(cs), Common::ToUnderlying(request), index);
}

static constexpr u32 USBGETAID(EndpointControlSelector cs, RequestCode request, u16 index)
{
  return USBGETAID(Common::ToUnderlying(cs), Common::ToUnderlying(request), index);
}

int LogitechMic::GetAudioControl(std::unique_ptr<CtrlMessage>& cmd)
{
  auto& system = cmd->GetEmulationKernel().GetSystem();
  auto& memory = system.GetMemory();
  const auto cs = static_cast<FeatureUnitControlSelector>(cmd->value >> 8);
  const auto request = static_cast<RequestCode>(cmd->request);
  const u8 cn = static_cast<u8>(cmd->value - 1);
  const u32 aid = USBGETAID(cs, request, cmd->index);
  int ret = IPC_STALL;
  DEBUG_LOG_FMT(IOS_USB,
                "GetAudioControl: bCs={:02x} bCn={:02x} bRequestType={:02x} bRequest={:02x} "
                "bIndex={:02x} aid={:08x}",
                Common::ToUnderlying(cs), cn, cmd->request_type, Common::ToUnderlying(request),
                cmd->index, aid);
  switch (aid)
  {
  case USBGETAID(FeatureUnitControlSelector::Mute, RequestCode::GetCur, 0x0200):
  {
    memory.Write_U8(m_sampler.mute ? 1 : 0, cmd->data_address);
    ret = 1;
    break;
  }
  case USBGETAID(FeatureUnitControlSelector::Volume, RequestCode::GetCur, 0x0200):
  {
    if (cn < 1 || cn == 0xff)
    {
      const uint16_t vol = (m_sampler.volume * 0x8800 + 127) / 255 + 0x8000;
      DEBUG_LOG_FMT(IOS_USB, "GetAudioControl: Get volume {:04x}", vol);
      memory.Write_U16(vol, cmd->data_address);
      ret = 2;
    }
    break;
  }
  case USBGETAID(FeatureUnitControlSelector::Volume, RequestCode::GetMin, 0x0200):
  {
    if (cn < 1 || cn == 0xff)
    {
      memory.Write_U16(0x8001, cmd->data_address);
      ret = 2;
    }
    break;
  }
  case USBGETAID(FeatureUnitControlSelector::Volume, RequestCode::GetMax, 0x0200):
  {
    if (cn < 1 || cn == 0xff)
    {
      memory.Write_U16(0x0800, cmd->data_address);
      ret = 2;
    }
    break;
  }
  case USBGETAID(FeatureUnitControlSelector::Volume, RequestCode::GetRes, 0x0200):
  {
    if (cn < 1 || cn == 0xff)
    {
      memory.Write_U16(0x0088, cmd->data_address);
      ret = 2;
    }
    break;
  }
  default:
  {
    WARN_LOG_FMT(IOS_USB,
                 "GetAudioControl: Unknown request: bCs={:02x} bCn={:02x} bRequestType={:02x} "
                 "bRequest={:02x} bIndex={:02x} aid={:08x}",
                 Common::ToUnderlying(cs), cn, cmd->request_type, Common::ToUnderlying(request),
                 cmd->index, aid);
    break;
  }
  }
  return ret;
}

int LogitechMic::SetAudioControl(std::unique_ptr<CtrlMessage>& cmd)
{
  auto& system = cmd->GetEmulationKernel().GetSystem();
  auto& memory = system.GetMemory();
  const auto cs = static_cast<FeatureUnitControlSelector>(cmd->value >> 8);
  const auto request = static_cast<RequestCode>(cmd->request);
  const u8 cn = static_cast<u8>(cmd->value - 1);
  const u32 aid = USBGETAID(cs, request, cmd->index);
  int ret = IPC_STALL;
  DEBUG_LOG_FMT(IOS_USB,
                "SetAudioControl: bCs={:02x} bCn={:02x} bRequestType={:02x} bRequest={:02x} "
                "bIndex={:02x} aid={:08x}",
                Common::ToUnderlying(cs), cn, cmd->request_type, Common::ToUnderlying(request),
                cmd->index, aid);
  switch (aid)
  {
  case USBGETAID(FeatureUnitControlSelector::Mute, RequestCode::SetCur, 0x0200):
  {
    m_sampler.mute = memory.Read_U8(cmd->data_address) & 0x01;
    DEBUG_LOG_FMT(IOS_USB, "SetAudioControl: Setting mute to {}", m_sampler.mute.load());
    ret = 0;
    break;
  }
  case USBGETAID(FeatureUnitControlSelector::Volume, RequestCode::SetCur, 0x0200):
  {
    if (cn < 1 || cn == 0xff)
    {
      // TODO: Lego Rock Band's mic sensitivity setting provides unknown values to this.
      uint16_t vol = memory.Read_U16(cmd->data_address);
      const uint16_t original_vol = vol;

      vol -= 0x8000;
      vol = (vol * 255 + 0x4400) / 0x8800;
      vol = std::min<uint16_t>(vol, 255);

      if (m_sampler.volume != vol)
        m_sampler.volume = static_cast<u8>(vol);

      DEBUG_LOG_FMT(IOS_USB, "SetAudioControl: Setting volume to [{:02x}] [original={:04x}]",
                    m_sampler.volume.load(), original_vol);

      ret = 0;
    }
    break;
  }
  case USBGETAID(FeatureUnitControlSelector::AutomaticGain, RequestCode::SetCur, 0x0200):
  {
    ret = 0;
    break;
  }
  default:
  {
    WARN_LOG_FMT(IOS_USB,
                 "SetAudioControl: Unknown request: bCs={:02x} bCn={:02x} bRequestType={:02x} "
                 "bRequest={:02x} bIndex={:02x} aid={:08x}",
                 Common::ToUnderlying(cs), cn, cmd->request_type, Common::ToUnderlying(request),
                 cmd->index, aid);
    break;
  }
  }
  return ret;
}

int LogitechMic::EndpointAudioControl(std::unique_ptr<CtrlMessage>& cmd)
{
  auto& system = cmd->GetEmulationKernel().GetSystem();
  auto& memory = system.GetMemory();
  const auto cs = static_cast<EndpointControlSelector>(cmd->value >> 8);
  const auto request = static_cast<RequestCode>(cmd->request);
  const u8 cn = static_cast<u8>(cmd->value - 1);
  const u32 aid = USBGETAID(cs, request, cmd->index);
  int ret = IPC_STALL;
  DEBUG_LOG_FMT(IOS_USB,
                "EndpointAudioControl: bCs={:02x} bCn={:02x} bRequestType={:02x} bRequest={:02x} "
                "bIndex={:02x} aid:{:08x}",
                Common::ToUnderlying(cs), cn, cmd->request_type, Common::ToUnderlying(request),
                cmd->index, aid);
  switch (aid)
  {
  case USBGETAID(EndpointControlSelector::SamplingFreq, RequestCode::SetCur, ENDPOINT_AUDIO_IN):
  {
    if (cn == 0xff)
    {
      const uint32_t sr = memory.Read_U8(cmd->data_address) |
                          (memory.Read_U8(cmd->data_address + 1) << 8) |
                          (memory.Read_U8(cmd->data_address + 2) << 16);
      if (m_sampler.sample_rate != sr)
      {
        m_sampler.sample_rate = sr;
        if (m_microphone != nullptr)
        {
          DEBUG_LOG_FMT(IOS_USB, "EndpointAudioControl: Setting sampling rate to {:d}", sr);
          m_microphone->SetSamplingRate(sr);
        }
      }
    }
    else if (cn < 1)
    {
      WARN_LOG_FMT(IOS_USB, "EndpointAudioControl: Unsupported SAMPLER_FREQ_CONTROL set [cn={:d}]",
                   cn);
    }
    ret = 0;
    break;
  }
  case USBGETAID(EndpointControlSelector::SamplingFreq, RequestCode::GetCur, ENDPOINT_AUDIO_IN):
  {
    memory.Write_U8(m_sampler.sample_rate & 0xff, cmd->data_address + 2);
    memory.Write_U8((m_sampler.sample_rate >> 8) & 0xff, cmd->data_address + 1);
    memory.Write_U8((m_sampler.sample_rate >> 16) & 0xff, cmd->data_address);
    ret = 3;
    break;
  }
  default:
  {
    WARN_LOG_FMT(IOS_USB,
                 "SetAudioControl: Unknown request: bCs={:02x} bCn={:02x} bRequestType={:02x} "
                 "bRequest={:02x} bIndex={:02x} aid={:08x}",
                 Common::ToUnderlying(cs), cn, cmd->request_type, Common::ToUnderlying(request),
                 cmd->index, aid);
    break;
  }
  }
  return ret;
}

static constexpr std::array<u8, 121> FULL_DESCRIPTOR = {
    /* Configuration 1 */
    0x09,       /* bLength */
    0x02,       /* bDescriptorType */
    0x79, 0x00, /* wTotalLength */
    0x02,       /* bNumInterfaces */
    0x01,       /* bConfigurationValue */
    0x03,       /* iConfiguration */
    0x80,       /* bmAttributes */
    0x3c,       /* bMaxPower */

    /* Interface 0, Alternate Setting 0, Audio Control */
    0x09, /* bLength */
    0x04, /* bDescriptorType */
    0x00, /* bInterfaceNumber */
    0x00, /* bAlternateSetting */
    0x00, /* bNumEndpoints */
    0x01, /* bInterfaceClass */
    0x01, /* bInterfaceSubClass */
    0x00, /* bInterfaceProtocol */
    0x00, /* iInterface */

    /* Audio Control Interface */
    0x09,       /* bLength */
    0x24,       /* bDescriptorType */
    0x01,       /* bDescriptorSubtype */
    0x00, 0x01, /* bcdADC */
    0x27, 0x00, /* wTotalLength */
    0x01,       /* bInCollection */
    0x01,       /* baInterfaceNr */

    /* Audio Input Terminal */
    0x0c,       /* bLength */
    0x24,       /* bDescriptorType */
    0x02,       /* bDescriptorSubtype */
    0x0d,       /* bTerminalID */
    0x01, 0x02, /* wTerminalType */
    0x00,       /* bAssocTerminal */
    0x01,       /* bNrChannels */
    0x00, 0x00, /* wChannelConfig */
    0x00,       /* iChannelNames */
    0x00,       /* iTerminal */

    /* Audio Feature Unit */
    0x09, /* bLength */
    0x24, /* bDescriptorType */
    0x06, /* bDescriptorSubtype */
    0x02, /* bUnitID */
    0x0d, /* bSourceID */
    0x01, /* bControlSize */
    0x03, /* bmaControls(0) */
    0x00, /* bmaControls(1) */
    0x00, /* iFeature */

    /* Audio Output Terminal */
    0x09,       /* bLength */
    0x24,       /* bDescriptorType */
    0x03,       /* bDescriptorSubtype */
    0x0a,       /* bTerminalID */
    0x01, 0x01, /* wTerminalType */
    0x00,       /* bAssocTerminal */
    0x02,       /* bSourceID */
    0x00,       /* iTerminal */

    /* Interface 1, Alternate Setting 0, Audio Streaming - Zero Bandwith */
    0x09, /* bLength */
    0x04, /* bDescriptorType */
    0x01, /* bInterfaceNumber */
    0x00, /* bAlternateSetting */
    0x00, /* bNumEndpoints */
    0x01, /* bInterfaceClass */
    0x02, /* bInterfaceSubClass */
    0x00, /* bInterfaceProtocol */
    0x00, /* iInterface */

    /* Interface 1, Alternate Setting 1, Audio Streaming - 1 channel */
    0x09, /* bLength */
    0x04, /* bDescriptorType */
    0x01, /* bInterfaceNumber */
    0x01, /* bAlternateSetting */
    0x01, /* bNumEndpoints */
    0x01, /* bInterfaceClass */
    0x02, /* bInterfaceSubClass */
    0x00, /* bInterfaceProtocol */
    0x00, /* iInterface */

    /* Audio Streaming Interface */
    0x07,       /* bLength */
    0x24,       /* bDescriptorType */
    0x01,       /* bDescriptorSubtype */
    0x0a,       /* bTerminalLink */
    0x00,       /* bDelay */
    0x01, 0x00, /* wFormatTag */

    /* Audio Type I Format */
    0x17,             /* bLength */
    0x24,             /* bDescriptorType */
    0x02,             /* bDescriptorSubtype */
    0x01,             /* bFormatType */
    0x01,             /* bNrChannels */
    0x02,             /* bSubFrameSize */
    0x10,             /* bBitResolution */
    0x05,             /* bSamFreqType */
    0x40, 0x1f, 0x00, /* tSamFreq 1 */
    0x11, 0x2b, 0x00, /* tSamFreq 2 */
    0x22, 0x56, 0x00, /* tSamFreq 3 */
    0x44, 0xac, 0x00, /* tSamFreq 4 */
    0x80, 0xbb, 0x00, /* tSamFreq 5 */

    /* Endpoint - Standard Descriptor */
    0x09,       /* bLength */
    0x05,       /* bDescriptorType */
    0x84,       /* bEndpointAddress */
    0x0d,       /* bmAttributes */
    0x60, 0x00, /* wMaxPacketSize */
    0x01,       /* bInterval */
    0x00,       /* bRefresh */
    0x00,       /* bSynchAddress */

    /* Endpoint - Audio Streaming */
    0x07,      /* bLength */
    0x25,      /* bDescriptorType */
    0x01,      /* bDescriptor */
    0x01,      /* bmAttributes */
    0x02,      /* bLockDelayUnits */
    0x01, 0x00 /* wLockDelay */
};

static constexpr u16 LogitechUSBHDR(u8 dir, u8 type, u8 recipient, RequestCode request)
{
  return USBHDR(dir, type, recipient, Common::ToUnderlying(request));
}

int LogitechMic::SubmitTransfer(std::unique_ptr<CtrlMessage> cmd)
{
  // Reference: https://www.usb.org/sites/default/files/audio10.pdf
  DEBUG_LOG_FMT(IOS_USB,
                "[{:04x}:{:04x} {}:{}] Control: bRequestType={:02x} bRequest={:02x} wValue={:04x}"
                " wIndex={:04x} wLength={:04x}",
                m_vid, m_pid, m_index, m_active_interface, cmd->request_type, cmd->request,
                cmd->value, cmd->index, cmd->length);
  switch (cmd->request_type << 8 | cmd->request)
  {
  case USBHDR(DIR_DEVICE2HOST, TYPE_STANDARD, REC_DEVICE, REQUEST_GET_DESCRIPTOR):
  {
    // It seems every game always pokes this at first twice; one with a length of 9 which gets the
    // config, and then another with the length provided by the config.
    DEBUG_LOG_FMT(IOS_USB, "[{:04x}:{:04x} {}:{}] REQUEST_GET_DESCRIPTOR index={:04x} value={:04x}",
                  m_vid, m_pid, m_index, m_active_interface, cmd->index, cmd->value);
    cmd->FillBuffer(FULL_DESCRIPTOR.data(), std::min<size_t>(cmd->length, FULL_DESCRIPTOR.size()));
    cmd->GetEmulationKernel().EnqueueIPCReply(cmd->ios_request, IPC_SUCCESS);
    break;
  }
  case USBHDR(DIR_HOST2DEVICE, TYPE_STANDARD, REC_INTERFACE, REQUEST_SET_INTERFACE):
  {
    DEBUG_LOG_FMT(IOS_USB, "[{:04x}:{:04x} {}:{}] REQUEST_SET_INTERFACE index={:04x} value={:04x}",
                  m_vid, m_pid, m_index, m_active_interface, cmd->index, cmd->value);
    if (static_cast<u8>(cmd->index) != m_active_interface)
    {
      const int ret = ChangeInterface(static_cast<u8>(cmd->index));
      if (ret < 0)
      {
        ERROR_LOG_FMT(IOS_USB, "[{:04x}:{:04x} {}:{}] Failed to change interface to {}", m_vid,
                      m_pid, m_index, m_active_interface, cmd->index);
        return ret;
      }
    }
    const int ret = SetAltSetting(static_cast<u8>(cmd->value));
    if (ret == 0)
      cmd->GetEmulationKernel().EnqueueIPCReply(cmd->ios_request, cmd->length);
    return ret;
  }
  case LogitechUSBHDR(DIR_DEVICE2HOST, TYPE_CLASS, REC_INTERFACE, RequestCode::GetCur):
  case LogitechUSBHDR(DIR_DEVICE2HOST, TYPE_CLASS, REC_INTERFACE, RequestCode::GetMin):
  case LogitechUSBHDR(DIR_DEVICE2HOST, TYPE_CLASS, REC_INTERFACE, RequestCode::GetMax):
  case LogitechUSBHDR(DIR_DEVICE2HOST, TYPE_CLASS, REC_INTERFACE, RequestCode::GetRes):
  {
    DEBUG_LOG_FMT(IOS_USB, "[{:04x}:{:04x} {}:{}] Get Control index={:04x} value={:04x}", m_vid,
                  m_pid, m_index, m_active_interface, cmd->index, cmd->value);
    const int ret = GetAudioControl(cmd);
    if (ret < 0)
    {
      ERROR_LOG_FMT(IOS_USB,
                    "[{:04x}:{:04x} {}:{}] Get Control Failed index={:04x} value={:04x} ret={}",
                    m_vid, m_pid, m_index, m_active_interface, cmd->index, cmd->value, ret);
      cmd->GetEmulationKernel().EnqueueIPCReply(cmd->ios_request, IPC_STALL);
    }
    else
    {
      cmd->GetEmulationKernel().EnqueueIPCReply(cmd->ios_request, ret);
    }
    break;
  }
  case LogitechUSBHDR(DIR_DEVICE2HOST, TYPE_CLASS, REC_INTERFACE, RequestCode::SetCur):
  case LogitechUSBHDR(DIR_DEVICE2HOST, TYPE_CLASS, REC_INTERFACE, RequestCode::SetMin):
  case LogitechUSBHDR(DIR_DEVICE2HOST, TYPE_CLASS, REC_INTERFACE, RequestCode::SetMax):
  case LogitechUSBHDR(DIR_DEVICE2HOST, TYPE_CLASS, REC_INTERFACE, RequestCode::SetRes):
  {
    DEBUG_LOG_FMT(IOS_USB, "[{:04x}:{:04x} {}:{}] Set Control index={:04x} value={:04x}", m_vid,
                  m_pid, m_index, m_active_interface, cmd->index, cmd->value);
    const int ret = SetAudioControl(cmd);
    if (ret < 0)
    {
      ERROR_LOG_FMT(IOS_USB,
                    "[{:04x}:{:04x} {}:{}] Set Control Failed index={:04x} value={:04x} ret={}",
                    m_vid, m_pid, m_index, m_active_interface, cmd->index, cmd->value, ret);
      cmd->GetEmulationKernel().EnqueueIPCReply(cmd->ios_request, IPC_STALL);
    }
    else
    {
      cmd->GetEmulationKernel().EnqueueIPCReply(cmd->ios_request, ret);
    }
    break;
  }
  case LogitechUSBHDR(DIR_HOST2DEVICE, TYPE_CLASS, REC_ENDPOINT, RequestCode::GetCur):
  case LogitechUSBHDR(DIR_HOST2DEVICE, TYPE_CLASS, REC_ENDPOINT, RequestCode::GetMin):
  case LogitechUSBHDR(DIR_HOST2DEVICE, TYPE_CLASS, REC_ENDPOINT, RequestCode::GetMax):
  case LogitechUSBHDR(DIR_HOST2DEVICE, TYPE_CLASS, REC_ENDPOINT, RequestCode::GetRes):
  case LogitechUSBHDR(DIR_HOST2DEVICE, TYPE_CLASS, REC_ENDPOINT, RequestCode::SetCur):
  case LogitechUSBHDR(DIR_HOST2DEVICE, TYPE_CLASS, REC_ENDPOINT, RequestCode::SetMin):
  case LogitechUSBHDR(DIR_HOST2DEVICE, TYPE_CLASS, REC_ENDPOINT, RequestCode::SetMax):
  case LogitechUSBHDR(DIR_HOST2DEVICE, TYPE_CLASS, REC_ENDPOINT, RequestCode::SetRes):
  {
    DEBUG_LOG_FMT(IOS_USB, "[{:04x}:{:04x} {}:{}] REC_ENDPOINT index={:04x} value={:04x}", m_vid,
                  m_pid, m_index, m_active_interface, cmd->index, cmd->value);
    const int ret = EndpointAudioControl(cmd);
    if (ret < 0)
    {
      ERROR_LOG_FMT(
          IOS_USB,
          "[{:04x}:{:04x} {}:{}] Enndpoint Control Failed index={:04x} value={:04x} ret={}", m_vid,
          m_pid, m_index, m_active_interface, cmd->index, cmd->value, ret);
      cmd->GetEmulationKernel().EnqueueIPCReply(cmd->ios_request, IPC_STALL);
    }
    else
    {
      cmd->GetEmulationKernel().EnqueueIPCReply(cmd->ios_request, ret);
    }
    break;
  }
  case LogitechUSBHDR(DIR_HOST2DEVICE, TYPE_CLASS, REC_INTERFACE, RequestCode::SetCur):
  case LogitechUSBHDR(DIR_HOST2DEVICE, TYPE_CLASS, REC_INTERFACE, RequestCode::SetMin):
  case LogitechUSBHDR(DIR_HOST2DEVICE, TYPE_CLASS, REC_INTERFACE, RequestCode::SetMax):
  case LogitechUSBHDR(DIR_HOST2DEVICE, TYPE_CLASS, REC_INTERFACE, RequestCode::SetRes):
  {
    DEBUG_LOG_FMT(IOS_USB,
                  "[{:04x}:{:04x} {}:{}] Set Control HOST2DEVICE index={:04x} value={:04x}", m_vid,
                  m_pid, m_index, m_active_interface, cmd->index, cmd->value);
    const int ret = SetAudioControl(cmd);
    if (ret < 0)
    {
      ERROR_LOG_FMT(
          IOS_USB,
          "[{:04x}:{:04x} {}:{}] Set Control HOST2DEVICE Failed index={:04x} value={:04x} ret={}",
          m_vid, m_pid, m_index, m_active_interface, cmd->index, cmd->value, ret);
      cmd->GetEmulationKernel().EnqueueIPCReply(cmd->ios_request, IPC_STALL);
    }
    else
    {
      cmd->GetEmulationKernel().EnqueueIPCReply(cmd->ios_request, ret);
    }
    break;
  }
  default:
    NOTICE_LOG_FMT(IOS_USB, "Unknown command");
    cmd->GetEmulationKernel().EnqueueIPCReply(cmd->ios_request, IPC_STALL);
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
  default:
  {
    WARN_LOG_FMT(IOS_USB,
                 "Logitech Mic isochronous transfer, unsupported endpoint: length={:04x} "
                 "endpoint={:02x} num_packets={:02x}",
                 cmd->length, cmd->endpoint, cmd->num_packets);
  }
  }

  cmd->FillBuffer(packets, cmd->length);
  cmd->ScheduleTransferCompletion(cmd->length, 1000);
  return IPC_SUCCESS;
}
}  // namespace IOS::HLE::USB
