// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/IOS/USB/Bluetooth/WiimoteDevice.h"

#include <cstring>
#include <memory>
#include <utility>
#include <vector>

#include "Common/Assert.h"
#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Common/StringUtil.h"
#include "Common/Swap.h"
#include "Core/Core.h"
#include "Core/HW/Wiimote.h"
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

WiimoteDevice::WiimoteDevice(Device::BluetoothEmu* host, int number, bdaddr_t bd, bool ready)
    : m_bd(bd),
      m_name(number == WIIMOTE_BALANCE_BOARD ? "Nintendo RVL-WBC-01" : "Nintendo RVL-CNT-01"),
      m_host(host)
{
  INFO_LOG(IOS_WIIMOTE, "Wiimote: #%i Constructed", number);

  m_connection_state = ready ? ConnectionState::Ready : ConnectionState::Inactive;
  m_connection_handle = 0x100 + number;
  memset(m_link_key, 0xA0 + number, HCI_KEY_SIZE);

  if (m_bd == BDADDR_ANY)
    m_bd = {{0x11, 0x02, 0x19, 0x79, static_cast<u8>(number)}};

  m_uclass[0] = 0x00;
  m_uclass[1] = 0x04;
  m_uclass[2] = 0x48;

  m_features[0] = 0xBC;
  m_features[1] = 0x02;
  m_features[2] = 0x04;
  m_features[3] = 0x38;
  m_features[4] = 0x08;
  m_features[5] = 0x00;
  m_features[6] = 0x00;
  m_features[7] = 0x00;

  m_lmp_version = 0x2;
  m_lmp_subversion = 0x229;
}

void WiimoteDevice::DoState(PointerWrap& p)
{
  bool passthrough_bluetooth = false;
  p.Do(passthrough_bluetooth);
  if (passthrough_bluetooth && p.GetMode() == PointerWrap::MODE_READ)
  {
    Core::DisplayMessage("State needs Bluetooth passthrough to be enabled. Aborting load state.",
                         3000);
    p.SetMode(PointerWrap::MODE_VERIFY);
    return;
  }

  // this function is usually not called... see Device::BluetoothEmu::DoState

  p.Do(m_connection_state);

  p.Do(m_hid_control_channel.connected);
  p.Do(m_hid_control_channel.connected_wait);
  p.Do(m_hid_control_channel.config);
  p.Do(m_hid_control_channel.config_wait);
  p.Do(m_hid_interrupt_channel.connected);
  p.Do(m_hid_interrupt_channel.connected_wait);
  p.Do(m_hid_interrupt_channel.config);
  p.Do(m_hid_interrupt_channel.config_wait);

  p.Do(m_bd);
  p.Do(m_connection_handle);
  p.Do(m_uclass);
  p.Do(m_features);
  p.Do(m_lmp_version);
  p.Do(m_lmp_subversion);
  p.Do(m_link_key);
  p.Do(m_name);

  p.Do(m_channel);
}

//
//
//
//
// ---  Simple and ugly state machine
//
//
//
//
//

bool WiimoteDevice::LinkChannel()
{
  if (m_connection_state != ConnectionState::Linking)
    return false;

  // try to connect L2CAP_PSM_HID_CNTL
  if (!m_hid_control_channel.connected)
  {
    if (m_hid_control_channel.connected_wait)
      return false;

    m_hid_control_channel.connected_wait = true;
    SendConnectionRequest(0x0040, L2CAP_PSM_HID_CNTL);
    return true;
  }

  // try to config L2CAP_PSM_HID_CNTL
  if (!m_hid_control_channel.config)
  {
    if (m_hid_control_channel.config_wait)
      return false;

    m_hid_control_channel.config_wait = true;
    SendConfigurationRequest(0x0040);
    return true;
  }

  // try to connect L2CAP_PSM_HID_INTR
  if (!m_hid_interrupt_channel.connected)
  {
    if (m_hid_interrupt_channel.connected_wait)
      return false;

    m_hid_interrupt_channel.connected_wait = true;
    SendConnectionRequest(0x0041, L2CAP_PSM_HID_INTR);
    return true;
  }

  // try to config L2CAP_PSM_HID_INTR
  if (!m_hid_interrupt_channel.config)
  {
    if (m_hid_interrupt_channel.config_wait)
      return false;

    m_hid_interrupt_channel.config_wait = true;
    SendConfigurationRequest(0x0041);
    return true;
  }

  DEBUG_LOG(IOS_WIIMOTE, "ConnectionState CONN_LINKING -> CONN_COMPLETE");
  m_connection_state = ConnectionState::Complete;

  // Update wiimote connection status in the UI
  Host_UpdateDisasmDialog();

  return false;
}

//
//
//
//
// ---  Events
//
//
//
//
//
void WiimoteDevice::Activate(bool ready)
{
  if (ready && (m_connection_state == ConnectionState::Inactive))
  {
    m_connection_state = ConnectionState::Ready;
  }
  else if (!ready)
  {
    m_host->RemoteDisconnect(m_connection_handle);
    EventDisconnect();
  }
}

void WiimoteDevice::EventConnectionAccepted()
{
  DEBUG_LOG(IOS_WIIMOTE, "ConnectionState %x -> CONN_LINKING", m_connection_state);
  m_connection_state = ConnectionState::Linking;
}

void WiimoteDevice::EventDisconnect()
{
  // Send disconnect message to plugin
  Wiimote::ControlChannel(m_connection_handle & 0xFF, 99, nullptr, 0);

  m_connection_state = ConnectionState::Inactive;

  // Clear channel flags
  ResetChannels();

  // Update wiimote connection status in the UI
  Host_UpdateDisasmDialog();
}

bool WiimoteDevice::EventPagingChanged(u8 page_mode) const
{
  return (m_connection_state == ConnectionState::Ready) && (page_mode & HCI_PAGE_SCAN_ENABLE);
}

void WiimoteDevice::ResetChannels()
{
  // reset connection process
  m_hid_control_channel = {};
  m_hid_interrupt_channel = {};
}

//
//
//
//
// ---  Input parsing
//
//
//
//
//

// This function receives L2CAP commands from the CPU
void WiimoteDevice::ExecuteL2capCmd(u8* ptr, u32 size)
{
  // parse the command
  l2cap_hdr_t* header = (l2cap_hdr_t*)ptr;
  u8* data = ptr + sizeof(l2cap_hdr_t);
  const u32 data_size = size - sizeof(l2cap_hdr_t);
  DEBUG_LOG(IOS_WIIMOTE, "  CID 0x%04x, Len 0x%x, DataSize 0x%x", header->dcid, header->length,
            data_size);

  if (header->length != data_size)
  {
    INFO_LOG(IOS_WIIMOTE, "Faulty packet. It is dropped.");
    return;
  }

  switch (header->dcid)
  {
  case L2CAP_SIGNAL_CID:
    SignalChannel(data, data_size);
    break;

  default:
  {
    DEBUG_ASSERT_MSG(IOS_WIIMOTE, DoesChannelExist(header->dcid),
                     "L2CAP: SendACLPacket to unknown channel %i", header->dcid);

    const auto itr = m_channel.find(header->dcid);
    const int number = m_connection_handle & 0xFF;

    if (itr != m_channel.end())
    {
      const SChannel& channel = itr->second;
      switch (channel.psm)
      {
      case L2CAP_PSM_SDP:
        HandleSDP(header->dcid, data, data_size);
        break;

      case L2CAP_PSM_HID_CNTL:
        if (number < MAX_BBMOTES)
          Wiimote::ControlChannel(number, header->dcid, data, data_size);
        break;

      case L2CAP_PSM_HID_INTR:
      {
        if (number < MAX_BBMOTES)
        {
          DEBUG_LOG(WIIMOTE, "Wiimote_InterruptChannel");
          DEBUG_LOG(WIIMOTE, "    Channel ID: %04x", header->dcid);
          const std::string temp = ArrayToString(data, data_size);
          DEBUG_LOG(WIIMOTE, "    Data: %s", temp.c_str());

          Wiimote::InterruptChannel(number, header->dcid, data, data_size);
        }
      }
      break;

      default:
        ERROR_LOG(IOS_WIIMOTE, "Channel 0x04%x has unknown PSM %x", header->dcid, channel.psm);
        break;
      }
    }
  }
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
      ERROR_LOG(IOS_WIIMOTE, "SignalChannel - L2CAP_COMMAND_REJ (something went wrong)."
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
      ERROR_LOG(IOS_WIIMOTE, "  Unknown Command-Code (0x%02x)", cmd_hdr->code);
      return;
    }

    data += cmd_hdr->length;
  }
}

//
//
//
//
// ---  Receive Commands from CPU
//
//
//
//
//

void WiimoteDevice::ReceiveConnectionReq(u8 ident, u8* data, u32 size)
{
  l2cap_con_req_cp* command_connection_req = (l2cap_con_req_cp*)data;

  // create the channel
  SChannel& channel = m_channel[command_connection_req->scid];
  channel.psm = command_connection_req->psm;
  channel.scid = command_connection_req->scid;
  channel.dcid = command_connection_req->scid;

  DEBUG_LOG(IOS_WIIMOTE, "[L2CAP] ReceiveConnectionRequest");
  DEBUG_LOG(IOS_WIIMOTE, "    Ident: 0x%02x", ident);
  DEBUG_LOG(IOS_WIIMOTE, "    PSM: 0x%04x", channel.psm);
  DEBUG_LOG(IOS_WIIMOTE, "    SCID: 0x%04x", channel.scid);
  DEBUG_LOG(IOS_WIIMOTE, "    DCID: 0x%04x", channel.dcid);

  // response
  l2cap_con_rsp_cp rsp;
  rsp.scid = channel.scid;
  rsp.dcid = channel.dcid;
  rsp.result = L2CAP_SUCCESS;
  rsp.status = L2CAP_NO_INFO;

  DEBUG_LOG(IOS_WIIMOTE, "[L2CAP] SendConnectionResponse");
  SendCommandToACL(ident, L2CAP_CONNECT_RSP, sizeof(l2cap_con_rsp_cp), (u8*)&rsp);
}

void WiimoteDevice::ReceiveConnectionResponse(u8 ident, u8* data, u32 size)
{
  l2cap_con_rsp_cp* rsp = (l2cap_con_rsp_cp*)data;

  DEBUG_ASSERT(size == sizeof(l2cap_con_rsp_cp));

  DEBUG_LOG(IOS_WIIMOTE, "[L2CAP] ReceiveConnectionResponse");
  DEBUG_LOG(IOS_WIIMOTE, "    DCID: 0x%04x", rsp->dcid);
  DEBUG_LOG(IOS_WIIMOTE, "    SCID: 0x%04x", rsp->scid);
  DEBUG_LOG(IOS_WIIMOTE, "    Result: 0x%04x", rsp->result);
  DEBUG_LOG(IOS_WIIMOTE, "    Status: 0x%04x", rsp->status);

  DEBUG_ASSERT(rsp->result == L2CAP_SUCCESS);
  DEBUG_ASSERT(rsp->status == L2CAP_NO_INFO);
  DEBUG_ASSERT(DoesChannelExist(rsp->scid));

  SChannel& channel = m_channel[rsp->scid];
  channel.dcid = rsp->dcid;

  // update state machine
  if (channel.psm == L2CAP_PSM_HID_CNTL)
    m_hid_control_channel.connected = true;
  else if (channel.psm == L2CAP_PSM_HID_INTR)
    m_hid_interrupt_channel.connected = true;
}

void WiimoteDevice::ReceiveConfigurationReq(u8 ident, u8* data, u32 size)
{
  u32 offset = 0;
  l2cap_cfg_req_cp* command_config_req = (l2cap_cfg_req_cp*)data;

  // Flags being 1 means that the options are sent in multi-packets
  DEBUG_ASSERT(command_config_req->flags == 0x00);
  DEBUG_ASSERT(DoesChannelExist(command_config_req->dcid));

  SChannel& channel = m_channel[command_config_req->dcid];

  DEBUG_LOG(IOS_WIIMOTE, "[L2CAP] ReceiveConfigurationRequest");
  DEBUG_LOG(IOS_WIIMOTE, "    Ident: 0x%02x", ident);
  DEBUG_LOG(IOS_WIIMOTE, "    DCID: 0x%04x", command_config_req->dcid);
  DEBUG_LOG(IOS_WIIMOTE, "    Flags: 0x%04x", command_config_req->flags);

  offset += sizeof(l2cap_cfg_req_cp);

  u8 temp_buffer[1024];
  u32 resp_len = 0;

  l2cap_cfg_rsp_cp* rsp = (l2cap_cfg_rsp_cp*)temp_buffer;
  rsp->scid = channel.dcid;
  rsp->flags = 0x00;
  rsp->result = L2CAP_SUCCESS;

  resp_len += sizeof(l2cap_cfg_rsp_cp);

  // read configuration options
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
      channel.mtu = mtu->mtu;
      DEBUG_LOG(IOS_WIIMOTE, "    MTU: 0x%04x", mtu->mtu);
    }
    break;

    case L2CAP_OPT_FLUSH_TIMO:
    {
      DEBUG_ASSERT(options->length == L2CAP_OPT_FLUSH_TIMO_SIZE);
      l2cap_cfg_opt_val_t* flush_time_out = (l2cap_cfg_opt_val_t*)&data[offset];
      channel.flush_time_out = flush_time_out->flush_timo;
      DEBUG_LOG(IOS_WIIMOTE, "    FlushTimeOut: 0x%04x", flush_time_out->flush_timo);
    }
    break;

    default:
      DEBUG_ASSERT_MSG(IOS_WIIMOTE, 0, "Unknown Option: 0x%02x", options->type);
      break;
    }

    offset += options->length;

    const u32 option_size = sizeof(l2cap_cfg_opt_t) + options->length;
    memcpy(&temp_buffer[resp_len], options, option_size);
    resp_len += option_size;
  }

  DEBUG_LOG(IOS_WIIMOTE, "[L2CAP] SendConfigurationResponse");
  SendCommandToACL(ident, L2CAP_CONFIG_RSP, resp_len, temp_buffer);

  // update state machine
  if (channel.psm == L2CAP_PSM_HID_CNTL)
    m_hid_control_channel.connected = true;
  else if (channel.psm == L2CAP_PSM_HID_INTR)
    m_hid_interrupt_channel.connected = true;
}

void WiimoteDevice::ReceiveConfigurationResponse(u8 ident, u8* data, u32 size)
{
  l2cap_cfg_rsp_cp* rsp = (l2cap_cfg_rsp_cp*)data;

  DEBUG_LOG(IOS_WIIMOTE, "[L2CAP] ReceiveConfigurationResponse");
  DEBUG_LOG(IOS_WIIMOTE, "    SCID: 0x%04x", rsp->scid);
  DEBUG_LOG(IOS_WIIMOTE, "    Flags: 0x%04x", rsp->flags);
  DEBUG_LOG(IOS_WIIMOTE, "    Result: 0x%04x", rsp->result);

  DEBUG_ASSERT(rsp->result == L2CAP_SUCCESS);

  // update state machine
  const SChannel& channel = m_channel[rsp->scid];

  if (channel.psm == L2CAP_PSM_HID_CNTL)
    m_hid_control_channel.config = true;
  else if (channel.psm == L2CAP_PSM_HID_INTR)
    m_hid_interrupt_channel.config = true;
}

void WiimoteDevice::ReceiveDisconnectionReq(u8 ident, u8* data, u32 size)
{
  l2cap_discon_req_cp* command_disconnection_req = (l2cap_discon_req_cp*)data;

  DEBUG_LOG(IOS_WIIMOTE, "[L2CAP] ReceiveDisconnectionReq");
  DEBUG_LOG(IOS_WIIMOTE, "    Ident: 0x%02x", ident);
  DEBUG_LOG(IOS_WIIMOTE, "    DCID: 0x%04x", command_disconnection_req->dcid);
  DEBUG_LOG(IOS_WIIMOTE, "    SCID: 0x%04x", command_disconnection_req->scid);

  // response
  l2cap_discon_req_cp rsp;
  rsp.dcid = command_disconnection_req->dcid;
  rsp.scid = command_disconnection_req->scid;

  DEBUG_LOG(IOS_WIIMOTE, "[L2CAP] SendDisconnectionResponse");
  SendCommandToACL(ident, L2CAP_DISCONNECT_RSP, sizeof(l2cap_discon_req_cp), (u8*)&rsp);
}

//
//
//
//
// ---  Send Commands To CPU
//
//
//
//
//

// We assume Wiimote is always connected
void WiimoteDevice::SendConnectionRequest(u16 scid, u16 psm)
{
  // create the channel
  SChannel& channel = m_channel[scid];
  channel.psm = psm;
  channel.scid = scid;

  l2cap_con_req_cp cr;
  cr.psm = psm;
  cr.scid = scid;

  DEBUG_LOG(IOS_WIIMOTE, "[L2CAP] SendConnectionRequest");
  DEBUG_LOG(IOS_WIIMOTE, "    Psm: 0x%04x", cr.psm);
  DEBUG_LOG(IOS_WIIMOTE, "    Scid: 0x%04x", cr.scid);

  SendCommandToACL(L2CAP_CONNECT_REQ, L2CAP_CONNECT_REQ, sizeof(l2cap_con_req_cp), (u8*)&cr);
}

// We don't initially disconnect Wiimote though ...
void WiimoteDevice::SendDisconnectRequest(u16 scid)
{
  // create the channel
  const SChannel& channel = m_channel[scid];

  l2cap_discon_req_cp cr;
  cr.dcid = channel.dcid;
  cr.scid = channel.scid;

  DEBUG_LOG(IOS_WIIMOTE, "[L2CAP] SendDisconnectionRequest");
  DEBUG_LOG(IOS_WIIMOTE, "    Dcid: 0x%04x", cr.dcid);
  DEBUG_LOG(IOS_WIIMOTE, "    Scid: 0x%04x", cr.scid);

  SendCommandToACL(L2CAP_DISCONNECT_REQ, L2CAP_DISCONNECT_REQ, sizeof(l2cap_discon_req_cp),
                   (u8*)&cr);
}

void WiimoteDevice::SendConfigurationRequest(u16 scid, u16 mtu, u16 flush_time_out)
{
  DEBUG_ASSERT(DoesChannelExist(scid));
  const SChannel& channel = m_channel[scid];

  u8 buffer[1024];
  int offset = 0;

  l2cap_cfg_req_cp* cr = (l2cap_cfg_req_cp*)&buffer[offset];
  cr->dcid = channel.dcid;
  cr->flags = 0;
  offset += sizeof(l2cap_cfg_req_cp);

  DEBUG_LOG(IOS_WIIMOTE, "[L2CAP] SendConfigurationRequest");
  DEBUG_LOG(IOS_WIIMOTE, "    Dcid: 0x%04x", cr->dcid);
  DEBUG_LOG(IOS_WIIMOTE, "    Flags: 0x%04x", cr->flags);

  l2cap_cfg_opt_t* options;

  // (shuffle2) currently we end up not appending options. this is because we don't
  // negotiate after trying to set MTU = 0 fails (stack will respond with
  // "configuration failed" msg...). This is still fine, we'll just use whatever the
  // Bluetooth stack defaults to.
  if (mtu || channel.mtu)
  {
    if (mtu == 0)
      mtu = channel.mtu;
    options = (l2cap_cfg_opt_t*)&buffer[offset];
    offset += sizeof(l2cap_cfg_opt_t);
    options->type = L2CAP_OPT_MTU;
    options->length = L2CAP_OPT_MTU_SIZE;
    *(u16*)&buffer[offset] = mtu;
    offset += L2CAP_OPT_MTU_SIZE;
    DEBUG_LOG(IOS_WIIMOTE, "    MTU: 0x%04x", mtu);
  }

  if (flush_time_out || channel.flush_time_out)
  {
    if (flush_time_out == 0)
      flush_time_out = channel.flush_time_out;
    options = (l2cap_cfg_opt_t*)&buffer[offset];
    offset += sizeof(l2cap_cfg_opt_t);
    options->type = L2CAP_OPT_FLUSH_TIMO;
    options->length = L2CAP_OPT_FLUSH_TIMO_SIZE;
    *(u16*)&buffer[offset] = flush_time_out;
    offset += L2CAP_OPT_FLUSH_TIMO_SIZE;
    DEBUG_LOG(IOS_WIIMOTE, "    FlushTimeOut: 0x%04x", flush_time_out);
  }

  SendCommandToACL(L2CAP_CONFIG_REQ, L2CAP_CONFIG_REQ, offset, buffer);
}

//
//
//
//
// ---  SDP
//
//
//
//
//

#define SDP_UINT8 0x08
#define SDP_UINT16 0x09
#define SDP_UINT32 0x0A
#define SDP_SEQ8 0x35
#define SDP_SEQ16 0x36

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
  m_host->SendACLPacket(GetConnectionHandle(), data_frame, header->length + sizeof(l2cap_hdr_t));
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
  ERROR_LOG(IOS_WIIMOTE, "ParseCont: wrong cont: %i", type_id);
  PanicAlert("ParseCont: wrong cont: %i", type_id);
  return 0;
}

static int ParseAttribList(u8* attrib_id_list, u16& start_id, u16& end_id)
{
  u32 attrib_offset = 0;
  CBigEndianBuffer attrib_list(attrib_id_list);

  const u8 sequence = attrib_list.Read8(attrib_offset);
  attrib_offset++;
  const u8 seq_size = attrib_list.Read8(attrib_offset);
  attrib_offset++;
  const u8 type_id = attrib_list.Read8(attrib_offset);
  attrib_offset++;

  if (MAX_LOGLEVEL >= LogTypes::LOG_LEVELS::LDEBUG)
  {
    DEBUG_ASSERT(sequence == SDP_SEQ8);
    (void)seq_size;
  }

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
    WARN_LOG(IOS_WIIMOTE, "Read just a single attrib - not tested");
    PanicAlert("Read just a single attrib - not tested");
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
    ERROR_LOG(IOS_WIIMOTE, "Unknown service handle %x", service_handle);
    PanicAlert("Unknown service handle %x", service_handle);
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
  m_host->SendACLPacket(GetConnectionHandle(), data_frame, header->length + sizeof(l2cap_hdr_t));

  // Debugger::PrintDataBuffer(LogTypes::WIIMOTE, data_frame, header->length + sizeof(l2cap_hdr_t),
  // "test response: ");
}

void WiimoteDevice::HandleSDP(u16 cid, u8* data, u32 size)
{
  // Debugger::PrintDataBuffer(LogTypes::WIIMOTE, data, size, "HandleSDP: ");

  CBigEndianBuffer buffer(data);

  switch (buffer.Read8(0))
  {
  // SDP_ServiceSearchRequest
  case 0x02:
  {
    WARN_LOG(IOS_WIIMOTE, "!!! SDP_ServiceSearchRequest !!!");

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
    WARN_LOG(IOS_WIIMOTE, "!!! SDP_ServiceAttributeRequest !!!");

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
    ERROR_LOG(IOS_WIIMOTE, "WIIMOTE: Unknown SDP command %x", data[0]);
    PanicAlert("WIIMOTE: Unknown SDP command %x", data[0]);
    break;
  }
}

//
//
//
//
// ---  Data Send Functions
//
//
//
//
//

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

  DEBUG_LOG(IOS_WIIMOTE, "Send ACL Command to CPU");
  DEBUG_LOG(IOS_WIIMOTE, "    Ident: 0x%02x", ident);
  DEBUG_LOG(IOS_WIIMOTE, "    Code: 0x%02x", code);

  // send ....
  m_host->SendACLPacket(GetConnectionHandle(), data_frame, header->length + sizeof(l2cap_hdr_t));

  // Debugger::PrintDataBuffer(LogTypes::WIIMOTE, data_frame, header->length + sizeof(l2cap_hdr_t),
  // "m_pHost->SendACLPacket: ");
}

void WiimoteDevice::ReceiveL2capData(u16 scid, const void* data, u32 size)
{
  // Allocate DataFrame
  u8 data_frame[1024];
  u32 offset = 0;
  l2cap_hdr_t* header = (l2cap_hdr_t*)data_frame;
  offset += sizeof(l2cap_hdr_t);

  // Check if we are already reporting on this channel
  DEBUG_ASSERT(DoesChannelExist(scid));
  const SChannel& channel = m_channel[scid];

  // Add an additional 4 byte header to the Wiimote report
  header->dcid = channel.dcid;
  header->length = size;

  // Copy the Wiimote report to data_frame
  memcpy(data_frame + offset, data, size);
  // Update offset to the final size of the report
  offset += size;

  // Send the report
  m_host->SendACLPacket(GetConnectionHandle(), data_frame, offset);
}
}  // namespace IOS::HLE

namespace Core
{
// This is called continuously from the Wiimote plugin as soon as it has received
// a reporting mode. size is the byte size of the report.
void Callback_WiimoteInterruptChannel(int number, u16 channel_id, const u8* data, u32 size)
{
  const auto bt = std::static_pointer_cast<IOS::HLE::Device::BluetoothEmu>(
      IOS::HLE::GetIOS()->GetDeviceByName("/dev/usb/oh1/57e/305"));
  if (bt)
    bt->AccessWiimoteByIndex(number)->ReceiveL2capData(channel_id, data, size);
}
}  // namespace Core
