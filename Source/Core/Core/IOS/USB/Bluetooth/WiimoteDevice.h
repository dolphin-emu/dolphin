// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <cstdint>
#include <map>
#include <string>

#include "Common/CommonTypes.h"
#include "Core/IOS/USB/Bluetooth/hci.h"

class PointerWrap;

namespace IOS::HLE
{
namespace Device
{
class BluetoothEmu;
}

class WiimoteDevice
{
public:
  WiimoteDevice(Device::BluetoothEmu* host, int number, bdaddr_t bd, bool ready = false);

  void DoState(PointerWrap& p);

  // ugly Host handling....
  // we really have to clean all this code

  bool IsConnected() const { return m_ConnectionState == ConnectionState::Complete; }
  bool IsInactive() const { return m_ConnectionState == ConnectionState::Inactive; }
  bool LinkChannel();
  void ResetChannels();
  void Activate(bool ready);
  void ExecuteL2capCmd(u8* ptr, u32 size);                      // From CPU
  void ReceiveL2capData(u16 scid, const void* data, u32 size);  // From Wiimote

  void EventConnectionAccepted();
  void EventDisconnect();
  bool EventPagingChanged(u8 page_mode) const;

  const bdaddr_t& GetBD() const { return m_BD; }
  const uint8_t* GetClass() const { return uclass; }
  u16 GetConnectionHandle() const { return m_ConnectionHandle; }
  const u8* GetFeatures() const { return features; }
  const char* GetName() const { return m_Name.c_str(); }
  u8 GetLMPVersion() const { return lmp_version; }
  u16 GetLMPSubVersion() const { return lmp_subversion; }
  u16 GetManufactorID() const { return 0x000F; }  // Broadcom Corporation
  const u8* GetLinkKey() const { return m_LinkKey; }

private:
  enum class ConnectionState
  {
    Inactive = -1,
    Ready,
    Linking,
    Complete
  };

  struct HIDChannelState
  {
    bool connected = false;
    bool connected_wait = false;
    bool config = false;
    bool config_wait = false;
  };

  ConnectionState m_ConnectionState;

  HIDChannelState m_hid_control_channel;
  HIDChannelState m_hid_interrupt_channel;

  // STATE_TO_SAVE
  bdaddr_t m_BD;
  u16 m_ConnectionHandle;
  uint8_t uclass[HCI_CLASS_SIZE];
  u8 features[HCI_FEATURES_SIZE];
  u8 lmp_version;
  u16 lmp_subversion;
  u8 m_LinkKey[HCI_KEY_SIZE];
  std::string m_Name;
  Device::BluetoothEmu* m_pHost;

  struct SChannel
  {
    u16 SCID;
    u16 DCID;
    u16 PSM;

    u16 MTU;
    u16 FlushTimeOut;
  };

  typedef std::map<u32, SChannel> CChannelMap;
  CChannelMap m_Channel;

  bool DoesChannelExist(u16 scid) const { return m_Channel.find(scid) != m_Channel.end(); }
  void SendCommandToACL(u8 ident, u8 code, u8 command_length, u8* command_data);

  void SignalChannel(u8* data, u32 size);

  void SendConnectionRequest(u16 scid, u16 psm);
  void SendConfigurationRequest(u16 scid, u16 mtu = 0, u16 flush_time_out = 0);
  void SendDisconnectRequest(u16 scid);

  void ReceiveConnectionReq(u8 ident, u8* data, u32 size);
  void ReceiveConnectionResponse(u8 ident, u8* data, u32 size);
  void ReceiveDisconnectionReq(u8 ident, u8* data, u32 size);
  void ReceiveConfigurationReq(u8 ident, u8* data, u32 size);
  void ReceiveConfigurationResponse(u8 ident, u8* data, u32 size);

  // some new ugly stuff
  // should be inside the plugin
  void HandleSDP(u16 cid, u8* data, u32 size);
  void SDPSendServiceSearchResponse(u16 cid, u16 transaction_id, u8* service_search_pattern,
                                    u16 maximum_service_record_count);

  void SDPSendServiceAttributeResponse(u16 cid, u16 transaction_id, u32 service_handle,
                                       u16 start_attr_id, u16 end_attr_id,
                                       u16 maximum_attribute_byte_count, u8* continuation_state);
};
}  // namespace IOS::HLE
