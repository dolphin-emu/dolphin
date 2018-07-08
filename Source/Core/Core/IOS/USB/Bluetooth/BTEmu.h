// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <cstddef>
#include <deque>
#include <memory>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"
#include "Core/HW/Wiimote.h"
#include "Core/IOS/Device.h"
#include "Core/IOS/IOS.h"
#include "Core/IOS/USB/Bluetooth/BTBase.h"
#include "Core/IOS/USB/Bluetooth/WiimoteDevice.h"
#include "Core/IOS/USB/Bluetooth/hci.h"
#include "Core/IOS/USB/USBV0.h"

class PointerWrap;

namespace IOS::HLE
{
struct SQueuedEvent
{
  u8 buffer[1024] = {};
  u32 size = 0;
  u16 connection_handle = 0;

  SQueuedEvent(u32 size_, u16 handle);
  SQueuedEvent() = default;
};

namespace Device
{
// Important to remember that this class is for /dev/usb/oh1/57e/305 ONLY
// /dev/usb/oh1 -> internal usb bus
// 57e/305 -> VendorID/ProductID of device on usb bus
// This device is ONLY the internal Bluetooth module (based on BCM2045 chip)
class BluetoothEmu final : public BluetoothBase
{
public:
  BluetoothEmu(Kernel& ios, const std::string& device_name);

  virtual ~BluetoothEmu();

  IPCCommandResult Close(u32 fd) override;
  IPCCommandResult IOCtlV(const IOCtlVRequest& request) override;

  void Update() override;

  // Send ACL data back to Bluetooth stack
  void SendACLPacket(u16 connection_handle, const u8* data, u32 size);

  bool RemoteDisconnect(u16 connection_handle);

  WiimoteDevice* AccessWiimoteByIndex(std::size_t index);

  void DoState(PointerWrap& p) override;

private:
  std::vector<WiimoteDevice> m_wiimotes;

  bdaddr_t m_controller_bd{{0x11, 0x02, 0x19, 0x79, 0x00, 0xff}};

  // this is used to trigger connecting via ACL
  u8 m_scan_enable = 0;

  std::unique_ptr<USB::V0CtrlMessage> m_ctrl_setup;
  std::unique_ptr<USB::V0IntrMessage> m_hci_endpoint;
  std::unique_ptr<USB::V0BulkMessage> m_acl_endpoint;
  std::deque<SQueuedEvent> m_event_queue;

  class ACLPool
  {
  public:
    explicit ACLPool(Kernel& ios) : m_ios(ios), m_queue() {}
    void Store(const u8* data, const u16 size, const u16 conn_handle);

    void WriteToEndpoint(const USB::V0BulkMessage& endpoint);

    bool IsEmpty() const { return m_queue.empty(); }
    // For SaveStates
    void DoState(PointerWrap& p) { p.Do(m_queue); }

  private:
    struct Packet
    {
      u8 data[ACL_PKT_SIZE];
      u16 size;
      u16 conn_handle;
    };

    Kernel& m_ios;
    std::deque<Packet> m_queue;
  } m_acl_pool{m_ios};

  u32 m_packet_count[MAX_BBMOTES] = {};
  u64 m_last_ticks = 0;

  WiimoteDevice* AccessWiimote(const bdaddr_t& address);
  WiimoteDevice* AccessWiimote(u16 connection_handle);

  // Send ACL data to a device (wiimote)
  void IncDataPacket(u16 connection_handle);
  void SendToDevice(u16 connection_handle, u8* data, u32 size);

  // Events
  void AddEventToQueue(const SQueuedEvent& event);
  bool SendEventCommandStatus(u16 opcode);
  void SendEventCommandComplete(u16 opcode, const void* data, u32 data_size);
  bool SendEventInquiryResponse();
  bool SendEventInquiryComplete();
  bool SendEventRemoteNameReq(const bdaddr_t& bd);
  bool SendEventRequestConnection(const WiimoteDevice& wiimote);
  bool SendEventConnectionComplete(const bdaddr_t& bd);
  bool SendEventReadClockOffsetComplete(u16 connection_handle);
  bool SendEventConPacketTypeChange(u16 connection_handle, u16 packet_type);
  bool SendEventReadRemoteVerInfo(u16 connection_handle);
  bool SendEventReadRemoteFeatures(u16 connection_handle);
  bool SendEventRoleChange(bdaddr_t bd, bool master);
  bool SendEventNumberOfCompletedPackets();
  bool SendEventAuthenticationCompleted(u16 connection_handle);
  bool SendEventModeChange(u16 connection_handle, u8 mode, u16 value);
  bool SendEventDisconnect(u16 connection_handle, u8 reason);
  bool SendEventRequestLinkKey(const bdaddr_t& bd);
  bool SendEventLinkKeyNotification(const u8 num_to_send);

  // Execute HCI Message
  void ExecuteHCICommandMessage(const USB::V0CtrlMessage& ctrl_message);

  // OGF 0x01 - Link control commands and return parameters
  void CommandWriteInquiryMode(const u8* input);
  void CommandWritePageScanType(const u8* input);
  void CommandHostBufferSize(const u8* input);
  void CommandInquiryCancel(const u8* input);
  void CommandRemoteNameReq(const u8* input);
  void CommandCreateCon(const u8* input);
  void CommandAcceptCon(const u8* input);
  void CommandReadClockOffset(const u8* input);
  void CommandReadRemoteVerInfo(const u8* input);
  void CommandReadRemoteFeatures(const u8* input);
  void CommandAuthenticationRequested(const u8* input);
  void CommandInquiry(const u8* input);
  void CommandDisconnect(const u8* input);
  void CommandLinkKeyNegRep(const u8* input);
  void CommandLinkKeyRep(const u8* input);
  void CommandDeleteStoredLinkKey(const u8* input);
  void CommandChangeConPacketType(const u8* input);

  // OGF 0x02 - Link policy commands and return parameters
  void CommandWriteLinkPolicy(const u8* input);
  void CommandSniffMode(const u8* input);

  // OGF 0x03 - Host Controller and Baseband commands and return parameters
  void CommandReset(const u8* input);
  void CommandWriteLocalName(const u8* input);
  void CommandWritePageTimeOut(const u8* input);
  void CommandWriteScanEnable(const u8* input);
  void CommandWriteUnitClass(const u8* input);
  void CommandReadStoredLinkKey(const u8* input);
  void CommandWritePinType(const u8* input);
  void CommandSetEventFilter(const u8* input);
  void CommandWriteInquiryScanType(const u8* input);
  void CommandWriteLinkSupervisionTimeout(const u8* input);

  // OGF 0x04 - Informational commands and return parameters
  void CommandReadBufferSize(const u8* input);
  void CommandReadLocalVer(const u8* input);
  void CommandReadLocalFeatures(const u8* input);
  void CommandReadBDAdrr(const u8* input);

  // OGF 0x3F - Vendor specific
  void CommandVendorSpecific_FC4C(const u8* input, u32 size);
  void CommandVendorSpecific_FC4F(const u8* input, u32 size);

  static void DisplayDisconnectMessage(int wiimote_number, int reason);

#pragma pack(push, 1)
#define CONF_PAD_MAX_REGISTERED 10

  struct ConfPadDevice
  {
    u8 bdaddr[6];
    char name[0x40];
  };

  struct ConfPads
  {
    u8 num_registered;
    ConfPadDevice registered[CONF_PAD_MAX_REGISTERED];
    ConfPadDevice active[MAX_BBMOTES];
    ConfPadDevice unknown;
  };
#pragma pack(pop)
};
}  // namespace Device
}  // namespace IOS::HLE
