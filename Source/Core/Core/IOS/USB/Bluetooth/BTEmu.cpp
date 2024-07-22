// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/IOS/USB/Bluetooth/BTEmu.h"

#include <algorithm>
#include <cstring>
#include <memory>
#include <string>

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
#include "Core/HW/WiimoteEmu/DesiredWiimoteState.h"
#include "Core/IOS/Device.h"
#include "Core/IOS/IOS.h"
#include "Core/NetPlayClient.h"
#include "Core/NetPlayProto.h"
#include "Core/SysConf.h"
#include "Core/System.h"
#include "InputCommon/ControllerInterface/ControllerInterface.h"

namespace IOS::HLE
{
SQueuedEvent::SQueuedEvent(u32 size_, u16 handle) : size(size_), connection_handle(handle)
{
  if (size > 1024)
    PanicAlertFmt("SQueuedEvent: The size is too large.");
}

BluetoothEmuDevice::BluetoothEmuDevice(EmulationKernel& ios, const std::string& device_name)
    : BluetoothBaseDevice(ios, device_name)
{
  SysConf sysconf{ios.GetFS()};
  if (!Core::WantsDeterminism())
    BackUpBTInfoSection(&sysconf);

  ConfPads bt_dinf{};

  for (u8 i = 0; i != MAX_BBMOTES; ++i)
  {
    // Note: BluetoothEmu::GetConnectionHandle and WiimoteDevice::GetNumber rely on final byte.
    const bdaddr_t tmp_bd = {0x11, 0x02, 0x19, 0x79, 0, i};

    // Previous records can be safely overwritten, since they are backed up
    std::ranges::copy(tmp_bd, std::rbegin(bt_dinf.active[i].bdaddr));
    std::ranges::copy(tmp_bd, std::rbegin(bt_dinf.registered[i].bdaddr));

    const auto& wm_name =
        (i == WIIMOTE_BALANCE_BOARD) ? "Nintendo RVL-WBC-01" : "Nintendo RVL-CNT-01";
    memcpy(bt_dinf.registered[i].name, wm_name, 20);
    memcpy(bt_dinf.active[i].name, wm_name, 20);

    DEBUG_LOG_FMT(IOS_WIIMOTE, "Wii Remote {} BT ID {:x},{:x},{:x},{:x},{:x},{:x}", i, tmp_bd[0],
                  tmp_bd[1], tmp_bd[2], tmp_bd[3], tmp_bd[4], tmp_bd[5]);

    const unsigned int hid_source_number =
        NetPlay::IsNetPlayRunning() ? NetPlay::NetPlay_GetLocalWiimoteForSlot(i) : i;
    m_wiimotes[i] = std::make_unique<WiimoteDevice>(this, tmp_bd, hid_source_number);
  }

  bt_dinf.num_registered = MAX_BBMOTES;

  // save now so that when games load sysconf file it includes the new Wii Remotes
  // and the correct order for connected Wii Remotes
  auto& section = sysconf.GetOrAddEntry("BT.DINF", SysConf::Entry::Type::BigArray)->bytes;
  section.resize(sizeof(ConfPads));
  std::memcpy(section.data(), &bt_dinf, sizeof(ConfPads));
  if (!sysconf.Save())
    PanicAlertFmtT("Failed to write BT.DINF to SYSCONF");
}

BluetoothEmuDevice::~BluetoothEmuDevice() = default;

template <typename T>
static void DoStateForMessage(EmulationKernel& ios, PointerWrap& p, std::unique_ptr<T>& message)
{
  u32 request_address = (message != nullptr) ? message->ios_request.address : 0;
  p.Do(request_address);
  if (request_address != 0)
  {
    IOCtlVRequest request{ios.GetSystem(), request_address};
    message = std::make_unique<T>(ios, request);
  }
}

void BluetoothEmuDevice::DoState(PointerWrap& p)
{
  bool passthrough_bluetooth = false;
  p.Do(passthrough_bluetooth);
  if (passthrough_bluetooth && p.IsReadMode())
  {
    Core::DisplayMessage("State needs Bluetooth passthrough to be enabled. Aborting load.", 4000);
    p.SetVerifyMode();
    return;
  }

  Device::DoState(p);
  p.Do(m_controller_bd);
  DoStateForMessage(GetEmulationKernel(), p, m_hci_endpoint);
  DoStateForMessage(GetEmulationKernel(), p, m_acl_endpoint);
  p.Do(m_last_ticks);
  p.DoArray(m_packet_count);
  p.Do(m_scan_enable);
  p.Do(m_event_queue);
  m_acl_pool.DoState(p);

  for (unsigned int i = 0; i < MAX_BBMOTES; i++)
    m_wiimotes[i]->DoState(p);
}

bool BluetoothEmuDevice::RemoteConnect(WiimoteDevice& wiimote)
{
  // If page scan is disabled the controller will not see this connection request.
  if (!(m_scan_enable & HCI_PAGE_SCAN_ENABLE))
    return false;

  SendEventRequestConnection(wiimote);
  return true;
}

bool BluetoothEmuDevice::RemoteDisconnect(const bdaddr_t& address)
{
  return SendEventDisconnect(GetConnectionHandle(address), 0x13);
}

std::optional<IPCReply> BluetoothEmuDevice::Close(u32 fd)
{
  // Clean up state
  m_scan_enable = 0;
  m_last_ticks = 0;
  memset(m_packet_count, 0, sizeof(m_packet_count));
  m_hci_endpoint.reset();
  m_acl_endpoint.reset();

  return Device::Close(fd);
}

std::optional<IPCReply> BluetoothEmuDevice::IOCtlV(const IOCtlVRequest& request)
{
  bool send_reply = true;
  switch (request.request)
  {
  case USB::IOCTLV_USBV0_CTRLMSG:  // HCI command is received from the stack
  {
    // Replies are generated inside
    ExecuteHCICommandMessage(USB::V0CtrlMessage(GetEmulationKernel(), request));
    send_reply = false;
    break;
  }

  case USB::IOCTLV_USBV0_BLKMSG:
  {
    const USB::V0BulkMessage ctrl{GetEmulationKernel(), request};
    switch (ctrl.endpoint)
    {
    case ACL_DATA_OUT:  // ACL data is received from the stack
    {
      auto& system = GetSystem();
      auto& memory = system.GetMemory();

      // This is the ACL datapath from CPU to Wii Remote
      const auto* acl_header = reinterpret_cast<hci_acldata_hdr_t*>(
          memory.GetPointerForRange(ctrl.data_address, sizeof(hci_acldata_hdr_t)));

      DEBUG_ASSERT(HCI_BC_FLAG(acl_header->con_handle) == HCI_POINT2POINT);
      DEBUG_ASSERT(HCI_PB_FLAG(acl_header->con_handle) == HCI_PACKET_START);

      SendToDevice(HCI_CON_HANDLE(acl_header->con_handle),
                   memory.GetPointerForRange(ctrl.data_address + sizeof(hci_acldata_hdr_t),
                                             acl_header->length),
                   acl_header->length);
      break;
    }
    case ACL_DATA_IN:  // We are given an ACL buffer to fill
    {
      m_acl_endpoint = std::make_unique<USB::V0BulkMessage>(GetEmulationKernel(), request);
      DEBUG_LOG_FMT(IOS_WIIMOTE, "ACL_DATA_IN: {:#010x}", request.address);
      send_reply = false;
      break;
    }
    default:
      DEBUG_ASSERT_MSG(IOS_WIIMOTE, 0, "Unknown USB::IOCTLV_USBV0_BLKMSG: {:#x}", ctrl.endpoint);
    }
    break;
  }

  case USB::IOCTLV_USBV0_INTRMSG:
  {
    const USB::V0IntrMessage ctrl{GetEmulationKernel(), request};
    if (ctrl.endpoint == HCI_EVENT)  // We are given a HCI buffer to fill
    {
      m_hci_endpoint = std::make_unique<USB::V0IntrMessage>(GetEmulationKernel(), request);
      DEBUG_LOG_FMT(IOS_WIIMOTE, "HCI_EVENT: {:#010x}", request.address);
      send_reply = false;
    }
    else
    {
      DEBUG_ASSERT_MSG(IOS_WIIMOTE, 0, "Unknown USB::IOCTLV_USBV0_INTRMSG: {:#x}", ctrl.endpoint);
    }
    break;
  }

  default:
    request.DumpUnknown(GetSystem(), GetDeviceName(), Common::Log::LogType::IOS_WIIMOTE);
  }

  if (!send_reply)
    return std::nullopt;
  return IPCReply(IPC_SUCCESS);
}

// Here we handle the USB::IOCTLV_USBV0_BLKMSG Ioctlv
void BluetoothEmuDevice::SendToDevice(u16 connection_handle, u8* data, u32 size)
{
  WiimoteDevice* wiimote = AccessWiimote(connection_handle);
  if (wiimote == nullptr)
    return;

  DEBUG_LOG_FMT(IOS_WIIMOTE, "Send ACL Packet to ConnectionHandle {:#06x}", connection_handle);
  IncDataPacket(connection_handle);
  wiimote->ExecuteL2capCmd(data, size);
}

void BluetoothEmuDevice::IncDataPacket(u16 connection_handle)
{
  m_packet_count[GetWiimoteNumberFromConnectionHandle(connection_handle)]++;
}

// Here we send ACL packets to CPU. They will consist of header + data.
// The header is for example 07 00 41 00 which means size 0x0007 and channel 0x0041.
void BluetoothEmuDevice::SendACLPacket(const bdaddr_t& source, const u8* data, u32 size)
{
  const u16 connection_handle = GetConnectionHandle(source);

  DEBUG_LOG_FMT(IOS_WIIMOTE, "ACL packet from {:x} ready to send to stack...", connection_handle);

  if (m_acl_endpoint && !m_hci_endpoint && m_event_queue.empty())
  {
    DEBUG_LOG_FMT(IOS_WIIMOTE, "ACL endpoint valid, sending packet to {:08x}",
                  m_acl_endpoint->ios_request.address);

    auto& system = GetSystem();
    auto& memory = system.GetMemory();

    hci_acldata_hdr_t* header = reinterpret_cast<hci_acldata_hdr_t*>(
        memory.GetPointerForRange(m_acl_endpoint->data_address, sizeof(hci_acldata_hdr_t)));
    header->con_handle = HCI_MK_CON_HANDLE(connection_handle, HCI_PACKET_START, HCI_POINT2POINT);
    header->length = size;

    // Write the packet to the buffer
    memcpy(reinterpret_cast<u8*>(header) + sizeof(hci_acldata_hdr_t), data, header->length);

    GetEmulationKernel().EnqueueIPCReply(m_acl_endpoint->ios_request,
                                         sizeof(hci_acldata_hdr_t) + size);
    m_acl_endpoint.reset();
  }
  else
  {
    DEBUG_LOG_FMT(IOS_WIIMOTE, "ACL endpoint not currently valid, queuing...");
    m_acl_pool.Store(data, size, connection_handle);
  }
}

// These messages are sent from the Wii Remote to the game, for example RequestConnection()
// or ConnectionComplete().
//
// Our IOS is so efficient that we could fill the buffer immediately
// rather than enqueue it to some other memory and this will do good for StateSave
void BluetoothEmuDevice::AddEventToQueue(const SQueuedEvent& event)
{
  DEBUG_LOG_FMT(IOS_WIIMOTE, "HCI event {:x} completed...",
                ((hci_event_hdr_t*)event.buffer)->event);

  if (m_hci_endpoint)
  {
    if (m_event_queue.empty())  // fast path :)
    {
      DEBUG_LOG_FMT(IOS_WIIMOTE, "HCI endpoint valid, sending packet to {:08x}",
                    m_hci_endpoint->ios_request.address);
      m_hci_endpoint->FillBuffer(event.buffer, event.size);

      // Send a reply to indicate HCI buffer is filled
      GetEmulationKernel().EnqueueIPCReply(m_hci_endpoint->ios_request, event.size);
      m_hci_endpoint.reset();
    }
    else  // push new one, pop oldest
    {
      DEBUG_LOG_FMT(IOS_WIIMOTE, "HCI endpoint not currently valid, queueing ({})...",
                    m_event_queue.size());
      m_event_queue.push_back(event);
      const SQueuedEvent& queued_event = m_event_queue.front();
      DEBUG_LOG_FMT(IOS_WIIMOTE,
                    "HCI event {:x} "
                    "being written from queue ({}) to {:08x}...",
                    ((hci_event_hdr_t*)queued_event.buffer)->event, m_event_queue.size() - 1,
                    m_hci_endpoint->ios_request.address);
      m_hci_endpoint->FillBuffer(queued_event.buffer, queued_event.size);

      // Send a reply to indicate HCI buffer is filled
      GetEmulationKernel().EnqueueIPCReply(m_hci_endpoint->ios_request, queued_event.size);
      m_hci_endpoint.reset();
      m_event_queue.pop_front();
    }
  }
  else
  {
    DEBUG_LOG_FMT(IOS_WIIMOTE, "HCI endpoint not currently valid, queuing ({})...",
                  m_event_queue.size());
    m_event_queue.push_back(event);
  }
}

void BluetoothEmuDevice::Update()
{
  // check HCI queue
  if (!m_event_queue.empty() && m_hci_endpoint)
  {
    // an endpoint has become available, and we have a stored response.
    const SQueuedEvent& event = m_event_queue.front();
    DEBUG_LOG_FMT(IOS_WIIMOTE, "HCI event {:x} being written from queue ({}) to {:08x}...",
                  ((hci_event_hdr_t*)event.buffer)->event, m_event_queue.size() - 1,
                  m_hci_endpoint->ios_request.address);
    m_hci_endpoint->FillBuffer(event.buffer, event.size);

    // Send a reply to indicate HCI buffer is filled
    GetEmulationKernel().EnqueueIPCReply(m_hci_endpoint->ios_request, event.size);
    m_hci_endpoint.reset();
    m_event_queue.pop_front();
  }

  // check ACL queue
  if (!m_acl_pool.IsEmpty() && m_acl_endpoint && m_event_queue.empty())
  {
    m_acl_pool.WriteToEndpoint(*m_acl_endpoint);
    m_acl_endpoint.reset();
  }

  for (auto& wiimote : m_wiimotes)
    wiimote->Update();

  const u64 interval = GetSystem().GetSystemTimers().GetTicksPerSecond() / Wiimote::UPDATE_FREQ;
  const u64 now = GetSystem().GetCoreTiming().GetTicks();

  if (now - m_last_ticks > interval)
  {
    g_controller_interface.SetCurrentInputChannel(ciface::InputChannel::Bluetooth);
    g_controller_interface.UpdateInput();

    std::array<WiimoteEmu::DesiredWiimoteState, MAX_BBMOTES> wiimote_states;
    std::array<WiimoteDevice::NextUpdateInputCall, MAX_BBMOTES> next_call;

    for (size_t i = 0; i < m_wiimotes.size(); ++i)
      next_call[i] = m_wiimotes[i]->PrepareInput(&wiimote_states[i]);

    if (NetPlay::IsNetPlayRunning())
    {
      std::array<WiimoteEmu::SerializedWiimoteState, MAX_BBMOTES> serialized;
      std::array<NetPlay::NetPlayClient::WiimoteDataBatchEntry, MAX_BBMOTES> batch;
      size_t batch_count = 0;
      for (size_t i = 0; i < 4; ++i)
      {
        if (next_call[i] == WiimoteDevice::NextUpdateInputCall::None)
          continue;
        serialized[i] = WiimoteEmu::SerializeDesiredState(wiimote_states[i]);
        batch[batch_count].state = &serialized[i];
        batch[batch_count].wiimote = static_cast<int>(i);
        ++batch_count;
      }

      if (batch_count > 0)
      {
        NetPlay::NetPlay_GetWiimoteData(
            std::span<NetPlay::NetPlayClient::WiimoteDataBatchEntry>(batch.data(), batch_count));

        for (size_t i = 0; i < batch_count; ++i)
        {
          const int wiimote = batch[i].wiimote;
          if (!WiimoteEmu::DeserializeDesiredState(&wiimote_states[wiimote], serialized[wiimote]))
            PanicAlertFmtT("Received invalid Wii Remote data from Netplay.");
        }
      }
    }

    for (size_t i = 0; i < m_wiimotes.size(); ++i)
      m_wiimotes[i]->UpdateInput(next_call[i], wiimote_states[i]);

    m_last_ticks = now;
  }

  SendEventNumberOfCompletedPackets();
}

void BluetoothEmuDevice::ACLPool::Store(const u8* data, const u16 size, const u16 conn_handle)
{
  if (m_queue.size() >= 100)
  {
    // Many simultaneous exchanges of ACL packets tend to cause the queue to fill up.
    ERROR_LOG_FMT(IOS_WIIMOTE, "ACL queue size reached 100 - current packet will be dropped!");
    return;
  }

  DEBUG_ASSERT_MSG(IOS_WIIMOTE, size < ACL_PKT_SIZE, "ACL packet too large for pool");

  m_queue.push_back(Packet());
  auto& packet = m_queue.back();

  std::copy(data, data + size, packet.data);
  packet.size = size;
  packet.conn_handle = conn_handle;
}

void BluetoothEmuDevice::ACLPool::WriteToEndpoint(const USB::V0BulkMessage& endpoint)
{
  auto& packet = m_queue.front();

  const u8* const data = packet.data;
  const u16 size = packet.size;
  const u16 conn_handle = packet.conn_handle;

  DEBUG_LOG_FMT(IOS_WIIMOTE, "ACL packet being written from queue to {:08x}",
                endpoint.ios_request.address);

  auto& system = m_ios.GetSystem();
  auto& memory = system.GetMemory();

  hci_acldata_hdr_t* header = (hci_acldata_hdr_t*)memory.GetPointerForRange(
      endpoint.data_address, sizeof(hci_acldata_hdr_t));
  header->con_handle = HCI_MK_CON_HANDLE(conn_handle, HCI_PACKET_START, HCI_POINT2POINT);
  header->length = size;

  // Write the packet to the buffer
  std::copy(data, data + size, (u8*)header + sizeof(hci_acldata_hdr_t));

  m_queue.pop_front();

  m_ios.EnqueueIPCReply(endpoint.ios_request, sizeof(hci_acldata_hdr_t) + size);
}

bool BluetoothEmuDevice::SendEventInquiryComplete(u8 num_responses)
{
  SQueuedEvent event(sizeof(SHCIEventInquiryComplete), 0);

  SHCIEventInquiryComplete* inquiry_complete = (SHCIEventInquiryComplete*)event.buffer;
  inquiry_complete->EventType = HCI_EVENT_INQUIRY_COMPL;
  inquiry_complete->PayloadLength = sizeof(SHCIEventInquiryComplete) - 2;
  inquiry_complete->EventStatus = 0x00;
  inquiry_complete->num_responses = num_responses;

  AddEventToQueue(event);

  DEBUG_LOG_FMT(IOS_WIIMOTE, "Event: Inquiry complete");

  return true;
}

bool BluetoothEmuDevice::SendEventInquiryResponse()
{
  // We only respond with the first discoverable remote.
  // The Wii instructs users to press 1+2 in the desired play order.
  // Responding with all remotes at once can place them in undesirable slots.
  // Additional scans will connect each remote in the proper order.
  constexpr u8 num_responses = 1;

  static_assert(
      sizeof(SHCIEventInquiryResult) - 2 + (num_responses * sizeof(hci_inquiry_response)) < 256);

  const auto iter =
      std::ranges::find_if(m_wiimotes, std::mem_fn(&WiimoteDevice::IsInquiryScanEnabled));
  if (iter == m_wiimotes.end())
  {
    // No remotes are discoverable.
    SendEventInquiryComplete(0);
    return false;
  }

  const auto& wiimote = *iter;

  SQueuedEvent event(
      u32(sizeof(SHCIEventInquiryResult) + num_responses * sizeof(hci_inquiry_response)), 0);

  const auto inquiry_result = reinterpret_cast<SHCIEventInquiryResult*>(event.buffer);
  inquiry_result->EventType = HCI_EVENT_INQUIRY_RESULT;
  inquiry_result->num_responses = num_responses;

  u8* const buffer = event.buffer + sizeof(SHCIEventInquiryResult);
  const auto response = reinterpret_cast<hci_inquiry_response*>(buffer);

  response->bdaddr = wiimote->GetBD();
  response->page_scan_rep_mode = 1;
  response->page_scan_period_mode = 0;
  response->page_scan_mode = 0;
  std::copy_n(wiimote->GetClass().begin(), HCI_CLASS_SIZE, response->uclass);
  response->clock_offset = 0x3818;

  DEBUG_LOG_FMT(IOS_WIIMOTE, "Event: Send Fake Inquiry of one controller");
  DEBUG_LOG_FMT(IOS_WIIMOTE, "  bd: {:02x}:{:02x}:{:02x}:{:02x}:{:02x}:{:02x}", response->bdaddr[0],
                response->bdaddr[1], response->bdaddr[2], response->bdaddr[3], response->bdaddr[4],
                response->bdaddr[5]);

  inquiry_result->PayloadLength =
      u8(sizeof(SHCIEventInquiryResult) - 2 +
         (inquiry_result->num_responses * sizeof(hci_inquiry_response)));

  AddEventToQueue(event);
  SendEventInquiryComplete(num_responses);
  return true;
}

bool BluetoothEmuDevice::SendEventConnectionComplete(const bdaddr_t& bd, u8 status)
{
  SQueuedEvent event(sizeof(SHCIEventConnectionComplete), 0);

  SHCIEventConnectionComplete* connection_complete = (SHCIEventConnectionComplete*)event.buffer;

  connection_complete->EventType = HCI_EVENT_CON_COMPL;
  connection_complete->PayloadLength = sizeof(SHCIEventConnectionComplete) - 2;
  connection_complete->EventStatus = status;
  connection_complete->Connection_Handle = GetConnectionHandle(bd);
  connection_complete->bdaddr = bd;
  connection_complete->LinkType = HCI_LINK_ACL;
  connection_complete->EncryptionEnabled = HCI_ENCRYPTION_MODE_NONE;

  AddEventToQueue(event);

  static constexpr const char* link_type[] = {
      "HCI_LINK_SCO     0x00 - Voice",
      "HCI_LINK_ACL     0x01 - Data",
      "HCI_LINK_eSCO    0x02 - eSCO",
  };

  DEBUG_LOG_FMT(IOS_WIIMOTE, "Event: SendEventConnectionComplete");
  DEBUG_LOG_FMT(IOS_WIIMOTE, "  Connection_Handle: {:#06x}",
                connection_complete->Connection_Handle);
  DEBUG_LOG_FMT(IOS_WIIMOTE, "  bd: {:02x}:{:02x}:{:02x}:{:02x}:{:02x}:{:02x}",
                connection_complete->bdaddr[0], connection_complete->bdaddr[1],
                connection_complete->bdaddr[2], connection_complete->bdaddr[3],
                connection_complete->bdaddr[4], connection_complete->bdaddr[5]);
  DEBUG_LOG_FMT(IOS_WIIMOTE, "  LinkType: {}", link_type[connection_complete->LinkType]);
  DEBUG_LOG_FMT(IOS_WIIMOTE, "  EncryptionEnabled: {}", connection_complete->EncryptionEnabled);

  return true;
}

bool BluetoothEmuDevice::SendEventRequestConnection(const WiimoteDevice& wiimote)
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

  DEBUG_LOG_FMT(IOS_WIIMOTE, "Event: SendEventRequestConnection");
  DEBUG_LOG_FMT(IOS_WIIMOTE, "  bd: {:02x}:{:02x}:{:02x}:{:02x}:{:02x}:{:02x}",
                event_request_connection->bdaddr[0], event_request_connection->bdaddr[1],
                event_request_connection->bdaddr[2], event_request_connection->bdaddr[3],
                event_request_connection->bdaddr[4], event_request_connection->bdaddr[5]);
  DEBUG_LOG_FMT(IOS_WIIMOTE, "  COD[0]: {:#04x}", event_request_connection->uclass[0]);
  DEBUG_LOG_FMT(IOS_WIIMOTE, "  COD[1]: {:#04x}", event_request_connection->uclass[1]);
  DEBUG_LOG_FMT(IOS_WIIMOTE, "  COD[2]: {:#04x}", event_request_connection->uclass[2]);
  DEBUG_LOG_FMT(IOS_WIIMOTE, "  LinkType: {}", link_type[event_request_connection->LinkType]);

  return true;
}

bool BluetoothEmuDevice::SendEventDisconnect(u16 connection_handle, u8 reason)
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

  DEBUG_LOG_FMT(IOS_WIIMOTE, "Event: SendEventDisconnect");
  DEBUG_LOG_FMT(IOS_WIIMOTE, "  Connection_Handle: {:#06x}", disconnect->Connection_Handle);
  DEBUG_LOG_FMT(IOS_WIIMOTE, "  Reason: {:#04x}", disconnect->Reason);

  return true;
}

bool BluetoothEmuDevice::SendEventAuthenticationCompleted(u16 connection_handle)
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

  DEBUG_LOG_FMT(IOS_WIIMOTE, "Event: SendEventAuthenticationCompleted");
  DEBUG_LOG_FMT(IOS_WIIMOTE, "  Connection_Handle: {:#06x}",
                event_authentication_completed->Connection_Handle);

  AddEventToQueue(event);

  return true;
}

bool BluetoothEmuDevice::SendEventRemoteNameReq(const bdaddr_t& bd)
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

  DEBUG_LOG_FMT(IOS_WIIMOTE, "Event: SendEventRemoteNameReq");
  DEBUG_LOG_FMT(IOS_WIIMOTE, "  bd: {:02x}:{:02x}:{:02x}:{:02x}:{:02x}:{:02x}",
                remote_name_req->bdaddr[0], remote_name_req->bdaddr[1], remote_name_req->bdaddr[2],
                remote_name_req->bdaddr[3], remote_name_req->bdaddr[4], remote_name_req->bdaddr[5]);
  DEBUG_LOG_FMT(IOS_WIIMOTE, "  RemoteName: {}",
                reinterpret_cast<char*>(remote_name_req->RemoteName));

  AddEventToQueue(event);

  return true;
}

bool BluetoothEmuDevice::SendEventReadRemoteFeatures(u16 connection_handle)
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
  std::copy_n(wiimote->GetFeatures().begin(), HCI_FEATURES_SIZE, read_remote_features->features);

  DEBUG_LOG_FMT(IOS_WIIMOTE, "Event: SendEventReadRemoteFeatures");
  DEBUG_LOG_FMT(IOS_WIIMOTE, "  Connection_Handle: {:#06x}",
                read_remote_features->ConnectionHandle);
  DEBUG_LOG_FMT(IOS_WIIMOTE, "  features: {:02x}:{:02x}:{:02x}:{:02x}:{:02x}:{:02x}:{:02x}:{:02x}",
                read_remote_features->features[0], read_remote_features->features[1],
                read_remote_features->features[2], read_remote_features->features[3],
                read_remote_features->features[4], read_remote_features->features[5],
                read_remote_features->features[6], read_remote_features->features[7]);

  AddEventToQueue(event);

  return true;
}

bool BluetoothEmuDevice::SendEventReadRemoteVerInfo(u16 connection_handle)
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

  DEBUG_LOG_FMT(IOS_WIIMOTE, "Event: SendEventReadRemoteVerInfo");
  DEBUG_LOG_FMT(IOS_WIIMOTE, "  Connection_Handle: {:#06x}",
                read_remote_ver_info->ConnectionHandle);
  DEBUG_LOG_FMT(IOS_WIIMOTE, "  lmp_version: {:#04x}", read_remote_ver_info->lmp_version);
  DEBUG_LOG_FMT(IOS_WIIMOTE, "  manufacturer: {:#06x}", read_remote_ver_info->manufacturer);
  DEBUG_LOG_FMT(IOS_WIIMOTE, "  lmp_subversion: {:#06x}", read_remote_ver_info->lmp_subversion);

  AddEventToQueue(event);

  return true;
}

void BluetoothEmuDevice::SendEventCommandComplete(u16 opcode, const void* data, u32 data_size)
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

  DEBUG_LOG_FMT(IOS_WIIMOTE, "Event: Command Complete (Opcode: {:#06x})", hci_event->Opcode);

  AddEventToQueue(event);
}

bool BluetoothEmuDevice::SendEventCommandStatus(u16 opcode)
{
  SQueuedEvent event(sizeof(SHCIEventStatus), 0);

  SHCIEventStatus* hci_event = (SHCIEventStatus*)event.buffer;
  hci_event->EventType = HCI_EVENT_COMMAND_STATUS;
  hci_event->PayloadLength = sizeof(SHCIEventStatus) - 2;
  hci_event->EventStatus = 0x0;
  hci_event->PacketIndicator = 0x01;
  hci_event->Opcode = opcode;

  INFO_LOG_FMT(IOS_WIIMOTE, "Event: Command Status (Opcode: {:#06x})", hci_event->Opcode);

  AddEventToQueue(event);

  return true;
}

bool BluetoothEmuDevice::SendEventRoleChange(bdaddr_t bd, bool master)
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

  DEBUG_LOG_FMT(IOS_WIIMOTE, "Event: SendEventRoleChange");
  DEBUG_LOG_FMT(IOS_WIIMOTE, "  bd: {:02x}:{:02x}:{:02x}:{:02x}:{:02x}:{:02x}",
                role_change->bdaddr[0], role_change->bdaddr[1], role_change->bdaddr[2],
                role_change->bdaddr[3], role_change->bdaddr[4], role_change->bdaddr[5]);
  DEBUG_LOG_FMT(IOS_WIIMOTE, "  NewRole: {}", role_change->NewRole);

  return true;
}

bool BluetoothEmuDevice::SendEventNumberOfCompletedPackets()
{
  SQueuedEvent event((u32)(sizeof(hci_event_hdr_t) + sizeof(hci_num_compl_pkts_ep) +
                           (sizeof(hci_num_compl_pkts_info) * m_wiimotes.size())),
                     0);

  DEBUG_LOG_FMT(IOS_WIIMOTE, "Event: SendEventNumberOfCompletedPackets");

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
    info->con_handle = GetConnectionHandle(m_wiimotes[i]->GetBD());

    DEBUG_LOG_FMT(IOS_WIIMOTE, "  Connection_Handle: {:#06x}", info->con_handle);
    DEBUG_LOG_FMT(IOS_WIIMOTE, "  Number_Of_Completed_Packets: {}", info->compl_pkts);

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
    DEBUG_LOG_FMT(IOS_WIIMOTE, "SendEventNumberOfCompletedPackets: no packets; no event");
  }

  return true;
}

bool BluetoothEmuDevice::SendEventModeChange(u16 connection_handle, u8 mode, u16 value)
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

  DEBUG_LOG_FMT(IOS_WIIMOTE, "Event: SendEventModeChange");
  DEBUG_LOG_FMT(IOS_WIIMOTE, "  Connection_Handle: {:#06x}", mode_change->Connection_Handle);
  DEBUG_LOG_FMT(IOS_WIIMOTE, "  Current Mode: {:#04x}", mode_change->CurrentMode = mode);

  AddEventToQueue(event);

  return true;
}

bool BluetoothEmuDevice::SendEventLinkKeyNotification(const u8 num_to_send)
{
  u8 payload_length = sizeof(hci_return_link_keys_ep) + sizeof(hci_link_key_rep_cp) * num_to_send;
  SQueuedEvent event(2 + payload_length, 0);
  SHCIEventLinkKeyNotification* event_link_key = (SHCIEventLinkKeyNotification*)event.buffer;

  DEBUG_LOG_FMT(IOS_WIIMOTE, "Event: SendEventLinkKeyNotification");

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
    link_key_info->bdaddr = m_wiimotes[i]->GetBD();
    std::copy_n(m_wiimotes[i]->GetLinkKey().begin(), HCI_KEY_SIZE, link_key_info->key);

    DEBUG_LOG_FMT(IOS_WIIMOTE, "  bd: {:02x}:{:02x}:{:02x}:{:02x}:{:02x}:{:02x}",
                  link_key_info->bdaddr[0], link_key_info->bdaddr[1], link_key_info->bdaddr[2],
                  link_key_info->bdaddr[3], link_key_info->bdaddr[4], link_key_info->bdaddr[5]);
  }

  AddEventToQueue(event);

  return true;
}

bool BluetoothEmuDevice::SendEventRequestLinkKey(const bdaddr_t& bd)
{
  SQueuedEvent event(sizeof(SHCIEventRequestLinkKey), 0);

  SHCIEventRequestLinkKey* event_request_link_key = (SHCIEventRequestLinkKey*)event.buffer;

  event_request_link_key->EventType = HCI_EVENT_LINK_KEY_REQ;
  event_request_link_key->PayloadLength = sizeof(SHCIEventRequestLinkKey) - 2;
  event_request_link_key->bdaddr = bd;

  DEBUG_LOG_FMT(IOS_WIIMOTE, "Event: SendEventRequestLinkKey");
  DEBUG_LOG_FMT(IOS_WIIMOTE, "  bd: {:02x}:{:02x}:{:02x}:{:02x}:{:02x}:{:02x}",
                event_request_link_key->bdaddr[0], event_request_link_key->bdaddr[1],
                event_request_link_key->bdaddr[2], event_request_link_key->bdaddr[3],
                event_request_link_key->bdaddr[4], event_request_link_key->bdaddr[5]);

  AddEventToQueue(event);

  return true;
}

bool BluetoothEmuDevice::SendEventReadClockOffsetComplete(u16 connection_handle)
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

  DEBUG_LOG_FMT(IOS_WIIMOTE, "Event: SendEventReadClockOffsetComplete");
  DEBUG_LOG_FMT(IOS_WIIMOTE, "  Connection_Handle: {:#06x}",
                read_clock_offset_complete->ConnectionHandle);
  DEBUG_LOG_FMT(IOS_WIIMOTE, "  ClockOffset: {:#06x}", read_clock_offset_complete->ClockOffset);

  AddEventToQueue(event);

  return true;
}

bool BluetoothEmuDevice::SendEventConPacketTypeChange(u16 connection_handle, u16 packet_type)
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

  DEBUG_LOG_FMT(IOS_WIIMOTE, "Event: SendEventConPacketTypeChange");
  DEBUG_LOG_FMT(IOS_WIIMOTE, "  Connection_Handle: {:#06x}",
                change_con_packet_type->ConnectionHandle);
  DEBUG_LOG_FMT(IOS_WIIMOTE, "  PacketType: {:#06x}", change_con_packet_type->PacketType);

  AddEventToQueue(event);

  return true;
}

// Command dispatcher
// This is called from the USB::IOCTLV_USBV0_CTRLMSG Ioctlv
void BluetoothEmuDevice::ExecuteHCICommandMessage(const USB::V0CtrlMessage& ctrl_message)
{
  auto& system = GetSystem();
  auto& memory = system.GetMemory();

  const u32 input_address = ctrl_message.data_address + 3;

  SCommandMessage msg;
  memory.CopyFromEmu(&msg, ctrl_message.data_address, sizeof(msg));

  const u16 ocf = HCI_OCF(msg.Opcode);
  const u16 ogf = HCI_OGF(msg.Opcode);

  DEBUG_LOG_FMT(IOS_WIIMOTE, "**************************************************");
  DEBUG_LOG_FMT(IOS_WIIMOTE, "Execute HCI Command: {:#06x} (ocf: {:#04x}, ogf: {:#04x})",
                msg.Opcode, ocf, ogf);

  switch (msg.Opcode)
  {
  //
  // --- read commands ---
  //
  case HCI_CMD_RESET:
    CommandReset(input_address);
    break;

  case HCI_CMD_READ_BUFFER_SIZE:
    CommandReadBufferSize(input_address);
    break;

  case HCI_CMD_READ_LOCAL_VER:
    CommandReadLocalVer(input_address);
    break;

  case HCI_CMD_READ_BDADDR:
    CommandReadBDAdrr(input_address);
    break;

  case HCI_CMD_READ_LOCAL_FEATURES:
    CommandReadLocalFeatures(input_address);
    break;

  case HCI_CMD_READ_STORED_LINK_KEY:
    CommandReadStoredLinkKey(input_address);
    break;

  case HCI_CMD_WRITE_UNIT_CLASS:
    CommandWriteUnitClass(input_address);
    break;

  case HCI_CMD_WRITE_LOCAL_NAME:
    CommandWriteLocalName(input_address);
    break;

  case HCI_CMD_WRITE_PIN_TYPE:
    CommandWritePinType(input_address);
    break;

  case HCI_CMD_HOST_BUFFER_SIZE:
    CommandHostBufferSize(input_address);
    break;

  case HCI_CMD_WRITE_PAGE_TIMEOUT:
    CommandWritePageTimeOut(input_address);
    break;

  case HCI_CMD_WRITE_SCAN_ENABLE:
    CommandWriteScanEnable(input_address);
    break;

  case HCI_CMD_WRITE_INQUIRY_MODE:
    CommandWriteInquiryMode(input_address);
    break;

  case HCI_CMD_WRITE_PAGE_SCAN_TYPE:
    CommandWritePageScanType(input_address);
    break;

  case HCI_CMD_SET_EVENT_FILTER:
    CommandSetEventFilter(input_address);
    break;

  case HCI_CMD_INQUIRY:
    CommandInquiry(input_address);
    break;

  case HCI_CMD_WRITE_INQUIRY_SCAN_TYPE:
    CommandWriteInquiryScanType(input_address);
    break;

  // vendor specific...
  case 0xFC4C:
    CommandVendorSpecific_FC4C(input_address, ctrl_message.length - 3);
    break;

  case 0xFC4F:
    CommandVendorSpecific_FC4F(input_address, ctrl_message.length - 3);
    break;

  case HCI_CMD_INQUIRY_CANCEL:
    CommandInquiryCancel(input_address);
    break;

  case HCI_CMD_REMOTE_NAME_REQ:
    CommandRemoteNameReq(input_address);
    break;

  case HCI_CMD_CREATE_CON:
    CommandCreateCon(input_address);
    break;

  case HCI_CMD_ACCEPT_CON:
    CommandAcceptCon(input_address);
    break;

  case HCI_CMD_CHANGE_CON_PACKET_TYPE:
    CommandChangeConPacketType(input_address);
    break;

  case HCI_CMD_READ_CLOCK_OFFSET:
    CommandReadClockOffset(input_address);
    break;

  case HCI_CMD_READ_REMOTE_VER_INFO:
    CommandReadRemoteVerInfo(input_address);
    break;

  case HCI_CMD_READ_REMOTE_FEATURES:
    CommandReadRemoteFeatures(input_address);
    break;

  case HCI_CMD_WRITE_LINK_POLICY_SETTINGS:
    CommandWriteLinkPolicy(input_address);
    break;

  case HCI_CMD_AUTH_REQ:
    CommandAuthenticationRequested(input_address);
    break;

  case HCI_CMD_SNIFF_MODE:
    CommandSniffMode(input_address);
    break;

  case HCI_CMD_DISCONNECT:
    CommandDisconnect(input_address);
    break;

  case HCI_CMD_WRITE_LINK_SUPERVISION_TIMEOUT:
    CommandWriteLinkSupervisionTimeout(input_address);
    break;

  case HCI_CMD_LINK_KEY_NEG_REP:
    CommandLinkKeyNegRep(input_address);
    break;

  case HCI_CMD_LINK_KEY_REP:
    CommandLinkKeyRep(input_address);
    break;

  case HCI_CMD_DELETE_STORED_LINK_KEY:
    CommandDeleteStoredLinkKey(input_address);
    break;

  default:
    // send fake okay msg...
    SendEventCommandComplete(msg.Opcode, nullptr, 0);

    if (ogf == HCI_OGF_VENDOR)
    {
      ERROR_LOG_FMT(IOS_WIIMOTE, "Command: vendor specific: {:#06x} (ocf: {:#x})", msg.Opcode, ocf);
      for (int i = 0; i < msg.len; i++)
      {
        ERROR_LOG_FMT(IOS_WIIMOTE, "  0x02{:#x}", memory.Read_U8(input_address + i));
      }
    }
    else
    {
      DEBUG_ASSERT_MSG(IOS_WIIMOTE, 0,
                       "Unknown USB_IOCTL_CTRLMSG: {:#06x} (ocf: {:#04x} ogf {:#04x})", msg.Opcode,
                       ocf, ogf);
    }
    break;
  }

  // HCI command is finished, send a reply to command
  GetEmulationKernel().EnqueueIPCReply(ctrl_message.ios_request, ctrl_message.length);
}

//
//
// --- command helper
//
//
void BluetoothEmuDevice::CommandInquiry(u32 input_address)
{
  auto& system = GetSystem();
  auto& memory = system.GetMemory();

  // Inquiry should not be called normally
  hci_inquiry_cp inquiry;
  memory.CopyFromEmu(&inquiry, input_address, sizeof(inquiry));

  INFO_LOG_FMT(IOS_WIIMOTE, "Command: HCI_CMD_INQUIRY:");
  DEBUG_LOG_FMT(IOS_WIIMOTE, "write:");
  DEBUG_LOG_FMT(IOS_WIIMOTE, "  LAP[0]: {:#04x}", inquiry.lap[0]);
  DEBUG_LOG_FMT(IOS_WIIMOTE, "  LAP[1]: {:#04x}", inquiry.lap[1]);
  DEBUG_LOG_FMT(IOS_WIIMOTE, "  LAP[2]: {:#04x}", inquiry.lap[2]);
  DEBUG_LOG_FMT(IOS_WIIMOTE, "  inquiry_length: {} (N x 1.28) sec", inquiry.inquiry_length);
  DEBUG_LOG_FMT(IOS_WIIMOTE, "  num_responses: {} (N x 1.28) sec", inquiry.num_responses);

  SendEventCommandStatus(HCI_CMD_INQUIRY);
  SendEventInquiryResponse();
}

void BluetoothEmuDevice::CommandInquiryCancel(u32 input_address)
{
  hci_inquiry_cancel_rp reply;
  reply.status = 0x00;

  INFO_LOG_FMT(IOS_WIIMOTE, "Command: HCI_CMD_INQUIRY_CANCEL");

  SendEventCommandComplete(HCI_CMD_INQUIRY_CANCEL, &reply, sizeof(hci_inquiry_cancel_rp));
}

void BluetoothEmuDevice::CommandCreateCon(u32 input_address)
{
  auto& system = GetSystem();
  auto& memory = system.GetMemory();

  hci_create_con_cp create_connection;
  memory.CopyFromEmu(&create_connection, input_address, sizeof(create_connection));

  INFO_LOG_FMT(IOS_WIIMOTE, "Command: HCI_CMD_CREATE_CON");
  DEBUG_LOG_FMT(IOS_WIIMOTE, "Input:");
  DEBUG_LOG_FMT(IOS_WIIMOTE, "  bd: {:02x}:{:02x}:{:02x}:{:02x}:{:02x}:{:02x}",
                create_connection.bdaddr[0], create_connection.bdaddr[1],
                create_connection.bdaddr[2], create_connection.bdaddr[3],
                create_connection.bdaddr[4], create_connection.bdaddr[5]);
  DEBUG_LOG_FMT(IOS_WIIMOTE, "  pkt_type: {}", create_connection.pkt_type);
  DEBUG_LOG_FMT(IOS_WIIMOTE, "  page_scan_rep_mode: {}", create_connection.page_scan_rep_mode);
  DEBUG_LOG_FMT(IOS_WIIMOTE, "  page_scan_mode: {}", create_connection.page_scan_mode);
  DEBUG_LOG_FMT(IOS_WIIMOTE, "  clock_offset: {}", create_connection.clock_offset);
  DEBUG_LOG_FMT(IOS_WIIMOTE, "  accept_role_switch: {}", create_connection.accept_role_switch);

  SendEventCommandStatus(HCI_CMD_CREATE_CON);

  WiimoteDevice* wiimote = AccessWiimote(create_connection.bdaddr);
  const bool successful = wiimote && wiimote->EventConnectionRequest();

  // Status 0x08 (Connection Timeout) if WiimoteDevice does not accept the connection.
  SendEventConnectionComplete(create_connection.bdaddr, successful ? 0x00 : 0x08);
}

void BluetoothEmuDevice::CommandDisconnect(u32 input_address)
{
  auto& system = GetSystem();
  auto& memory = system.GetMemory();

  hci_discon_cp disconnect;
  memory.CopyFromEmu(&disconnect, input_address, sizeof(disconnect));

  INFO_LOG_FMT(IOS_WIIMOTE, "Command: HCI_CMD_DISCONNECT");
  DEBUG_LOG_FMT(IOS_WIIMOTE, "  ConnectionHandle: {:#06x}", disconnect.con_handle);
  DEBUG_LOG_FMT(IOS_WIIMOTE, "  Reason: {:#04x}", disconnect.reason);

  SendEventCommandStatus(HCI_CMD_DISCONNECT);
  SendEventDisconnect(disconnect.con_handle, disconnect.reason);

  WiimoteDevice* wiimote = AccessWiimote(disconnect.con_handle);
  if (wiimote)
    wiimote->EventDisconnect(disconnect.reason);
}

void BluetoothEmuDevice::CommandAcceptCon(u32 input_address)
{
  auto& system = GetSystem();
  auto& memory = system.GetMemory();

  hci_accept_con_cp accept_connection;
  memory.CopyFromEmu(&accept_connection, input_address, sizeof(accept_connection));

  static constexpr const char* roles[] = {
      "Master (0x00)",
      "Slave (0x01)",
  };

  INFO_LOG_FMT(IOS_WIIMOTE, "Command: HCI_CMD_ACCEPT_CON");
  DEBUG_LOG_FMT(IOS_WIIMOTE, "  bd: {:02x}:{:02x}:{:02x}:{:02x}:{:02x}:{:02x}",
                accept_connection.bdaddr[0], accept_connection.bdaddr[1],
                accept_connection.bdaddr[2], accept_connection.bdaddr[3],
                accept_connection.bdaddr[4], accept_connection.bdaddr[5]);
  DEBUG_LOG_FMT(IOS_WIIMOTE, "  role: {}", roles[accept_connection.role]);

  SendEventCommandStatus(HCI_CMD_ACCEPT_CON);

  WiimoteDevice* wiimote = AccessWiimote(accept_connection.bdaddr);
  const bool successful = wiimote && wiimote->EventConnectionAccept();

  if (successful)
  {
    // This connection wants to be the master.
    // The controller performs a master-slave switch and notifies the host.
    if (accept_connection.role == 0)
      SendEventRoleChange(accept_connection.bdaddr, true);

    SendEventConnectionComplete(accept_connection.bdaddr, 0x00);
  }
  else
  {
    // Status 0x08 (Connection Timeout) if WiimoteDevice no longer wants this connection.
    SendEventConnectionComplete(accept_connection.bdaddr, 0x08);
  }
}

void BluetoothEmuDevice::CommandLinkKeyRep(u32 input_address)
{
  auto& system = GetSystem();
  auto& memory = system.GetMemory();

  hci_link_key_rep_cp key_rep;
  memory.CopyFromEmu(&key_rep, input_address, sizeof(key_rep));

  INFO_LOG_FMT(IOS_WIIMOTE, "Command: HCI_CMD_LINK_KEY_REP");
  DEBUG_LOG_FMT(IOS_WIIMOTE, "  bd: {:02x}:{:02x}:{:02x}:{:02x}:{:02x}:{:02x}", key_rep.bdaddr[0],
                key_rep.bdaddr[1], key_rep.bdaddr[2], key_rep.bdaddr[3], key_rep.bdaddr[4],
                key_rep.bdaddr[5]);

  hci_link_key_rep_rp reply;
  reply.status = 0x00;
  reply.bdaddr = key_rep.bdaddr;

  SendEventCommandComplete(HCI_CMD_LINK_KEY_REP, &reply, sizeof(hci_link_key_rep_rp));
}

void BluetoothEmuDevice::CommandLinkKeyNegRep(u32 input_address)
{
  auto& system = GetSystem();
  auto& memory = system.GetMemory();

  hci_link_key_neg_rep_cp key_neg;
  memory.CopyFromEmu(&key_neg, input_address, sizeof(key_neg));

  INFO_LOG_FMT(IOS_WIIMOTE, "Command: HCI_CMD_LINK_KEY_NEG_REP");
  DEBUG_LOG_FMT(IOS_WIIMOTE, "  bd: {:02x}:{:02x}:{:02x}:{:02x}:{:02x}:{:02x}", key_neg.bdaddr[0],
                key_neg.bdaddr[1], key_neg.bdaddr[2], key_neg.bdaddr[3], key_neg.bdaddr[4],
                key_neg.bdaddr[5]);

  hci_link_key_neg_rep_rp reply;
  reply.status = 0x00;
  reply.bdaddr = key_neg.bdaddr;

  SendEventCommandComplete(HCI_CMD_LINK_KEY_NEG_REP, &reply, sizeof(hci_link_key_neg_rep_rp));
}

void BluetoothEmuDevice::CommandChangeConPacketType(u32 input_address)
{
  auto& system = GetSystem();
  auto& memory = system.GetMemory();

  hci_change_con_pkt_type_cp change_packet_type;
  memory.CopyFromEmu(&change_packet_type, input_address, sizeof(change_packet_type));

  // ntd stack sets packet type 0xcc18, which is HCI_PKT_DH5 | HCI_PKT_DM5 | HCI_PKT_DH1 |
  // HCI_PKT_DM1
  // dunno what to do...run awayyyyyy!
  INFO_LOG_FMT(IOS_WIIMOTE, "Command: HCI_CMD_CHANGE_CON_PACKET_TYPE");
  DEBUG_LOG_FMT(IOS_WIIMOTE, "  ConnectionHandle: {:#06x}", change_packet_type.con_handle);
  DEBUG_LOG_FMT(IOS_WIIMOTE, "  PacketType: {:#06x}", change_packet_type.pkt_type);

  SendEventCommandStatus(HCI_CMD_CHANGE_CON_PACKET_TYPE);
  SendEventConPacketTypeChange(change_packet_type.con_handle, change_packet_type.pkt_type);
}

void BluetoothEmuDevice::CommandAuthenticationRequested(u32 input_address)
{
  auto& system = GetSystem();
  auto& memory = system.GetMemory();

  hci_auth_req_cp auth_req;
  memory.CopyFromEmu(&auth_req, input_address, sizeof(auth_req));

  INFO_LOG_FMT(IOS_WIIMOTE, "Command: HCI_CMD_AUTH_REQ");
  DEBUG_LOG_FMT(IOS_WIIMOTE, "  ConnectionHandle: {:#06x}", auth_req.con_handle);

  SendEventCommandStatus(HCI_CMD_AUTH_REQ);
  SendEventAuthenticationCompleted(auth_req.con_handle);
}

void BluetoothEmuDevice::CommandRemoteNameReq(u32 input_address)
{
  auto& system = GetSystem();
  auto& memory = system.GetMemory();

  hci_remote_name_req_cp remote_name_req;
  memory.CopyFromEmu(&remote_name_req, input_address, sizeof(remote_name_req));

  INFO_LOG_FMT(IOS_WIIMOTE, "Command: HCI_CMD_REMOTE_NAME_REQ");
  DEBUG_LOG_FMT(IOS_WIIMOTE, "  bd: {:02x}:{:02x}:{:02x}:{:02x}:{:02x}:{:02x}",
                remote_name_req.bdaddr[0], remote_name_req.bdaddr[1], remote_name_req.bdaddr[2],
                remote_name_req.bdaddr[3], remote_name_req.bdaddr[4], remote_name_req.bdaddr[5]);
  DEBUG_LOG_FMT(IOS_WIIMOTE, "  page_scan_rep_mode: {}", remote_name_req.page_scan_rep_mode);
  DEBUG_LOG_FMT(IOS_WIIMOTE, "  page_scan_mode: {}", remote_name_req.page_scan_mode);
  DEBUG_LOG_FMT(IOS_WIIMOTE, "  clock_offset: {}", remote_name_req.clock_offset);

  SendEventCommandStatus(HCI_CMD_REMOTE_NAME_REQ);
  SendEventRemoteNameReq(remote_name_req.bdaddr);
}

void BluetoothEmuDevice::CommandReadRemoteFeatures(u32 input_address)
{
  auto& system = GetSystem();
  auto& memory = system.GetMemory();

  hci_read_remote_features_cp read_remote_features;
  memory.CopyFromEmu(&read_remote_features, input_address, sizeof(read_remote_features));

  INFO_LOG_FMT(IOS_WIIMOTE, "Command: HCI_CMD_READ_REMOTE_FEATURES");
  DEBUG_LOG_FMT(IOS_WIIMOTE, "  ConnectionHandle: {:#06x}", read_remote_features.con_handle);

  SendEventCommandStatus(HCI_CMD_READ_REMOTE_FEATURES);
  SendEventReadRemoteFeatures(read_remote_features.con_handle);
}

void BluetoothEmuDevice::CommandReadRemoteVerInfo(u32 input_address)
{
  auto& system = GetSystem();
  auto& memory = system.GetMemory();

  hci_read_remote_ver_info_cp read_remote_ver_info;
  memory.CopyFromEmu(&read_remote_ver_info, input_address, sizeof(read_remote_ver_info));

  INFO_LOG_FMT(IOS_WIIMOTE, "Command: HCI_CMD_READ_REMOTE_VER_INFO");
  DEBUG_LOG_FMT(IOS_WIIMOTE, "  ConnectionHandle: {:#04x}", read_remote_ver_info.con_handle);

  SendEventCommandStatus(HCI_CMD_READ_REMOTE_VER_INFO);
  SendEventReadRemoteVerInfo(read_remote_ver_info.con_handle);
}

void BluetoothEmuDevice::CommandReadClockOffset(u32 input_address)
{
  auto& system = GetSystem();
  auto& memory = system.GetMemory();

  hci_read_clock_offset_cp read_clock_offset;
  memory.CopyFromEmu(&read_clock_offset, input_address, sizeof(read_clock_offset));

  INFO_LOG_FMT(IOS_WIIMOTE, "Command: HCI_CMD_READ_CLOCK_OFFSET");
  DEBUG_LOG_FMT(IOS_WIIMOTE, "  ConnectionHandle: {:#04x}", read_clock_offset.con_handle);

  SendEventCommandStatus(HCI_CMD_READ_CLOCK_OFFSET);
  SendEventReadClockOffsetComplete(read_clock_offset.con_handle);
}

void BluetoothEmuDevice::CommandSniffMode(u32 input_address)
{
  auto& system = GetSystem();
  auto& memory = system.GetMemory();

  hci_sniff_mode_cp sniff_mode;
  memory.CopyFromEmu(&sniff_mode, input_address, sizeof(sniff_mode));

  INFO_LOG_FMT(IOS_WIIMOTE, "Command: HCI_CMD_SNIFF_MODE");
  DEBUG_LOG_FMT(IOS_WIIMOTE, "  ConnectionHandle: {:#06x}", sniff_mode.con_handle);
  DEBUG_LOG_FMT(IOS_WIIMOTE, "  max_interval: {} msec", sniff_mode.max_interval * .625);
  DEBUG_LOG_FMT(IOS_WIIMOTE, "  min_interval: {} msec", sniff_mode.min_interval * .625);
  DEBUG_LOG_FMT(IOS_WIIMOTE, "  attempt: {} msec", sniff_mode.attempt * 1.25);
  DEBUG_LOG_FMT(IOS_WIIMOTE, "  timeout: {} msec", sniff_mode.timeout * 1.25);

  SendEventCommandStatus(HCI_CMD_SNIFF_MODE);
  SendEventModeChange(sniff_mode.con_handle, 0x02, sniff_mode.max_interval);  // 0x02 - sniff mode
}

void BluetoothEmuDevice::CommandWriteLinkPolicy(u32 input_address)
{
  auto& system = GetSystem();
  auto& memory = system.GetMemory();

  hci_write_link_policy_settings_cp link_policy;
  memory.CopyFromEmu(&link_policy, input_address, sizeof(link_policy));

  INFO_LOG_FMT(IOS_WIIMOTE, "Command: HCI_CMD_WRITE_LINK_POLICY_SETTINGS");
  DEBUG_LOG_FMT(IOS_WIIMOTE, "  ConnectionHandle: {:#06x}", link_policy.con_handle);
  DEBUG_LOG_FMT(IOS_WIIMOTE, "  Policy: {:#06x}", link_policy.settings);

  SendEventCommandStatus(HCI_CMD_WRITE_LINK_POLICY_SETTINGS);
}

void BluetoothEmuDevice::CommandReset(u32 input_address)
{
  hci_status_rp reply;
  reply.status = 0x00;

  INFO_LOG_FMT(IOS_WIIMOTE, "Command: HCI_CMD_RESET");
  SendEventCommandComplete(HCI_CMD_RESET, &reply, sizeof(hci_status_rp));

  // TODO: We should actually reset connections and channels and everything here.
}

void BluetoothEmuDevice::CommandSetEventFilter(u32 input_address)
{
  auto& system = GetSystem();
  auto& memory = system.GetMemory();

  hci_set_event_filter_cp set_event_filter;
  memory.CopyFromEmu(&set_event_filter, input_address, sizeof(set_event_filter));

  // It looks like software only ever sets a "new device inquiry response" filter.
  // This is one we can safely ignore because of our fake inquiry implementation
  // and documentation says controllers can opt to not implement this filter anyways.

  // TODO: There should be a warn log if an actual filter is being set.

  hci_set_event_filter_rp reply;
  reply.status = 0x00;

  INFO_LOG_FMT(IOS_WIIMOTE, "Command: HCI_CMD_SET_EVENT_FILTER:");
  DEBUG_LOG_FMT(IOS_WIIMOTE, "  filter_type: {}", set_event_filter.filter_type);
  DEBUG_LOG_FMT(IOS_WIIMOTE, "  filter_condition_type: {}", set_event_filter.filter_condition_type);

  SendEventCommandComplete(HCI_CMD_SET_EVENT_FILTER, &reply, sizeof(hci_set_event_filter_rp));
}

void BluetoothEmuDevice::CommandWritePinType(u32 input_address)
{
  auto& system = GetSystem();
  auto& memory = system.GetMemory();

  hci_write_pin_type_cp write_pin_type;
  memory.CopyFromEmu(&write_pin_type, input_address, sizeof(write_pin_type));

  hci_write_pin_type_rp reply;
  reply.status = 0x00;

  INFO_LOG_FMT(IOS_WIIMOTE, "Command: HCI_CMD_WRITE_PIN_TYPE:");
  DEBUG_LOG_FMT(IOS_WIIMOTE, "  pin_type: {:x}", write_pin_type.pin_type);

  SendEventCommandComplete(HCI_CMD_WRITE_PIN_TYPE, &reply, sizeof(hci_write_pin_type_rp));
}

void BluetoothEmuDevice::CommandReadStoredLinkKey(u32 input_address)
{
  auto& system = GetSystem();
  auto& memory = system.GetMemory();

  hci_read_stored_link_key_cp read_stored_link_key;
  memory.CopyFromEmu(&read_stored_link_key, input_address, sizeof(read_stored_link_key));

  hci_read_stored_link_key_rp reply;
  reply.status = 0x00;
  reply.num_keys_read = 0;
  reply.max_num_keys = 255;

  if (read_stored_link_key.read_all == 1)
    reply.num_keys_read = static_cast<u16>(m_wiimotes.size());
  else
    ERROR_LOG_FMT(IOS_WIIMOTE, "CommandReadStoredLinkKey isn't looking for all devices");

  INFO_LOG_FMT(IOS_WIIMOTE, "Command: HCI_CMD_READ_STORED_LINK_KEY:");
  DEBUG_LOG_FMT(IOS_WIIMOTE, "input:");
  DEBUG_LOG_FMT(IOS_WIIMOTE, "  bd: {:02x}:{:02x}:{:02x}:{:02x}:{:02x}:{:02x}",
                read_stored_link_key.bdaddr[0], read_stored_link_key.bdaddr[1],
                read_stored_link_key.bdaddr[2], read_stored_link_key.bdaddr[3],
                read_stored_link_key.bdaddr[4], read_stored_link_key.bdaddr[5]);
  DEBUG_LOG_FMT(IOS_WIIMOTE, "  read_all: {}", read_stored_link_key.read_all);
  DEBUG_LOG_FMT(IOS_WIIMOTE, "return:");
  DEBUG_LOG_FMT(IOS_WIIMOTE, "  max_num_keys: {}", reply.max_num_keys);
  DEBUG_LOG_FMT(IOS_WIIMOTE, "  num_keys_read: {}", reply.num_keys_read);

  SendEventLinkKeyNotification(static_cast<u8>(reply.num_keys_read));
  SendEventCommandComplete(HCI_CMD_READ_STORED_LINK_KEY, &reply,
                           sizeof(hci_read_stored_link_key_rp));
}

void BluetoothEmuDevice::CommandDeleteStoredLinkKey(u32 input_address)
{
  auto& system = GetSystem();
  auto& memory = system.GetMemory();

  hci_delete_stored_link_key_cp delete_stored_link_key;
  memory.CopyFromEmu(&delete_stored_link_key, input_address, sizeof(delete_stored_link_key));

  INFO_LOG_FMT(IOS_WIIMOTE, "Command: HCI_OCF_DELETE_STORED_LINK_KEY");
  DEBUG_LOG_FMT(IOS_WIIMOTE, "  bd: {:02x}:{:02x}:{:02x}:{:02x}:{:02x}:{:02x}",
                delete_stored_link_key.bdaddr[0], delete_stored_link_key.bdaddr[1],
                delete_stored_link_key.bdaddr[2], delete_stored_link_key.bdaddr[3],
                delete_stored_link_key.bdaddr[4], delete_stored_link_key.bdaddr[5]);
  DEBUG_LOG_FMT(IOS_WIIMOTE, "  delete_all: {:#x}", delete_stored_link_key.delete_all);

  const WiimoteDevice* wiimote = AccessWiimote(delete_stored_link_key.bdaddr);
  if (wiimote == nullptr)
    return;

  hci_delete_stored_link_key_rp reply;
  reply.status = 0x00;
  reply.num_keys_deleted = 0;

  SendEventCommandComplete(HCI_CMD_DELETE_STORED_LINK_KEY, &reply,
                           sizeof(hci_delete_stored_link_key_rp));

  ERROR_LOG_FMT(IOS_WIIMOTE, "HCI: CommandDeleteStoredLinkKey... Probably the security for linking "
                             "has failed. Could be a problem with loading the SCONF");
}

void BluetoothEmuDevice::CommandWriteLocalName(u32 input_address)
{
  auto& system = GetSystem();
  auto& memory = system.GetMemory();

  hci_write_local_name_cp write_local_name;
  memory.CopyFromEmu(&write_local_name, input_address, sizeof(write_local_name));

  hci_write_local_name_rp reply;
  reply.status = 0x00;

  INFO_LOG_FMT(IOS_WIIMOTE, "Command: HCI_CMD_WRITE_LOCAL_NAME:");
  DEBUG_LOG_FMT(IOS_WIIMOTE, "  local_name: {}", write_local_name.name);

  SendEventCommandComplete(HCI_CMD_WRITE_LOCAL_NAME, &reply, sizeof(hci_write_local_name_rp));
}

void BluetoothEmuDevice::CommandWritePageTimeOut(u32 input_address)
{
  auto& system = GetSystem();
  auto& memory = system.GetMemory();

  hci_write_page_timeout_cp write_page_timeout;
  memory.CopyFromEmu(&write_page_timeout, input_address, sizeof(write_page_timeout));

  hci_host_buffer_size_rp reply;
  reply.status = 0x00;

  INFO_LOG_FMT(IOS_WIIMOTE, "Command: HCI_CMD_WRITE_PAGE_TIMEOUT:");
  DEBUG_LOG_FMT(IOS_WIIMOTE, "  timeout: {}", write_page_timeout.timeout);

  SendEventCommandComplete(HCI_CMD_WRITE_PAGE_TIMEOUT, &reply, sizeof(hci_host_buffer_size_rp));
}

void BluetoothEmuDevice::CommandWriteScanEnable(u32 input_address)
{
  auto& system = GetSystem();
  auto& memory = system.GetMemory();

  hci_write_scan_enable_cp write_scan_enable;
  memory.CopyFromEmu(&write_scan_enable, input_address, sizeof(write_scan_enable));

  m_scan_enable = write_scan_enable.scan_enable;

  hci_write_scan_enable_rp reply;
  reply.status = 0x00;

  static constexpr const char* scanning[] = {
      "HCI_NO_SCAN_ENABLE",
      "HCI_INQUIRY_SCAN_ENABLE",
      "HCI_PAGE_SCAN_ENABLE",
      "HCI_INQUIRY_AND_PAGE_SCAN_ENABLE",
  };

  DEBUG_LOG_FMT(IOS_WIIMOTE, "Command: HCI_CMD_WRITE_SCAN_ENABLE: ({:#04x})",
                write_scan_enable.scan_enable);
  DEBUG_LOG_FMT(IOS_WIIMOTE, "  scan_enable: {}", scanning[write_scan_enable.scan_enable]);

  SendEventCommandComplete(HCI_CMD_WRITE_SCAN_ENABLE, &reply, sizeof(hci_write_scan_enable_rp));
}

void BluetoothEmuDevice::CommandWriteUnitClass(u32 input_address)
{
  auto& system = GetSystem();
  auto& memory = system.GetMemory();

  hci_write_unit_class_cp write_unit_class;
  memory.CopyFromEmu(&write_unit_class, input_address, sizeof(write_unit_class));

  hci_write_unit_class_rp reply;
  reply.status = 0x00;

  INFO_LOG_FMT(IOS_WIIMOTE, "Command: HCI_CMD_WRITE_UNIT_CLASS:");
  DEBUG_LOG_FMT(IOS_WIIMOTE, "  COD[0]: {:#04x}", write_unit_class.uclass[0]);
  DEBUG_LOG_FMT(IOS_WIIMOTE, "  COD[1]: {:#04x}", write_unit_class.uclass[1]);
  DEBUG_LOG_FMT(IOS_WIIMOTE, "  COD[2]: {:#04x}", write_unit_class.uclass[2]);

  SendEventCommandComplete(HCI_CMD_WRITE_UNIT_CLASS, &reply, sizeof(hci_write_unit_class_rp));
}

void BluetoothEmuDevice::CommandHostBufferSize(u32 input_address)
{
  auto& system = GetSystem();
  auto& memory = system.GetMemory();

  hci_host_buffer_size_cp host_buffer_size;
  memory.CopyFromEmu(&host_buffer_size, input_address, sizeof(host_buffer_size));

  hci_host_buffer_size_rp reply;
  reply.status = 0x00;

  INFO_LOG_FMT(IOS_WIIMOTE, "Command: HCI_CMD_HOST_BUFFER_SIZE:");
  DEBUG_LOG_FMT(IOS_WIIMOTE, "  max_acl_size: {}", host_buffer_size.max_acl_size);
  DEBUG_LOG_FMT(IOS_WIIMOTE, "  max_sco_size: {}", host_buffer_size.max_sco_size);
  DEBUG_LOG_FMT(IOS_WIIMOTE, "  num_acl_pkts: {}", host_buffer_size.num_acl_pkts);
  DEBUG_LOG_FMT(IOS_WIIMOTE, "  num_sco_pkts: {}", host_buffer_size.num_sco_pkts);

  SendEventCommandComplete(HCI_CMD_HOST_BUFFER_SIZE, &reply, sizeof(hci_host_buffer_size_rp));
}

void BluetoothEmuDevice::CommandWriteLinkSupervisionTimeout(u32 input_address)
{
  auto& system = GetSystem();
  auto& memory = system.GetMemory();

  hci_write_link_supervision_timeout_cp supervision;
  memory.CopyFromEmu(&supervision, input_address, sizeof(supervision));

  // timeout of 0 means timing out is disabled
  INFO_LOG_FMT(IOS_WIIMOTE, "Command: HCI_CMD_WRITE_LINK_SUPERVISION_TIMEOUT");
  DEBUG_LOG_FMT(IOS_WIIMOTE, "  con_handle: {:#06x}", supervision.con_handle);
  DEBUG_LOG_FMT(IOS_WIIMOTE, "  timeout: {:#04x}", supervision.timeout);

  hci_write_link_supervision_timeout_rp reply;
  reply.status = 0x00;
  reply.con_handle = supervision.con_handle;

  SendEventCommandComplete(HCI_CMD_WRITE_LINK_SUPERVISION_TIMEOUT, &reply,
                           sizeof(hci_write_link_supervision_timeout_rp));
}

void BluetoothEmuDevice::CommandWriteInquiryScanType(u32 input_address)
{
  auto& system = GetSystem();
  auto& memory = system.GetMemory();

  hci_write_inquiry_scan_type_cp set_event_filter;
  memory.CopyFromEmu(&set_event_filter, input_address, sizeof(set_event_filter));

  hci_write_inquiry_scan_type_rp reply;
  reply.status = 0x00;

  INFO_LOG_FMT(IOS_WIIMOTE, "Command: HCI_CMD_WRITE_INQUIRY_SCAN_TYPE:");
  DEBUG_LOG_FMT(IOS_WIIMOTE, "  type: {}", set_event_filter.type);

  SendEventCommandComplete(HCI_CMD_WRITE_INQUIRY_SCAN_TYPE, &reply,
                           sizeof(hci_write_inquiry_scan_type_rp));
}

void BluetoothEmuDevice::CommandWriteInquiryMode(u32 input_address)
{
  auto& system = GetSystem();
  auto& memory = system.GetMemory();

  hci_write_inquiry_mode_cp inquiry_mode;
  memory.CopyFromEmu(&inquiry_mode, input_address, sizeof(inquiry_mode));

  hci_write_inquiry_mode_rp reply;
  reply.status = 0x00;

  // TODO: Software seems to set an RSSI mode but our fake inquiries generate standard events.

  static constexpr const char* inquiry_mode_tag[] = {
      "Standard Inquiry Result event format (default)",
      "Inquiry Result format with RSSI",
      "Inquiry Result with RSSI format or Extended Inquiry Result format",
  };
  INFO_LOG_FMT(IOS_WIIMOTE, "Command: HCI_CMD_WRITE_INQUIRY_MODE:");
  DEBUG_LOG_FMT(IOS_WIIMOTE, "  mode: {}", inquiry_mode_tag[inquiry_mode.mode]);

  SendEventCommandComplete(HCI_CMD_WRITE_INQUIRY_MODE, &reply, sizeof(hci_write_inquiry_mode_rp));
}

void BluetoothEmuDevice::CommandWritePageScanType(u32 input_address)
{
  auto& system = GetSystem();
  auto& memory = system.GetMemory();

  hci_write_page_scan_type_cp write_page_scan_type;
  memory.CopyFromEmu(&write_page_scan_type, input_address, sizeof(write_page_scan_type));

  hci_write_page_scan_type_rp reply;
  reply.status = 0x00;

  static constexpr const char* page_scan_type[] = {
      "Mandatory: Standard Scan (default)",
      "Optional: Interlaced Scan",
  };

  INFO_LOG_FMT(IOS_WIIMOTE, "Command: HCI_CMD_WRITE_PAGE_SCAN_TYPE:");
  DEBUG_LOG_FMT(IOS_WIIMOTE, "  type: {}", page_scan_type[write_page_scan_type.type]);

  SendEventCommandComplete(HCI_CMD_WRITE_PAGE_SCAN_TYPE, &reply,
                           sizeof(hci_write_page_scan_type_rp));
}

void BluetoothEmuDevice::CommandReadLocalVer(u32 input_address)
{
  hci_read_local_ver_rp reply;
  reply.status = 0x00;
  reply.hci_version = 0x03;       // HCI version: 1.1
  reply.hci_revision = 0x40a7;    // current revision (?)
  reply.lmp_version = 0x03;       // LMP version: 1.1
  reply.manufacturer = 0x000F;    // manufacturer: reserved for tests
  reply.lmp_subversion = 0x430e;  // LMP subversion

  INFO_LOG_FMT(IOS_WIIMOTE, "Command: HCI_CMD_READ_LOCAL_VER:");
  DEBUG_LOG_FMT(IOS_WIIMOTE, "return:");
  DEBUG_LOG_FMT(IOS_WIIMOTE, "  status:         {}", reply.status);
  DEBUG_LOG_FMT(IOS_WIIMOTE, "  hci_revision:   {}", reply.hci_revision);
  DEBUG_LOG_FMT(IOS_WIIMOTE, "  lmp_version:    {}", reply.lmp_version);
  DEBUG_LOG_FMT(IOS_WIIMOTE, "  manufacturer:   {}", reply.manufacturer);
  DEBUG_LOG_FMT(IOS_WIIMOTE, "  lmp_subversion: {}", reply.lmp_subversion);

  SendEventCommandComplete(HCI_CMD_READ_LOCAL_VER, &reply, sizeof(hci_read_local_ver_rp));
}

void BluetoothEmuDevice::CommandReadLocalFeatures(u32 input_address)
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

  INFO_LOG_FMT(IOS_WIIMOTE, "Command: HCI_CMD_READ_LOCAL_FEATURES:");
  DEBUG_LOG_FMT(IOS_WIIMOTE, "return:");
  DEBUG_LOG_FMT(IOS_WIIMOTE, "  features: {:02x}:{:02x}:{:02x}:{:02x}:{:02x}:{:02x}:{:02x}:{:02x}",
                reply.features[0], reply.features[1], reply.features[2], reply.features[3],
                reply.features[4], reply.features[5], reply.features[6], reply.features[7]);

  SendEventCommandComplete(HCI_CMD_READ_LOCAL_FEATURES, &reply, sizeof(hci_read_local_features_rp));
}

void BluetoothEmuDevice::CommandReadBufferSize(u32 input_address)
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

  INFO_LOG_FMT(IOS_WIIMOTE, "Command: HCI_CMD_READ_BUFFER_SIZE:");
  DEBUG_LOG_FMT(IOS_WIIMOTE, "return:");
  DEBUG_LOG_FMT(IOS_WIIMOTE, "  max_acl_size: {}", reply.max_acl_size);
  DEBUG_LOG_FMT(IOS_WIIMOTE, "  num_acl_pkts: {}", reply.num_acl_pkts);
  DEBUG_LOG_FMT(IOS_WIIMOTE, "  max_sco_size: {}", reply.max_sco_size);
  DEBUG_LOG_FMT(IOS_WIIMOTE, "  num_sco_pkts: {}", reply.num_sco_pkts);

  SendEventCommandComplete(HCI_CMD_READ_BUFFER_SIZE, &reply, sizeof(hci_read_buffer_size_rp));
}

void BluetoothEmuDevice::CommandReadBDAdrr(u32 input_address)
{
  hci_read_bdaddr_rp reply;
  reply.status = 0x00;
  reply.bdaddr = m_controller_bd;

  INFO_LOG_FMT(IOS_WIIMOTE, "Command: HCI_CMD_READ_BDADDR:");
  DEBUG_LOG_FMT(IOS_WIIMOTE, "return:");
  DEBUG_LOG_FMT(IOS_WIIMOTE, "  bd: {:02x}:{:02x}:{:02x}:{:02x}:{:02x}:{:02x}", reply.bdaddr[0],
                reply.bdaddr[1], reply.bdaddr[2], reply.bdaddr[3], reply.bdaddr[4],
                reply.bdaddr[5]);

  SendEventCommandComplete(HCI_CMD_READ_BDADDR, &reply, sizeof(hci_read_bdaddr_rp));
}

void BluetoothEmuDevice::CommandVendorSpecific_FC4F(u32 input_address, u32 size)
{
  // callstack...
  // BTM_VendorSpecificCommad()
  // WUDiRemovePatch()
  // WUDiAppendRuntimePatch()
  // WUDiGetFirmwareVersion()
  // WUDiStackSetupComplete()

  hci_status_rp reply;
  reply.status = 0x00;

  INFO_LOG_FMT(IOS_WIIMOTE, "Command: CommandVendorSpecific_FC4F: (callstack WUDiRemovePatch)");
  DEBUG_LOG_FMT(IOS_WIIMOTE, "Input (size {:#x}):", size);

  Dolphin_Debugger::PrintDataBuffer(GetSystem(), Common::Log::LogType::IOS_WIIMOTE, input_address,
                                    size, "Data: ");

  SendEventCommandComplete(0xFC4F, &reply, sizeof(hci_status_rp));
}

void BluetoothEmuDevice::CommandVendorSpecific_FC4C(u32 input_address, u32 size)
{
  hci_status_rp reply;
  reply.status = 0x00;

  DEBUG_LOG_FMT(IOS_WIIMOTE, "Command: CommandVendorSpecific_FC4C:");
  DEBUG_LOG_FMT(IOS_WIIMOTE, "Input (size {:#x}):", size);
  Dolphin_Debugger::PrintDataBuffer(GetSystem(), Common::Log::LogType::IOS_WIIMOTE, input_address,
                                    size, "Data: ");

  SendEventCommandComplete(0xFC4C, &reply, sizeof(hci_status_rp));
}

WiimoteDevice* BluetoothEmuDevice::AccessWiimoteByIndex(std::size_t index)
{
  if (index < MAX_BBMOTES)
    return m_wiimotes[index].get();

  return nullptr;
}

u16 BluetoothEmuDevice::GetConnectionHandle(const bdaddr_t& address)
{
  // Handles are normally generated per connection but HLE allows fixed values for each remote.
  return 0x100 + address.back();
}

u32 BluetoothEmuDevice::GetWiimoteNumberFromConnectionHandle(u16 connection_handle)
{
  // Fixed handle values are generated in GetConnectionHandle.
  return connection_handle & 0xff;
}

WiimoteDevice* BluetoothEmuDevice::AccessWiimote(const bdaddr_t& address)
{
  // Fixed bluetooth addresses are generated in WiimoteDevice::WiimoteDevice.
  const auto wiimote = AccessWiimoteByIndex(address.back());

  if (wiimote && wiimote->GetBD() == address)
    return wiimote;

  return nullptr;
}

WiimoteDevice* BluetoothEmuDevice::AccessWiimote(u16 connection_handle)
{
  const auto wiimote =
      AccessWiimoteByIndex(GetWiimoteNumberFromConnectionHandle(connection_handle));

  if (wiimote)
    return wiimote;

  ERROR_LOG_FMT(IOS_WIIMOTE, "Can't find Wiimote by connection handle {:02x}", connection_handle);
  PanicAlertFmtT("Can't find Wii Remote by connection handle {0:02x}", connection_handle);

  return nullptr;
}

}  // namespace IOS::HLE
