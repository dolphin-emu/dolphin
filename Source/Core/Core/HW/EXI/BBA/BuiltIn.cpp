// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HW/EXI/BBA/BuiltIn.h"

#include <cstring>

#include <SFML/Network.hpp>

#include "Common/Assert.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Common/StringUtil.h"
#include "Common/Swap.h"
#include "Core/HW/EXI/EXI_Device.h"
#include "Core/HW/EXI/EXI_DeviceEthernet.h"
#include "VideoCommon/OnScreenDisplay.h"

// #define BBA_TRACK_PAGE_PTRS

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

  active = true;
  m_in_frame = std::make_unique<u8[]>(9004);
  m_out_frame = std::make_unique<u8[]>(9004);
  for (auto& buf : queue_data)
    buf = std::make_unique<u8[]>(2048);
  fake_mac = Common::GenerateMacAddress(Common::MACConsumer::BBA);
  const u32 ip = m_local_ip.empty() ? sf::IpAddress::getLocalAddress().toInteger() :
                                      sf::IpAddress(m_local_ip).toInteger();
  m_current_ip = htonl(ip);
  m_router_ip = (m_current_ip & 0xFFFFFF) | 0x01000000;
  // clear all ref
  for (int i = 0; i < std::size(network_ref); i++)
  {
    network_ref[i].ip = 0;
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
  active = false;

  // kill all active socket
  for (int i = 0; i < std::size(network_ref); i++)
  {
    if (network_ref[i].ip != 0)
    {
      network_ref[i].type == IPPROTO_TCP ? network_ref[i].tcp_socket.disconnect() :
                                           network_ref[i].udp_socket.unbind();
    }
    network_ref[i].ip = 0;
  }

  // Wait for read thread to exit.
  if (m_read_thread.joinable())
    m_read_thread.join();
}

bool CEXIETHERNET::BuiltInBBAInterface::IsActivated()
{
  return active;
}

void CEXIETHERNET::BuiltInBBAInterface::WriteToQueue(const u8* data, int length)
{
  queue_data_size[queue_write] = length;
  std::memcpy(queue_data[queue_write].get(), data, length);
  if (((queue_write + 1) & 15) == queue_read)
  {
    return;
  }
  queue_write = (queue_write + 1) & 15;
}

void CEXIETHERNET::BuiltInBBAInterface::HandleARP(Common::EthernetHeader* hwdata,
                                                  Common::ARPHeader* arpdata)
{
  std::memset(m_in_frame.get(), 0, 0x30);
  Common::EthernetHeader* hwpart = (Common::EthernetHeader*)m_in_frame.get();
  *hwpart = Common::EthernetHeader(ARP_PROTOCOL);
  hwpart->destination = *(Common::MACAddress*)&m_eth_ref->mBbaMem[BBA_NAFR_PAR0];
  hwpart->source = fake_mac;

  Common::ARPHeader* arppart = (Common::ARPHeader*)&m_in_frame[14];
  Common::MACAddress bba_mac;

  bba_mac = *(Common::MACAddress*)&m_eth_ref->mBbaMem[BBA_NAFR_PAR0];
  if (arpdata->target_ip == m_current_ip)
  {
    // game asked for himself, reply with his mac address
    *arppart = Common::ARPHeader(arpdata->target_ip, bba_mac, m_current_ip, bba_mac);
  }
  else
  {
    *arppart = Common::ARPHeader(arpdata->target_ip, fake_mac, m_current_ip, bba_mac);
  }

  WriteToQueue(&m_in_frame[0], 0x2a);
}

void CEXIETHERNET::BuiltInBBAInterface::HandleDHCP(Common::EthernetHeader* hwdata,
                                                   Common::UDPHeader* udpdata,
                                                   Common::DHCPBody* request)
{
  Common::DHCPBody* reply = (Common::DHCPBody*)&m_in_frame[0x2a];
  sockaddr_in from;
  sockaddr_in to;
  std::memset(m_in_frame.get(), 0, 0x156);

  // build layer
  Common::EthernetHeader* hwpart = (Common::EthernetHeader*)m_in_frame.get();
  Common::IPv4Header* ippart = (Common::IPv4Header*)&m_in_frame[14];
  Common::UDPHeader* udppart = (Common::UDPHeader*)&m_in_frame[0x22];
  *hwpart = Common::EthernetHeader(IP_PROTOCOL);
  hwpart->destination = *(Common::MACAddress*)&m_eth_ref->mBbaMem[BBA_NAFR_PAR0];
  hwpart->source = fake_mac;

  from.sin_addr.s_addr = m_router_ip;
  from.sin_family = IPPROTO_UDP;
  from.sin_port = htons(67);
  to.sin_addr.s_addr = m_current_ip;
  to.sin_family = IPPROTO_UDP;
  to.sin_port = udpdata->source_port;
  const std::vector<u8> ip_part = {((u8*)&m_router_ip)[0], ((u8*)&m_router_ip)[1],
                                   ((u8*)&m_router_ip)[2], ((u8*)&m_router_ip)[3]};

  *ippart = Common::IPv4Header(308, IPPROTO_UDP, from, to);

  *udppart = Common::UDPHeader(from, to, 300);

  *reply = Common::DHCPBody(request->transaction_id,
                            *(Common::MACAddress*)&m_eth_ref->mBbaMem[BBA_NAFR_PAR0], m_current_ip,
                            m_router_ip);

  // options
  request->options[2] == 1 ? reply->AddDHCPOption(1, 53, std::vector<u8>{2}) :
                             reply->AddDHCPOption(1, 53, std::vector<u8>{5});
  reply->AddDHCPOption(4, 54, ip_part);                            // dhcp server ip
  reply->AddDHCPOption(4, 51, std::vector<u8>{0, 1, 0x51, 0x80});  // lease time 24h
  reply->AddDHCPOption(4, 58, std::vector<u8>{0, 1, 0x51, 0x80});  // renewal
  reply->AddDHCPOption(4, 59, std::vector<u8>{0, 1, 0x51, 0x80});  // rebind
  reply->AddDHCPOption(4, 1, std::vector<u8>{255, 255, 255, 0});   // submask
  reply->AddDHCPOption(4, 28,
                       std::vector<u8>{ip_part[0], ip_part[1], ip_part[2], 255});  // broadcast ip
  reply->AddDHCPOption(4, 6, ip_part);                                             // dns server
  reply->AddDHCPOption(3, 15, std::vector<u8>{0x6c, 0x61, 0x6e});  // domaine name "lan"
  reply->AddDHCPOption(4, 3, ip_part);                             // router ip
  reply->AddDHCPOption(0, 255, {});                                // end

  udppart->checksum = Common::ComputeTCPNetworkChecksum(from, to, udppart, 308, IPPROTO_UDP);

  WriteToQueue(m_in_frame.get(), 0x156);
}

StackRef* CEXIETHERNET::BuiltInBBAInterface::GetAvaibleSlot(u16 port)
{
  if (port > 0)  // existing connection?
  {
    for (int i = 0; i < std::size(network_ref); i++)
    {
      if (network_ref[i].ip != 0 && network_ref[i].local == port)
        return &network_ref[i];
    }
  }
  for (int i = 0; i < std::size(network_ref); i++)
  {
    if (network_ref[i].ip == 0)
      return &network_ref[i];
  }
  return nullptr;
}

StackRef* CEXIETHERNET::BuiltInBBAInterface::GetTCPSlot(u16 src_port, u16 dst_port, u32 ip)
{
  for (int i = 0; i < std::size(network_ref); i++)
  {
    if (network_ref[i].ip == ip && network_ref[i].remote == dst_port &&
        network_ref[i].local == src_port)
    {
      return &network_ref[i];
    }
  }
  return nullptr;
}

int BuildFINFrame(StackRef* ref, u8* buf)
{
  std::memset(buf, 0, 0x36);
  Common::EthernetHeader* hwpart = (Common::EthernetHeader*)&buf[0];
  Common::IPv4Header* ippart = (Common::IPv4Header*)&buf[14];
  Common::TCPHeader* tcppart = (Common::TCPHeader*)&buf[0x22];

  *hwpart = Common::EthernetHeader(IP_PROTOCOL);
  hwpart->destination = ref->bba_mac;
  hwpart->source = ref->my_mac;

  *ippart = Common::IPv4Header(20, IPPROTO_TCP, ref->from, ref->to);

  *tcppart = Common::TCPHeader(ref->from, ref->to, ref->seq_num, ref->ack_num,
                               TCP_FLAG_FIN | TCP_FLAG_ACK | TCP_FLAG_RST);
  tcppart->checksum =
      Common::ComputeTCPNetworkChecksum(ref->from, ref->to, tcppart, 20, IPPROTO_TCP);

  for (auto& tcp_buf : ref->tcp_buffers)
    tcp_buf.used = false;

  return 0x36;
}

int BuildAckFrame(StackRef* ref, u8* buf)
{
  std::memset(buf, 0, 0x36);
  Common::EthernetHeader* hwpart = (Common::EthernetHeader*)&buf[0];
  Common::IPv4Header* ippart = (Common::IPv4Header*)&buf[14];
  Common::TCPHeader* tcppart = (Common::TCPHeader*)&buf[0x22];

  *hwpart = Common::EthernetHeader(IP_PROTOCOL);
  hwpart->destination = ref->bba_mac;
  hwpart->source = ref->my_mac;

  *ippart = Common::IPv4Header(20, IPPROTO_TCP, ref->from, ref->to);

  *tcppart = Common::TCPHeader(ref->from, ref->to, ref->seq_num, ref->ack_num, TCP_FLAG_ACK);
  tcppart->checksum =
      Common::ComputeTCPNetworkChecksum(ref->from, ref->to, tcppart, 20, IPPROTO_TCP);

  return 0x36;
}

void CEXIETHERNET::BuiltInBBAInterface::HandleTCPFrame(Common::EthernetHeader* hwdata,
                                                       Common::IPv4Header* ipdata,
                                                       Common::TCPHeader* tcpdata, u8* data)
{
  sf::IpAddress target;
  StackRef* ref =
      GetTCPSlot(tcpdata->source_port, tcpdata->destination_port, *(u32*)&ipdata->destination_addr);
  if (tcpdata->properties & (TCP_FLAG_FIN | TCP_FLAG_RST))
  {
    if (ref == nullptr)
      return;  // not found

    ref->ack_num++;
    const int size = BuildFINFrame(ref, m_in_frame.get());
    WriteToQueue(m_in_frame.get(), size);
    ref->ip = 0;
    ref->tcp_socket.disconnect();
  }
  else if (tcpdata->properties & TCP_FLAG_SIN)
  {
    // new connection
    if (ref != nullptr)
      return;
    ref = GetAvaibleSlot(0);

    ref->delay = GetTickCountStd();
    ref->local = tcpdata->source_port;
    ref->remote = tcpdata->destination_port;
    ref->ack_num = Common::swap32(tcpdata->sequence_number) + 1;
    ref->ack_base = ref->ack_num;
    ref->seq_num = 0x1000000;
    ref->window_size = Common::swap16(tcpdata->window_size);
    ref->type = IPPROTO_TCP;
    for (auto& tcp_buf : ref->tcp_buffers)
      tcp_buf.used = false;
    ref->from.sin_addr.s_addr = *(u32*)&ipdata->destination_addr;
    ref->from.sin_port = tcpdata->destination_port;
    ref->to.sin_addr.s_addr = *(u32*)&ipdata->source_addr;
    ref->to.sin_port = tcpdata->source_port;
    ref->bba_mac = *(Common::MACAddress*)&m_eth_ref->mBbaMem[BBA_NAFR_PAR0];
    ref->my_mac = fake_mac;
    ref->tcp_socket.setBlocking(false);

    // reply with a sin_ack
    std::memset(m_in_frame.get(), 0, 0x100);
    Common::EthernetHeader* hwpart = (Common::EthernetHeader*)m_in_frame.get();
    Common::IPv4Header* ippart = (Common::IPv4Header*)&m_in_frame[14];
    Common::TCPHeader* tcppart = (Common::TCPHeader*)&m_in_frame[0x22];

    *hwpart = Common::EthernetHeader(IP_PROTOCOL);
    hwpart->destination = ref->bba_mac;
    hwpart->source = fake_mac;

    *ippart = Common::IPv4Header(28, IPPROTO_TCP, ref->from, ref->to);

    *tcppart = Common::TCPHeader(ref->from, ref->to, ref->seq_num, ref->ack_num,
                                 0x70 | TCP_FLAG_SIN | TCP_FLAG_ACK);
    const u8 options[] = {0x02, 0x04, 0x05, 0xb4, 0x01, 0x01, 0x01, 0x01};
    std::memcpy(&m_in_frame[0x36], options, std::size(options));

    // do checksum
    tcppart->checksum =
        Common::ComputeTCPNetworkChecksum(ref->from, ref->to, tcppart, 28, IPPROTO_TCP);

    ref->seq_num++;
    target = sf::IpAddress(Common::swap32(ipdata->destination_addr));
    ref->tcp_socket.connect(target, Common::swap16(tcpdata->destination_port));
    ref->ready = false;
    ref->ip = *(u32*)ipdata->destination_addr;

    std::memcpy(&ref->tcp_buffers[0].data, m_in_frame.get(), 0x3e);
    ref->tcp_buffers[0].data_size = 0x3e;
    ref->tcp_buffers[0].seq_id = ref->seq_num - 1;
    ref->tcp_buffers[0].tick = GetTickCountStd() - 900;  // delay
    ref->tcp_buffers[0].used = true;
  }
  else
  {
    // data packet
    if (ref == nullptr)
      return;  // not found

    const int c = (tcpdata->properties & 0xf0) >> 2;  // header size
    const int size = Common::swap16(ipdata->total_len) - 20 - c;
    const u32 this_seq = Common::swap32(tcpdata->sequence_number);

    if (size > 0)
    {
      // only if data
      if ((int)(this_seq - ref->ack_num) >= 0)
      {
        ref->tcp_socket.send(data, size);
        ref->ack_num += size;
      }

      // send ack
      BuildAckFrame(ref, m_in_frame.get());

      WriteToQueue(m_in_frame.get(), 0x36);
    }
    // update windows size
    ref->window_size = htons(tcpdata->window_size);

    // clear any ack data
    if (tcpdata->properties & TCP_FLAG_ACK)
    {
      const u32 ack_num = Common::swap32(tcpdata->acknowledgement_number);
      for (auto& tcp_buf : ref->tcp_buffers)
      {
        if (!tcp_buf.used || tcp_buf.seq_id >= ack_num)
          continue;
        Common::TCPHeader* tcppart = (Common::TCPHeader*)&tcp_buf.data[0x22];
        const u32 seq_end =
            tcp_buf.seq_id + tcp_buf.data_size - ((tcppart->properties & 0xf0) >> 2) - 34;
        if (seq_end <= ack_num)
        {
          tcp_buf.used = false;  // confirmed data received
          if (!ref->ready && !ref->tcp_buffers[0].used)
            ref->ready = true;
          continue;
        }
        // partial data, adjust the packet for next ack
        const u16 ack_size = ack_num - tcp_buf.seq_id;
        const u16 new_data_size = tcp_buf.data_size - 0x36 - ack_size;
        std::memmove(&tcp_buf.data[0x36], &tcp_buf.data[0x36 + ack_size], new_data_size);
        tcp_buf.data_size -= ack_size;
        tcp_buf.seq_id += ack_size;
        tcppart->sequence_number = htonl(tcp_buf.seq_id);
        Common::IPv4Header* ippart = (Common::IPv4Header*)&tcp_buf.data[14];
        ippart->total_len = htons(tcp_buf.data_size - 14);
        tcppart->checksum = 0;
        tcppart->checksum = Common::ComputeTCPNetworkChecksum(ref->from, ref->to, tcppart,
                                                              new_data_size + 20, IPPROTO_TCP);
      }
    }
  }
}

/// <summary>
/// This is a litle hack, Mario Kart open some UDP port
/// and listen to it. We open it on our side manualy.
/// </summary>
void CEXIETHERNET::BuiltInBBAInterface::InitUDPPort(u16 port)
{
  StackRef* ref = GetAvaibleSlot(htons(port));
  if (ref == nullptr || ref->ip != 0)
    return;
  ref->ip = 0x08080808;  // change for ip
  ref->local = htons(port);
  ref->remote = htons(port);
  ref->type = 17;
  ref->bba_mac = *(Common::MACAddress*)&m_eth_ref->mBbaMem[BBA_NAFR_PAR0];
  ref->my_mac = fake_mac;
  ref->from.sin_addr.s_addr = 0;
  ref->from.sin_port = htons(port);
  ref->to.sin_addr.s_addr = m_current_ip;
  ref->to.sin_port = htons(port);
  ref->udp_socket.setBlocking(false);
  if (ref->udp_socket.bind(port) != sf::Socket::Done)
  {
    ERROR_LOG_FMT(SP1, "Couldn't open UDP socket");
    return;
  }
}

void CEXIETHERNET::BuiltInBBAInterface::HandleUDPFrame(Common::EthernetHeader* hwdata,
                                                       Common::IPv4Header* ipdata,
                                                       Common::UDPHeader* udpdata, u8* data)
{
  sf::IpAddress target;

  if (*(u32*)ipdata->destination_addr == 0)
    *(u32*)ipdata->destination_addr = m_router_ip;
  // dns request
  StackRef* ref = GetAvaibleSlot(udpdata->source_port);
  if (ref->ip == 0)
  {
    ref->ip = *(u32*)ipdata->destination_addr;  // change for ip
    ref->local = udpdata->source_port;
    ref->remote = udpdata->destination_port;
    ref->type = 17;
    ref->bba_mac = *(Common::MACAddress*)&m_eth_ref->mBbaMem[BBA_NAFR_PAR0];
    ref->my_mac = fake_mac;
    ref->from.sin_addr.s_addr = *(u32*)&ipdata->destination_addr;
    ref->from.sin_port = udpdata->destination_port;
    ref->to.sin_addr.s_addr = *(u32*)&ipdata->source_addr;
    ref->to.sin_port = udpdata->source_port;
    ref->udp_socket.setBlocking(false);
    if (ref->udp_socket.bind(htons(udpdata->source_port)) != sf::Socket::Done)
    {
      if (ref->udp_socket.bind(sf::Socket::AnyPort) != sf::Socket::Done)
      {
        ERROR_LOG_FMT(SP1, "Couldn't open UDP socket");
        return;
      }
    }
  }
  if (Common::swap16(udpdata->destination_port) == 1900)
  {
    InitUDPPort(26512);                                 // MK DD and 1080
    InitUDPPort(26502);                                 // Air Ride
    if (*(u32*)ipdata->destination_addr == 0xFAFFFFEF)  // force real broadcast
      *(u32*)ipdata->destination_addr = 0xFFFFFFFF;     // Multi cast cannot be read
    if (udpdata->length > 150)
    {
      // Quick hack to unlock the connection, throw it back at him
      Common::EthernetHeader* hwpart = (Common::EthernetHeader*)m_in_frame.get();
      Common::IPv4Header* ippart = (Common::IPv4Header*)&m_in_frame[14];
      std::memcpy(m_in_frame.get(), hwdata, htons(ipdata->total_len) + 14);
      hwpart->destination = hwdata->source;
      hwpart->source = hwdata->destination;
      *(u32*)ippart->destination_addr = *(u32*)ipdata->source_addr;
      *(u32*)ippart->source_addr = *(u32*)ipdata->destination_addr;
      WriteToQueue(m_in_frame.get(), htons(ipdata->total_len) + 14);
    }
  }
  if (Common::swap16(udpdata->destination_port) == 53)
  {
    target = sf::IpAddress(m_dns_ip.c_str());  // dns server ip
  }
  else
  {
    target = sf::IpAddress(Common::swap32(*(u32*)ipdata->destination_addr));
  }
  ref->udp_socket.send(data, Common::swap16(udpdata->length) - 8, target,
                       Common::swap16(udpdata->destination_port));
}

bool CEXIETHERNET::BuiltInBBAInterface::SendFrame(const u8* frame, u32 size)
{
  Common::EthernetHeader* hwdata;
  Common::IPv4Header* ipdata;
  Common::UDPHeader* udpdata;
  Common::TCPHeader* tcpdata;

  int offset = 0;
  std::lock_guard<std::mutex> lock(mtx);
  std::memcpy(m_out_frame.get(), frame, size);
  INFO_LOG_FMT(SP1, "Sending packet {:x}\n{}", size, ArrayToString(m_out_frame.get(), size, 16));
  // handle the packet data
  hwdata = (Common::EthernetHeader*)m_out_frame.get();
  if (hwdata->ethertype == 0x08)  // IPV4
  {
    // IP sub
    ipdata = (Common::IPv4Header*)&m_out_frame[14];
    switch (ipdata->protocol)
    {
    case IPPROTO_UDP:
      offset = 14 + (ipdata->version_ihl & 0xf) * 4;
      udpdata = (Common::UDPHeader*)&m_out_frame[offset];
      offset += Common::UDPHeader::SIZE;
      if (Common::swap16(udpdata->destination_port) == 67)
      {
        Common::DHCPBody* request = (Common::DHCPBody*)&m_out_frame[offset];
        HandleDHCP(hwdata, udpdata, request);
      }
      else
      {
        HandleUDPFrame(hwdata, ipdata, udpdata, &m_out_frame[offset]);
      }
      break;

    case IPPROTO_TCP:
      offset = 14 + (ipdata->version_ihl & 0xf) * 4;
      tcpdata = (Common::TCPHeader*)&m_out_frame[offset];
      offset += (tcpdata->properties & 0xf0) >> 2;
      HandleTCPFrame(hwdata, ipdata, tcpdata, &m_out_frame[offset]);
      break;
    }
  }
  if (hwdata->ethertype == 0x608)  // arp
  {
    Common::ARPHeader* arpdata = (Common::ARPHeader*)&m_out_frame[14];
    HandleARP(hwdata, arpdata);
  }

  m_eth_ref->SendComplete();
  return true;
}

size_t TryGetDataFromSocket(StackRef* ref, u8* buffer)
{
  Common::EthernetHeader* hwdata;
  Common::IPv4Header* ipdata;
  Common::UDPHeader* udpdata;
  Common::TCPHeader* tcpdata;
  size_t datasize = 0;  // this will be filled by the socket read later
  unsigned short remote_port;

  switch (ref->type)
  {
  case IPPROTO_UDP:
    ref->udp_socket.receive(&buffer[0x2a], 1500, datasize, ref->target, remote_port);
    if (datasize > 0)
    {
      std::memset(buffer, 0, 0x2a);
      hwdata = (Common::EthernetHeader*)buffer;
      ipdata = (Common::IPv4Header*)&buffer[14];
      udpdata = (Common::UDPHeader*)&buffer[0x22];

      // build header
      *hwdata = Common::EthernetHeader(IP_PROTOCOL);
      hwdata->destination = ref->bba_mac;
      hwdata->source = ref->my_mac;

      ref->from.sin_port = htons(remote_port);
      ref->from.sin_addr.s_addr = htonl(ref->target.toInteger());
      *ipdata = Common::IPv4Header((u16)(datasize + 8), IPPROTO_UDP, ref->from, ref->to);

      *udpdata = Common::UDPHeader(ref->from, ref->to, (u16)datasize);
      udpdata->checksum = Common::ComputeTCPNetworkChecksum(ref->from, ref->to, udpdata,
                                                            (u16)(datasize + 8), IPPROTO_UDP);
      datasize += 0x2a;
    }
    break;

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
    if (tcp_buffer != nullptr && ref->ready && can_go)
      st = ref->tcp_socket.receive(&buffer[0x36], 440, datasize);

    if (datasize > 0)
    {
      std::memset(buffer, 0, 0x36);
      hwdata = (Common::EthernetHeader*)buffer;
      ipdata = (Common::IPv4Header*)&buffer[14];
      tcpdata = (Common::TCPHeader*)&buffer[0x22];

      *hwdata = Common::EthernetHeader(IP_PROTOCOL);
      hwdata->destination = ref->bba_mac;
      hwdata->source = ref->my_mac;

      *ipdata = Common::IPv4Header((u16)(datasize + 20), IPPROTO_TCP, ref->from, ref->to);

      *tcpdata = Common::TCPHeader(ref->from, ref->to, ref->seq_num, ref->ack_num, TCP_FLAG_ACK);
      tcpdata->checksum = Common::ComputeTCPNetworkChecksum(ref->from, ref->to, tcpdata,
                                                            (u16)(datasize + 20), IPPROTO_TCP);

      // build buffer
      tcp_buffer->seq_id = ref->seq_num;
      tcp_buffer->data_size = (u16)datasize + 0x36;
      tcp_buffer->tick = GetTickCountStd();
      std::memcpy(&tcp_buffer->data[0], buffer, datasize + 0x36);
      tcp_buffer->seq_id = ref->seq_num;
      tcp_buffer->used = true;
      ref->seq_num += (u32)datasize;
      ref->poke_time = GetTickCountStd();
      datasize += 0x36;
    }
    if (GetTickCountStd() - ref->delay > 3000)
    {
      if (st == sf::Socket::Disconnected || st == sf::Socket::Error)
      {
        ref->ip = 0;
        ref->tcp_socket.disconnect();
        datasize = BuildFINFrame(ref, buffer);
      }
    }
    break;
  }

  return datasize;
}

void CEXIETHERNET::BuiltInBBAInterface::ReadThreadHandler(CEXIETHERNET::BuiltInBBAInterface* self)
{
  Common::EthernetHeader* hwdata;
  Common::IPv4Header* ipdata;

  while (!self->m_read_thread_shutdown.IsSet())
  {
    // test every open connection for incomming data / disconnection
    if (!self->m_read_enabled.IsSet())
      continue;
    size_t datasize = 0;

    u8 wp = self->m_eth_ref->page_ptr(BBA_RWP);
    const u8 rp = self->m_eth_ref->page_ptr(BBA_RRP);
    if (rp > wp)
      wp += 16;

    if ((wp - rp) >= 8)
      continue;

    // process queue file first
    if (self->queue_read != self->queue_write)
    {
      datasize = self->queue_data_size[self->queue_read];
      std::memcpy(self->m_eth_ref->mRecvBuffer.get(), &self->queue_data[self->queue_read][0],
                  datasize);
      self->queue_read++;
      self->queue_read &= 15;
    }
    else
    {
      // test connections data
      for (auto& net_ref : self->network_ref)
      {
        if (net_ref.ip == 0)
          continue;
        datasize = TryGetDataFromSocket(&net_ref, self->m_eth_ref->mRecvBuffer.get());
        if (datasize > 0)
          break;
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
        // late data, resend
        if (((self->queue_write + 1) & 15) != self->queue_read)
        {
          std::lock_guard<std::mutex> lock(self->mtx);
          self->queue_data_size[self->queue_write] = tcp_buf.data_size;
          std::memcpy(self->queue_data[self->queue_write].get(), &tcp_buf.data[0],
                      tcp_buf.data_size);
          self->queue_write = (self->queue_write + 1) & 15;
        }
      }
    }

    if (datasize > 0)
    {
      std::lock_guard<std::mutex> lock(self->mtx);
      u8* b = &self->m_eth_ref->mRecvBuffer[0];
      hwdata = (Common::EthernetHeader*)b;
      if (hwdata->ethertype == 0x8)  // IP_PROTOCOL
      {
        ipdata = (Common::IPv4Header*)&b[14];
        ipdata->identification = Common::swap16(++self->ip_frame_id);
        ipdata->header_checksum = 0;
        ipdata->header_checksum = htons(Common::ComputeNetworkChecksum(ipdata, 20));
      }
      self->m_eth_ref->mRecvBufferLength = (u32)datasize;
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
  queue_read = 0;
  queue_write = 0;
}
}  // namespace ExpansionInterface
