// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <SFML/Network.hpp>

#include "Common/BitUtils.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Core/HW/EXI/BBA/BuiltIn.h"
#include "Core/HW/EXI/EXI_Device.h"
#include "Core/HW/EXI/EXI_DeviceEthernet.h"

namespace ExpansionInterface
{
u64 GetTickCountStd()
{
  using namespace std::chrono;
  return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

bool CEXIETHERNET::BuiltInBBAInterface::Activate()
{
  if (IsActivated())
    return true;

  m_active = true;
  for (auto& buf : m_queue_data)
    buf.reserve(2048);
  m_fake_mac = Common::GenerateMacAddress(Common::MACConsumer::BBA);

  // Workaround to get the host IP (might not be accurate)
  const u32 ip = m_local_ip.empty() ? sf::IpAddress::getLocalAddress().toInteger() :
                                      sf::IpAddress(m_local_ip).toInteger();
  m_current_ip = htonl(ip);
  m_router_ip = (m_current_ip & 0xFFFFFF) | 0x01000000;

  // clear all ref
  for (auto& ref : network_ref)
  {
    ref.ip = 0;
  }

  return RecvInit();
}

void CEXIETHERNET::BuiltInBBAInterface::Deactivate()
{
  // Is the BBA Active? If not skip shutdown
  if (!IsActivated())
    return;
  // Signal read thread to exit.
  m_read_enabled.Clear();
  m_read_thread_shutdown.Set();
  m_active = false;

  // kill all active socket
  for (auto& ref : network_ref)
  {
    if (ref.ip != 0)
    {
      ref.type == IPPROTO_TCP ? ref.tcp_socket.disconnect() : ref.udp_socket.unbind();
    }
    ref.ip = 0;
  }

  // Wait for read thread to exit.
  if (m_read_thread.joinable())
    m_read_thread.join();
}

bool CEXIETHERNET::BuiltInBBAInterface::IsActivated()
{
  return m_active;
}

void CEXIETHERNET::BuiltInBBAInterface::WriteToQueue(const std::vector<u8>& data)
{
  m_queue_data[m_queue_write] = data;
  const u8 next_write_index = (m_queue_write + 1) & 15;
  if (next_write_index != m_queue_read)
    m_queue_write = next_write_index;
}

void CEXIETHERNET::BuiltInBBAInterface::HandleARP(const Common::ARPPacket& packet)
{
  const auto& [hwdata, arpdata] = packet;
  const Common::MACAddress bba_mac =
      Common::BitCastPtr<Common::MACAddress>(&m_eth_ref->mBbaMem[BBA_NAFR_PAR0]);
  Common::ARPPacket response(bba_mac, m_fake_mac);

  if (arpdata.target_ip == m_current_ip)
  {
    // game asked for himself, reply with his mac address
    response.arp_header = Common::ARPHeader(arpdata.target_ip, bba_mac, m_current_ip, bba_mac);
  }
  else
  {
    response.arp_header = Common::ARPHeader(arpdata.target_ip, m_fake_mac, m_current_ip, bba_mac);
  }

  WriteToQueue(response.Build());
}

void CEXIETHERNET::BuiltInBBAInterface::HandleDHCP(const Common::UDPPacket& packet)
{
  const auto& [hwdata, ip, udp_header, ip_options, data] = packet;
  const Common::DHCPPacket dhcp(packet.data);
  const Common::DHCPBody& request = dhcp.body;
  sockaddr_in from;
  sockaddr_in to;
  from.sin_addr.s_addr = m_router_ip;
  from.sin_family = IPPROTO_UDP;
  from.sin_port = htons(67);
  to.sin_addr.s_addr = m_current_ip;
  to.sin_family = IPPROTO_UDP;
  to.sin_port = udp_header.source_port;

  const u8* router_ip_ptr = reinterpret_cast<const u8*>(&m_router_ip);
  const std::vector<u8> ip_part(router_ip_ptr, router_ip_ptr + sizeof(m_router_ip));

  const std::vector<u8> timeout_24h = {0, 1, 0x51, 0x80};

  const Common::MACAddress bba_mac =
      Common::BitCastPtr<Common::MACAddress>(&m_eth_ref->mBbaMem[BBA_NAFR_PAR0]);
  Common::DHCPPacket reply;
  reply.body = Common::DHCPBody(request.transaction_id, bba_mac, m_current_ip, m_router_ip);

  // options
  // send our emulated lan settings

  (dhcp.options.size() == 0 || dhcp.options[0].size() < 2 || dhcp.options[0].at(2) == 1) ?
      reply.AddOption(53, {2}) :  // default, send a suggestion
      reply.AddOption(53, {5});
  reply.AddOption(54, ip_part);                                    // dhcp server ip
  reply.AddOption(51, timeout_24h);                                // lease time 24h
  reply.AddOption(58, timeout_24h);                                // renewal time
  reply.AddOption(59, timeout_24h);                                // rebind time
  reply.AddOption(1, {255, 255, 255, 0});                          // submask
  reply.AddOption(28, {ip_part[0], ip_part[1], ip_part[2], 255});  // broadcast ip
  reply.AddOption(6, ip_part);                                     // dns server
  reply.AddOption(15, {0x6c, 0x61, 0x6e});                         // domain name "lan"
  reply.AddOption(3, ip_part);                                     // router ip
  reply.AddOption(255, {});                                        // end

  Common::UDPPacket response(bba_mac, m_fake_mac, from, to, reply.Build());

  WriteToQueue(response.Build());
}

StackRef* CEXIETHERNET::BuiltInBBAInterface::GetAvailableSlot(u16 port)
{
  if (port > 0)  // existing connection?
  {
    for (auto& ref : network_ref)
    {
      if (ref.ip != 0 && ref.local == port)
        return &ref;
    }
  }
  for (auto& ref : network_ref)
  {
    if (ref.ip == 0)
      return &ref;
  }
  return nullptr;
}

StackRef* CEXIETHERNET::BuiltInBBAInterface::GetTCPSlot(u16 src_port, u16 dst_port, u32 ip)
{
  for (auto& ref : network_ref)
  {
    if (ref.ip == ip && ref.remote == dst_port && ref.local == src_port)
    {
      return &ref;
    }
  }
  return nullptr;
}

std::vector<u8> BuildFINFrame(StackRef* ref)
{
  Common::TCPPacket result(ref->bba_mac, ref->my_mac, ref->from, ref->to, ref->seq_num,
                           ref->ack_num, TCP_FLAG_FIN | TCP_FLAG_ACK | TCP_FLAG_RST);

  for (auto& tcp_buf : ref->tcp_buffers)
    tcp_buf.used = false;
  return result.Build();
}

std::vector<u8> BuildAckFrame(StackRef* ref)
{
  Common::TCPPacket result(ref->bba_mac, ref->my_mac, ref->from, ref->to, ref->seq_num,
                           ref->ack_num, TCP_FLAG_ACK);
  return result.Build();
}

void CEXIETHERNET::BuiltInBBAInterface::HandleTCPFrame(const Common::TCPPacket& packet)
{
  const auto& [hwdata, ip_header, tcp_header, ip_options, tcp_options, data] = packet;
  sf::IpAddress target;
  StackRef* ref = GetTCPSlot(tcp_header.source_port, tcp_header.destination_port,
                             Common::BitCast<u32>(ip_header.destination_addr));
  const u16 properties = ntohs(tcp_header.properties);
  if (properties & (TCP_FLAG_FIN | TCP_FLAG_RST))
  {
    if (ref == nullptr)
      return;  // not found

    ref->ack_num++;
    WriteToQueue(BuildFINFrame(ref));
    ref->ip = 0;
    ref->tcp_socket.disconnect();
  }
  else if (properties & TCP_FLAG_SIN)
  {
    // new connection
    if (ref != nullptr)
      return;
    ref = GetAvailableSlot(0);

    ref->delay = GetTickCountStd();
    ref->local = tcp_header.source_port;
    ref->remote = tcp_header.destination_port;
    ref->ack_num = ntohl(tcp_header.sequence_number) + 1;
    ref->ack_base = ref->ack_num;
    ref->seq_num = 0x1000000;
    ref->window_size = ntohl(tcp_header.window_size);
    ref->type = IPPROTO_TCP;
    for (auto& tcp_buf : ref->tcp_buffers)
      tcp_buf.used = false;
    ref->from.sin_addr.s_addr = Common::BitCast<u32>(ip_header.destination_addr);
    ref->from.sin_port = tcp_header.destination_port;
    ref->to.sin_addr.s_addr = Common::BitCast<u32>(ip_header.source_addr);
    ref->to.sin_port = tcp_header.source_port;
    ref->bba_mac = Common::BitCastPtr<Common::MACAddress>(&m_eth_ref->mBbaMem[BBA_NAFR_PAR0]);
    ref->my_mac = m_fake_mac;
    ref->tcp_socket.setBlocking(false);

    // reply with a sin_ack
    Common::TCPPacket result(ref->bba_mac, ref->my_mac, ref->from, ref->to, ref->seq_num,
                             ref->ack_num, TCP_FLAG_SIN | TCP_FLAG_ACK);

    result.tcp_options = {0x02, 0x04, 0x05, 0xb4, 0x01, 0x01, 0x01, 0x01};

    ref->seq_num++;
    target = sf::IpAddress(ntohl(Common::BitCast<u32>(ip_header.destination_addr)));
    ref->tcp_socket.connect(target, ntohs(tcp_header.destination_port));
    ref->ready = false;
    ref->ip = Common::BitCast<u32>(ip_header.destination_addr);

    ref->tcp_buffers[0].data = result.Build();
    ref->tcp_buffers[0].seq_id = ref->seq_num - 1;
    ref->tcp_buffers[0].tick = GetTickCountStd() - 900;  // delay
    ref->tcp_buffers[0].used = true;
  }
  else
  {
    // data packet
    if (ref == nullptr)
      return;  // not found

    const int size =
        ntohs(ip_header.total_len) - ip_header.DefinedSize() - tcp_header.GetHeaderSize();
    const u32 this_seq = ntohl(tcp_header.sequence_number);

    if (size > 0)
    {
      // only if contain data
      if (static_cast<int>(this_seq - ref->ack_num) >= 0 && data.size() >= size)
      {
        ref->tcp_socket.send(data.data(), size);
        ref->ack_num += size;
      }

      // send ack
      WriteToQueue(BuildAckFrame(ref));
    }
    // update windows size
    ref->window_size = ntohs(tcp_header.window_size);

    // clear any ack data
    if (ntohs(tcp_header.properties) & TCP_FLAG_ACK)
    {
      const u32 ack_num = ntohl(tcp_header.acknowledgement_number);
      for (auto& tcp_buf : ref->tcp_buffers)
      {
        if (!tcp_buf.used || tcp_buf.seq_id >= ack_num)
          continue;

        Common::PacketView view(tcp_buf.data.data(), tcp_buf.data.size());
        auto tcp_packet = view.GetTCPPacket();  // This is always a tcp packet
        if (!tcp_packet.has_value())            // should never happen but just in case
          continue;

        const u32 seq_end = static_cast<u32>(tcp_buf.seq_id + tcp_packet->data.size());
        if (seq_end <= ack_num)
        {
          tcp_buf.used = false;  // confirmed data received
          if (!ref->ready && !ref->tcp_buffers[0].used)
            ref->ready = true;
          continue;
        }
        // partial data, adjust the packet for next ack
        const u16 ack_size = ack_num - tcp_buf.seq_id;
        tcp_packet->data.erase(tcp_packet->data.begin(), tcp_packet->data.begin() + ack_size);

        tcp_buf.seq_id += ack_size;
        tcp_packet->tcp_header.sequence_number = htonl(tcp_buf.seq_id);
        tcp_buf.data = tcp_packet->Build();
      }
    }
  }
}

// This is a little hack, some games open a UDP port
// and listen to it. We open it on our side manually.
void CEXIETHERNET::BuiltInBBAInterface::InitUDPPort(u16 port)
{
  StackRef* ref = GetAvailableSlot(htons(port));
  if (ref == nullptr || ref->ip != 0)
    return;
  ref->ip = m_router_ip;  // change for ip
  ref->local = htons(port);
  ref->remote = htons(port);
  ref->type = IPPROTO_UDP;
  ref->bba_mac = Common::BitCastPtr<Common::MACAddress>(&m_eth_ref->mBbaMem[BBA_NAFR_PAR0]);
  ref->my_mac = m_fake_mac;
  ref->from.sin_addr.s_addr = 0;
  ref->from.sin_port = htons(port);
  ref->to.sin_addr.s_addr = m_current_ip;
  ref->to.sin_port = htons(port);
  ref->udp_socket.setBlocking(false);
  if (ref->udp_socket.bind(port) != sf::Socket::Done)
  {
    ERROR_LOG_FMT(SP1, "Couldn't open UDP socket");
    PanicAlertFmt("Could't open port {:x}, this game might not work proprely in LAN mode.", port);
    return;
  }
}

void CEXIETHERNET::BuiltInBBAInterface::HandleUDPFrame(const Common::UDPPacket& packet)
{
  const auto& [hwdata, ip_header, udp_header, ip_options, data] = packet;
  sf::IpAddress target;
  const u32 destination_addr = ip_header.destination_addr == Common::IP_ADDR_ANY ?
                                   m_router_ip :  // dns request
                                   Common::BitCast<u32>(ip_header.destination_addr);

  StackRef* ref = GetAvailableSlot(udp_header.source_port);
  if (ref->ip == 0)
  {
    ref->ip = destination_addr;  // change for ip
    ref->local = udp_header.source_port;
    ref->remote = udp_header.destination_port;
    ref->type = IPPROTO_UDP;
    ref->bba_mac = Common::BitCastPtr<Common::MACAddress>(&m_eth_ref->mBbaMem[BBA_NAFR_PAR0]);
    ref->my_mac = m_fake_mac;
    ref->from.sin_addr.s_addr = destination_addr;
    ref->from.sin_port = udp_header.destination_port;
    ref->to.sin_addr.s_addr = Common::BitCast<u32>(ip_header.source_addr);
    ref->to.sin_port = udp_header.source_port;
    ref->udp_socket.setBlocking(false);
    if (ref->udp_socket.bind(htons(udp_header.source_port)) != sf::Socket::Done)
    {
      PanicAlertFmt(
          "Port {:x} is already in use, this game might not work as intented in LAN Mode.",
          htons(udp_header.source_port));
      if (ref->udp_socket.bind(sf::Socket::AnyPort) != sf::Socket::Done)
      {
        ERROR_LOG_FMT(SP1, "Couldn't open UDP socket");
        return;
      }
      if (ntohs(udp_header.destination_port) == 1900)
      {
        InitUDPPort(26512);  // MK DD and 1080
        InitUDPPort(26502);  // Air Ride
        if (udp_header.length > 150)
        {
          // Quick hack to unlock the connection, throw it back at him
          Common::UDPPacket reply = packet;
          reply.eth_header.destination = hwdata.source;
          reply.eth_header.source = hwdata.destination;
          reply.ip_header.destination_addr = ip_header.source_addr;
          if (ip_header.destination_addr == Common::IP_ADDR_SSDP)
            reply.ip_header.source_addr = Common::IP_ADDR_BROADCAST;
          else
            reply.ip_header.source_addr = Common::BitCast<Common::IPAddress>(destination_addr);
          WriteToQueue(reply.Build());
        }
      }
    }
  }
  if (ntohs(udp_header.destination_port) == 53)
    target = sf::IpAddress(m_dns_ip.c_str());  // dns server ip
  else if (ip_header.destination_addr == Common::IP_ADDR_SSDP)
    target = sf::IpAddress(0xFFFFFFFF);  // force real broadcast
  else
    target = sf::IpAddress(ntohl(Common::BitCast<u32>(ip_header.destination_addr)));
  ref->udp_socket.send(data.data(), data.size(), target, ntohs(udp_header.destination_port));
}

bool CEXIETHERNET::BuiltInBBAInterface::SendFrame(const u8* frame, u32 size)
{
  std::lock_guard<std::mutex> lock(m_mtx);
  const Common::PacketView view(frame, size);

  const std::optional<u16> ethertype = view.GetEtherType();
  if (!ethertype.has_value())
  {
    ERROR_LOG_FMT(SP1, "Unable to send frame with invalid ethernet header");
    return false;
  }

  switch (*ethertype)
  {
  case Common::IPV4_ETHERTYPE:
  {
    const std::optional<u8> ip_proto = view.GetIPProto();
    if (!ip_proto.has_value())
    {
      ERROR_LOG_FMT(SP1, "Unable to send frame with invalid IP header");
      return false;
    }

    switch (*ip_proto)
    {
    case IPPROTO_UDP:
    {
      const auto udp_packet = view.GetUDPPacket();
      if (!udp_packet.has_value())
      {
        ERROR_LOG_FMT(SP1, "Unable to send frame with invalid UDP header");
        return false;
      }

      if (ntohs(udp_packet->udp_header.destination_port) == 67)
      {
        HandleDHCP(*udp_packet);
      }
      else
      {
        HandleUDPFrame(*udp_packet);
      }
      break;
    }

    case IPPROTO_TCP:
    {
      const auto tcp_packet = view.GetTCPPacket();
      if (!tcp_packet.has_value())
      {
        ERROR_LOG_FMT(SP1, "Unable to send frame with invalid TCP header");
        return false;
      }

      HandleTCPFrame(*tcp_packet);
      break;
    }
    }
    break;
  }

  case Common::ARP_ETHERTYPE:
  {
    const auto arp_packet = view.GetARPPacket();
    if (!arp_packet.has_value())
    {
      ERROR_LOG_FMT(SP1, "Unable to send frame with invalid ARP header");
      return false;
    }

    HandleARP(*arp_packet);
    break;
  }

  default:
    ERROR_LOG_FMT(SP1, "Unsupported EtherType {#06x}", *ethertype);
    return false;
  }

  m_eth_ref->SendComplete();
  return true;
}

std::optional<std::vector<u8>> TryGetDataFromSocket(StackRef* ref)
{
  size_t datasize = 0;  // Set by socket.receive using a non-const reference
  unsigned short remote_port;

  switch (ref->type)
  {
  case IPPROTO_UDP:
  {
    std::array<u8, MAX_UDP_LENGTH> buffer;
    ref->udp_socket.receive(buffer.data(), MAX_UDP_LENGTH, datasize, ref->target, remote_port);
    if (datasize > 0)
    {
      ref->from.sin_port = htons(remote_port);
      ref->from.sin_addr.s_addr = htonl(ref->target.toInteger());
      const std::vector<u8> udp_data(buffer.begin(), buffer.begin() + datasize);
      Common::UDPPacket packet(ref->bba_mac, ref->my_mac, ref->from, ref->to, udp_data);
      return packet.Build();
    }
    break;
  }

  case IPPROTO_TCP:
    sf::Socket::Status st = sf::Socket::Status::Done;
    TcpBuffer* tcp_buffer = nullptr;
    for (auto& tcp_buf : ref->tcp_buffers)
    {
      if (tcp_buf.used)
        continue;
      tcp_buffer = &tcp_buf;
      break;
    }

    // set default size to 0 to avoid issue
    datasize = 0;
    const bool can_go = (GetTickCountStd() - ref->poke_time > 100 || ref->window_size > 2000);
    std::array<u8, MAX_TCP_LENGTH> buffer;
    if (tcp_buffer != nullptr && ref->ready && can_go)
      st = ref->tcp_socket.receive(buffer.data(), MAX_TCP_LENGTH, datasize);

    if (datasize > 0)
    {
      Common::TCPPacket packet(ref->bba_mac, ref->my_mac, ref->from, ref->to, ref->seq_num,
                               ref->ack_num, TCP_FLAG_ACK);
      packet.data = std::vector<u8>(buffer.begin(), buffer.begin() + datasize);

      // build buffer
      tcp_buffer->seq_id = ref->seq_num;
      tcp_buffer->tick = GetTickCountStd();
      tcp_buffer->data = packet.Build();
      tcp_buffer->seq_id = ref->seq_num;
      tcp_buffer->used = true;
      ref->seq_num += static_cast<u32>(datasize);
      ref->poke_time = GetTickCountStd();
      return tcp_buffer->data;
    }
    if (GetTickCountStd() - ref->delay > 3000)
    {
      if (st == sf::Socket::Disconnected || st == sf::Socket::Error)
      {
        ref->ip = 0;
        ref->tcp_socket.disconnect();
        return BuildFINFrame(ref);
      }
    }
    break;
  }

  return std::nullopt;
}

// Change the IP identification and recompute the checksum
static void SetIPIdentification(u8* ptr, std::size_t size, u16 value)
{
  if (size < Common::EthernetHeader::SIZE + Common::IPv4Header::SIZE)
    return;

  u8* const ip_ptr = ptr + Common::EthernetHeader::SIZE;
  const u8 ip_header_size = (*ip_ptr & 0xf) * 4;
  if (size < Common::EthernetHeader::SIZE + ip_header_size)
    return;

  u8* const ip_id_ptr = ip_ptr + offsetof(Common::IPv4Header, identification);
  Common::BitCastPtr<u16>(ip_id_ptr) = htons(value);

  u8* const ip_checksum_ptr = ip_ptr + offsetof(Common::IPv4Header, header_checksum);
  auto checksum_bitcast_ptr = Common::BitCastPtr<u16>(ip_checksum_ptr);
  checksum_bitcast_ptr = u16(0);
  checksum_bitcast_ptr = htons(Common::ComputeNetworkChecksum(ip_ptr, ip_header_size));
}

void CEXIETHERNET::BuiltInBBAInterface::ReadThreadHandler(CEXIETHERNET::BuiltInBBAInterface* self)
{
  while (!self->m_read_thread_shutdown.IsSet())
  {
    // make thread less cpu hungry
    std::this_thread::sleep_for(std::chrono::milliseconds(1));

    if (!self->m_read_enabled.IsSet())
      continue;
    size_t datasize = 0;

    u8 wp = self->m_eth_ref->page_ptr(BBA_RWP);
    const u8 rp = self->m_eth_ref->page_ptr(BBA_RRP);
    if (rp > wp)
      wp += 16;

    if ((wp - rp) >= 8)
      continue;

    std::lock_guard<std::mutex> lock(self->m_mtx);
    // process queue file first
    if (self->m_queue_read != self->m_queue_write)
    {
      datasize = self->m_queue_data[self->m_queue_read].size();
      if (datasize > BBA_RECV_SIZE)
      {
        ERROR_LOG_FMT(SP1, "Frame size is exceiding BBA capacity, frame stack might be corrupted"
                           "Killing Dolphin...");
        std::exit(0);
      }
      std::memcpy(self->m_eth_ref->mRecvBuffer.get(), self->m_queue_data[self->m_queue_read].data(),
                  datasize);
      self->m_queue_read++;
      self->m_queue_read &= 15;
    }
    else
    {
      // test connections data
      for (auto& net_ref : self->network_ref)
      {
        if (net_ref.ip == 0)
          continue;
        const auto socket_data = TryGetDataFromSocket(&net_ref);
        if (socket_data.has_value())
        {
          datasize = socket_data->size();
          std::memcpy(self->m_eth_ref->mRecvBuffer.get(), socket_data->data(), datasize);
          break;
        }
      }
    }

    // test and add any sleeping tcp data
    for (auto& net_ref : self->network_ref)
    {
      if (net_ref.ip == 0 || net_ref.type != IPPROTO_TCP)
        continue;
      for (auto& tcp_buf : net_ref.tcp_buffers)
      {
        if (!tcp_buf.used || (GetTickCountStd() - tcp_buf.tick) <= 1000)
          continue;

        tcp_buf.tick = GetTickCountStd();
        // timmed out packet, resend
        if (((self->m_queue_write + 1) & 15) != self->m_queue_read)
        {
          self->WriteToQueue(tcp_buf.data);
        }
      }
    }

    if (datasize > 0)
    {
      u8* buffer = reinterpret_cast<u8*>(self->m_eth_ref->mRecvBuffer.get());
      Common::PacketView packet(buffer, datasize);
      const auto packet_type = packet.GetEtherType();
      if (packet_type.has_value() && packet_type == IP_PROTOCOL)
      {
        SetIPIdentification(buffer, datasize, ++self->m_ip_frame_id);
      }
      self->m_eth_ref->mRecvBufferLength = datasize > 64 ? static_cast<u32>(datasize) : 64;
      self->m_eth_ref->RecvHandlePacket();
    }
  }
}

bool CEXIETHERNET::BuiltInBBAInterface::RecvInit()
{
  m_read_thread = std::thread(ReadThreadHandler, this);
  return true;
}

void CEXIETHERNET::BuiltInBBAInterface::RecvStart()
{
  m_read_enabled.Set();
}

void CEXIETHERNET::BuiltInBBAInterface::RecvStop()
{
  m_read_enabled.Clear();
  for (auto& net_ref : network_ref)
  {
    if (net_ref.ip != 0)
    {
      net_ref.type == IPPROTO_TCP ? net_ref.tcp_socket.disconnect() : net_ref.udp_socket.unbind();
    }
    net_ref.ip = 0;
  }
  m_queue_read = 0;
  m_queue_write = 0;
}
}  // namespace ExpansionInterface
