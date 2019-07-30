// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/IOS/USB/Bluetooth/BTEmu.h"

#include <algorithm>
#include <cstring>
#include <memory>
#include <string>

#include <fmt/format.h>

#include "Common/Assert.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Common/NandPaths.h"
#include "Common/StringUtil.h"
#include "Core/Core.h"
#include "Core/CoreTiming.h"
#include "Core/Debugger/Debugger_SymbolMap.h"
#include "Core/HW/Memmap.h"
#include "Core/HW/SystemTimers.h"
#include "Core/HW/Wiimote.h"
#include "Core/IOS/Device.h"
#include "Core/IOS/IOS.h"
#include "Core/SysConf.h"
#include "InputCommon/ControllerInterface/ControllerInterface.h"

namespace IOS::HLE
{
SQueuedEvent::SQueuedEvent(u32 size_, u16 handle) : size(size_), connection_handle(handle)
{
  if (size > 1024)
    PanicAlert("SQueuedEvent: The size is too large.");
}

namespace Device
{
BluetoothEmu::BluetoothEmu(Kernel& ios, const std::string& device_name)
    : BluetoothBase(ios, device_name)
{
  SysConf sysconf{ios.GetFS()};
  if (!Core::WantsDeterminism())
    BackUpBTInfoSection(&sysconf);

  ConfPads bt_dinf{};
  bdaddr_t tmp_bd;
  u8 i = 0;
  while (i < MAX_BBMOTES)
  {
    // Previous records can be safely overwritten, since they are backed up
    tmp_bd[5] = bt_dinf.active[i].bdaddr[0] = bt_dinf.registered[i].bdaddr[0] = i;
    tmp_bd[4] = bt_dinf.active[i].bdaddr[1] = bt_dinf.registered[i].bdaddr[1] = 0;
    tmp_bd[3] = bt_dinf.active[i].bdaddr[2] = bt_dinf.registered[i].bdaddr[2] = 0x79;
    tmp_bd[2] = bt_dinf.active[i].bdaddr[3] = bt_dinf.registered[i].bdaddr[3] = 0x19;
    tmp_bd[1] = bt_dinf.active[i].bdaddr[4] = bt_dinf.registered[i].bdaddr[4] = 2;
    tmp_bd[0] = bt_dinf.active[i].bdaddr[5] = bt_dinf.registered[i].bdaddr[5] = 0x11;

    const char* wm_name;
    if (i == WIIMOTE_BALANCE_BOARD)
      wm_name = "Nintendo RVL-WBC-01";
    else
      wm_name = "Nintendo RVL-CNT-01";
    memcpy(bt_dinf.registered[i].name, wm_name, 20);
    memcpy(bt_dinf.active[i].name, wm_name, 20);

    DEBUG_LOG(IOS_WIIMOTE, "Wii Remote %d BT ID %x,%x,%x,%x,%x,%x", i, tmp_bd[0], tmp_bd[1],
              tmp_bd[2], tmp_bd[3], tmp_bd[4], tmp_bd[5]);
    m_wiimotes.emplace_back(this, i, tmp_bd, g_wiimote_sources[i] != WIIMOTE_SRC_NONE);
    i++;
  }

  bt_dinf.num_registered = MAX_BBMOTES;

  // save now so that when games load sysconf file it includes the new Wii Remotes
  // and the correct order for connected Wii Remotes
  auto& section = sysconf.GetOrAddEntry("BT.DINF", SysConf::Entry::Type::BigArray)->bytes;
  section.resize(sizeof(ConfPads));
  std::memcpy(section.data(), &bt_dinf, sizeof(ConfPads));
  if (!sysconf.Save())
    PanicAlertT("Failed to write BT.DINF to SYSCONF");
}

BluetoothEmu::~BluetoothEmu()
{
  m_wiimotes.clear();
}

template <typename T>
static void DoStateForMessage(Kernel& ios, PointerWrap& p, std::unique_ptr<T>& message)
{
  u32 request_address = (message != nullptr) ? message->ios_request.address : 0;
  p.Do(request_address);
  if (request_address != 0)
  {
    IOCtlVRequest request{request_address};
    message = std::make_unique<T>(ios, request);
  }
}

void BluetoothEmu::DoState(PointerWrap& p)
{
  bool passthrough_bluetooth = false;
  p.Do(passthrough_bluetooth);
  if (passthrough_bluetooth && p.GetMode() == PointerWrap::MODE_READ)
  {
    Core::DisplayMessage("State needs Bluetooth passthrough to be enabled. Aborting load.", 4000);
    p.SetMode(PointerWrap::MODE_VERIFY);
    return;
  }

  p.Do(m_is_active);
  p.Do(m_controller_bd);
  DoStateForMessage(m_ios, p, m_ctrl_setup);
  DoStateForMessage(m_ios, p, m_hci_endpoint);
  DoStateForMessage(m_ios, p, m_acl_endpoint);
  p.Do(m_last_ticks);
  p.DoArray(m_packet_count);
  p.Do(m_scan_enable);
  p.Do(m_event_queue);
  m_acl_pool.DoState(p);

  for (unsigned int i = 0; i < MAX_BBMOTES; i++)
    m_wiimotes[i].DoState(p);
}

bool BluetoothEmu::RemoteDisconnect(u16 connection_handle)
{
  return SendEventDisconnect(connection_handle, 0x13);
}

IPCCommandResult BluetoothEmu::Close(u32 fd)
{
  // Clean up state
  m_scan_enable = 0;
  m_last_ticks = 0;
  memset(m_packet_count, 0, sizeof(m_packet_count));
  m_hci_endpoint.reset();
  m_acl_endpoint.reset();

  return Device::Close(fd);
}

IPCCommandResult BluetoothEmu::IOCtlV(const IOCtlVRequest& request)
{
  bool send_reply = true;
  switch (request.request)
  {
  case USB::IOCTLV_USBV0_CTRLMSG:  // HCI command is received from the stack
  {
    m_ctrl_setup = std::make_unique<USB::V0CtrlMessage>(m_ios, request);
    // Replies are generated inside
    ExecuteHCICommandMessage(*m_ctrl_setup);
    m_ctrl_setup.reset();
    send_reply = false;
    break;
  }

  case USB::IOCTLV_USBV0_BLKMSG:
  {
    const USB::V0BulkMessage ctrl{m_ios, request};
    switch (ctrl.endpoint)
    {
    case ACL_DATA_OUT:  // ACL data is received from the stack
    {
      // This is the ACL datapath from CPU to Wii Remote
      const auto* acl_header =
          reinterpret_cast<hci_acldata_hdr_t*>(Memory::GetPointer(ctrl.data_address));

      DEBUG_ASSERT(HCI_BC_FLAG(acl_header->con_handle) == HCI_POINT2POINT);
      DEBUG_ASSERT(HCI_PB_FLAG(acl_header->con_handle) == HCI_PACKET_START);

      SendToDevice(HCI_CON_HANDLE(acl_header->con_handle),
                   Memory::GetPointer(ctrl.data_address + sizeof(hci_acldata_hdr_t)),
                   acl_header->length);
      break;
    }
    case ACL_DATA_IN:  // We are given an ACL buffer to fill
    {
      m_acl_endpoint = std::make_unique<USB::V0BulkMessage>(m_ios, request);
      DEBUG_LOG(IOS_WIIMOTE, "ACL_DATA_IN: 0x%08x ", request.address);
      send_reply = false;
      break;
    }
    default:
      DEBUG_ASSERT_MSG(IOS_WIIMOTE, 0, "Unknown USB::IOCTLV_USBV0_BLKMSG: %x", ctrl.endpoint);
    }
    break;
  }

  case USB::IOCTLV_USBV0_INTRMSG:
  {
    const USB::V0IntrMessage ctrl{m_ios, request};
    if (ctrl.endpoint == HCI_EVENT)  // We are given a HCI buffer to fill
    {
      m_hci_endpoint = std::make_unique<USB::V0IntrMessage>(m_ios, request);
      DEBUG_LOG(IOS_WIIMOTE, "HCI_EVENT: 0x%08x ", request.address);
      send_reply = false;
    }
    else
    {
      DEBUG_ASSERT_MSG(IOS_WIIMOTE, 0, "Unknown USB::IOCTLV_USBV0_INTRMSG: %x", ctrl.endpoint);
    }
    break;
  }

  default:
    request.DumpUnknown(GetDeviceName(), LogTypes::IOS_WIIMOTE);
  }

  return send_reply ? GetDefaultReply(IPC_SUCCESS) : GetNoReply();
}

// Here we handle the USB::IOCTLV_USBV0_BLKMSG Ioctlv
void BluetoothEmu::SendToDevice(u16 connection_handle, u8* data, u32 size)
{
  WiimoteDevice* wiimote = AccessWiimote(connection_handle);
  if (wiimote == nullptr)
    return;

  DEBUG_LOG(IOS_WIIMOTE, "Send ACL Packet to ConnectionHandle 0x%04x", connection_handle);
  IncDataPacket(connection_handle);
  wiimote->ExecuteL2capCmd(data, size);
}

void BluetoothEmu::IncDataPacket(u16 connection_handle)
{
  m_packet_count[connection_handle & 0xff]++;
}

// Here we send ACL packets to CPU. They will consist of header + data.
// The header is for example 07 00 41 00 which means size 0x0007 and channel 0x0041.
void BluetoothEmu::SendACLPacket(u16 connection_handle, const u8* data, u32 size)
{
  DEBUG_LOG(IOS_WIIMOTE, "ACL packet from %x ready to send to stack...", connection_handle);

  if (m_acl_endpoint && !m_hci_endpoint && m_event_queue.empty())
  {
    DEBUG_LOG(IOS_WIIMOTE, "ACL endpoint valid, sending packet to %08x",
              m_acl_endpoint->ios_request.address);

    hci_acldata_hdr_t* header =
        reinterpret_cast<hci_acldata_hdr_t*>(Memory::GetPointer(m_acl_endpoint->data_address));
    header->con_handle = HCI_MK_CON_HANDLE(connection_handle, HCI_PACKET_START, HCI_POINT2POINT);
    header->length = size;

    // Write the packet to the buffer
    memcpy(reinterpret_cast<u8*>(header) + sizeof(hci_acldata_hdr_t), data, header->length);

    m_ios.EnqueueIPCReply(m_acl_endpoint->ios_request, sizeof(hci_acldata_hdr_t) + size);
    m_acl_endpoint.reset();
  }
  else
  {
    DEBUG_LOG(IOS_WIIMOTE, "ACL endpoint not currently valid, queuing...");
    m_acl_pool.Store(data, size, connection_handle);
  }
}

// These messages are sent from the Wii Remote to the game, for example RequestConnection()
// or ConnectionComplete().
//
// Our IOS is so efficient that we could fill the buffer immediately
// rather than enqueue it to some other memory and this will do good for StateSave
void BluetoothEmu::AddEventToQueue(const SQueuedEvent& event)
{
  DEBUG_LOG(IOS_WIIMOTE, "HCI event %x completed...", ((hci_event_hdr_t*)event.buffer)->event);

  if (m_hci_endpoint)
  {
    if (m_event_queue.empty())  // fast path :)
    {
      DEBUG_LOG(IOS_WIIMOTE, "HCI endpoint valid, sending packet to %08x",
                m_hci_endpoint->ios_request.address);
      m_hci_endpoint->FillBuffer(event.buffer, event.size);

      // Send a reply to indicate HCI buffer is filled
      m_ios.EnqueueIPCReply(m_hci_endpoint->ios_request, event.size);
      m_hci_endpoint.reset();
    }
    else  // push new one, pop oldest
    {
      DEBUG_LOG(IOS_WIIMOTE, "HCI endpoint not currently valid, queueing (%zu)...",
                m_event_queue.size());
      m_event_queue.push_back(event);
      const SQueuedEvent& queued_event = m_event_queue.front();
      DEBUG_LOG(IOS_WIIMOTE,
                "HCI event %x "
                "being written from queue (%zu) to %08x...",
                ((hci_event_hdr_t*)queued_event.buffer)->event, m_event_queue.size() - 1,
                m_hci_endpoint->ios_request.address);
      m_hci_endpoint->FillBuffer(queued_event.buffer, queued_event.size);

      // Send a reply to indicate HCI buffer is filled
      m_ios.EnqueueIPCReply(m_hci_endpoint->ios_request, queued_event.size);
      m_hci_endpoint.reset();
      m_event_queue.pop_front();
    }
  }
  else
  {
    DEBUG_LOG(IOS_WIIMOTE, "HCI endpoint not currently valid, queuing (%zu)...",
              m_event_queue.size());
    m_event_queue.push_back(event);
  }
}

void BluetoothEmu::Update()
{
  // check HCI queue
  if (!m_event_queue.empty() && m_hci_endpoint)
  {
    // an endpoint has become available, and we have a stored response.
    const SQueuedEvent& event = m_event_queue.front();
    DEBUG_LOG(IOS_WIIMOTE, "HCI event %x being written from queue (%zu) to %08x...",
              ((hci_event_hdr_t*)event.buffer)->event, m_event_queue.size() - 1,
              m_hci_endpoint->ios_request.address);
    m_hci_endpoint->FillBuffer(event.buffer, event.size);

    // Send a reply to indicate HCI buffer is filled
    m_ios.EnqueueIPCReply(m_hci_endpoint->ios_request, event.size);
    m_hci_endpoint.reset();
    m_event_queue.pop_front();
  }

  // check ACL queue
  if (!m_acl_pool.IsEmpty() && m_acl_endpoint && m_event_queue.empty())
  {
    m_acl_pool.WriteToEndpoint(*m_acl_endpoint);
    m_acl_endpoint.reset();
  }

  // We wait for ScanEnable to be sent from the Bluetooth stack through HCI_CMD_WRITE_SCAN_ENABLE
  // before we initiate the connection.
  //
  // FiRES: TODO find a better way to do this

  // Create ACL connection
  if (m_hci_endpoint && (m_scan_enable & HCI_PAGE_SCAN_ENABLE))
  {
    for (const auto& wiimote : m_wiimotes)
    {
      if (wiimote.EventPagingChanged(m_scan_enable))
        SendEventRequestConnection(wiimote);
    }
  }

  // Link channels when connected
  if (m_acl_endpoint)
  {
    for (auto& wiimote : m_wiimotes)
    {
      if (wiimote.LinkChannel())
        break;
    }
  }

  const u64 interval = SystemTimers::GetTicksPerSecond() / Wiimote::UPDATE_FREQ;
  const u64 now = CoreTiming::GetTicks();

  if (now - m_last_ticks > interval)
  {
    g_controller_interface.UpdateInput();
    for (unsigned int i = 0; i < m_wiimotes.size(); i++)
      Wiimote::Update(i, m_wiimotes[i].IsConnected());
    m_last_ticks = now;
  }

  SendEventNumberOfCompletedPackets();
}

void BluetoothEmu::ACLPool::Store(const u8* data, const u16 size, const u16 conn_handle)
{
  if (m_queue.size() >= 100)
  {
    // Many simultaneous exchanges of ACL packets tend to cause the queue to fill up.
    ERROR_LOG(IOS_WIIMOTE, "ACL queue size reached 100 - current packet will be dropped!");
    return;
  }

  DEBUG_ASSERT_MSG(IOS_WIIMOTE, size < ACL_PKT_SIZE, "ACL packet too large for pool");

  m_queue.push_back(Packet());
  auto& packet = m_queue.back();

  std::copy(data, data + size, packet.data);
  packet.size = size;
  packet.conn_handle = conn_handle;
}

void BluetoothEmu::ACLPool::WriteToEndpoint(const USB::V0BulkMessage& endpoint)
{
  auto& packet = m_queue.front();

  const u8* const data = packet.data;
  const u16 size = packet.size;
  const u16 conn_handle = packet.conn_handle;

  DEBUG_LOG(IOS_WIIMOTE,
            "ACL packet being written from "
            "queue to %08x",
            endpoint.ios_request.address);

  hci_acldata_hdr_t* header = (hci_acldata_hdr_t*)Memory::GetPointer(endpoint.data_address);
  header->con_handle = HCI_MK_CON_HANDLE(conn_handle, HCI_PACKET_START, HCI_POINT2POINT);
  header->length = size;

  // Write the packet to the buffer
  std::copy(data, data + size, (u8*)header + sizeof(hci_acldata_hdr_t));

  m_queue.pop_front();

  m_ios.EnqueueIPCReply(endpoint.ios_request, sizeof(hci_acldata_hdr_t) + size);
}

bool BluetoothEmu::SendEventInquiryComplete()
{
  SQueuedEvent event(sizeof(SHCIEventInquiryComplete), 0);

  SHCIEventInquiryComplete* inquiry_complete = (SHCIEventInquiryComplete*)event.buffer;
  inquiry_complete->EventType = HCI_EVENT_INQUIRY_COMPL;
  inquiry_complete->PayloadLength = sizeof(SHCIEventInquiryComplete) - 2;
  inquiry_complete->EventStatus = 0x00;

  AddEventToQueue(event);

  DEBUG_LOG(IOS_WIIMOTE, "Event: Inquiry complete");

  return true;
}

bool BluetoothEmu::SendEventInquiryResponse()
{
  if (m_wiimotes.empty())
    return false;

  DEBUG_ASSERT(sizeof(SHCIEventInquiryResult) - 2 +
                   (m_wiimotes.size() * sizeof(hci_inquiry_response)) <
               256);

  SQueuedEvent event(static_cast<u32>(sizeof(SHCIEventInquiryResult) +
                                      m_wiimotes.size() * sizeof(hci_inquiry_response)),
                     0);

  SHCIEventInquiryResult* inquiry_result = (SHCIEventInquiryResult*)event.buffer;

  inquiry_result->EventType = HCI_EVENT_INQUIRY_RESULT;
  inquiry_result->PayloadLength =
      (u8)(sizeof(SHCIEventInquiryResult) - 2 + (m_wiimotes.size() * sizeof(hci_inquiry_response)));
  inquiry_result->num_responses = (u8)m_wiimotes.size();

  for (size_t i = 0; i < m_wiimotes.size(); i++)
  {
    if (m_wiimotes[i].IsConnected())
      continue;

    u8* buffer = event.buffer + sizeof(SHCIEventInquiryResult) + i * sizeof(hci_inquiry_response);
    hci_inquiry_response* response = (hci_inquiry_response*)buffer;

    response->bdaddr = m_wiimotes[i].GetBD();
    response->uclass[0] = m_wiimotes[i].GetClass()[0];
    response->uclass[1] = m_wiimotes[i].GetClass()[1];
    response->uclass[2] = m_wiimotes[i].GetClass()[2];

    response->page_scan_rep_mode = 1;
    response->page_scan_period_mode = 0;
    response->page_scan_mode = 0;
    response->clock_offset = 0x3818;

    DEBUG_LOG(IOS_WIIMOTE, "Event: Send Fake Inquiry of one controller");
    DEBUG_LOG(IOS_WIIMOTE, "  bd: %02x:%02x:%02x:%02x:%02x:%02x", response->bdaddr[0],
              response->bdaddr[1], response->bdaddr[2], response->bdaddr[3], response->bdaddr[4],
              response->bdaddr[5]);
  }

  AddEventToQueue(event);

  return true;
}

bool BluetoothEmu::SendEventConnectionComplete(const bdaddr_t& bd)
{
  WiimoteDevice* wiimote = AccessWiimote(bd);
  if (wiimote == nullptr)
    return false;

  SQueuedEvent event(sizeof(SHCIEventConnectionComplete), 0);

  SHCIEventConnectionComplete* connection_complete = (SHCIEventConnectionComplete*)event.buffer;

  connection_complete->EventType = HCI_EVENT_CON_COMPL;
  connection_complete->PayloadLength = sizeof(SHCIEventConnectionComplete) - 2;
  connection_complete->EventStatus = 0x00;
  connection_complete->Connection_Handle = wiimote->GetConnectionHandle();
  connection_complete->bdaddr = bd;
  connection_complete->LinkType = HCI_LINK_ACL;
  connection_complete->EncryptionEnabled = HCI_ENCRYPTION_MODE_NONE;

  AddEventToQueue(event);

  WiimoteDevice* connection_wiimote = AccessWiimote(connection_complete->Connection_Handle);
  if (connection_wiimote)
    connection_wiimote->EventConnectionAccepted();

  static constexpr const char* link_type[] = {
      "HCI_LINK_SCO     0x00 - Voice",
      "HCI_LINK_ACL     0x01 - Data",
      "HCI_LINK_eSCO    0x02 - eSCO",
  };

  DEBUG_LOG(IOS_WIIMOTE, "Event: SendEventConnectionComplete");
  DEBUG_LOG(IOS_WIIMOTE, "  Connection_Handle: 0x%04x", connection_complete->Connection_Handle);
  DEBUG_LOG(IOS_WIIMOTE, "  bd: %02x:%02x:%02x:%02x:%02x:%02x", connection_complete->bdaddr[0],
            connection_complete->bdaddr[1], connection_complete->bdaddr[2],
            connection_complete->bdaddr[3], connection_complete->bdaddr[4],
            connection_complete->bdaddr[5]);
  DEBUG_LOG(IOS_WIIMOTE, "  LinkType: %s", link_type[connection_complete->LinkType]);
  DEBUG_LOG(IOS_WIIMOTE, "  EncryptionEnabled: %i", connection_complete->EncryptionEnabled);

  return true;
}

// This is called from Update() after ScanEnable has been enabled.
bool BluetoothEmu::SendEventRequestConnection(const WiimoteDevice& wiimote)
{
  SQueuedEvent event(sizeof(SHCIEventRequestConnection), 0);

  SHCIEventRequestConnection* event_request_connection = (SHCIEventRequestConnection*)event.buffer;

  event_request_connection->EventType = HCI_EVENT_CON_REQ;
  event_request_connection->PayloadLength = sizeof(SHCIEventRequestConnection) - 2;
  event_request_connection->bdaddr = wiimote.GetBD();
  event_request_connection->uclass[0] = wiimote.GetClass()[0];
  event_request_connection->uclass[1] = wiimote.GetClass()[1];
  event_request_connection->uclass[2] = wiimote.GetClass()[2];
  event_request_connection->LinkType = HCI_LINK_ACL;

  AddEventToQueue(event);

  static constexpr const char* link_type[] = {
      "HCI_LINK_SCO     0x00 - Voice",
      "HCI_LINK_ACL     0x01 - Data",
      "HCI_LINK_eSCO    0x02 - eSCO",
  };

  DEBUG_LOG(IOS_WIIMOTE, "Event: SendEventRequestConnection");
  DEBUG_LOG(IOS_WIIMOTE, "  bd: %02x:%02x:%02x:%02x:%02x:%02x", event_request_connection->bdaddr[0],
            event_request_connection->bdaddr[1], event_request_connection->bdaddr[2],
            event_request_connection->bdaddr[3], event_request_connection->bdaddr[4],
            event_request_connection->bdaddr[5]);
  DEBUG_LOG(IOS_WIIMOTE, "  COD[0]: 0x%02x", event_request_connection->uclass[0]);
  DEBUG_LOG(IOS_WIIMOTE, "  COD[1]: 0x%02x", event_request_connection->uclass[1]);
  DEBUG_LOG(IOS_WIIMOTE, "  COD[2]: 0x%02x", event_request_connection->uclass[2]);
  DEBUG_LOG(IOS_WIIMOTE, "  LinkType: %s", link_type[event_request_connection->LinkType]);

  return true;
}

bool BluetoothEmu::SendEventDisconnect(u16 connection_handle, u8 reason)
{
  WiimoteDevice* wiimote = AccessWiimote(connection_handle);
  if (wiimote == nullptr)
    return false;

  SQueuedEvent event(sizeof(SHCIEventDisconnectCompleted), connection_handle);

  SHCIEventDisconnectCompleted* disconnect = (SHCIEventDisconnectCompleted*)event.buffer;
  disconnect->EventType = HCI_EVENT_DISCON_COMPL;
  disconnect->PayloadLength = sizeof(SHCIEventDisconnectCompleted) - 2;
  disconnect->EventStatus = 0;
  disconnect->Connection_Handle = connection_handle;
  disconnect->Reason = reason;

  AddEventToQueue(event);

  DEBUG_LOG(IOS_WIIMOTE, "Event: SendEventDisconnect");
  DEBUG_LOG(IOS_WIIMOTE, "  Connection_Handle: 0x%04x", disconnect->Connection_Handle);
  DEBUG_LOG(IOS_WIIMOTE, "  Reason: 0x%02x", disconnect->Reason);

  return true;
}

bool BluetoothEmu::SendEventAuthenticationCompleted(u16 connection_handle)
{
  WiimoteDevice* wiimote = AccessWiimote(connection_handle);
  if (wiimote == nullptr)
    return false;

  SQueuedEvent event(sizeof(SHCIEventAuthenticationCompleted), connection_handle);

  SHCIEventAuthenticationCompleted* event_authentication_completed =
      (SHCIEventAuthenticationCompleted*)event.buffer;
  event_authentication_completed->EventType = HCI_EVENT_AUTH_COMPL;
  event_authentication_completed->PayloadLength = sizeof(SHCIEventAuthenticationCompleted) - 2;
  event_authentication_completed->EventStatus = 0;
  event_authentication_completed->Connection_Handle = connection_handle;

  DEBUG_LOG(IOS_WIIMOTE, "Event: SendEventAuthenticationCompleted");
  DEBUG_LOG(IOS_WIIMOTE, "  Connection_Handle: 0x%04x",
            event_authentication_completed->Connection_Handle);

  AddEventToQueue(event);

  return true;
}

bool BluetoothEmu::SendEventRemoteNameReq(const bdaddr_t& bd)
{
  WiimoteDevice* wiimote = AccessWiimote(bd);
  if (wiimote == nullptr)
    return false;

  SQueuedEvent event(sizeof(SHCIEventRemoteNameReq), 0);

  SHCIEventRemoteNameReq* remote_name_req = (SHCIEventRemoteNameReq*)event.buffer;

  remote_name_req->EventType = HCI_EVENT_REMOTE_NAME_REQ_COMPL;
  remote_name_req->PayloadLength = sizeof(SHCIEventRemoteNameReq) - 2;
  remote_name_req->EventStatus = 0x00;
  remote_name_req->bdaddr = bd;
  strcpy((char*)remote_name_req->RemoteName, wiimote->GetName());

  DEBUG_LOG(IOS_WIIMOTE, "Event: SendEventRemoteNameReq");
  DEBUG_LOG(IOS_WIIMOTE, "  bd: %02x:%02x:%02x:%02x:%02x:%02x", remote_name_req->bdaddr[0],
            remote_name_req->bdaddr[1], remote_name_req->bdaddr[2], remote_name_req->bdaddr[3],
            remote_name_req->bdaddr[4], remote_name_req->bdaddr[5]);
  DEBUG_LOG(IOS_WIIMOTE, "  RemoteName: %s", remote_name_req->RemoteName);

  AddEventToQueue(event);

  return true;
}

bool BluetoothEmu::SendEventReadRemoteFeatures(u16 connection_handle)
{
  WiimoteDevice* wiimote = AccessWiimote(connection_handle);
  if (wiimote == nullptr)
    return false;

  SQueuedEvent event(sizeof(SHCIEventReadRemoteFeatures), connection_handle);

  SHCIEventReadRemoteFeatures* read_remote_features = (SHCIEventReadRemoteFeatures*)event.buffer;

  read_remote_features->EventType = HCI_EVENT_READ_REMOTE_FEATURES_COMPL;
  read_remote_features->PayloadLength = sizeof(SHCIEventReadRemoteFeatures) - 2;
  read_remote_features->EventStatus = 0x00;
  read_remote_features->ConnectionHandle = connection_handle;
  read_remote_features->features[0] = wiimote->GetFeatures()[0];
  read_remote_features->features[1] = wiimote->GetFeatures()[1];
  read_remote_features->features[2] = wiimote->GetFeatures()[2];
  read_remote_features->features[3] = wiimote->GetFeatures()[3];
  read_remote_features->features[4] = wiimote->GetFeatures()[4];
  read_remote_features->features[5] = wiimote->GetFeatures()[5];
  read_remote_features->features[6] = wiimote->GetFeatures()[6];
  read_remote_features->features[7] = wiimote->GetFeatures()[7];

  DEBUG_LOG(IOS_WIIMOTE, "Event: SendEventReadRemoteFeatures");
  DEBUG_LOG(IOS_WIIMOTE, "  Connection_Handle: 0x%04x", read_remote_features->ConnectionHandle);
  DEBUG_LOG(IOS_WIIMOTE, "  features: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x",
            read_remote_features->features[0], read_remote_features->features[1],
            read_remote_features->features[2], read_remote_features->features[3],
            read_remote_features->features[4], read_remote_features->features[5],
            read_remote_features->features[6], read_remote_features->features[7]);

  AddEventToQueue(event);

  return true;
}

bool BluetoothEmu::SendEventReadRemoteVerInfo(u16 connection_handle)
{
  WiimoteDevice* wiimote = AccessWiimote(connection_handle);
  if (wiimote == nullptr)
    return false;

  SQueuedEvent event(sizeof(SHCIEventReadRemoteVerInfo), connection_handle);

  SHCIEventReadRemoteVerInfo* read_remote_ver_info = (SHCIEventReadRemoteVerInfo*)event.buffer;
  read_remote_ver_info->EventType = HCI_EVENT_READ_REMOTE_VER_INFO_COMPL;
  read_remote_ver_info->PayloadLength = sizeof(SHCIEventReadRemoteVerInfo) - 2;
  read_remote_ver_info->EventStatus = 0x00;
  read_remote_ver_info->ConnectionHandle = connection_handle;
  read_remote_ver_info->lmp_version = wiimote->GetLMPVersion();
  read_remote_ver_info->manufacturer = wiimote->GetManufactorID();
  read_remote_ver_info->lmp_subversion = wiimote->GetLMPSubVersion();

  DEBUG_LOG(IOS_WIIMOTE, "Event: SendEventReadRemoteVerInfo");
  DEBUG_LOG(IOS_WIIMOTE, "  Connection_Handle: 0x%04x", read_remote_ver_info->ConnectionHandle);
  DEBUG_LOG(IOS_WIIMOTE, "  lmp_version: 0x%02x", read_remote_ver_info->lmp_version);
  DEBUG_LOG(IOS_WIIMOTE, "  manufacturer: 0x%04x", read_remote_ver_info->manufacturer);
  DEBUG_LOG(IOS_WIIMOTE, "  lmp_subversion: 0x%04x", read_remote_ver_info->lmp_subversion);

  AddEventToQueue(event);

  return true;
}

void BluetoothEmu::SendEventCommandComplete(u16 opcode, const void* data, u32 data_size)
{
  DEBUG_ASSERT((sizeof(SHCIEventCommand) - 2 + data_size) < 256);

  SQueuedEvent event(sizeof(SHCIEventCommand) + data_size, 0);

  SHCIEventCommand* hci_event = reinterpret_cast<SHCIEventCommand*>(event.buffer);
  hci_event->EventType = HCI_EVENT_COMMAND_COMPL;
  hci_event->PayloadLength = (u8)(sizeof(SHCIEventCommand) - 2 + data_size);
  hci_event->PacketIndicator = 0x01;
  hci_event->Opcode = opcode;

  // add the payload
  if (data != nullptr && data_size > 0)
  {
    u8* payload = event.buffer + sizeof(SHCIEventCommand);
    memcpy(payload, data, data_size);
  }

  DEBUG_LOG(IOS_WIIMOTE, "Event: Command Complete (Opcode: 0x%04x)", hci_event->Opcode);

  AddEventToQueue(event);
}

bool BluetoothEmu::SendEventCommandStatus(u16 opcode)
{
  SQueuedEvent event(sizeof(SHCIEventStatus), 0);

  SHCIEventStatus* hci_event = (SHCIEventStatus*)event.buffer;
  hci_event->EventType = HCI_EVENT_COMMAND_STATUS;
  hci_event->PayloadLength = sizeof(SHCIEventStatus) - 2;
  hci_event->EventStatus = 0x0;
  hci_event->PacketIndicator = 0x01;
  hci_event->Opcode = opcode;

  INFO_LOG(IOS_WIIMOTE, "Event: Command Status (Opcode: 0x%04x)", hci_event->Opcode);

  AddEventToQueue(event);

  return true;
}

bool BluetoothEmu::SendEventRoleChange(bdaddr_t bd, bool master)
{
  WiimoteDevice* wiimote = AccessWiimote(bd);
  if (wiimote == nullptr)
    return false;

  SQueuedEvent event(sizeof(SHCIEventRoleChange), 0);

  SHCIEventRoleChange* role_change = (SHCIEventRoleChange*)event.buffer;

  role_change->EventType = HCI_EVENT_ROLE_CHANGE;
  role_change->PayloadLength = sizeof(SHCIEventRoleChange) - 2;
  role_change->EventStatus = 0x00;
  role_change->bdaddr = bd;
  role_change->NewRole = master ? 0x00 : 0x01;

  AddEventToQueue(event);

  DEBUG_LOG(IOS_WIIMOTE, "Event: SendEventRoleChange");
  DEBUG_LOG(IOS_WIIMOTE, "  bd: %02x:%02x:%02x:%02x:%02x:%02x", role_change->bdaddr[0],
            role_change->bdaddr[1], role_change->bdaddr[2], role_change->bdaddr[3],
            role_change->bdaddr[4], role_change->bdaddr[5]);
  DEBUG_LOG(IOS_WIIMOTE, "  NewRole: %i", role_change->NewRole);

  return true;
}

bool BluetoothEmu::SendEventNumberOfCompletedPackets()
{
  SQueuedEvent event((u32)(sizeof(hci_event_hdr_t) + sizeof(hci_num_compl_pkts_ep) +
                           (sizeof(hci_num_compl_pkts_info) * m_wiimotes.size())),
                     0);

  DEBUG_LOG(IOS_WIIMOTE, "Event: SendEventNumberOfCompletedPackets");

  auto* event_hdr = (hci_event_hdr_t*)event.buffer;
  auto* hci_event = (hci_num_compl_pkts_ep*)((u8*)event_hdr + sizeof(hci_event_hdr_t));
  auto* info = (hci_num_compl_pkts_info*)((u8*)hci_event + sizeof(hci_num_compl_pkts_ep));

  event_hdr->event = HCI_EVENT_NUM_COMPL_PKTS;
  event_hdr->length = sizeof(hci_num_compl_pkts_ep);
  hci_event->num_con_handles = 0;

  u32 acc = 0;

  for (unsigned int i = 0; i < m_wiimotes.size(); i++)
  {
    event_hdr->length += sizeof(hci_num_compl_pkts_info);
    hci_event->num_con_handles++;
    info->compl_pkts = m_packet_count[i];
    info->con_handle = m_wiimotes[i].GetConnectionHandle();

    DEBUG_LOG(IOS_WIIMOTE, "  Connection_Handle: 0x%04x", info->con_handle);
    DEBUG_LOG(IOS_WIIMOTE, "  Number_Of_Completed_Packets: %i", info->compl_pkts);

    acc += info->compl_pkts;
    m_packet_count[i] = 0;
    info++;
  }

  if (acc)
  {
    AddEventToQueue(event);
  }
  else
  {
    DEBUG_LOG(IOS_WIIMOTE, "SendEventNumberOfCompletedPackets: no packets; no event");
  }

  return true;
}

bool BluetoothEmu::SendEventModeChange(u16 connection_handle, u8 mode, u16 value)
{
  WiimoteDevice* wiimote = AccessWiimote(connection_handle);
  if (wiimote == nullptr)
    return false;

  SQueuedEvent event(sizeof(SHCIEventModeChange), connection_handle);

  SHCIEventModeChange* mode_change = (SHCIEventModeChange*)event.buffer;
  mode_change->EventType = HCI_EVENT_MODE_CHANGE;
  mode_change->PayloadLength = sizeof(SHCIEventModeChange) - 2;
  mode_change->EventStatus = 0;
  mode_change->Connection_Handle = connection_handle;
  mode_change->CurrentMode = mode;
  mode_change->Value = value;

  DEBUG_LOG(IOS_WIIMOTE, "Event: SendEventModeChange");
  DEBUG_LOG(IOS_WIIMOTE, "  Connection_Handle: 0x%04x", mode_change->Connection_Handle);
  DEBUG_LOG(IOS_WIIMOTE, "  Current Mode: 0x%02x", mode_change->CurrentMode = mode);

  AddEventToQueue(event);

  return true;
}

bool BluetoothEmu::SendEventLinkKeyNotification(const u8 num_to_send)
{
  u8 payload_length = sizeof(hci_return_link_keys_ep) + sizeof(hci_link_key_rep_cp) * num_to_send;
  SQueuedEvent event(2 + payload_length, 0);
  SHCIEventLinkKeyNotification* event_link_key = (SHCIEventLinkKeyNotification*)event.buffer;

  DEBUG_LOG(IOS_WIIMOTE, "Event: SendEventLinkKeyNotification");

  // event header
  event_link_key->EventType = HCI_EVENT_RETURN_LINK_KEYS;
  event_link_key->PayloadLength = payload_length;
  // this is really hci_return_link_keys_ep.num_keys
  event_link_key->numKeys = num_to_send;

  // copy infos - this only works correctly if we're meant to start at first device and read all
  // keys
  for (int i = 0; i < num_to_send; i++)
  {
    hci_link_key_rep_cp* link_key_info =
        (hci_link_key_rep_cp*)((u8*)&event_link_key->bdaddr + sizeof(hci_link_key_rep_cp) * i);
    link_key_info->bdaddr = m_wiimotes[i].GetBD();
    memcpy(link_key_info->key, m_wiimotes[i].GetLinkKey(), HCI_KEY_SIZE);

    DEBUG_LOG(IOS_WIIMOTE, "  bd: %02x:%02x:%02x:%02x:%02x:%02x", link_key_info->bdaddr[0],
              link_key_info->bdaddr[1], link_key_info->bdaddr[2], link_key_info->bdaddr[3],
              link_key_info->bdaddr[4], link_key_info->bdaddr[5]);
  }

  AddEventToQueue(event);

  return true;
};

bool BluetoothEmu::SendEventRequestLinkKey(const bdaddr_t& bd)
{
  SQueuedEvent event(sizeof(SHCIEventRequestLinkKey), 0);

  SHCIEventRequestLinkKey* event_request_link_key = (SHCIEventRequestLinkKey*)event.buffer;

  event_request_link_key->EventType = HCI_EVENT_LINK_KEY_REQ;
  event_request_link_key->PayloadLength = sizeof(SHCIEventRequestLinkKey) - 2;
  event_request_link_key->bdaddr = bd;

  DEBUG_LOG(IOS_WIIMOTE, "Event: SendEventRequestLinkKey");
  DEBUG_LOG(IOS_WIIMOTE, "  bd: %02x:%02x:%02x:%02x:%02x:%02x", event_request_link_key->bdaddr[0],
            event_request_link_key->bdaddr[1], event_request_link_key->bdaddr[2],
            event_request_link_key->bdaddr[3], event_request_link_key->bdaddr[4],
            event_request_link_key->bdaddr[5]);

  AddEventToQueue(event);

  return true;
};

bool BluetoothEmu::SendEventReadClockOffsetComplete(u16 connection_handle)
{
  WiimoteDevice* wiimote = AccessWiimote(connection_handle);
  if (wiimote == nullptr)
    return false;

  SQueuedEvent event(sizeof(SHCIEventReadClockOffsetComplete), connection_handle);

  SHCIEventReadClockOffsetComplete* read_clock_offset_complete =
      (SHCIEventReadClockOffsetComplete*)event.buffer;
  read_clock_offset_complete->EventType = HCI_EVENT_READ_CLOCK_OFFSET_COMPL;
  read_clock_offset_complete->PayloadLength = sizeof(SHCIEventReadClockOffsetComplete) - 2;
  read_clock_offset_complete->EventStatus = 0x00;
  read_clock_offset_complete->ConnectionHandle = connection_handle;
  read_clock_offset_complete->ClockOffset = 0x3818;

  DEBUG_LOG(IOS_WIIMOTE, "Event: SendEventReadClockOffsetComplete");
  DEBUG_LOG(IOS_WIIMOTE, "  Connection_Handle: 0x%04x",
            read_clock_offset_complete->ConnectionHandle);
  DEBUG_LOG(IOS_WIIMOTE, "  ClockOffset: 0x%04x", read_clock_offset_complete->ClockOffset);

  AddEventToQueue(event);

  return true;
}

bool BluetoothEmu::SendEventConPacketTypeChange(u16 connection_handle, u16 packet_type)
{
  WiimoteDevice* wiimote = AccessWiimote(connection_handle);
  if (wiimote == nullptr)
    return false;

  SQueuedEvent event(sizeof(SHCIEventConPacketTypeChange), connection_handle);

  SHCIEventConPacketTypeChange* change_con_packet_type =
      (SHCIEventConPacketTypeChange*)event.buffer;
  change_con_packet_type->EventType = HCI_EVENT_CON_PKT_TYPE_CHANGED;
  change_con_packet_type->PayloadLength = sizeof(SHCIEventConPacketTypeChange) - 2;
  change_con_packet_type->EventStatus = 0x00;
  change_con_packet_type->ConnectionHandle = connection_handle;
  change_con_packet_type->PacketType = packet_type;

  DEBUG_LOG(IOS_WIIMOTE, "Event: SendEventConPacketTypeChange");
  DEBUG_LOG(IOS_WIIMOTE, "  Connection_Handle: 0x%04x", change_con_packet_type->ConnectionHandle);
  DEBUG_LOG(IOS_WIIMOTE, "  PacketType: 0x%04x", change_con_packet_type->PacketType);

  AddEventToQueue(event);

  return true;
}

// Command dispatcher
// This is called from the USB::IOCTLV_USBV0_CTRLMSG Ioctlv
void BluetoothEmu::ExecuteHCICommandMessage(const USB::V0CtrlMessage& ctrl_message)
{
  const u8* input = Memory::GetPointer(ctrl_message.data_address + 3);

  SCommandMessage msg;
  std::memcpy(&msg, Memory::GetPointer(ctrl_message.data_address), sizeof(msg));

  const u16 ocf = HCI_OCF(msg.Opcode);
  const u16 ogf = HCI_OGF(msg.Opcode);

  DEBUG_LOG(IOS_WIIMOTE, "**************************************************");
  DEBUG_LOG(IOS_WIIMOTE, "Execute HCI Command: 0x%04x (ocf: 0x%02x, ogf: 0x%02x)", msg.Opcode, ocf,
            ogf);

  switch (msg.Opcode)
  {
  //
  // --- read commands ---
  //
  case HCI_CMD_RESET:
    CommandReset(input);
    break;

  case HCI_CMD_READ_BUFFER_SIZE:
    CommandReadBufferSize(input);
    break;

  case HCI_CMD_READ_LOCAL_VER:
    CommandReadLocalVer(input);
    break;

  case HCI_CMD_READ_BDADDR:
    CommandReadBDAdrr(input);
    break;

  case HCI_CMD_READ_LOCAL_FEATURES:
    CommandReadLocalFeatures(input);
    break;

  case HCI_CMD_READ_STORED_LINK_KEY:
    CommandReadStoredLinkKey(input);
    break;

  case HCI_CMD_WRITE_UNIT_CLASS:
    CommandWriteUnitClass(input);
    break;

  case HCI_CMD_WRITE_LOCAL_NAME:
    CommandWriteLocalName(input);
    break;

  case HCI_CMD_WRITE_PIN_TYPE:
    CommandWritePinType(input);
    break;

  case HCI_CMD_HOST_BUFFER_SIZE:
    CommandHostBufferSize(input);
    break;

  case HCI_CMD_WRITE_PAGE_TIMEOUT:
    CommandWritePageTimeOut(input);
    break;

  case HCI_CMD_WRITE_SCAN_ENABLE:
    CommandWriteScanEnable(input);
    break;

  case HCI_CMD_WRITE_INQUIRY_MODE:
    CommandWriteInquiryMode(input);
    break;

  case HCI_CMD_WRITE_PAGE_SCAN_TYPE:
    CommandWritePageScanType(input);
    break;

  case HCI_CMD_SET_EVENT_FILTER:
    CommandSetEventFilter(input);
    break;

  case HCI_CMD_INQUIRY:
    CommandInquiry(input);
    break;

  case HCI_CMD_WRITE_INQUIRY_SCAN_TYPE:
    CommandWriteInquiryScanType(input);
    break;

  // vendor specific...
  case 0xFC4C:
    CommandVendorSpecific_FC4C(input, ctrl_message.length - 3);
    break;

  case 0xFC4F:
    CommandVendorSpecific_FC4F(input, ctrl_message.length - 3);
    break;

  case HCI_CMD_INQUIRY_CANCEL:
    CommandInquiryCancel(input);
    break;

  case HCI_CMD_REMOTE_NAME_REQ:
    CommandRemoteNameReq(input);
    break;

  case HCI_CMD_CREATE_CON:
    CommandCreateCon(input);
    break;

  case HCI_CMD_ACCEPT_CON:
    CommandAcceptCon(input);
    break;

  case HCI_CMD_CHANGE_CON_PACKET_TYPE:
    CommandChangeConPacketType(input);
    break;

  case HCI_CMD_READ_CLOCK_OFFSET:
    CommandReadClockOffset(input);
    break;

  case HCI_CMD_READ_REMOTE_VER_INFO:
    CommandReadRemoteVerInfo(input);
    break;

  case HCI_CMD_READ_REMOTE_FEATURES:
    CommandReadRemoteFeatures(input);
    break;

  case HCI_CMD_WRITE_LINK_POLICY_SETTINGS:
    CommandWriteLinkPolicy(input);
    break;

  case HCI_CMD_AUTH_REQ:
    CommandAuthenticationRequested(input);
    break;

  case HCI_CMD_SNIFF_MODE:
    CommandSniffMode(input);
    break;

  case HCI_CMD_DISCONNECT:
    CommandDisconnect(input);
    break;

  case HCI_CMD_WRITE_LINK_SUPERVISION_TIMEOUT:
    CommandWriteLinkSupervisionTimeout(input);
    break;

  case HCI_CMD_LINK_KEY_NEG_REP:
    CommandLinkKeyNegRep(input);
    break;

  case HCI_CMD_LINK_KEY_REP:
    CommandLinkKeyRep(input);
    break;

  case HCI_CMD_DELETE_STORED_LINK_KEY:
    CommandDeleteStoredLinkKey(input);
    break;

  default:
    // send fake okay msg...
    SendEventCommandComplete(msg.Opcode, nullptr, 0);

    if (ogf == HCI_OGF_VENDOR)
    {
      ERROR_LOG(IOS_WIIMOTE, "Command: vendor specific: 0x%04X (ocf: 0x%x)", msg.Opcode, ocf);
      for (int i = 0; i < msg.len; i++)
      {
        ERROR_LOG(IOS_WIIMOTE, "  0x02%x", input[i]);
      }
    }
    else
    {
      DEBUG_ASSERT_MSG(IOS_WIIMOTE, 0, "Unknown USB_IOCTL_CTRLMSG: 0x%04X (ocf: 0x%x  ogf 0x%x)",
                       msg.Opcode, ocf, ogf);
    }
    break;
  }

  // HCI command is finished, send a reply to command
  m_ios.EnqueueIPCReply(ctrl_message.ios_request, ctrl_message.length);
}

//
//
// --- command helper
//
//
void BluetoothEmu::CommandInquiry(const u8* input)
{
  // Inquiry should not be called normally
  hci_inquiry_cp inquiry;
  std::memcpy(&inquiry, input, sizeof(inquiry));

  INFO_LOG(IOS_WIIMOTE, "Command: HCI_CMD_INQUIRY:");
  DEBUG_LOG(IOS_WIIMOTE, "write:");
  DEBUG_LOG(IOS_WIIMOTE, "  LAP[0]: 0x%02x", inquiry.lap[0]);
  DEBUG_LOG(IOS_WIIMOTE, "  LAP[1]: 0x%02x", inquiry.lap[1]);
  DEBUG_LOG(IOS_WIIMOTE, "  LAP[2]: 0x%02x", inquiry.lap[2]);
  DEBUG_LOG(IOS_WIIMOTE, "  inquiry_length: %i (N x 1.28) sec", inquiry.inquiry_length);
  DEBUG_LOG(IOS_WIIMOTE, "  num_responses: %i (N x 1.28) sec", inquiry.num_responses);

  SendEventCommandStatus(HCI_CMD_INQUIRY);
  SendEventInquiryResponse();
  SendEventInquiryComplete();
}

void BluetoothEmu::CommandInquiryCancel(const u8* input)
{
  hci_inquiry_cancel_rp reply;
  reply.status = 0x00;

  INFO_LOG(IOS_WIIMOTE, "Command: HCI_CMD_INQUIRY_CANCEL");

  SendEventCommandComplete(HCI_CMD_INQUIRY_CANCEL, &reply, sizeof(hci_inquiry_cancel_rp));
}

void BluetoothEmu::CommandCreateCon(const u8* input)
{
  hci_create_con_cp create_connection;
  std::memcpy(&create_connection, input, sizeof(create_connection));

  INFO_LOG(IOS_WIIMOTE, "Command: HCI_CMD_CREATE_CON");
  DEBUG_LOG(IOS_WIIMOTE, "Input:");
  DEBUG_LOG(IOS_WIIMOTE, "  bd: %02x:%02x:%02x:%02x:%02x:%02x", create_connection.bdaddr[0],
            create_connection.bdaddr[1], create_connection.bdaddr[2], create_connection.bdaddr[3],
            create_connection.bdaddr[4], create_connection.bdaddr[5]);
  INFO_LOG(IOS_WIIMOTE, "Command: HCI_CMD_ACCEPT_CON");

  DEBUG_LOG(IOS_WIIMOTE, "  pkt_type: %i", create_connection.pkt_type);
  DEBUG_LOG(IOS_WIIMOTE, "  page_scan_rep_mode: %i", create_connection.page_scan_rep_mode);
  DEBUG_LOG(IOS_WIIMOTE, "  page_scan_mode: %i", create_connection.page_scan_mode);
  DEBUG_LOG(IOS_WIIMOTE, "  clock_offset: %i", create_connection.clock_offset);
  DEBUG_LOG(IOS_WIIMOTE, "  accept_role_switch: %i", create_connection.accept_role_switch);

  SendEventCommandStatus(HCI_CMD_CREATE_CON);
  SendEventConnectionComplete(create_connection.bdaddr);
}

void BluetoothEmu::CommandDisconnect(const u8* input)
{
  hci_discon_cp disconnect;
  std::memcpy(&disconnect, input, sizeof(disconnect));

  INFO_LOG(IOS_WIIMOTE, "Command: HCI_CMD_DISCONNECT");
  DEBUG_LOG(IOS_WIIMOTE, "  ConnectionHandle: 0x%04x", disconnect.con_handle);
  DEBUG_LOG(IOS_WIIMOTE, "  Reason: 0x%02x", disconnect.reason);

  DisplayDisconnectMessage((disconnect.con_handle & 0xFF) + 1, disconnect.reason);

  SendEventCommandStatus(HCI_CMD_DISCONNECT);
  SendEventDisconnect(disconnect.con_handle, disconnect.reason);

  WiimoteDevice* wiimote = AccessWiimote(disconnect.con_handle);
  if (wiimote)
    wiimote->EventDisconnect();
}

void BluetoothEmu::CommandAcceptCon(const u8* input)
{
  hci_accept_con_cp accept_connection;
  std::memcpy(&accept_connection, input, sizeof(accept_connection));

  static constexpr const char* roles[] = {
      "Master (0x00)",
      "Slave (0x01)",
  };

  INFO_LOG(IOS_WIIMOTE, "Command: HCI_CMD_ACCEPT_CON");
  DEBUG_LOG(IOS_WIIMOTE, "  bd: %02x:%02x:%02x:%02x:%02x:%02x", accept_connection.bdaddr[0],
            accept_connection.bdaddr[1], accept_connection.bdaddr[2], accept_connection.bdaddr[3],
            accept_connection.bdaddr[4], accept_connection.bdaddr[5]);
  DEBUG_LOG(IOS_WIIMOTE, "  role: %s", roles[accept_connection.role]);

  SendEventCommandStatus(HCI_CMD_ACCEPT_CON);

  // this connection wants to be the master
  if (accept_connection.role == 0)
  {
    SendEventRoleChange(accept_connection.bdaddr, true);
  }

  SendEventConnectionComplete(accept_connection.bdaddr);
}

void BluetoothEmu::CommandLinkKeyRep(const u8* input)
{
  hci_link_key_rep_cp key_rep;
  std::memcpy(&key_rep, input, sizeof(key_rep));

  INFO_LOG(IOS_WIIMOTE, "Command: HCI_CMD_LINK_KEY_REP");
  DEBUG_LOG(IOS_WIIMOTE, "  bd: %02x:%02x:%02x:%02x:%02x:%02x", key_rep.bdaddr[0],
            key_rep.bdaddr[1], key_rep.bdaddr[2], key_rep.bdaddr[3], key_rep.bdaddr[4],
            key_rep.bdaddr[5]);

  hci_link_key_rep_rp reply;
  reply.status = 0x00;
  reply.bdaddr = key_rep.bdaddr;

  SendEventCommandComplete(HCI_CMD_LINK_KEY_REP, &reply, sizeof(hci_link_key_rep_rp));
}

void BluetoothEmu::CommandLinkKeyNegRep(const u8* input)
{
  hci_link_key_neg_rep_cp key_neg;
  std::memcpy(&key_neg, input, sizeof(key_neg));

  INFO_LOG(IOS_WIIMOTE, "Command: HCI_CMD_LINK_KEY_NEG_REP");
  DEBUG_LOG(IOS_WIIMOTE, "  bd: %02x:%02x:%02x:%02x:%02x:%02x", key_neg.bdaddr[0],
            key_neg.bdaddr[1], key_neg.bdaddr[2], key_neg.bdaddr[3], key_neg.bdaddr[4],
            key_neg.bdaddr[5]);

  hci_link_key_neg_rep_rp reply;
  reply.status = 0x00;
  reply.bdaddr = key_neg.bdaddr;

  SendEventCommandComplete(HCI_CMD_LINK_KEY_NEG_REP, &reply, sizeof(hci_link_key_neg_rep_rp));
}

void BluetoothEmu::CommandChangeConPacketType(const u8* input)
{
  hci_change_con_pkt_type_cp change_packet_type;
  std::memcpy(&change_packet_type, input, sizeof(change_packet_type));

  // ntd stack sets packet type 0xcc18, which is HCI_PKT_DH5 | HCI_PKT_DM5 | HCI_PKT_DH1 |
  // HCI_PKT_DM1
  // dunno what to do...run awayyyyyy!
  INFO_LOG(IOS_WIIMOTE, "Command: HCI_CMD_CHANGE_CON_PACKET_TYPE");
  DEBUG_LOG(IOS_WIIMOTE, "  ConnectionHandle: 0x%04x", change_packet_type.con_handle);
  DEBUG_LOG(IOS_WIIMOTE, "  PacketType: 0x%04x", change_packet_type.pkt_type);

  SendEventCommandStatus(HCI_CMD_CHANGE_CON_PACKET_TYPE);
  SendEventConPacketTypeChange(change_packet_type.con_handle, change_packet_type.pkt_type);
}

void BluetoothEmu::CommandAuthenticationRequested(const u8* input)
{
  hci_auth_req_cp auth_req;
  std::memcpy(&auth_req, input, sizeof(auth_req));

  INFO_LOG(IOS_WIIMOTE, "Command: HCI_CMD_AUTH_REQ");
  DEBUG_LOG(IOS_WIIMOTE, "  ConnectionHandle: 0x%04x", auth_req.con_handle);

  SendEventCommandStatus(HCI_CMD_AUTH_REQ);
  SendEventAuthenticationCompleted(auth_req.con_handle);
}

void BluetoothEmu::CommandRemoteNameReq(const u8* input)
{
  hci_remote_name_req_cp remote_name_req;
  std::memcpy(&remote_name_req, input, sizeof(remote_name_req));

  INFO_LOG(IOS_WIIMOTE, "Command: HCI_CMD_REMOTE_NAME_REQ");
  DEBUG_LOG(IOS_WIIMOTE, "  bd: %02x:%02x:%02x:%02x:%02x:%02x", remote_name_req.bdaddr[0],
            remote_name_req.bdaddr[1], remote_name_req.bdaddr[2], remote_name_req.bdaddr[3],
            remote_name_req.bdaddr[4], remote_name_req.bdaddr[5]);
  DEBUG_LOG(IOS_WIIMOTE, "  page_scan_rep_mode: %i", remote_name_req.page_scan_rep_mode);
  DEBUG_LOG(IOS_WIIMOTE, "  page_scan_mode: %i", remote_name_req.page_scan_mode);
  DEBUG_LOG(IOS_WIIMOTE, "  clock_offset: %i", remote_name_req.clock_offset);

  SendEventCommandStatus(HCI_CMD_REMOTE_NAME_REQ);
  SendEventRemoteNameReq(remote_name_req.bdaddr);
}

void BluetoothEmu::CommandReadRemoteFeatures(const u8* input)
{
  hci_read_remote_features_cp read_remote_features;
  std::memcpy(&read_remote_features, input, sizeof(read_remote_features));

  INFO_LOG(IOS_WIIMOTE, "Command: HCI_CMD_READ_REMOTE_FEATURES");
  DEBUG_LOG(IOS_WIIMOTE, "  ConnectionHandle: 0x%04x", read_remote_features.con_handle);

  SendEventCommandStatus(HCI_CMD_READ_REMOTE_FEATURES);
  SendEventReadRemoteFeatures(read_remote_features.con_handle);
}

void BluetoothEmu::CommandReadRemoteVerInfo(const u8* input)
{
  hci_read_remote_ver_info_cp read_remote_ver_info;
  std::memcpy(&read_remote_ver_info, input, sizeof(read_remote_ver_info));

  INFO_LOG(IOS_WIIMOTE, "Command: HCI_CMD_READ_REMOTE_VER_INFO");
  DEBUG_LOG(IOS_WIIMOTE, "  ConnectionHandle: 0x%02x", read_remote_ver_info.con_handle);

  SendEventCommandStatus(HCI_CMD_READ_REMOTE_VER_INFO);
  SendEventReadRemoteVerInfo(read_remote_ver_info.con_handle);
}

void BluetoothEmu::CommandReadClockOffset(const u8* input)
{
  hci_read_clock_offset_cp read_clock_offset;
  std::memcpy(&read_clock_offset, input, sizeof(read_clock_offset));

  INFO_LOG(IOS_WIIMOTE, "Command: HCI_CMD_READ_CLOCK_OFFSET");
  DEBUG_LOG(IOS_WIIMOTE, "  ConnectionHandle: 0x%02x", read_clock_offset.con_handle);

  SendEventCommandStatus(HCI_CMD_READ_CLOCK_OFFSET);
  SendEventReadClockOffsetComplete(read_clock_offset.con_handle);
}

void BluetoothEmu::CommandSniffMode(const u8* input)
{
  hci_sniff_mode_cp sniff_mode;
  std::memcpy(&sniff_mode, input, sizeof(sniff_mode));

  INFO_LOG(IOS_WIIMOTE, "Command: HCI_CMD_SNIFF_MODE");
  DEBUG_LOG(IOS_WIIMOTE, "  ConnectionHandle: 0x%04x", sniff_mode.con_handle);
  DEBUG_LOG(IOS_WIIMOTE, "  max_interval: %f msec", sniff_mode.max_interval * .625);
  DEBUG_LOG(IOS_WIIMOTE, "  min_interval: %f msec", sniff_mode.min_interval * .625);
  DEBUG_LOG(IOS_WIIMOTE, "  attempt: %f msec", sniff_mode.attempt * 1.25);
  DEBUG_LOG(IOS_WIIMOTE, "  timeout: %f msec", sniff_mode.timeout * 1.25);

  SendEventCommandStatus(HCI_CMD_SNIFF_MODE);
  SendEventModeChange(sniff_mode.con_handle, 0x02, sniff_mode.max_interval);  // 0x02 - sniff mode
}

void BluetoothEmu::CommandWriteLinkPolicy(const u8* input)
{
  hci_write_link_policy_settings_cp link_policy;
  std::memcpy(&link_policy, input, sizeof(link_policy));

  INFO_LOG(IOS_WIIMOTE, "Command: HCI_CMD_WRITE_LINK_POLICY_SETTINGS");
  DEBUG_LOG(IOS_WIIMOTE, "  ConnectionHandle: 0x%04x", link_policy.con_handle);
  DEBUG_LOG(IOS_WIIMOTE, "  Policy: 0x%04x", link_policy.settings);

  SendEventCommandStatus(HCI_CMD_WRITE_LINK_POLICY_SETTINGS);
}

void BluetoothEmu::CommandReset(const u8* input)
{
  hci_status_rp reply;
  reply.status = 0x00;

  INFO_LOG(IOS_WIIMOTE, "Command: HCI_CMD_RESET");

  SendEventCommandComplete(HCI_CMD_RESET, &reply, sizeof(hci_status_rp));
}

void BluetoothEmu::CommandSetEventFilter(const u8* input)
{
  hci_set_event_filter_cp set_event_filter;
  std::memcpy(&set_event_filter, input, sizeof(set_event_filter));

  hci_set_event_filter_rp reply;
  reply.status = 0x00;

  INFO_LOG(IOS_WIIMOTE, "Command: HCI_CMD_SET_EVENT_FILTER:");
  DEBUG_LOG(IOS_WIIMOTE, "  filter_type: %i", set_event_filter.filter_type);
  DEBUG_LOG(IOS_WIIMOTE, "  filter_condition_type: %i", set_event_filter.filter_condition_type);

  SendEventCommandComplete(HCI_CMD_SET_EVENT_FILTER, &reply, sizeof(hci_set_event_filter_rp));
}

void BluetoothEmu::CommandWritePinType(const u8* input)
{
  hci_write_pin_type_cp write_pin_type;
  std::memcpy(&write_pin_type, input, sizeof(write_pin_type));

  hci_write_pin_type_rp reply;
  reply.status = 0x00;

  INFO_LOG(IOS_WIIMOTE, "Command: HCI_CMD_WRITE_PIN_TYPE:");
  DEBUG_LOG(IOS_WIIMOTE, "  pin_type: %x", write_pin_type.pin_type);

  SendEventCommandComplete(HCI_CMD_WRITE_PIN_TYPE, &reply, sizeof(hci_write_pin_type_rp));
}

void BluetoothEmu::CommandReadStoredLinkKey(const u8* input)
{
  hci_read_stored_link_key_cp read_stored_link_key;
  std::memcpy(&read_stored_link_key, input, sizeof(read_stored_link_key));

  hci_read_stored_link_key_rp reply;
  reply.status = 0x00;
  reply.num_keys_read = 0;
  reply.max_num_keys = 255;

  if (read_stored_link_key.read_all == 1)
  {
    reply.num_keys_read = static_cast<u16>(m_wiimotes.size());
  }
  else
  {
    ERROR_LOG(IOS_WIIMOTE, "CommandReadStoredLinkKey isn't looking for all devices");
  }

  INFO_LOG(IOS_WIIMOTE, "Command: HCI_CMD_READ_STORED_LINK_KEY:");
  DEBUG_LOG(IOS_WIIMOTE, "input:");
  DEBUG_LOG(IOS_WIIMOTE, "  bd: %02x:%02x:%02x:%02x:%02x:%02x", read_stored_link_key.bdaddr[0],
            read_stored_link_key.bdaddr[1], read_stored_link_key.bdaddr[2],
            read_stored_link_key.bdaddr[3], read_stored_link_key.bdaddr[4],
            read_stored_link_key.bdaddr[5]);
  DEBUG_LOG(IOS_WIIMOTE, "  read_all: %i", read_stored_link_key.read_all);
  DEBUG_LOG(IOS_WIIMOTE, "return:");
  DEBUG_LOG(IOS_WIIMOTE, "  max_num_keys: %i", reply.max_num_keys);
  DEBUG_LOG(IOS_WIIMOTE, "  num_keys_read: %i", reply.num_keys_read);

  SendEventLinkKeyNotification(static_cast<u8>(reply.num_keys_read));
  SendEventCommandComplete(HCI_CMD_READ_STORED_LINK_KEY, &reply,
                           sizeof(hci_read_stored_link_key_rp));
}

void BluetoothEmu::CommandDeleteStoredLinkKey(const u8* input)
{
  hci_delete_stored_link_key_cp delete_stored_link_key;
  std::memcpy(&delete_stored_link_key, input, sizeof(delete_stored_link_key));

  INFO_LOG(IOS_WIIMOTE, "Command: HCI_OCF_DELETE_STORED_LINK_KEY");
  DEBUG_LOG(IOS_WIIMOTE, "  bd: %02x:%02x:%02x:%02x:%02x:%02x", delete_stored_link_key.bdaddr[0],
            delete_stored_link_key.bdaddr[1], delete_stored_link_key.bdaddr[2],
            delete_stored_link_key.bdaddr[3], delete_stored_link_key.bdaddr[4],
            delete_stored_link_key.bdaddr[5]);
  DEBUG_LOG(IOS_WIIMOTE, "  delete_all: 0x%01x", delete_stored_link_key.delete_all);

  const WiimoteDevice* wiimote = AccessWiimote(delete_stored_link_key.bdaddr);
  if (wiimote == nullptr)
    return;

  hci_delete_stored_link_key_rp reply;
  reply.status = 0x00;
  reply.num_keys_deleted = 0;

  SendEventCommandComplete(HCI_CMD_DELETE_STORED_LINK_KEY, &reply,
                           sizeof(hci_delete_stored_link_key_rp));

  ERROR_LOG(IOS_WIIMOTE, "HCI: CommandDeleteStoredLinkKey... Probably the security for linking "
                         "has failed. Could be a problem with loading the SCONF");
}

void BluetoothEmu::CommandWriteLocalName(const u8* input)
{
  hci_write_local_name_cp write_local_name;
  std::memcpy(&write_local_name, input, sizeof(write_local_name));

  hci_write_local_name_rp reply;
  reply.status = 0x00;

  INFO_LOG(IOS_WIIMOTE, "Command: HCI_CMD_WRITE_LOCAL_NAME:");
  DEBUG_LOG(IOS_WIIMOTE, "  local_name: %s", write_local_name.name);

  SendEventCommandComplete(HCI_CMD_WRITE_LOCAL_NAME, &reply, sizeof(hci_write_local_name_rp));
}

// Here we normally receive the timeout interval.
// But not from homebrew games that use lwbt. Why not?
void BluetoothEmu::CommandWritePageTimeOut(const u8* input)
{
  hci_write_page_timeout_cp write_page_timeout;
  std::memcpy(&write_page_timeout, input, sizeof(write_page_timeout));

  hci_host_buffer_size_rp reply;
  reply.status = 0x00;

  INFO_LOG(IOS_WIIMOTE, "Command: HCI_CMD_WRITE_PAGE_TIMEOUT:");
  DEBUG_LOG(IOS_WIIMOTE, "  timeout: %i", write_page_timeout.timeout);

  SendEventCommandComplete(HCI_CMD_WRITE_PAGE_TIMEOUT, &reply, sizeof(hci_host_buffer_size_rp));
}

// This will enable ScanEnable so that Update() can start the Wii Remote.
void BluetoothEmu::CommandWriteScanEnable(const u8* input)
{
  hci_write_scan_enable_cp write_scan_enable;
  std::memcpy(&write_scan_enable, input, sizeof(write_scan_enable));

  m_scan_enable = write_scan_enable.scan_enable;

  hci_write_scan_enable_rp reply;
  reply.status = 0x00;

  static constexpr const char* scanning[] = {
      "HCI_NO_SCAN_ENABLE",
      "HCI_INQUIRY_SCAN_ENABLE",
      "HCI_PAGE_SCAN_ENABLE",
      "HCI_INQUIRY_AND_PAGE_SCAN_ENABLE",
  };

  DEBUG_LOG(IOS_WIIMOTE, "Command: HCI_CMD_WRITE_SCAN_ENABLE: (0x%02x)",
            write_scan_enable.scan_enable);
  DEBUG_LOG(IOS_WIIMOTE, "  scan_enable: %s", scanning[write_scan_enable.scan_enable]);

  SendEventCommandComplete(HCI_CMD_WRITE_SCAN_ENABLE, &reply, sizeof(hci_write_scan_enable_rp));
}

void BluetoothEmu::CommandWriteUnitClass(const u8* input)
{
  hci_write_unit_class_cp write_unit_class;
  std::memcpy(&write_unit_class, input, sizeof(write_unit_class));

  hci_write_unit_class_rp reply;
  reply.status = 0x00;

  INFO_LOG(IOS_WIIMOTE, "Command: HCI_CMD_WRITE_UNIT_CLASS:");
  DEBUG_LOG(IOS_WIIMOTE, "  COD[0]: 0x%02x", write_unit_class.uclass[0]);
  DEBUG_LOG(IOS_WIIMOTE, "  COD[1]: 0x%02x", write_unit_class.uclass[1]);
  DEBUG_LOG(IOS_WIIMOTE, "  COD[2]: 0x%02x", write_unit_class.uclass[2]);

  SendEventCommandComplete(HCI_CMD_WRITE_UNIT_CLASS, &reply, sizeof(hci_write_unit_class_rp));
}

void BluetoothEmu::CommandHostBufferSize(const u8* input)
{
  hci_host_buffer_size_cp host_buffer_size;
  std::memcpy(&host_buffer_size, input, sizeof(host_buffer_size));

  hci_host_buffer_size_rp reply;
  reply.status = 0x00;

  INFO_LOG(IOS_WIIMOTE, "Command: HCI_CMD_HOST_BUFFER_SIZE:");
  DEBUG_LOG(IOS_WIIMOTE, "  max_acl_size: %i", host_buffer_size.max_acl_size);
  DEBUG_LOG(IOS_WIIMOTE, "  max_sco_size: %i", host_buffer_size.max_sco_size);
  DEBUG_LOG(IOS_WIIMOTE, "  num_acl_pkts: %i", host_buffer_size.num_acl_pkts);
  DEBUG_LOG(IOS_WIIMOTE, "  num_sco_pkts: %i", host_buffer_size.num_sco_pkts);

  SendEventCommandComplete(HCI_CMD_HOST_BUFFER_SIZE, &reply, sizeof(hci_host_buffer_size_rp));
}

void BluetoothEmu::CommandWriteLinkSupervisionTimeout(const u8* input)
{
  hci_write_link_supervision_timeout_cp supervision;
  std::memcpy(&supervision, input, sizeof(supervision));

  // timeout of 0 means timing out is disabled
  INFO_LOG(IOS_WIIMOTE, "Command: HCI_CMD_WRITE_LINK_SUPERVISION_TIMEOUT");
  DEBUG_LOG(IOS_WIIMOTE, "  con_handle: 0x%04x", supervision.con_handle);
  DEBUG_LOG(IOS_WIIMOTE, "  timeout: 0x%02x", supervision.timeout);

  hci_write_link_supervision_timeout_rp reply;
  reply.status = 0x00;
  reply.con_handle = supervision.con_handle;

  SendEventCommandComplete(HCI_CMD_WRITE_LINK_SUPERVISION_TIMEOUT, &reply,
                           sizeof(hci_write_link_supervision_timeout_rp));
}

void BluetoothEmu::CommandWriteInquiryScanType(const u8* input)
{
  hci_write_inquiry_scan_type_cp set_event_filter;
  std::memcpy(&set_event_filter, input, sizeof(set_event_filter));

  hci_write_inquiry_scan_type_rp reply;
  reply.status = 0x00;

  INFO_LOG(IOS_WIIMOTE, "Command: HCI_CMD_WRITE_INQUIRY_SCAN_TYPE:");
  DEBUG_LOG(IOS_WIIMOTE, "  type: %i", set_event_filter.type);

  SendEventCommandComplete(HCI_CMD_WRITE_INQUIRY_SCAN_TYPE, &reply,
                           sizeof(hci_write_inquiry_scan_type_rp));
}

void BluetoothEmu::CommandWriteInquiryMode(const u8* input)
{
  hci_write_inquiry_mode_cp inquiry_mode;
  std::memcpy(&inquiry_mode, input, sizeof(inquiry_mode));

  hci_write_inquiry_mode_rp reply;
  reply.status = 0x00;

  static constexpr const char* inquiry_mode_tag[] = {
      "Standard Inquiry Result event format (default)",
      "Inquiry Result format with RSSI",
      "Inquiry Result with RSSI format or Extended Inquiry Result format",
  };
  INFO_LOG(IOS_WIIMOTE, "Command: HCI_CMD_WRITE_INQUIRY_MODE:");
  DEBUG_LOG(IOS_WIIMOTE, "  mode: %s", inquiry_mode_tag[inquiry_mode.mode]);

  SendEventCommandComplete(HCI_CMD_WRITE_INQUIRY_MODE, &reply, sizeof(hci_write_inquiry_mode_rp));
}

void BluetoothEmu::CommandWritePageScanType(const u8* input)
{
  hci_write_page_scan_type_cp write_page_scan_type;
  std::memcpy(&write_page_scan_type, input, sizeof(write_page_scan_type));

  hci_write_page_scan_type_rp reply;
  reply.status = 0x00;

  static constexpr const char* page_scan_type[] = {
      "Mandatory: Standard Scan (default)",
      "Optional: Interlaced Scan",
  };

  INFO_LOG(IOS_WIIMOTE, "Command: HCI_CMD_WRITE_PAGE_SCAN_TYPE:");
  DEBUG_LOG(IOS_WIIMOTE, "  type: %s", page_scan_type[write_page_scan_type.type]);

  SendEventCommandComplete(HCI_CMD_WRITE_PAGE_SCAN_TYPE, &reply,
                           sizeof(hci_write_page_scan_type_rp));
}

void BluetoothEmu::CommandReadLocalVer(const u8* input)
{
  hci_read_local_ver_rp reply;
  reply.status = 0x00;
  reply.hci_version = 0x03;       // HCI version: 1.1
  reply.hci_revision = 0x40a7;    // current revision (?)
  reply.lmp_version = 0x03;       // LMP version: 1.1
  reply.manufacturer = 0x000F;    // manufacturer: reserved for tests
  reply.lmp_subversion = 0x430e;  // LMP subversion

  INFO_LOG(IOS_WIIMOTE, "Command: HCI_CMD_READ_LOCAL_VER:");
  DEBUG_LOG(IOS_WIIMOTE, "return:");
  DEBUG_LOG(IOS_WIIMOTE, "  status:         %i", reply.status);
  DEBUG_LOG(IOS_WIIMOTE, "  hci_revision:   %i", reply.hci_revision);
  DEBUG_LOG(IOS_WIIMOTE, "  lmp_version:    %i", reply.lmp_version);
  DEBUG_LOG(IOS_WIIMOTE, "  manufacturer:   %i", reply.manufacturer);
  DEBUG_LOG(IOS_WIIMOTE, "  lmp_subversion: %i", reply.lmp_subversion);

  SendEventCommandComplete(HCI_CMD_READ_LOCAL_VER, &reply, sizeof(hci_read_local_ver_rp));
}

void BluetoothEmu::CommandReadLocalFeatures(const u8* input)
{
  hci_read_local_features_rp reply;
  reply.status = 0x00;
  reply.features[0] = 0xFF;
  reply.features[1] = 0xFF;
  reply.features[2] = 0x8D;
  reply.features[3] = 0xFE;
  reply.features[4] = 0x9B;
  reply.features[5] = 0xF9;
  reply.features[6] = 0x00;
  reply.features[7] = 0x80;

  INFO_LOG(IOS_WIIMOTE, "Command: HCI_CMD_READ_LOCAL_FEATURES:");
  DEBUG_LOG(IOS_WIIMOTE, "return:");
  DEBUG_LOG(IOS_WIIMOTE, "  features: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x", reply.features[0],
            reply.features[1], reply.features[2], reply.features[3], reply.features[4],
            reply.features[5], reply.features[6], reply.features[7]);

  SendEventCommandComplete(HCI_CMD_READ_LOCAL_FEATURES, &reply, sizeof(hci_read_local_features_rp));
}

void BluetoothEmu::CommandReadBufferSize(const u8* input)
{
  hci_read_buffer_size_rp reply;
  reply.status = 0x00;
  reply.max_acl_size = ACL_PKT_SIZE;
  // Due to how the widcomm stack which Nintendo uses is coded, we must never
  // let the stack think the controller is buffering more than 10 data packets
  // - it will cause a u8 underflow and royally screw things up.
  reply.num_acl_pkts = ACL_PKT_NUM;
  reply.max_sco_size = SCO_PKT_SIZE;
  reply.num_sco_pkts = SCO_PKT_NUM;

  INFO_LOG(IOS_WIIMOTE, "Command: HCI_CMD_READ_BUFFER_SIZE:");
  DEBUG_LOG(IOS_WIIMOTE, "return:");
  DEBUG_LOG(IOS_WIIMOTE, "  max_acl_size: %i", reply.max_acl_size);
  DEBUG_LOG(IOS_WIIMOTE, "  num_acl_pkts: %i", reply.num_acl_pkts);
  DEBUG_LOG(IOS_WIIMOTE, "  max_sco_size: %i", reply.max_sco_size);
  DEBUG_LOG(IOS_WIIMOTE, "  num_sco_pkts: %i", reply.num_sco_pkts);

  SendEventCommandComplete(HCI_CMD_READ_BUFFER_SIZE, &reply, sizeof(hci_read_buffer_size_rp));
}

void BluetoothEmu::CommandReadBDAdrr(const u8* input)
{
  hci_read_bdaddr_rp reply;
  reply.status = 0x00;
  reply.bdaddr = m_controller_bd;

  INFO_LOG(IOS_WIIMOTE, "Command: HCI_CMD_READ_BDADDR:");
  DEBUG_LOG(IOS_WIIMOTE, "return:");
  DEBUG_LOG(IOS_WIIMOTE, "  bd: %02x:%02x:%02x:%02x:%02x:%02x", reply.bdaddr[0], reply.bdaddr[1],
            reply.bdaddr[2], reply.bdaddr[3], reply.bdaddr[4], reply.bdaddr[5]);

  SendEventCommandComplete(HCI_CMD_READ_BDADDR, &reply, sizeof(hci_read_bdaddr_rp));
}

void BluetoothEmu::CommandVendorSpecific_FC4F(const u8* input, u32 size)
{
  // callstack...
  // BTM_VendorSpecificCommad()
  // WUDiRemovePatch()
  // WUDiAppendRuntimePatch()
  // WUDiGetFirmwareVersion()
  // WUDiStackSetupComplete()

  hci_status_rp reply;
  reply.status = 0x00;

  INFO_LOG(IOS_WIIMOTE, "Command: CommandVendorSpecific_FC4F: (callstack WUDiRemovePatch)");
  DEBUG_LOG(IOS_WIIMOTE, "Input (size 0x%x):", size);

  Dolphin_Debugger::PrintDataBuffer(LogTypes::IOS_WIIMOTE, input, size, "Data: ");

  SendEventCommandComplete(0xFC4F, &reply, sizeof(hci_status_rp));
}

void BluetoothEmu::CommandVendorSpecific_FC4C(const u8* input, u32 size)
{
  hci_status_rp reply;
  reply.status = 0x00;

  DEBUG_LOG(IOS_WIIMOTE, "Command: CommandVendorSpecific_FC4C:");
  DEBUG_LOG(IOS_WIIMOTE, "Input (size 0x%x):", size);
  Dolphin_Debugger::PrintDataBuffer(LogTypes::IOS_WIIMOTE, input, size, "Data: ");

  SendEventCommandComplete(0xFC4C, &reply, sizeof(hci_status_rp));
}

//
//
// --- helper
//
//
WiimoteDevice* BluetoothEmu::AccessWiimoteByIndex(std::size_t index)
{
  const u16 connection_handle = static_cast<u16>(0x100 + index);
  return AccessWiimote(connection_handle);
}

WiimoteDevice* BluetoothEmu::AccessWiimote(const bdaddr_t& address)
{
  const auto iterator =
      std::find_if(m_wiimotes.begin(), m_wiimotes.end(),
                   [&address](const WiimoteDevice& remote) { return remote.GetBD() == address; });
  return iterator != m_wiimotes.cend() ? &*iterator : nullptr;
}

WiimoteDevice* BluetoothEmu::AccessWiimote(u16 connection_handle)
{
  for (auto& wiimote : m_wiimotes)
  {
    if (wiimote.GetConnectionHandle() == connection_handle)
      return &wiimote;
  }

  ERROR_LOG(IOS_WIIMOTE, "Can't find Wiimote by connection handle %02x", connection_handle);
  PanicAlertT("Can't find Wii Remote by connection handle %02x", connection_handle);
  return nullptr;
}

void BluetoothEmu::DisplayDisconnectMessage(const int wiimote_number, const int reason)
{
  // TODO: If someone wants to be fancy we could also figure out what the values for pDiscon->reason
  // mean
  // and display things like "Wii Remote %i disconnected due to inactivity!" etc.
  Core::DisplayMessage(
      fmt::format("Wii Remote {} disconnected by emulated software", wiimote_number), 3000);
}
}  // namespace Device
}  // namespace IOS::HLE
