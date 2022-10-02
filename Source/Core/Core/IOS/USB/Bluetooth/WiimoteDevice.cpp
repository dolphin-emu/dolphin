// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/IOS/USB/Bluetooth/WiimoteDevice.h"

#include <cstring>
#include <memory>
#include <utility>
#include <vector>

#include <fmt/format.h>

#include "Common/Assert.h"
#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Common/StringUtil.h"
#include "Common/Swap.h"
#include "Core/Core.h"
#include "Core/HW/Wiimote.h"
#include "Core/HW/WiimoteCommon/WiimoteConstants.h"
#include "Core/HW/WiimoteCommon/WiimoteHid.h"
#include "Core/HW/WiimoteEmu/DesiredWiimoteState.h"
#include "Core/Host.h"
#include "Core/IOS/USB/Bluetooth/BTEmu.h"
#include "Core/IOS/USB/Bluetooth/WiimoteHIDAttr.h"
#include "Core/IOS/USB/Bluetooth/l2cap.h"

namespace IOS::HLE
{
class CBigEndianBuffer
{
public:
  explicit CBigEndianBuffer(u8* buffer) : m_buffer(buffer) {}
  u8 Read8(u32 offset) const { return m_buffer[offset]; }
  u16 Read16(u32 offset) const { return Common::swap16(&m_buffer[offset]); }
  u32 Read32(u32 offset) const { return Common::swap32(&m_buffer[offset]); }
  void Write8(u32 offset, u8 data) { m_buffer[offset] = data; }
  void Write16(u32 offset, u16 data)
  {
    const u16 swapped = Common::swap16(data);
    std::memcpy(&m_buffer[offset], &swapped, sizeof(u16));
  }
  void Write32(u32 offset, u32 data)
  {
    const u32 swapped = Common::swap32(data);
    std::memcpy(&m_buffer[offset], &swapped, sizeof(u32));
  }
  u8* GetPointer(u32 offset) { return &m_buffer[offset]; }

private:
  u8* m_buffer;
};

constexpr int CONNECTION_MESSAGE_TIME = 3000;

WiimoteDevice::WiimoteDevice(BluetoothEmuDevice* host, bdaddr_t bd, unsigned int hid_source_number)
    : m_host(host), m_bd(bd),
      m_name(GetNumber() == WIIMOTE_BALANCE_BOARD ? "Nintendo RVL-WBC-01" : "Nintendo RVL-CNT-01")

{
  INFO_LOG_FMT(IOS_WIIMOTE, "Wiimote: #{} Constructed", GetNumber());

  m_link_key.fill(0xa0 + GetNumber());
  m_class = {0x00, 0x04, 0x48};
  m_features = {0xBC, 0x02, 0x04, 0x38, 0x08, 0x00, 0x00, 0x00};
  m_lmp_version = 0x2;
  m_lmp_subversion = 0x229;

  const auto hid_source = WiimoteCommon::GetHIDWiimoteSource(hid_source_number);

  if (hid_source)
  {
    hid_source->SetWiimoteDeviceIndex(GetNumber());

    // UGLY: This prevents an OSD message in SetSource -> Activate.
    SetBasebandState(BasebandState::RequestConnection);
  }

  SetSource(hid_source);
}

WiimoteDevice::~WiimoteDevice() = default;

WiimoteDevice::SChannel::SChannel() : psm(L2CAP_PSM_ANY), remote_cid(L2CAP_NULL_CID)
{
}

bool WiimoteDevice::SChannel::IsAccepted() const
{
  return remote_cid != L2CAP_NULL_CID;
}

bool WiimoteDevice::SChannel::IsRemoteConfigured() const
{
  return remote_mtu != 0;
}

bool WiimoteDevice::SChannel::IsComplete() const
{
  return IsAccepted() && IsRemoteConfigured() && state == State::Complete;
}

void WiimoteDevice::DoState(PointerWrap& p)
{
  p.Do(m_baseband_state);
  p.Do(m_hid_state);
  p.Do(m_bd);
  p.Do(m_class);
  p.Do(m_features);
  p.Do(m_lmp_version);
  p.Do(m_lmp_subversion);
  p.Do(m_link_key);
  p.Do(m_name);
  p.Do(m_channels);
  p.Do(m_connection_request_counter);
}

u32 WiimoteDevice::GetNumber() const
{
  return GetBD().back();
}

bool WiimoteDevice::IsInquiryScanEnabled() const
{
  // Our Wii Remote is conveniently discoverable as long as it's enabled and doesn't have a
  // baseband connection.
  return !IsConnected() && IsSourceValid();
}

bool WiimoteDevice::IsPageScanEnabled() const
{
  // Our Wii Remote will accept a connection as long as it isn't currently connected.
  return !IsConnected() && IsSourceValid();
}

void WiimoteDevice::SetBasebandState(BasebandState new_state)
{
  // Prevent button press from immediately causing connection attempts.
  m_connection_request_counter = ::Wiimote::UPDATE_FREQ;

  const bool was_connected = IsConnected();

  m_baseband_state = new_state;

  // Update wiimote connection checkboxes in UI.
  Host_UpdateDisasmDialog();

  if (!IsSourceValid())
    return;

  if (IsConnected() && !was_connected)
    m_hid_source->EventLinked();
  else if (!IsConnected() && was_connected)
    m_hid_source->EventUnlinked();
}

const WiimoteDevice::SChannel* WiimoteDevice::FindChannelWithPSM(u16 psm) const
{
  for (auto& [cid, channel] : m_channels)
  {
    if (channel.psm == psm)
      return &channel;
  }

  return nullptr;
}

WiimoteDevice::SChannel* WiimoteDevice::FindChannelWithPSM(u16 psm)
{
  for (auto& [cid, channel] : m_channels)
  {
    if (channel.psm == psm)
      return &channel;
  }

  return nullptr;
}

u16 WiimoteDevice::GenerateChannelID() const
{
  // "Identifiers from 0x0001 to 0x003F are reserved"
  constexpr u16 starting_id = 0x40;

  u16 cid = starting_id;

  while (m_channels.count(cid) != 0)
    ++cid;

  return cid;
}

bool WiimoteDevice::LinkChannel(u16 psm)
{
  const auto* const channel = FindChannelWithPSM(psm);

  // Attempt to connect the channel.
  if (!channel)
  {
    SendConnectionRequest(psm);
    return false;
  }

  return channel->IsComplete();
}

bool WiimoteDevice::IsSourceValid() const
{
  return m_hid_source != nullptr;
}

bool WiimoteDevice::IsConnected() const
{
  return m_baseband_state == BasebandState::Complete;
}

void WiimoteDevice::Activate(bool connect)
{
  if (connect && m_baseband_state == BasebandState::Inactive)
  {
    SetBasebandState(BasebandState::RequestConnection);

    Core::DisplayMessage(fmt::format("Wii Remote {} connected", GetNumber() + 1),
                         CONNECTION_MESSAGE_TIME);
  }
  else if (!connect && IsConnected())
  {
    Reset();

    // Does a real remote gracefully disconnect l2cap channels first?
    // Not doing that doesn't seem to break anything.
    m_host->RemoteDisconnect(GetBD());

    Core::DisplayMessage(fmt::format("Wii Remote {} disconnected", GetNumber() + 1),
                         CONNECTION_MESSAGE_TIME);
  }
}

bool WiimoteDevice::EventConnectionRequest()
{
  if (!IsPageScanEnabled())
    return false;

  Core::DisplayMessage(
      fmt::format("Wii Remote {} connected from emulated software", GetNumber() + 1),
      CONNECTION_MESSAGE_TIME);

  SetBasebandState(BasebandState::Complete);

  return true;
}

bool WiimoteDevice::EventConnectionAccept()
{
  if (!IsPageScanEnabled())
    return false;

  SetBasebandState(BasebandState::Complete);

  // A connection acceptance means the remote seeked out the connection.
  // In this situation the remote actively creates HID channels.
  m_hid_state = HIDState::Linking;

  return true;
}

void WiimoteDevice::EventDisconnect(u8 reason)
{
  // If someone wants to be fancy we could also figure out the values for reason
  // and display things like "Wii Remote %i disconnected due to inactivity!" etc.
  // FYI: It looks like reason is always 0x13 (User Ended Connection).

  Core::DisplayMessage(
      fmt::format("Wii Remote {} disconnected by emulated software", GetNumber() + 1),
      CONNECTION_MESSAGE_TIME);

  Reset();
}

void WiimoteDevice::SetSource(WiimoteCommon::HIDWiimote* hid_source)
{
  if (m_hid_source && IsConnected())
  {
    Activate(false);
  }

  m_hid_source = hid_source;

  if (m_hid_source)
  {
    m_hid_source->SetInterruptCallback(std::bind(&WiimoteDevice::InterruptDataInputCallback, this,
                                                 std::placeholders::_1, std::placeholders::_2,
                                                 std::placeholders::_3));
    Activate(true);
  }
}

void WiimoteDevice::Reset()
{
  SetBasebandState(BasebandState::Inactive);
  m_hid_state = HIDState::Inactive;
  m_channels = {};
}

void WiimoteDevice::Update()
{
  if (m_baseband_state == BasebandState::RequestConnection)
  {
    if (m_host->RemoteConnect(*this))
    {
      // After a connection request is visible to the controller switch to inactive.
      SetBasebandState(BasebandState::Inactive);
    }
  }

  if (!IsConnected())
    return;

  // Send configuration for any newly connected channels.
  for (auto& [cid, channel] : m_channels)
  {
    if (channel.IsAccepted() && channel.state == SChannel::State::Inactive)
    {
      // A real wii remote has been observed requesting this MTU.
      constexpr u16 REQUEST_MTU = 185;

      channel.state = SChannel::State::ConfigurationPending;
      SendConfigurationRequest(channel.remote_cid, REQUEST_MTU, L2CAP_FLUSH_TIMO_DEFAULT);
    }
  }

  // If the connection originated from the wii remote it will create
  // HID control and interrupt channels (in that order).
  if (m_hid_state == HIDState::Linking)
  {
    if (LinkChannel(L2CAP_PSM_HID_CNTL) && LinkChannel(L2CAP_PSM_HID_INTR))
    {
      DEBUG_LOG_FMT(IOS_WIIMOTE, "HID linking is complete.");
      m_hid_state = HIDState::Inactive;
    }
  }
}

WiimoteDevice::NextUpdateInputCall
WiimoteDevice::PrepareInput(WiimoteEmu::DesiredWiimoteState* wiimote_state)
{
  if (m_connection_request_counter)
    --m_connection_request_counter;

  if (!IsSourceValid())
    return NextUpdateInputCall::None;

  if (m_baseband_state == BasebandState::Inactive)
  {
    // Allow button press to trigger activation after a second of no connection activity.
    if (!m_connection_request_counter)
    {
      wiimote_state->buttons = m_hid_source->GetCurrentlyPressedButtons();
      return NextUpdateInputCall::Activate;
    }
    return NextUpdateInputCall::None;
  }

  // Verify interrupt channel is connected and configured.
  const auto* channel = FindChannelWithPSM(L2CAP_PSM_HID_INTR);
  if (channel && channel->IsComplete())
  {
    m_hid_source->PrepareInput(wiimote_state);
    return NextUpdateInputCall::Update;
  }
  return NextUpdateInputCall::None;
}

void WiimoteDevice::UpdateInput(NextUpdateInputCall next_call,
                                const WiimoteEmu::DesiredWiimoteState& wiimote_state)
{
  switch (next_call)
  {
  case NextUpdateInputCall::Activate:
    if (wiimote_state.buttons.hex & WiimoteCommon::ButtonData::BUTTON_MASK)
      Activate(true);
    break;
  case NextUpdateInputCall::Update:
    m_hid_source->Update(wiimote_state);
    break;
  default:
    break;
  }
}

// This function receives L2CAP commands from the CPU
void WiimoteDevice::ExecuteL2capCmd(u8* ptr, u32 size)
{
  l2cap_hdr_t* header = (l2cap_hdr_t*)ptr;
  u8* data = ptr + sizeof(l2cap_hdr_t);
  const u32 data_size = size - sizeof(l2cap_hdr_t);
  DEBUG_LOG_FMT(IOS_WIIMOTE, "  CID {:#06x}, Len {:#x}, DataSize {:#x}", header->dcid,
                header->length, data_size);

  if (header->length != data_size)
  {
    INFO_LOG_FMT(IOS_WIIMOTE, "Faulty packet. It is dropped.");
    return;
  }

  if (header->dcid == L2CAP_SIGNAL_CID)
  {
    SignalChannel(data, data_size);
    return;
  }

  const auto itr = m_channels.find(header->dcid);
  if (itr == m_channels.end())
  {
    ERROR_LOG_FMT(IOS_WIIMOTE, "L2CAP: SendACLPacket to unknown channel {}", header->dcid);
    return;
  }

  const SChannel& channel = itr->second;
  switch (channel.psm)
  {
  case L2CAP_PSM_SDP:
    HandleSDP(header->dcid, data, data_size);
    break;

  // Original (non-TR) remotes process "set reports" on control channel.
  // Commercial games don't use this. Some homebrew does. (e.g. Gecko OS)
  case L2CAP_PSM_HID_CNTL:
  {
    const u8 hid_type = data[0];
    if (hid_type == ((WiimoteCommon::HID_TYPE_SET_REPORT << 4) | WiimoteCommon::HID_PARAM_OUTPUT))
    {
      struct DataFrame
      {
        l2cap_hdr_t header;
        u8 hid_type;
      } data_frame;

      static_assert(sizeof(data_frame) == sizeof(data_frame.hid_type) + sizeof(l2cap_hdr_t));

      data_frame.header.dcid = channel.remote_cid;
      data_frame.header.length = sizeof(data_frame.hid_type);
      data_frame.hid_type = WiimoteCommon::HID_HANDSHAKE_SUCCESS;

      m_host->SendACLPacket(GetBD(), reinterpret_cast<const u8*>(&data_frame), sizeof(data_frame));

      // Does the wii remote reply on the control or interrupt channel in this situation?
      m_hid_source->InterruptDataOutput(data + sizeof(hid_type), data_size - sizeof(hid_type));
    }
    else
    {
      ERROR_LOG_FMT(IOS_WIIMOTE, "Unknown HID-type ({:#x}) on L2CAP_PSM_HID_CNTL", hid_type);
    }
  }
  break;

  case L2CAP_PSM_HID_INTR:
  {
    const u8 hid_type = data[0];
    if (hid_type == ((WiimoteCommon::HID_TYPE_DATA << 4) | WiimoteCommon::HID_PARAM_OUTPUT))
      m_hid_source->InterruptDataOutput(data + sizeof(hid_type), data_size - sizeof(hid_type));
    else
      ERROR_LOG_FMT(IOS_WIIMOTE, "Unknown HID-type ({:#x}) on L2CAP_PSM_HID_INTR", hid_type);
  }
  break;

  default:
    ERROR_LOG_FMT(IOS_WIIMOTE, "Channel {:#x} has unknown PSM {:x}", header->dcid, channel.psm);
    break;
  }
}

void WiimoteDevice::SignalChannel(u8* data, u32 size)
{
  while (size >= sizeof(l2cap_cmd_hdr_t))
  {
    l2cap_cmd_hdr_t* cmd_hdr = (l2cap_cmd_hdr_t*)data;
    data += sizeof(l2cap_cmd_hdr_t);
    size = size - sizeof(l2cap_cmd_hdr_t) - cmd_hdr->length;

    switch (cmd_hdr->code)
    {
    case L2CAP_COMMAND_REJ:
      ERROR_LOG_FMT(IOS_WIIMOTE, "SignalChannel - L2CAP_COMMAND_REJ (something went wrong)."
                                 "Try to replace your SYSCONF file with a new copy.");
      break;

    case L2CAP_CONNECT_REQ:
      ReceiveConnectionReq(cmd_hdr->ident, data, cmd_hdr->length);
      break;

    case L2CAP_CONNECT_RSP:
      ReceiveConnectionResponse(cmd_hdr->ident, data, cmd_hdr->length);
      break;

    case L2CAP_CONFIG_REQ:
      ReceiveConfigurationReq(cmd_hdr->ident, data, cmd_hdr->length);
      break;

    case L2CAP_CONFIG_RSP:
      ReceiveConfigurationResponse(cmd_hdr->ident, data, cmd_hdr->length);
      break;

    case L2CAP_DISCONNECT_REQ:
      ReceiveDisconnectionReq(cmd_hdr->ident, data, cmd_hdr->length);
      break;

    default:
      ERROR_LOG_FMT(IOS_WIIMOTE, "  Unknown Command-Code ({:#04x})", cmd_hdr->code);
      return;
    }

    data += cmd_hdr->length;
  }
}

void WiimoteDevice::ReceiveConnectionReq(u8 ident, u8* data, u32 size)
{
  l2cap_con_req_cp* command_connection_req = (l2cap_con_req_cp*)data;

  DEBUG_LOG_FMT(IOS_WIIMOTE, "[L2CAP] ReceiveConnectionRequest");
  DEBUG_LOG_FMT(IOS_WIIMOTE, "    Ident: {:#04x}", ident);
  DEBUG_LOG_FMT(IOS_WIIMOTE, "    PSM: {:#06x}", command_connection_req->psm);
  DEBUG_LOG_FMT(IOS_WIIMOTE, "    SCID: {:#06x}", command_connection_req->scid);

  l2cap_con_rsp_cp rsp = {};
  rsp.scid = command_connection_req->scid;
  rsp.status = L2CAP_NO_INFO;

  if (FindChannelWithPSM(command_connection_req->psm) != nullptr)
  {
    ERROR_LOG_FMT(IOS_WIIMOTE, "Multiple channels with same PSM ({}) are not allowed.",
                  command_connection_req->psm);

    // A real wii remote refuses multiple connections with the same PSM.
    rsp.result = L2CAP_NO_RESOURCES;
    rsp.dcid = L2CAP_NULL_CID;
  }
  else
  {
    // Create the channel.
    const u16 local_cid = GenerateChannelID();

    SChannel& channel = m_channels[local_cid];
    channel.psm = command_connection_req->psm;
    channel.remote_cid = command_connection_req->scid;

    if (channel.psm != L2CAP_PSM_SDP && channel.psm != L2CAP_PSM_HID_CNTL &&
        channel.psm != L2CAP_PSM_HID_INTR)
    {
      WARN_LOG_FMT(IOS_WIIMOTE, "L2CAP connection with unknown psm ({:#x})", channel.psm);
    }

    rsp.result = L2CAP_SUCCESS;
    rsp.dcid = local_cid;
  }

  DEBUG_LOG_FMT(IOS_WIIMOTE, "[L2CAP] SendConnectionResponse");
  SendCommandToACL(ident, L2CAP_CONNECT_RSP, sizeof(l2cap_con_rsp_cp), (u8*)&rsp);
}

void WiimoteDevice::ReceiveConnectionResponse(u8 ident, u8* data, u32 size)
{
  l2cap_con_rsp_cp* rsp = (l2cap_con_rsp_cp*)data;

  DEBUG_ASSERT(size == sizeof(l2cap_con_rsp_cp));

  DEBUG_LOG_FMT(IOS_WIIMOTE, "[L2CAP] ReceiveConnectionResponse");
  DEBUG_LOG_FMT(IOS_WIIMOTE, "    DCID: {:#06x}", rsp->dcid);
  DEBUG_LOG_FMT(IOS_WIIMOTE, "    SCID: {:#06x}", rsp->scid);
  DEBUG_LOG_FMT(IOS_WIIMOTE, "    Result: {:#06x}", rsp->result);
  DEBUG_LOG_FMT(IOS_WIIMOTE, "    Status: {:#06x}", rsp->status);

  DEBUG_ASSERT(rsp->result == L2CAP_SUCCESS);
  DEBUG_ASSERT(rsp->status == L2CAP_NO_INFO);
  DEBUG_ASSERT(DoesChannelExist(rsp->scid));

  SChannel& channel = m_channels[rsp->scid];
  channel.remote_cid = rsp->dcid;
}

void WiimoteDevice::ReceiveConfigurationReq(u8 ident, u8* data, u32 size)
{
  u32 offset = 0;
  l2cap_cfg_req_cp* command_config_req = (l2cap_cfg_req_cp*)data;

  // Flags being 1 means that the options are sent in multi-packets
  DEBUG_ASSERT(command_config_req->flags == 0x00);
  DEBUG_ASSERT(DoesChannelExist(command_config_req->dcid));

  SChannel& channel = m_channels[command_config_req->dcid];

  DEBUG_LOG_FMT(IOS_WIIMOTE, "[L2CAP] ReceiveConfigurationRequest");
  DEBUG_LOG_FMT(IOS_WIIMOTE, "    Ident: {:#04x}", ident);
  DEBUG_LOG_FMT(IOS_WIIMOTE, "    DCID: {:#06x}", command_config_req->dcid);
  DEBUG_LOG_FMT(IOS_WIIMOTE, "    Flags: {:#06x}", command_config_req->flags);

  offset += sizeof(l2cap_cfg_req_cp);

  u8 temp_buffer[1024];
  u32 resp_len = 0;

  l2cap_cfg_rsp_cp* rsp = (l2cap_cfg_rsp_cp*)temp_buffer;
  rsp->scid = channel.remote_cid;
  rsp->flags = 0x00;
  rsp->result = L2CAP_SUCCESS;

  resp_len += sizeof(l2cap_cfg_rsp_cp);

  // If the option is not provided, configure the default.
  u16 remote_mtu = L2CAP_MTU_DEFAULT;

  // Read configuration options.
  while (offset < size)
  {
    l2cap_cfg_opt_t* options = (l2cap_cfg_opt_t*)&data[offset];
    offset += sizeof(l2cap_cfg_opt_t);

    switch (options->type)
    {
    case L2CAP_OPT_MTU:
    {
      DEBUG_ASSERT(options->length == L2CAP_OPT_MTU_SIZE);
      l2cap_cfg_opt_val_t* mtu = (l2cap_cfg_opt_val_t*)&data[offset];
      remote_mtu = mtu->mtu;
      DEBUG_LOG_FMT(IOS_WIIMOTE, "    MTU: {:#06x}", mtu->mtu);
    }
    break;

    // We don't care what the flush timeout is. Our packets are not dropped.
    case L2CAP_OPT_FLUSH_TIMO:
    {
      DEBUG_ASSERT(options->length == L2CAP_OPT_FLUSH_TIMO_SIZE);
      l2cap_cfg_opt_val_t* flush_time_out = (l2cap_cfg_opt_val_t*)&data[offset];
      DEBUG_LOG_FMT(IOS_WIIMOTE, "    FlushTimeOut: {:#06x}", flush_time_out->flush_timo);
    }
    break;

    default:
      DEBUG_ASSERT_MSG(IOS_WIIMOTE, 0, "Unknown Option: {:#04x}", options->type);
      break;
    }

    offset += options->length;

    const u32 option_size = sizeof(l2cap_cfg_opt_t) + options->length;
    memcpy(&temp_buffer[resp_len], options, option_size);
    resp_len += option_size;
  }

  DEBUG_LOG_FMT(IOS_WIIMOTE, "[L2CAP] SendConfigurationResponse");
  SendCommandToACL(ident, L2CAP_CONFIG_RSP, resp_len, temp_buffer);

  channel.remote_mtu = remote_mtu;
}

void WiimoteDevice::ReceiveConfigurationResponse(u8 ident, u8* data, u32 size)
{
  l2cap_cfg_rsp_cp* rsp = (l2cap_cfg_rsp_cp*)data;

  DEBUG_LOG_FMT(IOS_WIIMOTE, "[L2CAP] ReceiveConfigurationResponse");
  DEBUG_LOG_FMT(IOS_WIIMOTE, "    SCID: {:#06x}", rsp->scid);
  DEBUG_LOG_FMT(IOS_WIIMOTE, "    Flags: {:#06x}", rsp->flags);
  DEBUG_LOG_FMT(IOS_WIIMOTE, "    Result: {:#06x}", rsp->result);

  DEBUG_ASSERT(rsp->result == L2CAP_SUCCESS);

  m_channels[rsp->scid].state = SChannel::State::Complete;
}

void WiimoteDevice::ReceiveDisconnectionReq(u8 ident, u8* data, u32 size)
{
  l2cap_discon_req_cp* command_disconnection_req = (l2cap_discon_req_cp*)data;

  DEBUG_LOG_FMT(IOS_WIIMOTE, "[L2CAP] ReceiveDisconnectionReq");
  DEBUG_LOG_FMT(IOS_WIIMOTE, "    Ident: {:#04x}", ident);
  DEBUG_LOG_FMT(IOS_WIIMOTE, "    DCID: {:#06x}", command_disconnection_req->dcid);
  DEBUG_LOG_FMT(IOS_WIIMOTE, "    SCID: {:#06x}", command_disconnection_req->scid);

  DEBUG_ASSERT(DoesChannelExist(command_disconnection_req->dcid));

  m_channels.erase(command_disconnection_req->dcid);

  l2cap_discon_req_cp rsp;
  rsp.dcid = command_disconnection_req->dcid;
  rsp.scid = command_disconnection_req->scid;

  DEBUG_LOG_FMT(IOS_WIIMOTE, "[L2CAP] SendDisconnectionResponse");
  SendCommandToACL(ident, L2CAP_DISCONNECT_RSP, sizeof(l2cap_discon_req_cp), (u8*)&rsp);
}

void WiimoteDevice::SendConnectionRequest(u16 psm)
{
  DEBUG_ASSERT(FindChannelWithPSM(psm) == nullptr);

  const u16 local_cid = GenerateChannelID();

  // Create the channel.
  SChannel& channel = m_channels[local_cid];
  channel.psm = psm;

  l2cap_con_req_cp cr;
  cr.psm = psm;
  cr.scid = local_cid;

  DEBUG_LOG_FMT(IOS_WIIMOTE, "[L2CAP] SendConnectionRequest");
  DEBUG_LOG_FMT(IOS_WIIMOTE, "    Psm: {:#06x}", cr.psm);
  DEBUG_LOG_FMT(IOS_WIIMOTE, "    Scid: {:#06x}", cr.scid);

  SendCommandToACL(L2CAP_CONNECT_REQ, L2CAP_CONNECT_REQ, sizeof(l2cap_con_req_cp), (u8*)&cr);
}

void WiimoteDevice::SendConfigurationRequest(u16 cid, u16 mtu, u16 flush_time_out)
{
  u8 buffer[1024];
  int offset = 0;

  l2cap_cfg_req_cp* cr = (l2cap_cfg_req_cp*)&buffer[offset];
  cr->dcid = cid;
  cr->flags = 0;
  offset += sizeof(l2cap_cfg_req_cp);

  DEBUG_LOG_FMT(IOS_WIIMOTE, "[L2CAP] SendConfigurationRequest");
  DEBUG_LOG_FMT(IOS_WIIMOTE, "    Dcid: {:#06x}", cr->dcid);
  DEBUG_LOG_FMT(IOS_WIIMOTE, "    Flags: {:#06x}", cr->flags);

  l2cap_cfg_opt_t* options;

  if (mtu != L2CAP_MTU_DEFAULT)
  {
    options = (l2cap_cfg_opt_t*)&buffer[offset];
    offset += sizeof(l2cap_cfg_opt_t);
    options->type = L2CAP_OPT_MTU;
    options->length = L2CAP_OPT_MTU_SIZE;
    *(u16*)&buffer[offset] = mtu;
    offset += L2CAP_OPT_MTU_SIZE;
    DEBUG_LOG_FMT(IOS_WIIMOTE, "    MTU: {:#06x}", mtu);
  }

  if (flush_time_out != L2CAP_FLUSH_TIMO_DEFAULT)
  {
    options = (l2cap_cfg_opt_t*)&buffer[offset];
    offset += sizeof(l2cap_cfg_opt_t);
    options->type = L2CAP_OPT_FLUSH_TIMO;
    options->length = L2CAP_OPT_FLUSH_TIMO_SIZE;
    *(u16*)&buffer[offset] = flush_time_out;
    offset += L2CAP_OPT_FLUSH_TIMO_SIZE;
    DEBUG_LOG_FMT(IOS_WIIMOTE, "    FlushTimeOut: {:#06x}", flush_time_out);
  }

  SendCommandToACL(L2CAP_CONFIG_REQ, L2CAP_CONFIG_REQ, offset, buffer);
}

[[maybe_unused]] constexpr u8 SDP_UINT8 = 0x08;
[[maybe_unused]] constexpr u8 SDP_UINT16 = 0x09;
constexpr u8 SDP_UINT32 = 0x0A;
constexpr u8 SDP_SEQ8 = 0x35;
[[maybe_unused]] constexpr u8 SDP_SEQ16 = 0x36;

void WiimoteDevice::SDPSendServiceSearchResponse(u16 cid, u16 transaction_id,
                                                 u8* service_search_pattern,
                                                 u16 maximum_service_record_count)
{
  // verify block... we handle search pattern for HID service only
  {
    CBigEndianBuffer buffer(service_search_pattern);
    DEBUG_ASSERT(buffer.Read8(0) == SDP_SEQ8);  // data sequence
    DEBUG_ASSERT(buffer.Read8(1) == 0x03);      // sequence size

    // HIDClassID
    DEBUG_ASSERT(buffer.Read8(2) == 0x19);
    DEBUG_ASSERT(buffer.Read16(3) == 0x1124);
  }

  u8 data_frame[1000];
  CBigEndianBuffer buffer(data_frame);

  int offset = 0;
  l2cap_hdr_t* header = (l2cap_hdr_t*)&data_frame[offset];
  offset += sizeof(l2cap_hdr_t);
  header->dcid = cid;

  buffer.Write8(offset, 0x03);
  offset++;
  buffer.Write16(offset, transaction_id);
  offset += 2;  // Transaction ID
  buffer.Write16(offset, 0x0009);
  offset += 2;  // Param length
  buffer.Write16(offset, 0x0001);
  offset += 2;  // TotalServiceRecordCount
  buffer.Write16(offset, 0x0001);
  offset += 2;  // CurrentServiceRecordCount
  buffer.Write32(offset, 0x10000);
  offset += 4;  // ServiceRecordHandleList[4]
  buffer.Write8(offset, 0x00);
  offset++;  // No continuation state;

  header->length = (u16)(offset - sizeof(l2cap_hdr_t));
  m_host->SendACLPacket(GetBD(), data_frame, header->length + sizeof(l2cap_hdr_t));
}

static u32 ParseCont(u8* cont)
{
  u32 attrib_offset = 0;
  CBigEndianBuffer attrib_list(cont);
  const u8 type_id = attrib_list.Read8(attrib_offset);
  attrib_offset++;

  if (type_id == 0x02)
  {
    return attrib_list.Read16(attrib_offset);
  }
  else if (type_id == 0x00)
  {
    return 0x00;
  }
  ERROR_LOG_FMT(IOS_WIIMOTE, "ParseCont: wrong cont: {}", type_id);
  PanicAlertFmt("ParseCont: wrong cont: {}", type_id);
  return 0;
}

static int ParseAttribList(u8* attrib_id_list, u16& start_id, u16& end_id)
{
  u32 attrib_offset = 0;
  CBigEndianBuffer attrib_list(attrib_id_list);

  const u8 sequence = attrib_list.Read8(attrib_offset);
  attrib_offset++;
  [[maybe_unused]] const u8 seq_size = attrib_list.Read8(attrib_offset);
  attrib_offset++;
  const u8 type_id = attrib_list.Read8(attrib_offset);
  attrib_offset++;

  DEBUG_ASSERT(sequence == SDP_SEQ8);

  if (type_id == SDP_UINT32)
  {
    start_id = attrib_list.Read16(attrib_offset);
    attrib_offset += 2;
    end_id = attrib_list.Read16(attrib_offset);
    attrib_offset += 2;
  }
  else
  {
    start_id = attrib_list.Read16(attrib_offset);
    attrib_offset += 2;
    end_id = start_id;
    WARN_LOG_FMT(IOS_WIIMOTE, "Read just a single attrib - not tested");
    PanicAlertFmt("Read just a single attrib - not tested");
  }

  return attrib_offset;
}

void WiimoteDevice::SDPSendServiceAttributeResponse(u16 cid, u16 transaction_id, u32 service_handle,
                                                    u16 start_attr_id, u16 end_attr_id,
                                                    u16 maximum_attribute_byte_count,
                                                    u8* continuation_state)
{
  if (service_handle != 0x10000)
  {
    ERROR_LOG_FMT(IOS_WIIMOTE, "Unknown service handle {:x}", service_handle);
    PanicAlertFmt("Unknown service handle {:x}", service_handle);
  }

  const u32 cont_state = ParseCont(continuation_state);

  u32 packet_size = 0;
  const u8* packet = GetAttribPacket(service_handle, cont_state, packet_size);

  // generate package
  u8 data_frame[1000];
  CBigEndianBuffer buffer(data_frame);

  int offset = 0;
  l2cap_hdr_t* header = (l2cap_hdr_t*)&data_frame[offset];
  offset += sizeof(l2cap_hdr_t);
  header->dcid = cid;

  buffer.Write8(offset, 0x05);
  offset++;
  buffer.Write16(offset, transaction_id);
  offset += 2;  // Transaction ID

  memcpy(buffer.GetPointer(offset), packet, packet_size);
  offset += packet_size;

  header->length = (u16)(offset - sizeof(l2cap_hdr_t));
  m_host->SendACLPacket(GetBD(), data_frame, header->length + sizeof(l2cap_hdr_t));
}

void WiimoteDevice::HandleSDP(u16 cid, u8* data, u32 size)
{
  CBigEndianBuffer buffer(data);

  switch (buffer.Read8(0))
  {
  // SDP_ServiceSearchRequest
  case 0x02:
  {
    WARN_LOG_FMT(IOS_WIIMOTE, "!!! SDP_ServiceSearchRequest !!!");

    DEBUG_ASSERT(size == 13);

    const u16 transaction_id = buffer.Read16(1);
    u8* service_search_pattern = buffer.GetPointer(5);
    const u16 maximum_service_record_count = buffer.Read16(10);

    SDPSendServiceSearchResponse(cid, transaction_id, service_search_pattern,
                                 maximum_service_record_count);
  }
  break;

  // SDP_ServiceAttributeRequest
  case 0x04:
  {
    WARN_LOG_FMT(IOS_WIIMOTE, "!!! SDP_ServiceAttributeRequest !!!");

    u16 start_attr_id, end_attr_id;
    u32 offset = 1;

    const u16 transaction_id = buffer.Read16(offset);
    offset += 2;
    // u16 parameter_length = buffer.Read16(offset);
    offset += 2;
    const u32 service_handle = buffer.Read32(offset);
    offset += 4;
    const u16 maximum_attribute_byte_count = buffer.Read16(offset);
    offset += 2;
    offset += ParseAttribList(buffer.GetPointer(offset), start_attr_id, end_attr_id);
    u8* continuation_state = buffer.GetPointer(offset);

    SDPSendServiceAttributeResponse(cid, transaction_id, service_handle, start_attr_id, end_attr_id,
                                    maximum_attribute_byte_count, continuation_state);
  }
  break;

  default:
    ERROR_LOG_FMT(IOS_WIIMOTE, "Unknown SDP command {:x}", data[0]);
    PanicAlertFmt("WIIMOTE: Unknown SDP command {:x}", data[0]);
    break;
  }
}

void WiimoteDevice::SendCommandToACL(u8 ident, u8 code, u8 command_length, u8* command_data)
{
  u8 data_frame[1024];
  u32 offset = 0;

  l2cap_hdr_t* header = (l2cap_hdr_t*)&data_frame[offset];
  offset += sizeof(l2cap_hdr_t);
  header->length = sizeof(l2cap_cmd_hdr_t) + command_length;
  header->dcid = L2CAP_SIGNAL_CID;

  l2cap_cmd_hdr_t* command = (l2cap_cmd_hdr_t*)&data_frame[offset];
  offset += sizeof(l2cap_cmd_hdr_t);
  command->code = code;
  command->ident = ident;
  command->length = command_length;

  memcpy(&data_frame[offset], command_data, command_length);

  DEBUG_LOG_FMT(IOS_WIIMOTE, "Send ACL Command to CPU");
  DEBUG_LOG_FMT(IOS_WIIMOTE, "    Ident: {:#04x}", ident);
  DEBUG_LOG_FMT(IOS_WIIMOTE, "    Code: {:#04x}", code);

  m_host->SendACLPacket(GetBD(), data_frame, header->length + sizeof(l2cap_hdr_t));
}

void WiimoteDevice::InterruptDataInputCallback(u8 hid_type, const u8* data, u32 size)
{
  const auto* const channel = FindChannelWithPSM(L2CAP_PSM_HID_INTR);

  if (!channel)
  {
    WARN_LOG_FMT(IOS_WIIMOTE, "Data callback with invalid L2CAP_PSM_HID_INTR channel.");
    return;
  }

  struct DataFrame
  {
    l2cap_hdr_t header;
    u8 hid_type;
    std::array<u8, WiimoteCommon::MAX_PAYLOAD - sizeof(hid_type)> data;
  } data_frame;

  static_assert(sizeof(data_frame) == sizeof(data_frame.data) + sizeof(u8) + sizeof(l2cap_hdr_t));

  data_frame.header.dcid = channel->remote_cid;
  data_frame.header.length = u16(sizeof(hid_type) + size);
  data_frame.hid_type = hid_type;
  std::copy_n(data, size, data_frame.data.data());

  const u32 data_frame_size = data_frame.header.length + sizeof(l2cap_hdr_t);

  // This should never be a problem as l2cap requires a minimum MTU of 48 bytes.
  DEBUG_ASSERT(data_frame_size <= channel->remote_mtu);

  m_host->SendACLPacket(GetBD(), reinterpret_cast<const u8*>(&data_frame), data_frame_size);
}
}  // namespace IOS::HLE
