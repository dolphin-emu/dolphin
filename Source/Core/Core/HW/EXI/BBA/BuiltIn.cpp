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
unsigned long long GetTickCountStd()
{
  using namespace std::chrono;
  return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

bool CEXIETHERNET::BuiltInBBAInterface::Activate()
{
  if (IsActivated())
    return true;

  active = true;
  fake_mac = Common::GenerateMacAddress(Common::MACConsumer::BBA);
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

void CEXIETHERNET::BuiltInBBAInterface::WriteToQueue(char* data, int length)
{
  queue_data_size[queue_write] = length;
  memcpy(queue_data[queue_write], data, length);
  if (((queue_write + 1) & 15) == queue_read)
  {
    return;
  }
  queue_write = (queue_write + 1) & 15;
}

void CEXIETHERNET::BuiltInBBAInterface::HandleARP(Common::EthernetHeader* hwdata,
                                                  Common::ARPHeader* arpdata)
{
  /* TODO - Handle proprely
  Right now all request send the "router" info (192.168.1.1)*/
  memset(&m_in_frame, 0, 0x30);
  Common::EthernetHeader* hwpart = (Common::EthernetHeader*)&m_in_frame[0];
  *hwpart = Common::EthernetHeader(ARP_PROTOCOL);
  memcpy(&hwpart->destination, &m_eth_ref->mBbaMem[BBA_NAFR_PAR0], Common::MAC_ADDRESS_SIZE);
  hwpart->source = fake_mac;

  Common::ARPHeader* arppart = (Common::ARPHeader*)&m_in_frame[14];
  Common::MACAddress bba_mac;

  memcpy(&bba_mac, &m_eth_ref->mBbaMem[BBA_NAFR_PAR0], Common::MAC_ADDRESS_SIZE);
  *arppart = Common::ARPHeader(0x0101a8c0, fake_mac, 0xc701a8c0, bba_mac);
  
  WriteToQueue((char*)&m_in_frame, 0x2a);
}

void CEXIETHERNET::BuiltInBBAInterface::HandleDHCP(Common::EthernetHeader* hwdata,
                                                   Common::UDPHeader* udpdata,
                                                   Common::DHCPBody* request)
{
  Common::DHCPBody* reply = (Common::DHCPBody*)&m_in_frame[0x2a];
  sockaddr_in from;
  sockaddr_in to;
  memset(&m_in_frame, 0, 0x156);

  // build layer
  Common::EthernetHeader* hwpart = (Common::EthernetHeader*)&m_in_frame[0];
  Common::IPv4Header* ippart = (Common::IPv4Header*)&m_in_frame[14];
  Common::UDPHeader* udppart = (Common::UDPHeader*)&m_in_frame[0x22];
  *hwpart = Common::EthernetHeader(IP_PROTOCOL);
  memcpy(&hwpart->destination, &m_eth_ref->mBbaMem[BBA_NAFR_PAR0], Common::MAC_ADDRESS_SIZE);
  hwpart->source = fake_mac;

  from.sin_addr.s_addr = 0x0101a8c0;
  from.sin_family = IPPROTO_UDP;
  from.sin_port = htons(67);
  to.sin_addr.s_addr = 0xc701a8c0;
  to.sin_family = IPPROTO_UDP;
  to.sin_port = udpdata->source_port;

  *ippart = Common::IPv4Header(308, IPPROTO_UDP, from, to);

  *udppart = Common::UDPHeader(from, to, 300);

  *reply = Common::DHCPBody(request->transaction_id,
                            *(Common::MACAddress*)&m_eth_ref->mBbaMem[BBA_NAFR_PAR0], 0xc701a8c0,
                            0x0101a8c0);

  // options
  request->options[2] == 1 ? reply->AddDHCPOption(1, 53, new u8[1]{2}) :
                             reply->AddDHCPOption(1, 53, new u8[1]{5});
  reply->AddDHCPOption(4, 54, new u8[4]{192, 168, 1, 1});    // dhcp server ip
  reply->AddDHCPOption(4, 51, new u8[4]{0, 1, 0x51, 0x80});  // lease time 24h
  reply->AddDHCPOption(4, 58, new u8[4]{0, 1, 0x51, 0x80});  // renewal
  reply->AddDHCPOption(4, 59, new u8[4]{0, 1, 0x51, 0x80});  // rebind
  reply->AddDHCPOption(4, 1, new u8[4]{255, 255, 255, 0});   // submask
  reply->AddDHCPOption(4, 28, new u8[4]{192, 168, 1, 255});  // broadcast ip
  reply->AddDHCPOption(4, 6, new u8[4]{192, 168, 1, 1});     // dns server
  reply->AddDHCPOption(3, 15, new u8[3]{0x6c, 0x61, 0x6e});  // domaine name "lan"
  reply->AddDHCPOption(4, 3, new u8[4]{192, 168, 1, 1});     // router ip
  reply->AddDHCPOption(0, 255, {});                          // end

  udppart->checksum = Common::ComputeTCPNetworkChecksum(from, to, udppart, 308, IPPROTO_UDP);

  WriteToQueue((char*)&m_in_frame, 0x156);
}

u8 CEXIETHERNET::BuiltInBBAInterface::GetAvaibleSlot(u16 port)
{
  if (port > 0)  // existing connection?
  {
    for (int i = 0; i < std::size(network_ref); i++)
      if (network_ref[i].ip != 0 && network_ref[i].remote == port)
        return i;
  }
  for (int i = 0; i < std::size(network_ref); i++)
    if (network_ref[i].ip == 0)
      return i;
  return 0;
}

int CEXIETHERNET::BuiltInBBAInterface::GetTCPSlot(u16 src_port, u16 dst_port, u32 ip)
{
  for (int i = 0; i < std::size(network_ref); i++)
    if (network_ref[i].ip == ip && network_ref[i].remote == dst_port && network_ref[i].local == src_port)
      return i;

  return -1;
}

int BuildFINFrame(StackRef* ref, char* buf)
{
  memset(buf, 0, 0x36);
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
      Common::ComputeTCPNetworkChecksum(ref->from, ref->to, tcppart, 28, IPPROTO_TCP);

  for (int l = 0; l < MAX_TCP_BUFFER; l++)
    ref->TcpBuffers[l].used = false;

  return 0x36;
}

void CEXIETHERNET::BuiltInBBAInterface::HandleTCPFrame(Common::EthernetHeader* hwdata,
                                                       Common::IPv4Header* ipdata,
                                                       Common::TCPHeader* tcpdata, char* data)
{
  sf::IpAddress target;

  if (tcpdata->properties & (TCP_FLAG_FIN | TCP_FLAG_RST))
  {
    int i = GetTCPSlot(tcpdata->source_port, tcpdata->destination_port,
                       *(u32*)&ipdata->destination_addr);
    if (i == -1)
      return;  // not found

    network_ref[i].ack_num++;
    int size = BuildFINFrame(&network_ref[i], (char*)&m_in_frame);
    WriteToQueue((char*)&m_in_frame, size);
    network_ref[i].ip = 0;
    network_ref[i].tcp_socket.disconnect();
  }
  else if (tcpdata->properties & TCP_FLAG_SIN)
  {
    // new connection
    int i = GetTCPSlot(tcpdata->source_port, tcpdata->destination_port,
                       *(u32*)&ipdata->destination_addr);
    if (i != -1)
      return;
    i = GetAvaibleSlot(0);

    network_ref[i].delay = 0x3000;
    network_ref[i].local = tcpdata->source_port;
    network_ref[i].remote = tcpdata->destination_port;
    network_ref[i].ack_num = Common::swap32(tcpdata->sequence_number) + 1;
    network_ref[i].ack_base = network_ref[i].ack_num;
    network_ref[i].seq_num = 0x1000000;
    network_ref[i].window_size = Common::swap16(tcpdata->window_size);
    network_ref[i].type = IPPROTO_TCP;
    for (int l = 0; l < MAX_TCP_BUFFER; l++)
      network_ref[i].TcpBuffers[l].used = false;
    network_ref[i].from.sin_addr.s_addr = *(u32*)&ipdata->destination_addr;
    network_ref[i].from.sin_port = tcpdata->destination_port;
    network_ref[i].to.sin_addr.s_addr = *(u32*)&ipdata->source_addr;
    network_ref[i].to.sin_port = tcpdata->source_port;
    memcpy(&network_ref[i].bba_mac, &m_eth_ref->mBbaMem[BBA_NAFR_PAR0], Common::MAC_ADDRESS_SIZE);
    network_ref[i].my_mac = fake_mac;
    network_ref[i].tcp_socket.setBlocking(false);

    // reply with a sin_ack
    memset(&m_in_frame, 0, 0x100);
    Common::EthernetHeader* hwpart = (Common::EthernetHeader*)&m_in_frame[0];
    Common::IPv4Header* ippart = (Common::IPv4Header*)&m_in_frame[14];
    Common::TCPHeader* tcppart = (Common::TCPHeader*)&m_in_frame[0x22];

    *hwpart = Common::EthernetHeader(IP_PROTOCOL);
    hwpart->destination = network_ref[i].bba_mac;
    hwpart->source = fake_mac;

    *ippart = Common::IPv4Header(28, IPPROTO_TCP, network_ref[i].from, network_ref[i].to);

    *tcppart =
        Common::TCPHeader(network_ref[i].from, network_ref[i].to, network_ref[i].seq_num,
                          network_ref[i].ack_num, 0x70 | TCP_FLAG_SIN | TCP_FLAG_ACK);
    u8 options[] = {0x02, 0x04, 0x05, 0xb4, 0x01, 0x01, 0x01, 0x01};
    memcpy(&m_in_frame[0x36], options, std::size(options));

    // do checksum
    tcppart->checksum = Common::ComputeTCPNetworkChecksum(network_ref[i].from, network_ref[i].to,
                                                          tcppart, 28, IPPROTO_TCP);

    network_ref[i].seq_num++;
    target = sf::IpAddress(Common::swap32(ipdata->destination_addr));
    network_ref[i].tcp_socket.connect(target, Common::swap16(tcpdata->destination_port));
    network_ref[i].ready = false;
    network_ref[i].ip = *(u32*)ipdata->destination_addr;

    memcpy(&network_ref[i].TcpBuffers[0].data, &m_in_frame, 0x3e);
    network_ref[i].TcpBuffers[0].data_size = 0x3e;
    network_ref[i].TcpBuffers[0].seq_id = network_ref[i].seq_num - 1;
    network_ref[i].TcpBuffers[0].tick = GetTickCountStd() - 1900;  // delay
    network_ref[i].TcpBuffers[0].used = true;
  }
  else
  {
    // data packet
    int i = GetTCPSlot(tcpdata->source_port, tcpdata->destination_port, *(u32*)ipdata->destination_addr);
    if (i == -1)
      return;  // not found

    int c = (tcpdata->properties & 0xf0) >> 2;  // header size
    int size = Common::swap16(ipdata->total_len) - 20 - c;
    u32 this_seq = Common::swap32(tcpdata->sequence_number);

    if (size > 0)
    {
      // only if data
      if ((int)(this_seq - network_ref[i].ack_num) >= 0)
      {
        network_ref[i].tcp_socket.send(data, size);
        network_ref[i].ack_num += size;
      }

      // send ack
      memset(&m_in_frame, 0, 0x36);
      Common::EthernetHeader* hwpart = (Common::EthernetHeader*)&m_in_frame[0];
      Common::IPv4Header* ippart = (Common::IPv4Header*)&m_in_frame[14];
      Common::TCPHeader* tcppart = (Common::TCPHeader*)&m_in_frame[0x22];

      *hwpart = Common::EthernetHeader(IP_PROTOCOL);
      memcpy(&hwpart->destination, &m_eth_ref->mBbaMem[BBA_NAFR_PAR0], Common::MAC_ADDRESS_SIZE);
      hwpart->source = fake_mac;

      *ippart = Common::IPv4Header(20, IPPROTO_TCP, network_ref[i].from, network_ref[i].to);

      *tcppart =
          Common::TCPHeader(network_ref[i].from, network_ref[i].to, network_ref[i].seq_num,
                            network_ref[i].ack_num, TCP_FLAG_ACK);
      tcppart->checksum = Common::ComputeTCPNetworkChecksum(network_ref[i].from, network_ref[i].to,
                                                            tcppart, 20, IPPROTO_TCP);

      WriteToQueue((char*)&m_in_frame, 0x36);
    }

    // clear any ack data
    if (tcpdata->properties & TCP_FLAG_ACK)
    {
      for (int l = 0; l < MAX_TCP_BUFFER; l++)
      {
        if (network_ref[i].TcpBuffers[l].used)
        {
          if (network_ref[i].TcpBuffers[l].seq_id < Common::swap32(tcpdata->acknowledgement_number))
          {
            network_ref[i].TcpBuffers[l].used = false;  // confirmed data received
            if (!network_ref[i].ready && !network_ref[i].TcpBuffers[0].used)
              network_ref[i].ready = true;
          }
        }
      }
    }
  }
}

void CEXIETHERNET::BuiltInBBAInterface::HandleUDPFrame(Common::EthernetHeader* hwdata,
                                                       Common::IPv4Header* ipdata,
                                                       Common::UDPHeader* udpdata, char* data)
{
  sf::IpAddress target;

  if (*(u32*)ipdata->destination_addr == 0)
    *(u32*)ipdata->destination_addr = 0x0101a8c0;
  // dns request
  int i = GetAvaibleSlot(udpdata->destination_port);
  if (network_ref[i].ip == 0)
  {
    network_ref[i].ip = *(u32*)ipdata->destination_addr;  // change for ip
    network_ref[i].local = udpdata->source_port;
    network_ref[i].remote = udpdata->destination_port;
    network_ref[i].type = 17;
    memcpy(&network_ref[i].bba_mac, &m_eth_ref->mBbaMem[BBA_NAFR_PAR0], Common::MAC_ADDRESS_SIZE);
    network_ref[i].my_mac = fake_mac;
    network_ref[i].from.sin_addr.s_addr = *(u32*)&ipdata->destination_addr;
    network_ref[i].from.sin_port = udpdata->destination_port;
    network_ref[i].to.sin_addr.s_addr = *(u32*)&ipdata->source_addr;
    network_ref[i].to.sin_port = udpdata->source_port;
    network_ref[i].udp_socket.setBlocking(false);
    if (network_ref[i].udp_socket.bind(sf::Socket::AnyPort) != sf::Socket::Done)
    {
      ERROR_LOG_FMT(SP1, "Couldn't open UDP socket");
      return;
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

  network_ref[i].udp_socket.send(data, Common::swap16(udpdata->length) - 8, target,
                     Common::swap16(udpdata->destination_port));
}

bool CEXIETHERNET::BuiltInBBAInterface::SendFrame(const u8* frame, u32 size)
{
  Common::EthernetHeader* hwdata;
  Common::IPv4Header* ipdata;
  Common::UDPHeader* udpdata;
  Common::TCPHeader* tcpdata;

  int offset = 0;
  mtx.lock();
  isSent = true;
  memmove(m_out_frame, frame, size);

  // handle the packet data
  hwdata = (Common::EthernetHeader*)&m_out_frame[0];
  if (hwdata->ethertype == 0x08)  // IPV4
  {
    // IP sub
    ipdata = (Common::IPv4Header*)&m_out_frame[14];
    switch (ipdata->protocol)
    {
    case IPPROTO_UDP:
      udpdata = (Common::UDPHeader*)&m_out_frame[14 + ((ipdata->version_ihl & 0xf) * 4)];
      offset = 14 + ((ipdata->version_ihl & 0xf) * 4) + 8;
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
      tcpdata = (Common::TCPHeader*)&m_out_frame[14 + ((ipdata->version_ihl & 0xf) * 4)];
      offset = 14 + ((ipdata->version_ihl & 0xf) * 4) + (((tcpdata->properties >> 4) & 0xf) * 4);
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
  mtx.unlock();
  return true;
}

size_t TryGetDataFromSocket(StackRef* ref, u8* buffer)
{
  Common::EthernetHeader* hwdata;
  Common::IPv4Header* ipdata;
  Common::UDPHeader* udpdata;
  Common::TCPHeader* tcpdata;
  size_t datasize = 0;
  unsigned short prt;
  int l;

  switch (ref->type)
  {
  case IPPROTO_UDP:
    prt = ref->remote;
    ref->udp_socket.receive(&buffer[0x2a], 1500, datasize, ref->target, prt);
    if (datasize > 0)
    {
      memset(&buffer[0], 0, 0x2a);
      hwdata = (Common::EthernetHeader*)&buffer[0];
      ipdata = (Common::IPv4Header*)&buffer[14];
      udpdata = (Common::UDPHeader*)&buffer[0x22];

      // build header
      *hwdata = Common::EthernetHeader(IP_PROTOCOL);
      hwdata->destination = ref->bba_mac;
      hwdata->source = ref->my_mac;

      *ipdata = Common::IPv4Header((u16)(datasize + 8), IPPROTO_UDP, ref->from, ref->to);
      
      *udpdata = Common::UDPHeader(ref->from, ref->to, (u16)datasize);
      udpdata->checksum = Common::ComputeTCPNetworkChecksum(ref->from, ref->to, udpdata,
                                                            (u16)(datasize + 8), IPPROTO_UDP);
      datasize += 0x2a;
    }
    break;

  case IPPROTO_TCP:
    l = MAX_TCP_BUFFER;
    sf::Socket::Status st = sf::Socket::Status::Done;
    for (l = 0; l < MAX_TCP_BUFFER; l++)
    {
      if (!ref->TcpBuffers[l].used)
      {
        break;  // free holder
      }
    }
    datasize = 0;
    if ((l < MAX_TCP_BUFFER) && ref->ready)
      st = ref->tcp_socket.receive(&buffer[0x36], 1460, datasize);

    if (datasize > 0)
    {
      memset(&buffer[0], 0, 0x36);
      hwdata = (Common::EthernetHeader*)&buffer[0];
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
      ref->TcpBuffers[l].seq_id = ref->seq_num;
      ref->TcpBuffers[l].data_size = (u16)datasize + 0x36;
      ref->TcpBuffers[l].tick = GetTickCountStd();
      memcpy(&ref->TcpBuffers[l].data[0], &buffer[0], datasize + 0x36);
      ref->TcpBuffers[l].seq_id = ref->seq_num;
      ref->TcpBuffers[l].used = true;
      ref->seq_num += (u32)datasize;
      datasize += 0x36;
    }
    if (ref->delay == 0)
    {
      if (st == sf::Socket::Disconnected || st == sf::Socket::Error)
      {
        ref->ip = 0;
        ref->tcp_socket.disconnect();
        datasize = BuildFINFrame(ref, (char*)&buffer[0]);
      }
    }
    else
    {
      ref->delay--;
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
    if (self->m_read_enabled.IsSet())
    {
      size_t datasize = 0;

      u8 wp, rp;
      wp = self->m_eth_ref->page_ptr(BBA_RWP);
      rp = self->m_eth_ref->page_ptr(BBA_RRP);
      if (rp > wp)
        wp += 16;

      // process queue file first
      if ((wp - rp) < 8)
      {
        if (self->queue_read != self->queue_write)
        {
          datasize = self->queue_data_size[self->queue_read];
          memcpy(self->m_eth_ref->mRecvBuffer.get(), &self->queue_data[self->queue_read], datasize);
          self->queue_read++;
          self->queue_read &= 15;
        }
        else
        {
          // test connections data
          for (int i = 0; i < std::size(self->network_ref); i++)
          {
            if (self->network_ref[i].ip != 0)
            {
              datasize =
                  TryGetDataFromSocket(&self->network_ref[i], self->m_eth_ref->mRecvBuffer.get());
              if (datasize > 0)
                break;
            }
          }
        }

        // test and add any sleeping tcp data
        for (int c = 0; c < std::size(self->network_ref); c++)
        {
          if (self->network_ref[c].ip != 0 && self->network_ref[c].type == IPPROTO_TCP)
          {
            for (int l = 0; l < MAX_TCP_BUFFER; l++)
            {
              if (self->network_ref[c].TcpBuffers[l].used)
              {
                if (GetTickCountStd() - self->network_ref[c].TcpBuffers[l].tick > 2000)
                {
                  self->network_ref[c].TcpBuffers[l].tick = GetTickCountStd();
                  // late data, resend
                  if (((self->queue_write + 1) & 15) != self->queue_read)
                  {
                    self->mtx.lock();
                    self->queue_data_size[self->queue_write] =
                        self->network_ref[c].TcpBuffers[l].data_size;
                    memcpy(&self->queue_data[self->queue_write],
                           &self->network_ref[c].TcpBuffers[l].data[0],
                           self->network_ref[c].TcpBuffers[l].data_size);
                    self->queue_write = (self->queue_write + 1) & 15;
                    self->mtx.unlock();
                  }
                }
              }
            }
          }
        }

        if (datasize > 0)
        {
          self->mtx.lock();
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
          self->mtx.unlock();
        }
      }
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
  for (int i = 0; i < std::size(network_ref); i++)
  {
    if (network_ref[i].ip != 0)
    {
      network_ref[i].type == IPPROTO_TCP ? network_ref[i].tcp_socket.disconnect() :
                                           network_ref[i].udp_socket.unbind();
    }
    network_ref[i].ip = 0;
    queue_read = 0;
    queue_write = 0;
  }
}
}  // namespace ExpansionInterface
