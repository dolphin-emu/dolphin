// Copyright 2021 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoCommon/FreeLookUDPServer.h"

#include "Common/MathUtil.h"
#include "VideoCommon/FreeLookUDPSpec.h"

namespace
{
template <typename T>
T GetMessage(const u8*& data)
{
  T msg;
  memcpy(&msg, data, sizeof(T));
  data += sizeof(T);
  return msg;
}
}  // namespace

FreeLookUDPServer::FreeLookUDPServer(std::string address, u16 port)
{
  m_socket.setBlocking(false);
  m_socket.bind(port, address);
}

void FreeLookUDPServer::Update()
{
  std::array<u8, sf::UdpSocket::MaxDatagramSize> buffer;

  sf::IpAddress sender;
  unsigned short port;
  std::size_t bytes_received;
  while (m_socket.receive(buffer.data(), buffer.size(), bytes_received, sender, port) ==
         sf::Socket::Done)
  {
    ProcessReceivedData(buffer.data(), bytes_received);
  }
}

void FreeLookUDPServer::ProcessReceivedData(const u8* data, std::size_t packet_size)
{
  const std::size_t header_size = sizeof(PacketHeader);
  if (packet_size < header_size)
    return;

  PacketHeader header;
  memcpy(&header, data, header_size);

  if (header.version != FREE_LOOK_PROTO_VERSION)
    return;

  packet_size -= header_size;
  data += header_size;

  if (header.payload_size > packet_size)
    return;

  for (u8 i = 0; i < header.message_count; i++)
  {
    switch (header.msg_type)
    {
    case MessageTransformByQuaternionPosition::msg_type:
    {
      auto msg = GetMessage<MessageTransformByQuaternionPosition>(data);
      m_view = Common::Matrix44::Translate(Common::Vec3{static_cast<float>(msg.posx),
                                                        static_cast<float>(msg.posy),
                                                        static_cast<float>(msg.posz)}) *
               Common::Matrix44::FromQuaternion(
                   Common::Quaternion(static_cast<float>(msg.qw), static_cast<float>(msg.qx),
                                      static_cast<float>(msg.qy), static_cast<float>(msg.qz)));
    }
    break;

    case MessageTransformByEulerPosition::msg_type:
    {
      auto msg = GetMessage<MessageTransformByEulerPosition>(data);

      const double divisor = 360.0 / MathUtil::TAU;
      const double pitch = msg.pitch / divisor;
      const double yaw = msg.yaw / divisor;
      const double roll = msg.roll / divisor;
      const Common::Quaternion rotation = Common::Quaternion::RotateXYZ(Common::Vec3{
          static_cast<float>(pitch), static_cast<float>(yaw), static_cast<float>(roll)});

      m_view = Common::Matrix44::Translate(Common::Vec3{static_cast<float>(msg.posx),
                                                        static_cast<float>(msg.posy),
                                                        static_cast<float>(msg.posz)}) *
               Common::Matrix44::FromQuaternion(rotation);
    }
    break;

    case MessageTransformByMatrix::msg_type:
    {
      auto msg = GetMessage<MessageTransformByMatrix>(data);
      for (u8 j = 0; j < 16; j++)
      {
        m_view.data[j] = msg.data[j];
      }
    }
    break;

    case MessageFieldOfView::msg_type:
    {
      auto msg = GetMessage<MessageFieldOfView>(data);
      m_fov_multiplier.x = msg.horizontal_multiplier;
      m_fov_multiplier.y = msg.vertical_multiplier;
    }
    break;
    }
  }
}
