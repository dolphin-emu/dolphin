// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cstdint>
#include <map>
#include <string>

#include "Common/CommonTypes.h"
#include "Core/HW/WiimoteCommon/WiimoteHid.h"
#include "Core/IOS/USB/Bluetooth/hci.h"

class PointerWrap;

namespace WiimoteEmu
{
struct DesiredWiimoteState;
}

namespace IOS::HLE
{
class BluetoothEmuDevice;

class WiimoteDevice
{
public:
  using ClassType = std::array<u8, HCI_CLASS_SIZE>;
  using FeaturesType = std::array<u8, HCI_FEATURES_SIZE>;
  using LinkKeyType = std::array<u8, HCI_KEY_SIZE>;

  WiimoteDevice(BluetoothEmuDevice* host, bdaddr_t bd, unsigned int hid_source_number);
  ~WiimoteDevice();

  WiimoteDevice(const WiimoteDevice&) = delete;
  WiimoteDevice& operator=(const WiimoteDevice&) = delete;
  WiimoteDevice(WiimoteDevice&&) = delete;
  WiimoteDevice& operator=(WiimoteDevice&&) = delete;

  void Reset();

  // Called every BluetoothEmu::Update.
  void Update();

  // Called every ~200hz.
  enum class NextUpdateInputCall
  {
    None,
    Activate,
    Update
  };
  NextUpdateInputCall PrepareInput(WiimoteEmu::DesiredWiimoteState* wiimote_state);
  void UpdateInput(NextUpdateInputCall next_call,
                   const WiimoteEmu::DesiredWiimoteState& wiimote_state);

  void DoState(PointerWrap& p);

  bool IsInquiryScanEnabled() const;
  bool IsPageScanEnabled() const;

  u32 GetNumber() const;

  bool IsSourceValid() const;
  bool IsConnected() const;

  // User-initiated. Produces UI messages.
  void Activate(bool ready);

  // From CPU
  void ExecuteL2capCmd(u8* ptr, u32 size);
  // From Wiimote
  void InterruptDataInputCallback(u8 hid_type, const u8* data, u32 size);

  bool EventConnectionAccept();
  bool EventConnectionRequest();
  void EventDisconnect(u8 reason);

  // nullptr may be passed to disable the remote.
  void SetSource(WiimoteCommon::HIDWiimote*);

  const bdaddr_t& GetBD() const { return m_bd; }
  const char* GetName() const { return m_name.c_str(); }
  u8 GetLMPVersion() const { return m_lmp_version; }
  u16 GetLMPSubVersion() const { return m_lmp_subversion; }
  // Broadcom Corporation
  u16 GetManufactorID() const { return 0x000F; }
  const ClassType& GetClass() const { return m_class; }
  const FeaturesType& GetFeatures() const { return m_features; }
  const LinkKeyType& GetLinkKey() const { return m_link_key; }

private:
  enum class BasebandState
  {
    Inactive,
    RequestConnection,
    Complete,
  };

  enum class HIDState
  {
    Inactive,
    Linking,
  };

  struct SChannel
  {
    enum class State
    {
      Inactive,
      ConfigurationPending,
      Complete,
    };

    SChannel();

    bool IsAccepted() const;
    bool IsRemoteConfigured() const;
    bool IsComplete() const;

    State state = State::Inactive;
    u16 psm;
    u16 remote_cid;
    u16 remote_mtu = 0;
  };

  using ChannelMap = std::map<u16, SChannel>;

  BluetoothEmuDevice* m_host;
  WiimoteCommon::HIDWiimote* m_hid_source = nullptr;

  // State to save:
  BasebandState m_baseband_state = BasebandState::Inactive;
  HIDState m_hid_state = HIDState::Inactive;
  bdaddr_t m_bd;
  ClassType m_class;
  FeaturesType m_features;
  u8 m_lmp_version;
  u16 m_lmp_subversion;
  LinkKeyType m_link_key;
  std::string m_name;
  ChannelMap m_channels;
  u8 m_connection_request_counter = 0;

  void SetBasebandState(BasebandState);

  const SChannel* FindChannelWithPSM(u16 psm) const;
  SChannel* FindChannelWithPSM(u16 psm);

  bool LinkChannel(u16 psm);
  u16 GenerateChannelID() const;

  bool DoesChannelExist(u16 scid) const { return m_channels.count(scid) != 0; }
  void SendCommandToACL(u8 ident, u8 code, u8 command_length, u8* command_data);

  void SignalChannel(u8* data, u32 size);

  void SendConnectionRequest(u16 psm);
  void SendConfigurationRequest(u16 cid, u16 mtu, u16 flush_time_out);

  void ReceiveConnectionReq(u8 ident, u8* data, u32 size);
  void ReceiveConnectionResponse(u8 ident, u8* data, u32 size);
  void ReceiveDisconnectionReq(u8 ident, u8* data, u32 size);
  void ReceiveConfigurationReq(u8 ident, u8* data, u32 size);
  void ReceiveConfigurationResponse(u8 ident, u8* data, u32 size);

  void HandleSDP(u16 cid, u8* data, u32 size);
  void SDPSendServiceSearchResponse(u16 cid, u16 transaction_id, u8* service_search_pattern,
                                    u16 maximum_service_record_count);

  void SDPSendServiceAttributeResponse(u16 cid, u16 transaction_id, u32 service_handle,
                                       u16 start_attr_id, u16 end_attr_id,
                                       u16 maximum_attribute_byte_count, u8* continuation_state);

  std::string GetDisplayName();
};
}  // namespace IOS::HLE
