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
#include "Core/Core.h"
#include "Core/HW/Wiimote.h"
#include "Core/Host.h"
#include "Core/IOS/USB/Bluetooth/BTEmu.h"
#include "Core/IOS/USB/Bluetooth/WiimoteHIDAttr.h"
#include "Core/IOS/USB/Bluetooth/l2cap.h"

namespace IOS
{
namespace HLE
{
WiimoteDevice::WiimoteDevice(Device::BluetoothEmu* host, int number, bdaddr_t bd, bool ready)
    : m_BD(bd),
      m_Name(number == WIIMOTE_BALANCE_BOARD ? "Nintendo RVL-WBC-01" : "Nintendo RVL-CNT-01"),
      m_pHost(host)
{
  INFO_LOG(IOS_WIIMOTE, "Wiimote: #%i Constructed", number);

  m_ConnectionState = (ready) ? CONN_READY : CONN_INACTIVE;
  m_ConnectionHandle = 0x100 + number;
  memset(m_LinkKey, 0xA0 + number, HCI_KEY_SIZE);

  if (m_BD == BDADDR_ANY)
    m_BD = {{0x11, 0x02, 0x19, 0x79, static_cast<u8>(number)}};

  uclass[0] = 0x00;
  uclass[1] = 0x04;
  uclass[2] = 0x48;

  features[0] = 0xBC;
  features[1] = 0x02;
  features[2] = 0x04;
  features[3] = 0x38;
  features[4] = 0x08;
  features[5] = 0x00;
  features[6] = 0x00;
  features[7] = 0x00;

  lmp_version = 0x2;
  lmp_subversion = 0x229;
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

  p.Do(m_ConnectionState);

  p.Do(m_HIDControlChannel_Connected);
  p.Do(m_HIDControlChannel_ConnectedWait);
  p.Do(m_HIDControlChannel_Config);
  p.Do(m_HIDControlChannel_ConfigWait);
  p.Do(m_HIDInterruptChannel_Connected);
  p.Do(m_HIDInterruptChannel_ConnectedWait);
  p.Do(m_HIDInterruptChannel_Config);
  p.Do(m_HIDInterruptChannel_ConfigWait);

  p.Do(m_BD);
  p.Do(m_ConnectionHandle);
  p.Do(uclass);
  p.Do(features);
  p.Do(lmp_version);
  p.Do(lmp_subversion);
  p.Do(m_LinkKey);
  p.Do(m_Name);

  p.Do(m_Channel);
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
  if (m_ConnectionState != CONN_LINKING)
    return false;

  // try to connect L2CAP_PSM_HID_CNTL
  if (!m_HIDControlChannel_Connected)
  {
    if (m_HIDControlChannel_ConnectedWait)
      return false;

    m_HIDControlChannel_ConnectedWait = true;
    SendConnectionRequest(0x0040, L2CAP_PSM_HID_CNTL);
    return true;
  }

  // try to config L2CAP_PSM_HID_CNTL
  if (!m_HIDControlChannel_Config)
  {
    if (m_HIDControlChannel_ConfigWait)
      return false;

    m_HIDControlChannel_ConfigWait = true;
    SendConfigurationRequest(0x0040);
    return true;
  }

  // try to connect L2CAP_PSM_HID_INTR
  if (!m_HIDInterruptChannel_Connected)
  {
    if (m_HIDInterruptChannel_ConnectedWait)
      return false;

    m_HIDInterruptChannel_ConnectedWait = true;
    SendConnectionRequest(0x0041, L2CAP_PSM_HID_INTR);
    return true;
  }

  // try to config L2CAP_PSM_HID_INTR
  if (!m_HIDInterruptChannel_Config)
  {
    if (m_HIDInterruptChannel_ConfigWait)
      return false;

    m_HIDInterruptChannel_ConfigWait = true;
    SendConfigurationRequest(0x0041);
    return true;
  }

  DEBUG_LOG(IOS_WIIMOTE, "ConnectionState CONN_LINKING -> CONN_COMPLETE");
  m_ConnectionState = CONN_COMPLETE;

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
  if (ready && (m_ConnectionState == CONN_INACTIVE))
  {
    m_ConnectionState = CONN_READY;
  }
  else if (!ready)
  {
    m_pHost->RemoteDisconnect(m_ConnectionHandle);
    EventDisconnect();
  }
}

void WiimoteDevice::EventConnectionAccepted()
{
  DEBUG_LOG(IOS_WIIMOTE, "ConnectionState %x -> CONN_LINKING", m_ConnectionState);
  m_ConnectionState = CONN_LINKING;
}

void WiimoteDevice::EventDisconnect()
{
  // Send disconnect message to plugin
  Wiimote::ControlChannel(m_ConnectionHandle & 0xFF, 99, nullptr, 0);

  m_ConnectionState = CONN_INACTIVE;
  // Clear channel flags
  ResetChannels();
}

bool WiimoteDevice::EventPagingChanged(u8 _pageMode)
{
  if ((m_ConnectionState == CONN_READY) && (_pageMode & HCI_PAGE_SCAN_ENABLE))
    return true;

  return false;
}

void WiimoteDevice::ResetChannels()
{
  // reset connection process
  m_HIDControlChannel_Connected = false;
  m_HIDControlChannel_Config = false;
  m_HIDInterruptChannel_Connected = false;
  m_HIDInterruptChannel_Config = false;
  m_HIDControlChannel_ConnectedWait = false;
  m_HIDControlChannel_ConfigWait = false;
  m_HIDInterruptChannel_ConnectedWait = false;
  m_HIDInterruptChannel_ConfigWait = false;
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
void WiimoteDevice::ExecuteL2capCmd(u8* _pData, u32 _Size)
{
  // parse the command
  l2cap_hdr_t* pHeader = (l2cap_hdr_t*)_pData;
  u8* pData = _pData + sizeof(l2cap_hdr_t);
  u32 DataSize = _Size - sizeof(l2cap_hdr_t);
  DEBUG_LOG(IOS_WIIMOTE, "  CID 0x%04x, Len 0x%x, DataSize 0x%x", pHeader->dcid, pHeader->length,
            DataSize);

  if (pHeader->length != DataSize)
  {
    INFO_LOG(IOS_WIIMOTE, "Faulty packet. It is dropped.");
    return;
  }

  switch (pHeader->dcid)
  {
  case L2CAP_SIGNAL_CID:
    SignalChannel(pData, DataSize);
    break;

  default:
  {
    _dbg_assert_msg_(IOS_WIIMOTE, DoesChannelExist(pHeader->dcid),
                     "L2CAP: SendACLPacket to unknown channel %i", pHeader->dcid);
    CChannelMap::iterator itr = m_Channel.find(pHeader->dcid);

    const int number = m_ConnectionHandle & 0xFF;

    if (itr != m_Channel.end())
    {
      SChannel& rChannel = itr->second;
      switch (rChannel.PSM)
      {
      case L2CAP_PSM_SDP:
        HandleSDP(pHeader->dcid, pData, DataSize);
        break;

      case L2CAP_PSM_HID_CNTL:
        if (number < MAX_BBMOTES)
          Wiimote::ControlChannel(number, pHeader->dcid, pData, DataSize);
        break;

      case L2CAP_PSM_HID_INTR:
      {
        if (number < MAX_BBMOTES)
        {
          DEBUG_LOG(WIIMOTE, "Wiimote_InterruptChannel");
          DEBUG_LOG(WIIMOTE, "    Channel ID: %04x", pHeader->dcid);
          std::string Temp = ArrayToString((const u8*)pData, DataSize);
          DEBUG_LOG(WIIMOTE, "    Data: %s", Temp.c_str());

          Wiimote::InterruptChannel(number, pHeader->dcid, pData, DataSize);
        }
      }
      break;

      default:
        ERROR_LOG(IOS_WIIMOTE, "Channel 0x04%x has unknown PSM %x", pHeader->dcid, rChannel.PSM);
        break;
      }
    }
  }
  break;
  }
}

void WiimoteDevice::SignalChannel(u8* _pData, u32 _Size)
{
  while (_Size >= sizeof(l2cap_cmd_hdr_t))
  {
    l2cap_cmd_hdr_t* cmd_hdr = (l2cap_cmd_hdr_t*)_pData;
    _pData += sizeof(l2cap_cmd_hdr_t);
    _Size = _Size - sizeof(l2cap_cmd_hdr_t) - cmd_hdr->length;

    switch (cmd_hdr->code)
    {
    case L2CAP_COMMAND_REJ:
      ERROR_LOG(IOS_WIIMOTE, "SignalChannel - L2CAP_COMMAND_REJ (something went wrong)."
                             "Try to replace your SYSCONF file with a new copy.");
      break;

    case L2CAP_CONNECT_REQ:
      ReceiveConnectionReq(cmd_hdr->ident, _pData, cmd_hdr->length);
      break;

    case L2CAP_CONNECT_RSP:
      ReceiveConnectionResponse(cmd_hdr->ident, _pData, cmd_hdr->length);
      break;

    case L2CAP_CONFIG_REQ:
      ReceiveConfigurationReq(cmd_hdr->ident, _pData, cmd_hdr->length);
      break;

    case L2CAP_CONFIG_RSP:
      ReceiveConfigurationResponse(cmd_hdr->ident, _pData, cmd_hdr->length);
      break;

    case L2CAP_DISCONNECT_REQ:
      ReceiveDisconnectionReq(cmd_hdr->ident, _pData, cmd_hdr->length);
      break;

    default:
      ERROR_LOG(IOS_WIIMOTE, "  Unknown Command-Code (0x%02x)", cmd_hdr->code);
      return;
    }

    _pData += cmd_hdr->length;
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

void WiimoteDevice::ReceiveConnectionReq(u8 _Ident, u8* _pData, u32 _Size)
{
  l2cap_con_req_cp* pCommandConnectionReq = (l2cap_con_req_cp*)_pData;

  // create the channel
  SChannel& rChannel = m_Channel[pCommandConnectionReq->scid];
  rChannel.PSM = pCommandConnectionReq->psm;
  rChannel.SCID = pCommandConnectionReq->scid;
  rChannel.DCID = pCommandConnectionReq->scid;

  DEBUG_LOG(IOS_WIIMOTE, "[L2CAP] ReceiveConnectionRequest");
  DEBUG_LOG(IOS_WIIMOTE, "    Ident: 0x%02x", _Ident);
  DEBUG_LOG(IOS_WIIMOTE, "    PSM: 0x%04x", rChannel.PSM);
  DEBUG_LOG(IOS_WIIMOTE, "    SCID: 0x%04x", rChannel.SCID);
  DEBUG_LOG(IOS_WIIMOTE, "    DCID: 0x%04x", rChannel.DCID);

  // response
  l2cap_con_rsp_cp Rsp;
  Rsp.scid = rChannel.SCID;
  Rsp.dcid = rChannel.DCID;
  Rsp.result = L2CAP_SUCCESS;
  Rsp.status = L2CAP_NO_INFO;

  DEBUG_LOG(IOS_WIIMOTE, "[L2CAP] SendConnectionResponse");
  SendCommandToACL(_Ident, L2CAP_CONNECT_RSP, sizeof(l2cap_con_rsp_cp), (u8*)&Rsp);

  // update state machine
  /*
  if (rChannel.PSM == L2CAP_PSM_HID_CNTL)
    m_HIDControlChannel_Connected = true;
  else if (rChannel.PSM == L2CAP_PSM_HID_INTR)
    m_HIDInterruptChannel_Connected = true;
  */
}

void WiimoteDevice::ReceiveConnectionResponse(u8 _Ident, u8* _pData, u32 _Size)
{
  l2cap_con_rsp_cp* rsp = (l2cap_con_rsp_cp*)_pData;

  _dbg_assert_(IOS_WIIMOTE, _Size == sizeof(l2cap_con_rsp_cp));

  DEBUG_LOG(IOS_WIIMOTE, "[L2CAP] ReceiveConnectionResponse");
  DEBUG_LOG(IOS_WIIMOTE, "    DCID: 0x%04x", rsp->dcid);
  DEBUG_LOG(IOS_WIIMOTE, "    SCID: 0x%04x", rsp->scid);
  DEBUG_LOG(IOS_WIIMOTE, "    Result: 0x%04x", rsp->result);
  DEBUG_LOG(IOS_WIIMOTE, "    Status: 0x%04x", rsp->status);

  _dbg_assert_(IOS_WIIMOTE, rsp->result == L2CAP_SUCCESS);
  _dbg_assert_(IOS_WIIMOTE, rsp->status == L2CAP_NO_INFO);
  _dbg_assert_(IOS_WIIMOTE, DoesChannelExist(rsp->scid));

  SChannel& rChannel = m_Channel[rsp->scid];
  rChannel.DCID = rsp->dcid;

  // update state machine
  if (rChannel.PSM == L2CAP_PSM_HID_CNTL)
    m_HIDControlChannel_Connected = true;
  else if (rChannel.PSM == L2CAP_PSM_HID_INTR)
    m_HIDInterruptChannel_Connected = true;
}

void WiimoteDevice::ReceiveConfigurationReq(u8 _Ident, u8* _pData, u32 _Size)
{
  u32 Offset = 0;
  l2cap_cfg_req_cp* pCommandConfigReq = (l2cap_cfg_req_cp*)_pData;

  _dbg_assert_(IOS_WIIMOTE, pCommandConfigReq->flags ==
                                0x00);  // 1 means that the options are send in multi-packets
  _dbg_assert_(IOS_WIIMOTE, DoesChannelExist(pCommandConfigReq->dcid));

  SChannel& rChannel = m_Channel[pCommandConfigReq->dcid];

  DEBUG_LOG(IOS_WIIMOTE, "[L2CAP] ReceiveConfigurationRequest");
  DEBUG_LOG(IOS_WIIMOTE, "    Ident: 0x%02x", _Ident);
  DEBUG_LOG(IOS_WIIMOTE, "    DCID: 0x%04x", pCommandConfigReq->dcid);
  DEBUG_LOG(IOS_WIIMOTE, "    Flags: 0x%04x", pCommandConfigReq->flags);

  Offset += sizeof(l2cap_cfg_req_cp);

  u8 TempBuffer[1024];
  u32 RespLen = 0;

  l2cap_cfg_rsp_cp* Rsp = (l2cap_cfg_rsp_cp*)TempBuffer;
  Rsp->scid = rChannel.DCID;
  Rsp->flags = 0x00;
  Rsp->result = L2CAP_SUCCESS;

  RespLen += sizeof(l2cap_cfg_rsp_cp);

  // read configuration options
  while (Offset < _Size)
  {
    l2cap_cfg_opt_t* pOptions = (l2cap_cfg_opt_t*)&_pData[Offset];
    Offset += sizeof(l2cap_cfg_opt_t);

    switch (pOptions->type)
    {
    case L2CAP_OPT_MTU:
    {
      _dbg_assert_(IOS_WIIMOTE, pOptions->length == L2CAP_OPT_MTU_SIZE);
      l2cap_cfg_opt_val_t* pMTU = (l2cap_cfg_opt_val_t*)&_pData[Offset];
      rChannel.MTU = pMTU->mtu;
      DEBUG_LOG(IOS_WIIMOTE, "    MTU: 0x%04x", pMTU->mtu);
    }
    break;

    case L2CAP_OPT_FLUSH_TIMO:
    {
      _dbg_assert_(IOS_WIIMOTE, pOptions->length == L2CAP_OPT_FLUSH_TIMO_SIZE);
      l2cap_cfg_opt_val_t* pFlushTimeOut = (l2cap_cfg_opt_val_t*)&_pData[Offset];
      rChannel.FlushTimeOut = pFlushTimeOut->flush_timo;
      DEBUG_LOG(IOS_WIIMOTE, "    FlushTimeOut: 0x%04x", pFlushTimeOut->flush_timo);
    }
    break;

    default:
      _dbg_assert_msg_(IOS_WIIMOTE, 0, "Unknown Option: 0x%02x", pOptions->type);
      break;
    }

    Offset += pOptions->length;

    u32 OptionSize = sizeof(l2cap_cfg_opt_t) + pOptions->length;
    memcpy(&TempBuffer[RespLen], pOptions, OptionSize);
    RespLen += OptionSize;
  }

  DEBUG_LOG(IOS_WIIMOTE, "[L2CAP] SendConfigurationResponse");
  SendCommandToACL(_Ident, L2CAP_CONFIG_RSP, RespLen, TempBuffer);

  // update state machine
  if (rChannel.PSM == L2CAP_PSM_HID_CNTL)
    m_HIDControlChannel_Connected = true;
  else if (rChannel.PSM == L2CAP_PSM_HID_INTR)
    m_HIDInterruptChannel_Connected = true;
}

void WiimoteDevice::ReceiveConfigurationResponse(u8 _Ident, u8* _pData, u32 _Size)
{
  l2cap_cfg_rsp_cp* rsp = (l2cap_cfg_rsp_cp*)_pData;

  DEBUG_LOG(IOS_WIIMOTE, "[L2CAP] ReceiveConfigurationResponse");
  DEBUG_LOG(IOS_WIIMOTE, "    SCID: 0x%04x", rsp->scid);
  DEBUG_LOG(IOS_WIIMOTE, "    Flags: 0x%04x", rsp->flags);
  DEBUG_LOG(IOS_WIIMOTE, "    Result: 0x%04x", rsp->result);

  _dbg_assert_(IOS_WIIMOTE, rsp->result == L2CAP_SUCCESS);

  // update state machine
  SChannel& rChannel = m_Channel[rsp->scid];

  if (rChannel.PSM == L2CAP_PSM_HID_CNTL)
    m_HIDControlChannel_Config = true;
  else if (rChannel.PSM == L2CAP_PSM_HID_INTR)
    m_HIDInterruptChannel_Config = true;
}

void WiimoteDevice::ReceiveDisconnectionReq(u8 _Ident, u8* _pData, u32 _Size)
{
  l2cap_discon_req_cp* pCommandDisconnectionReq = (l2cap_discon_req_cp*)_pData;

  DEBUG_LOG(IOS_WIIMOTE, "[L2CAP] ReceiveDisconnectionReq");
  DEBUG_LOG(IOS_WIIMOTE, "    Ident: 0x%02x", _Ident);
  DEBUG_LOG(IOS_WIIMOTE, "    DCID: 0x%04x", pCommandDisconnectionReq->dcid);
  DEBUG_LOG(IOS_WIIMOTE, "    SCID: 0x%04x", pCommandDisconnectionReq->scid);

  // response
  l2cap_discon_req_cp Rsp;
  Rsp.dcid = pCommandDisconnectionReq->dcid;
  Rsp.scid = pCommandDisconnectionReq->scid;

  DEBUG_LOG(IOS_WIIMOTE, "[L2CAP] SendDisconnectionResponse");
  SendCommandToACL(_Ident, L2CAP_DISCONNECT_RSP, sizeof(l2cap_discon_req_cp), (u8*)&Rsp);
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
  SChannel& rChannel = m_Channel[scid];
  rChannel.PSM = psm;
  rChannel.SCID = scid;

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
  SChannel& rChannel = m_Channel[scid];

  l2cap_discon_req_cp cr;
  cr.dcid = rChannel.DCID;
  cr.scid = rChannel.SCID;

  DEBUG_LOG(IOS_WIIMOTE, "[L2CAP] SendDisconnectionRequest");
  DEBUG_LOG(IOS_WIIMOTE, "    Dcid: 0x%04x", cr.dcid);
  DEBUG_LOG(IOS_WIIMOTE, "    Scid: 0x%04x", cr.scid);

  SendCommandToACL(L2CAP_DISCONNECT_REQ, L2CAP_DISCONNECT_REQ, sizeof(l2cap_discon_req_cp),
                   (u8*)&cr);
}

void WiimoteDevice::SendConfigurationRequest(u16 scid, u16 MTU, u16 FlushTimeOut)
{
  _dbg_assert_(IOS_WIIMOTE, DoesChannelExist(scid));
  SChannel& rChannel = m_Channel[scid];

  u8 Buffer[1024];
  int Offset = 0;

  l2cap_cfg_req_cp* cr = (l2cap_cfg_req_cp*)&Buffer[Offset];
  cr->dcid = rChannel.DCID;
  cr->flags = 0;
  Offset += sizeof(l2cap_cfg_req_cp);

  DEBUG_LOG(IOS_WIIMOTE, "[L2CAP] SendConfigurationRequest");
  DEBUG_LOG(IOS_WIIMOTE, "    Dcid: 0x%04x", cr->dcid);
  DEBUG_LOG(IOS_WIIMOTE, "    Flags: 0x%04x", cr->flags);

  l2cap_cfg_opt_t* pOptions;

  // (shuffle2) currently we end up not appending options. this is because we don't
  // negotiate after trying to set MTU = 0 fails (stack will respond with
  // "configuration failed" msg...). This is still fine, we'll just use whatever the
  // Bluetooth stack defaults to.
  if (MTU || rChannel.MTU)
  {
    if (MTU == 0)
      MTU = rChannel.MTU;
    pOptions = (l2cap_cfg_opt_t*)&Buffer[Offset];
    Offset += sizeof(l2cap_cfg_opt_t);
    pOptions->type = L2CAP_OPT_MTU;
    pOptions->length = L2CAP_OPT_MTU_SIZE;
    *(u16*)&Buffer[Offset] = MTU;
    Offset += L2CAP_OPT_MTU_SIZE;
    DEBUG_LOG(IOS_WIIMOTE, "    MTU: 0x%04x", MTU);
  }

  if (FlushTimeOut || rChannel.FlushTimeOut)
  {
    if (FlushTimeOut == 0)
      FlushTimeOut = rChannel.FlushTimeOut;
    pOptions = (l2cap_cfg_opt_t*)&Buffer[Offset];
    Offset += sizeof(l2cap_cfg_opt_t);
    pOptions->type = L2CAP_OPT_FLUSH_TIMO;
    pOptions->length = L2CAP_OPT_FLUSH_TIMO_SIZE;
    *(u16*)&Buffer[Offset] = FlushTimeOut;
    Offset += L2CAP_OPT_FLUSH_TIMO_SIZE;
    DEBUG_LOG(IOS_WIIMOTE, "    FlushTimeOut: 0x%04x", FlushTimeOut);
  }

  SendCommandToACL(L2CAP_CONFIG_REQ, L2CAP_CONFIG_REQ, Offset, Buffer);
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

void WiimoteDevice::SDPSendServiceSearchResponse(u16 cid, u16 TransactionID,
                                                 u8* pServiceSearchPattern,
                                                 u16 MaximumServiceRecordCount)
{
  // verify block... we handle search pattern for HID service only
  {
    CBigEndianBuffer buffer(pServiceSearchPattern);
    _dbg_assert_(IOS_WIIMOTE, buffer.Read8(0) == SDP_SEQ8);  // data sequence
    _dbg_assert_(IOS_WIIMOTE, buffer.Read8(1) == 0x03);      // sequence size

    // HIDClassID
    _dbg_assert_(IOS_WIIMOTE, buffer.Read8(2) == 0x19);
    _dbg_assert_(IOS_WIIMOTE, buffer.Read16(3) == 0x1124);
  }

  u8 DataFrame[1000];
  CBigEndianBuffer buffer(DataFrame);

  int Offset = 0;
  l2cap_hdr_t* pHeader = (l2cap_hdr_t*)&DataFrame[Offset];
  Offset += sizeof(l2cap_hdr_t);
  pHeader->dcid = cid;

  buffer.Write8(Offset, 0x03);
  Offset++;
  buffer.Write16(Offset, TransactionID);
  Offset += 2;  // Transaction ID
  buffer.Write16(Offset, 0x0009);
  Offset += 2;  // Param length
  buffer.Write16(Offset, 0x0001);
  Offset += 2;  // TotalServiceRecordCount
  buffer.Write16(Offset, 0x0001);
  Offset += 2;  // CurrentServiceRecordCount
  buffer.Write32(Offset, 0x10000);
  Offset += 4;  // ServiceRecordHandleList[4]
  buffer.Write8(Offset, 0x00);
  Offset++;  // No continuation state;

  pHeader->length = (u16)(Offset - sizeof(l2cap_hdr_t));
  m_pHost->SendACLPacket(GetConnectionHandle(), DataFrame, pHeader->length + sizeof(l2cap_hdr_t));
}

static u32 ParseCont(u8* pCont)
{
  u32 attribOffset = 0;
  CBigEndianBuffer attribList(pCont);
  u8 typeID = attribList.Read8(attribOffset);
  attribOffset++;

  if (typeID == 0x02)
  {
    return attribList.Read16(attribOffset);
  }
  else if (typeID == 0x00)
  {
    return 0x00;
  }
  ERROR_LOG(IOS_WIIMOTE, "ParseCont: wrong cont: %i", typeID);
  PanicAlert("ParseCont: wrong cont: %i", typeID);
  return 0;
}

static int ParseAttribList(u8* pAttribIDList, u16& _startID, u16& _endID)
{
  u32 attribOffset = 0;
  CBigEndianBuffer attribList(pAttribIDList);

  u8 sequence = attribList.Read8(attribOffset);
  attribOffset++;
  u8 seqSize = attribList.Read8(attribOffset);
  attribOffset++;
  u8 typeID = attribList.Read8(attribOffset);
  attribOffset++;

  if (MAX_LOGLEVEL >= LogTypes::LOG_LEVELS::LDEBUG)
  {
    _dbg_assert_(IOS_WIIMOTE, sequence == SDP_SEQ8);
    (void)seqSize;
  }

  if (typeID == SDP_UINT32)
  {
    _startID = attribList.Read16(attribOffset);
    attribOffset += 2;
    _endID = attribList.Read16(attribOffset);
    attribOffset += 2;
  }
  else
  {
    _startID = attribList.Read16(attribOffset);
    attribOffset += 2;
    _endID = _startID;
    WARN_LOG(IOS_WIIMOTE, "Read just a single attrib - not tested");
    PanicAlert("Read just a single attrib - not tested");
  }

  return attribOffset;
}

void WiimoteDevice::SDPSendServiceAttributeResponse(u16 cid, u16 TransactionID, u32 ServiceHandle,
                                                    u16 startAttrID, u16 endAttrID,
                                                    u16 MaximumAttributeByteCount,
                                                    u8* pContinuationState)
{
  if (ServiceHandle != 0x10000)
  {
    ERROR_LOG(IOS_WIIMOTE, "Unknown service handle %x", ServiceHandle);
    PanicAlert("Unknown service handle %x", ServiceHandle);
  }

  // _dbg_assert_(IOS_WIIMOTE, ServiceHandle == 0x10000);

  u32 contState = ParseCont(pContinuationState);

  u32 packetSize = 0;
  const u8* pPacket = GetAttribPacket(ServiceHandle, contState, packetSize);

  // generate package
  u8 DataFrame[1000];
  CBigEndianBuffer buffer(DataFrame);

  int Offset = 0;
  l2cap_hdr_t* pHeader = (l2cap_hdr_t*)&DataFrame[Offset];
  Offset += sizeof(l2cap_hdr_t);
  pHeader->dcid = cid;

  buffer.Write8(Offset, 0x05);
  Offset++;
  buffer.Write16(Offset, TransactionID);
  Offset += 2;  // Transaction ID

  memcpy(buffer.GetPointer(Offset), pPacket, packetSize);
  Offset += packetSize;

  pHeader->length = (u16)(Offset - sizeof(l2cap_hdr_t));
  m_pHost->SendACLPacket(GetConnectionHandle(), DataFrame, pHeader->length + sizeof(l2cap_hdr_t));

  // Debugger::PrintDataBuffer(LogTypes::WIIMOTE, DataFrame, pHeader->length + sizeof(l2cap_hdr_t),
  // "test response: ");
}

void WiimoteDevice::HandleSDP(u16 cid, u8* _pData, u32 _Size)
{
  // Debugger::PrintDataBuffer(LogTypes::WIIMOTE, _pData, _Size, "HandleSDP: ");

  CBigEndianBuffer buffer(_pData);

  switch (buffer.Read8(0))
  {
  // SDP_ServiceSearchRequest
  case 0x02:
  {
    WARN_LOG(IOS_WIIMOTE, "!!! SDP_ServiceSearchRequest !!!");

    _dbg_assert_(IOS_WIIMOTE, _Size == 13);

    u16 TransactionID = buffer.Read16(1);
    u8* pServiceSearchPattern = buffer.GetPointer(5);
    u16 MaximumServiceRecordCount = buffer.Read16(10);

    SDPSendServiceSearchResponse(cid, TransactionID, pServiceSearchPattern,
                                 MaximumServiceRecordCount);
  }
  break;

  // SDP_ServiceAttributeRequest
  case 0x04:
  {
    WARN_LOG(IOS_WIIMOTE, "!!! SDP_ServiceAttributeRequest !!!");

    u16 startAttrID, endAttrID;
    u32 offset = 1;

    u16 TransactionID = buffer.Read16(offset);
    offset += 2;
    // u16 ParameterLength = buffer.Read16(offset);
    offset += 2;
    u32 ServiceHandle = buffer.Read32(offset);
    offset += 4;
    u16 MaximumAttributeByteCount = buffer.Read16(offset);
    offset += 2;
    offset += ParseAttribList(buffer.GetPointer(offset), startAttrID, endAttrID);
    u8* pContinuationState = buffer.GetPointer(offset);

    SDPSendServiceAttributeResponse(cid, TransactionID, ServiceHandle, startAttrID, endAttrID,
                                    MaximumAttributeByteCount, pContinuationState);
  }
  break;

  default:
    ERROR_LOG(IOS_WIIMOTE, "WIIMOTE: Unknown SDP command %x", _pData[0]);
    PanicAlert("WIIMOTE: Unknown SDP command %x", _pData[0]);
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

void WiimoteDevice::SendCommandToACL(u8 _Ident, u8 _Code, u8 _CommandLength, u8* _pCommandData)
{
  u8 DataFrame[1024];
  u32 Offset = 0;

  l2cap_hdr_t* pHeader = (l2cap_hdr_t*)&DataFrame[Offset];
  Offset += sizeof(l2cap_hdr_t);
  pHeader->length = sizeof(l2cap_cmd_hdr_t) + _CommandLength;
  pHeader->dcid = L2CAP_SIGNAL_CID;

  l2cap_cmd_hdr_t* pCommand = (l2cap_cmd_hdr_t*)&DataFrame[Offset];
  Offset += sizeof(l2cap_cmd_hdr_t);
  pCommand->code = _Code;
  pCommand->ident = _Ident;
  pCommand->length = _CommandLength;

  memcpy(&DataFrame[Offset], _pCommandData, _CommandLength);

  DEBUG_LOG(IOS_WIIMOTE, "Send ACL Command to CPU");
  DEBUG_LOG(IOS_WIIMOTE, "    Ident: 0x%02x", _Ident);
  DEBUG_LOG(IOS_WIIMOTE, "    Code: 0x%02x", _Code);

  // send ....
  m_pHost->SendACLPacket(GetConnectionHandle(), DataFrame, pHeader->length + sizeof(l2cap_hdr_t));

  // Debugger::PrintDataBuffer(LogTypes::WIIMOTE, DataFrame, pHeader->length + sizeof(l2cap_hdr_t),
  // "m_pHost->SendACLPacket: ");
}

void WiimoteDevice::ReceiveL2capData(u16 scid, const void* _pData, u32 _Size)
{
  // Allocate DataFrame
  u8 DataFrame[1024];
  u32 Offset = 0;
  l2cap_hdr_t* pHeader = (l2cap_hdr_t*)DataFrame;
  Offset += sizeof(l2cap_hdr_t);

  // Check if we are already reporting on this channel
  _dbg_assert_(IOS_WIIMOTE, DoesChannelExist(scid));
  SChannel& rChannel = m_Channel[scid];

  // Add an additional 4 byte header to the Wiimote report
  pHeader->dcid = rChannel.DCID;
  pHeader->length = _Size;

  // Copy the Wiimote report to DataFrame
  memcpy(DataFrame + Offset, _pData, _Size);
  // Update Offset to the final size of the report
  Offset += _Size;

  // Update the status bar
  Host_SetWiiMoteConnectionState(2);

  // Send the report
  m_pHost->SendACLPacket(GetConnectionHandle(), DataFrame, Offset);
}
}  // namespace HLE
}  // namespace IOS

namespace Core
{
/* This is called continuously from the Wiimote plugin as soon as it has received
   a reporting mode. _Size is the byte size of the report. */
void Callback_WiimoteInterruptChannel(int _number, u16 _channelID, const void* _pData, u32 _Size)
{
  const u8* pData = (const u8*)_pData;

  DEBUG_LOG(WIIMOTE, "====================");
  DEBUG_LOG(WIIMOTE, "Callback_WiimoteInterruptChannel: (Wiimote: #%i)", _number);
  DEBUG_LOG(WIIMOTE, "   Data: %s", ArrayToString(pData, _Size, 50).c_str());
  DEBUG_LOG(WIIMOTE, "   Channel: %x", _channelID);

  const auto bt = std::static_pointer_cast<IOS::HLE::Device::BluetoothEmu>(
      IOS::HLE::GetIOS()->GetDeviceByName("/dev/usb/oh1/57e/305"));
  if (bt)
    bt->m_WiiMotes[_number].ReceiveL2capData(_channelID, _pData, _Size);
}
}
