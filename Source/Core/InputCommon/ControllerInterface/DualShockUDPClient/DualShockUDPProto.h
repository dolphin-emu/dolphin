// Copyright 2019 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <cstring>
#include <optional>

#include "Common/CommonTypes.h"
#include "Common/Hash.h"

namespace ciface::DualShockUDPClient::Proto
{
// CemuHook DualShockUDP protocol implementation using UdpServer.cs from
// https://github.com/Ryochan7/DS4Windows as documentation.
//
// WARNING: Little endian host assumed

constexpr u16 CEMUHOOK_PROTOCOL_VERSION = 1001;

constexpr int PORT_COUNT = 4;

constexpr std::array<u8, 4> CLIENT{'D', 'S', 'U', 'C'};
constexpr std::array<u8, 4> SERVER{'D', 'S', 'U', 'S'};

#pragma pack(push, 1)

enum class DsState : u8
{
  Disconnected = 0x00,
  Reserved = 0x01,
  Connected = 0x02
};

enum class DsConnection : u8
{
  None = 0x00,
  Usb = 0x01,
  Bluetooth = 0x02
};

enum class DsModel : u8
{
  None = 0,
  PartialGyro = 1,
  FullGyro = 2,
  Generic = 3
};

enum class DsBattery : u8
{
  None = 0x00,
  Dying = 0x01,
  Low = 0x02,
  Medium = 0x03,
  High = 0x04,
  Full = 0x05,
  Charging = 0xEE,
  Charged = 0xEF
};

enum RegisterFlags : u8
{
  AllPads = 0x00,
  PadID = 0x01,
  PadMACAdddress = 0x02
};

struct MessageHeader
{
  std::array<u8, 4> source;
  u16 protocol_version;
  u16 message_length;  // actually message size minus header size
  u32 crc32;
  u32 source_uid;
};

struct Touch
{
  u8 active;
  u8 id;
  s16 x;
  s16 y;
};

namespace MessageType
{
struct VersionRequest
{
  static constexpr auto FROM = CLIENT;
  static constexpr auto TYPE = 0x100000U;
  MessageHeader header;
  u32 message_type;
};

struct VersionResponse
{
  static constexpr auto FROM = SERVER;
  static constexpr auto TYPE = 0x100000U;
  MessageHeader header;
  u32 message_type;
  u16 max_protocol_version;
  std::array<u8, 2> padding;
};

struct ListPorts
{
  static constexpr auto FROM = CLIENT;
  static constexpr auto TYPE = 0x100001U;
  MessageHeader header;
  u32 message_type;
  u32 pad_request_count;
  std::array<u8, 4> pad_ids;
};

struct PortInfo
{
  static constexpr auto FROM = SERVER;
  static constexpr auto TYPE = 0x100001U;
  MessageHeader header;
  u32 message_type;
  u8 pad_id;
  DsState pad_state;
  DsModel model;
  DsConnection connection_type;
  std::array<u8, 6> pad_mac_address;
  DsBattery battery_status;
  u8 padding;
};

struct PadDataRequest
{
  static constexpr auto FROM = CLIENT;
  static constexpr auto TYPE = 0x100002U;
  MessageHeader header;
  u32 message_type;
  RegisterFlags register_flags;
  u8 pad_id_to_register;
  std::array<u8, 6> mac_address_to_register;
};

struct PadDataResponse
{
  static constexpr auto FROM = SERVER;
  static constexpr auto TYPE = 0x100002U;
  MessageHeader header;
  u32 message_type;
  u8 pad_id;
  DsState pad_state;
  DsModel model;
  DsConnection connection_type;
  std::array<u8, 6> pad_mac_address;
  DsBattery battery_status;
  u8 active;
  u32 hid_packet_counter;
  u8 button_states1;
  u8 button_states2;
  u8 button_ps;
  u8 button_touch;
  u8 left_stick_x;
  u8 left_stick_y_inverted;
  u8 right_stick_x;
  u8 right_stick_y_inverted;
  u8 button_dpad_left_analog;
  u8 button_dpad_down_analog;
  u8 button_dpad_right_analog;
  u8 button_dpad_up_analog;
  u8 button_square_analog;
  u8 button_cross_analog;
  u8 button_circle_analog;
  u8 button_triangle_analog;
  u8 button_r1_analog;
  u8 button_l1_analog;
  u8 trigger_r2;
  u8 trigger_l2;
  Touch touch1;
  Touch touch2;
  u64 accelerometer_timestamp_us;
  float accelerometer_x_g;
  float accelerometer_y_g;
  float accelerometer_z_g;
  float gyro_pitch_deg_s;
  float gyro_yaw_deg_s;
  float gyro_roll_deg_s;
};

struct FromServer
{
  union
  {
    struct
    {
      MessageHeader header;
      u32 message_type;
    };
    MessageType::VersionResponse version_response;
    MessageType::PortInfo port_info;
    MessageType::PadDataResponse pad_data_response;
  };
};

struct FromClient
{
  union
  {
    struct
    {
      MessageHeader header;
      u32 message_type;
    };
    MessageType::VersionRequest version_request;
    MessageType::ListPorts list_ports;
    MessageType::PadDataRequest pad_data_request;
  };
};
}  // namespace MessageType

template <typename MsgType>
struct Message
{
  Message() = default;

  explicit Message(u32 source_uid)
  {
    m_message.header.source = MsgType::FROM;
    m_message.header.protocol_version = CEMUHOOK_PROTOCOL_VERSION;
    m_message.header.message_length = sizeof(*this) - sizeof(m_message.header);
    m_message.header.source_uid = source_uid;
    m_message.message_type = MsgType::TYPE;
  }

  void Finish()
  {
    m_message.header.crc32 =
        Common::ComputeCRC32(reinterpret_cast<const u8*>(&m_message), sizeof(m_message));
  }

  template <class ToMsgType>
  std::optional<ToMsgType> CheckAndCastTo()
  {
    const u32 crc32_in_header = m_message.header.crc32;
    // zero out the crc32 in the packet once we got it since that's whats needed for calculation
    m_message.header.crc32 = 0;
    const u32 crc32_calculated =
        Common::ComputeCRC32(reinterpret_cast<const u8*>(&m_message), sizeof(ToMsgType));
    if (crc32_in_header != crc32_calculated)
    {
      NOTICE_LOG_FMT(
          CONTROLLERINTERFACE,
          "DualShockUDPClient Received message with bad CRC in header: got {:08x}, expected {:08x}",
          crc32_in_header, crc32_calculated);
      return std::nullopt;
    }
    if (m_message.header.protocol_version > CEMUHOOK_PROTOCOL_VERSION)
      return std::nullopt;
    if (m_message.header.source != ToMsgType::FROM)
      return std::nullopt;
    if (m_message.message_type != ToMsgType::TYPE)
      return std::nullopt;
    if (m_message.header.message_length + sizeof(m_message.header) > sizeof(ToMsgType))
      return std::nullopt;

    ToMsgType tomsg;
    memcpy(&tomsg, &m_message, sizeof(tomsg));
    return tomsg;
  }

  MsgType m_message{};
};

#pragma pack(pop)
}  // namespace ciface::DualShockUDPClient::Proto
