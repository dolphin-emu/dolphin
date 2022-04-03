// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HW/EXI/BBA/BuiltIn.h"
#include "Common/Assert.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Common/StringUtil.h"
#include "Core/HW/EXI/EXI_Device.h"
#include "Core/HW/EXI/EXI_DeviceEthernet.h"

#include "VideoCommon/OnScreenDisplay.h"
#include "Common/Swap.h"

#include <SFML/Network.hpp>

#include <cstring>

//#define BBA_TRACK_PAGE_PTRS



namespace ExpansionInterface
{

  u16 htons(u16 val)
{
  return (val << 8 | val >> 8);
}

bool CEXIETHERNET::BuiltInBBAInterface::Activate()
{
  if (IsActivated())
    return true;

  active = true;

  //clear all ref
  for (int i = 0; i < 10; i++)
  {
    NetRef[i].ip = 0;
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

  //kill all active socket
  for (int i = 0; i < 10; i++)
  {
    if (NetRef[i].ip != 0)
    {
      if (NetRef[i].type == 6)
      {
        tcp_socket[i].disconnect();
      }
      else
      {
        udp_socket[i].unbind();
      }
    }
    NetRef[i].ip = 0;
  }

  // Wait for read thread to exit.
  if (m_read_thread.joinable())
    m_read_thread.join();
}

bool CEXIETHERNET::BuiltInBBAInterface::IsActivated()
{
  return active;
}



u16 CalcCRC(u16* data, u8 size)
{
  u32 result = 0;
  for (int i = 0; i < size; i++)
  {
    result += *data;
    data++;
  }
  result = ((result & 0xffff) + (result >> 16)) ^ 0xffff;
  return result;
}

u16 CalcIPCRC(u16* data, u16 size, u32 dest, u32 src, u16 type)
{
  u32 result = htons(size);

  result += (dest & 0xffff) + (dest >> 16);
  result += (src & 0xffff) + (src >> 16);
  result += (type << 8);

  for (int i = 0; i < size; i+=2)
  {
    result += *data;
    data++;
  }
  result = ((result & 0xffff) + (result >> 16)) ^ 0xffff;
  return result;
}

void AddDHCPOption(net_dhcp* data, u8 size, u8 fnc, u8 d1, u8 d2, u8 d3, u8 d4)
{
  int i = 0;
  while (data->options[i] != 0)
    i += data->options[i+1]+2;
  data->options[i++] = fnc;
  data->options[i++] = size;
  if (size > 0)
    data->options[i++] = d1;
  if (size > 1)
    data->options[i++] = d2;
  if (size > 2)
    data->options[i++] = d3;
  if (size > 3)
    data->options[i++] = d4;

}

void CEXIETHERNET::BuiltInBBAInterface::WriteToQueue(char* data, int length)
{
  queue_data_size[queue_write] = length;
  memcpy(queue_data[queue_write], data, length);
  queue_write = (queue_write + 1) & 15;
#ifdef BBA_TRACK_PAGE_PTRS
  INFO_LOG_FMT(SP1, "queue_write  {:x}", queue_write);
#endif
}

void CEXIETHERNET::BuiltInBBAInterface::HandleARP(net_hw_lvl* hwdata, net_arp_lvl* arpdata)
{
  /* TODO - Handle proprely
  Right now all request send the "router" info (192.168.1.1)*/
  memset(&m_in_frame, 0, 0x30);
  net_hw_lvl* hwpart = (net_hw_lvl*)&m_in_frame[0];
  memcpy(&hwpart->dest_mac, &m_eth_ref->mBbaMem[BBA_NAFR_PAR0], 6);
  memcpy(&hwpart->src_mac, fake_mac, 6);
  hwpart->protocol = 0x608;

  net_arp_lvl* arppart = (net_arp_lvl*)&m_in_frame[14];
  arppart->hw_type = 0x100;
  arppart->p_type = 8;
  arppart->wh_size = 6;
  arppart->p_size = 4;
  arppart->opcode = 0x200;
  arppart->send_ip = 0x0101a8c0;
  arppart->targ_ip = 0xc701a8c0;
  memcpy(&arppart->targ_mac, &m_eth_ref->mBbaMem[BBA_NAFR_PAR0], 6);
  memcpy(&arppart->send_mac, fake_mac, 6);

  WriteToQueue((char*)&m_in_frame, 0x2a);
}



void CEXIETHERNET::BuiltInBBAInterface::HandleDHCP(net_hw_lvl* hwdata, net_udp_lvl* udpdata,
                                                   net_dhcp* request)
{

    net_dhcp* reply = (net_dhcp*)&m_in_frame[0x2a];
    memset(&m_in_frame, 0, 0x156);

    // build layer
    net_hw_lvl* hwpart = (net_hw_lvl*)&m_in_frame[0];
    net_ipv4_lvl* ippart = (net_ipv4_lvl*)&m_in_frame[14];
    net_udp_lvl* udppart = (net_udp_lvl*)&m_in_frame[0x22];
    memcpy(&hwpart->dest_mac, &m_eth_ref->mBbaMem[BBA_NAFR_PAR0], 6);
    memcpy(&hwpart->src_mac, fake_mac, 6);
    hwpart->protocol = 0x8;

    ippart->dest_ip = 0xc701a8c0;
    ippart->src_ip = 0x0101a8c0;
    ippart->header = 0x45;
    ippart->protocol = 17;
    ippart->seqId = htons(++ip_frame_id);
    ippart->size = htons(328);
    ippart->ttl = 64;

    udppart->dest_port = udpdata->src_port;
    udppart->src_port = htons(67);
    udppart->size = htons(308);

    reply->msg_type = 2;  // reply proposition or ack
    reply->hw_type = 1;
    reply->hw_addr = 6;
    reply->transaction_id = request->transaction_id;
    reply->your_ip = 0xc701a8c0;
    reply->server_ip = 0x0101a8c0;
    memcpy(&reply->client_mac, &hwdata->src_mac, 6);
    reply->magic_cookie = 0x63538263;
    // options
    AddDHCPOption(reply, 1, 53, request->options[2] == 1 ? 2 : 5, 0, 0, 0);
    AddDHCPOption(reply, 4, 54, 192, 168, 1, 1); //dhcp server ip
    AddDHCPOption(reply, 4, 51, 0, 1, 0x51, 0x80); //lease time 24h
    AddDHCPOption(reply, 4, 58, 0, 1, 0x51, 0x80); //renewal
    AddDHCPOption(reply, 4, 59, 0, 1, 0x51, 0x80); //rebind
    AddDHCPOption(reply, 4, 1, 255, 255, 255, 0); //submask
    AddDHCPOption(reply, 4, 28, 192, 168, 1, 255);  //broadcast ip
    AddDHCPOption(reply, 4, 6, 192, 168, 1, 1);   //dns server
    AddDHCPOption(reply, 3, 15, 0x6c, 0x61, 0x6e, 0); //domaine name "lan"
    AddDHCPOption(reply, 4, 3, 192, 168, 1, 1);        // router ip
    AddDHCPOption(reply, 0, 255, 0, 0, 0, 0); //end

    udppart->crc = CalcIPCRC((u16*)udppart, 308, ippart->dest_ip, ippart->src_ip, 17);
    ippart->crc = CalcCRC((u16*)ippart, 10);

    WriteToQueue((char*)&m_in_frame, 0x156);

}

u8 CEXIETHERNET::BuiltInBBAInterface::GetAvaibleSlot(u16 port)
{
  if (port > 0)   //existing connection?
  {
    for (int i = 0; i < 10; i++)
      if (NetRef[i].ip != 0 && NetRef[i].remote == port)
        return i;
  }
  for (int i = 0; i < 10; i++)
    if (NetRef[i].ip == 0)
      return i;
  return 0;
}

int CEXIETHERNET::BuiltInBBAInterface::GetTCPSlot(u16 port, u32 ip)
{

  for (int i = 0; i < 10; i++)
    if (NetRef[i].ip == ip && NetRef[i].remote == port)
      return i;
  
  return -1;
}

int CEXIETHERNET::BuiltInBBAInterface::BuildFINFrame(char* buf, bool ack, int i)
{
  memset(buf, 0, 0x36);
  net_hw_lvl* hwpart = (net_hw_lvl*)&buf[0];
  net_ipv4_lvl* ippart = (net_ipv4_lvl*)&buf[14];
  net_tcp_lvl* tcppart = (net_tcp_lvl*)&buf[0x22];
  memcpy(&hwpart->dest_mac, &m_eth_ref->mBbaMem[BBA_NAFR_PAR0], 6);
  memcpy(&hwpart->src_mac, fake_mac, 6);
  hwpart->protocol = 0x8;

  ippart->dest_ip = 0xc701a8c0;
  ippart->src_ip = NetRef[i].ip;
  ippart->header = 0x45;
  ippart->protocol = 6;
  ippart->seqId = htons(++ip_frame_id);
  ippart->size = htons(0x28);
  ippart->ttl = 64;
  ippart->crc = CalcCRC((u16*)ippart, 10);

  tcppart->src_port = NetRef[i].remote;
  tcppart->dest_port = NetRef[i].local;
  tcppart->seq_num = Common::swap32(NetRef[i].seq_num);
  tcppart->ack_num = Common::swap32(NetRef[i].ack_num);
  tcppart->flag_length = (0x50 | TCP_FLAG_FIN | TCP_FLAG_ACK);
  tcppart->win_size = 0x7c;
  tcppart->crc = CalcIPCRC((u16*)tcppart, 20, ippart->dest_ip, ippart->src_ip, 6);
  return 0x36;
}

void CEXIETHERNET::BuiltInBBAInterface::HandleTCPFrame(net_hw_lvl* hwdata, net_ipv4_lvl* ipdata,
                                                       net_tcp_lvl* tcpdata, char * data)
{
  sf::IpAddress target;

  if (tcpdata->flag_length & TCP_FLAG_FIN)
  {
    int i = GetTCPSlot(tcpdata->dest_port, ipdata->dest_ip);
    if (i == -1)
      return;     //not found
    //int c =
    BuildFINFrame((char*)&m_in_frame, true, i);
    WriteToQueue((char*)&m_in_frame, 0x36);
    NetRef[i].ip = 0;
    tcp_socket[i].disconnect();

  }
  else if (tcpdata->flag_length & TCP_FLAG_SIN)
  {
    //new connection
    int i = GetTCPSlot(tcpdata->dest_port, ipdata->dest_ip);
    if (i != -1)
      return;
    i = GetAvaibleSlot(0);

    NetRef[i].delay = 0x3000;
    NetRef[i].local = tcpdata->src_port;
    NetRef[i].remote = tcpdata->dest_port;
    NetRef[i].ack_num = Common::swap32(tcpdata->seq_num)+1;
    NetRef[i].seq_num = 0;
    NetRef[i].window_size = htons(tcpdata->win_size);
    NetRef[i].type = 6;
    tcp_socket[i].setBlocking(false);
    target = sf::IpAddress(Common::swap32(ipdata->dest_ip));
    tcp_socket[i].connect(target, htons(tcpdata->dest_port));
    NetRef[i].ip = ipdata->dest_ip;

    //reply with a sin_ack
    memset(&m_in_frame, 0, 0x100);
    net_hw_lvl* hwpart = (net_hw_lvl*)&m_in_frame[0];
    net_ipv4_lvl* ippart = (net_ipv4_lvl*)&m_in_frame[14];
    net_tcp_lvl* tcppart = (net_tcp_lvl*)&m_in_frame[0x22];
    memcpy(&hwpart->dest_mac, &m_eth_ref->mBbaMem[BBA_NAFR_PAR0], 6);
    memcpy(&hwpart->src_mac, fake_mac, 6);
    hwpart->protocol = 0x8;

    ippart->dest_ip = 0xc701a8c0;
    ippart->src_ip = NetRef[i].ip;
    ippart->header = 0x45;
    ippart->protocol = 6;
    ippart->seqId = htons(++ip_frame_id);
    ippart->size = htons(0x30);
    ippart->ttl = 64;
    ippart->crc = CalcCRC((u16*)ippart, 10);

    tcppart->src_port = tcpdata->dest_port;
    tcppart->dest_port = tcpdata->src_port;
    tcppart->seq_num = Common::swap32(NetRef[i].seq_num);
    tcppart->ack_num = Common::swap32(NetRef[i].ack_num);
    tcppart->flag_length = (0x70 | TCP_FLAG_SIN | TCP_FLAG_ACK);
    tcppart->win_size = 0x7c;
    tcppart->options[0] = 0x02;
    tcppart->options[1] = 0x04;
    tcppart->options[2] = 0x05;
    tcppart->options[3] = 0xb4;
    tcppart->options[4] = 0x01;
    tcppart->options[5] = 0x01;
    tcppart->options[6] = 0x01;
    tcppart->options[7] = 0x01;
    tcppart->crc = CalcIPCRC((u16*)tcppart, 28, ippart->dest_ip, ippart->src_ip, 6);

    WriteToQueue((char*)&m_in_frame, 0x3d);
    NetRef[i].seq_num++;
  }
  else
  {
    //data packet
    int i = GetTCPSlot(tcpdata->dest_port, ipdata->dest_ip);
    if (i == -1)
      return;  // not found

    int c = (tcpdata->flag_length & 0xf0) >> 2; //header size
    int size = htons(ipdata->size) - 20 - c;
    NetRef[i].window_size = htons(tcpdata->win_size);
    if (size > 0)
    {
      //only if data

      if ((int)(Common::swap32(tcpdata->seq_num) - NetRef[i].ack_num) >= 0)
      {
        tcp_socket[i].send(data, size);
        NetRef[i].ack_num += size;
#ifdef BBA_TRACK_PAGE_PTRS
        INFO_LOG_FMT(SP1, "SendData {:x}", size);
#endif
      }

      //send ack
      memset(&m_in_frame, 0, 0x36);
      net_hw_lvl* hwpart = (net_hw_lvl*)&m_in_frame[0];
      net_ipv4_lvl* ippart = (net_ipv4_lvl*)&m_in_frame[14];
      net_tcp_lvl* tcppart = (net_tcp_lvl*)&m_in_frame[0x22];
      memcpy(&hwpart->dest_mac, &m_eth_ref->mBbaMem[BBA_NAFR_PAR0], 6);
      memcpy(&hwpart->src_mac, fake_mac, 6);
      hwpart->protocol = 0x8;

      ippart->dest_ip = 0xc701a8c0;
      ippart->src_ip = NetRef[i].ip;
      ippart->header = 0x45;
      ippart->protocol = 6;
      ippart->seqId = htons(++ip_frame_id);
      ippart->size = htons(0x28);
      ippart->ttl = 64;
      ippart->crc = CalcCRC((u16*)ippart, 10);

      tcppart->src_port = NetRef[i].remote;
      tcppart->dest_port = NetRef[i].local;
      tcppart->seq_num = Common::swap32(NetRef[i].seq_num);
      tcppart->ack_num = Common::swap32(NetRef[i].ack_num);
      tcppart->flag_length = (0x50 | TCP_FLAG_ACK);
      tcppart->win_size = 0x7c;
      tcppart->crc = CalcIPCRC((u16*)tcppart, 20, ippart->dest_ip, ippart->src_ip, 6);

      WriteToQueue((char*)&m_in_frame, 0x36);
    }

  }
}


bool CEXIETHERNET::BuiltInBBAInterface::SendFrame(const u8* frame, u32 size)
{
  net_hw_lvl* hwdata;
  net_ipv4_lvl* ipdata;
  net_udp_lvl* udpdata;
  net_tcp_lvl* tcpdata;
  int offset = 0;
  int i;
  sf::IpAddress target;
  mtx.lock();
  isSent = true;
  memmove(m_out_frame, frame, size);
#ifdef BBA_TRACK_PAGE_PTRS
  INFO_LOG_FMT(SP1, "SendFrame {:x}\n{}", size, ArrayToString((u8 *) &m_out_frame, size, 16));
#endif
  DEBUG_LOG_FMT(SP1, "Frame data: {:x}\n{}", size, ArrayToString((u8*)&m_out_frame, size, 16));
  bool handled = false;
  //handle the packet data
  hwdata = (net_hw_lvl*)&m_out_frame[0];
  if (hwdata->protocol == 0x08)
  {
    //IP sub
    ipdata = (net_ipv4_lvl*)&m_out_frame[14];
    switch (ipdata->protocol)
    {
    case 17:  //udp
      udpdata = (net_udp_lvl*)&m_out_frame[14+((ipdata->header & 0xf) * 4)];
      offset = 14 + ((ipdata->header & 0xf) * 4) + 8;
      if (htons(udpdata->dest_port) == 67)
      {
        net_dhcp* request = (net_dhcp*)&m_out_frame[offset];
        HandleDHCP(hwdata, udpdata, request);
        handled = true;
      }
      else 
      {
        if (ipdata->dest_ip == 0)
          ipdata->dest_ip = 0x0101a8c0;
        //dns request
        i = GetAvaibleSlot(udpdata->dest_port);
        if (NetRef[i].ip == 0)
        {
          NetRef[i].ip = ipdata->dest_ip;  // change for ip
          NetRef[i].local = udpdata->src_port;
          NetRef[i].remote = udpdata->dest_port;
          NetRef[i].type = 17;
          udp_socket[i].setBlocking(false);
          if (udp_socket[i].bind(sf::Socket::AnyPort) != sf::Socket::Done)
          {
            ERROR_LOG_FMT(SP1, "Couldn't open UDP socket");
            mtx.unlock();
            return false;
          }
        }
        if (htons(udpdata->dest_port) == 53)
        {
          target = sf::IpAddress(m_dns_ip.c_str());  // dns server ip
        }
        else
        {
          target = sf::IpAddress(Common::swap32(ipdata->dest_ip));
        }

        udp_socket[i].send(&m_out_frame[offset], htons(udpdata->size) - 8, target,
                           htons(udpdata->dest_port));
        handled = true;
      }


      break;

    case 6:   //tcp
      tcpdata = (net_tcp_lvl*)&m_out_frame[14 + ((ipdata->header & 0xf) * 4)];
      offset = 14 + ((ipdata->header & 0xf) * 4) + (((tcpdata->flag_length >> 4) & 0xf)*4);
      HandleTCPFrame(hwdata, ipdata, tcpdata, &m_out_frame[offset]);
      handled = true;
      break;
    }

  }
  if (hwdata->protocol == 0x608) //arp
  {
    net_arp_lvl* arpdata = (net_arp_lvl*)&m_out_frame[14];
    HandleARP(hwdata, arpdata);
    handled = true;
  }
#ifdef BBA_TRACK_PAGE_PTRS
  if (!handled)
  {
    INFO_LOG_FMT(SP1, "ERROR");
  }
#endif
  m_eth_ref->SendComplete();
  mtx.unlock();
  return true;
}

void CEXIETHERNET::BuiltInBBAInterface::ReadThreadHandler(CEXIETHERNET::BuiltInBBAInterface* self)
{
  sf::IpAddress sender;
  unsigned char buf[2048];
  net_hw_lvl* hwdata;
  net_ipv4_lvl* ipdata;
  net_udp_lvl* udpdata;
  int retrigger = 0;
  bool ready = false;

  while (!self->m_read_thread_shutdown.IsSet())
  {
    //test every open connection for incomming data / disconnection
    if (self->m_read_enabled.IsSet())
    {
      size_t datasize = 0;

      u8 wp, rp;
      wp = self->m_eth_ref->page_ptr(BBA_RWP);
      rp = self->m_eth_ref->page_ptr(BBA_RRP);
      if (rp > wp)
        wp += 16;

      // process queue file first
      if ((wp-rp) < 10)
      {
#ifdef BBA_TRACK_PAGE_PTRS
        if (!ready)
        {
          ready = true;
          INFO_LOG_FMT(SP1, "BBA is ready");
        }
#endif
        if (self->queue_read != self->queue_write)
        {
          datasize = self->queue_data_size[self->queue_read];
          memcpy(self->m_eth_ref->mRecvBuffer.get(), &self->queue_data[self->queue_read], datasize);
          self->queue_read++;
          self->queue_read &= 15;
#ifdef BBA_TRACK_PAGE_PTRS
          INFO_LOG_FMT(SP1, "Send Queue {:x} {:x}", self->queue_read, datasize);
#endif
        }
        else
        {
          //test connections
          for (int i = 0; i < 10; i++)
          {
            if (self->NetRef[i].ip != 0)
            {
              if (self->NetRef[i].type == 17)
              {
                //udp
                unsigned short prt = self->NetRef[i].remote;
                self->udp_socket[i].receive(&buf[0x2a], 1500, datasize,
                                            self->NetRef[i].target, prt);
                if (datasize > 0)
                {
                  memset(&buf[0], 0, 0x2a);
                  hwdata = (net_hw_lvl*)&buf[0];
                  ipdata = (net_ipv4_lvl*)&buf[14];
                  udpdata = (net_udp_lvl*)&buf[0x22];
                  //build header
                  hwdata->protocol = 8;
                  memcpy(&hwdata->dest_mac, &self->m_eth_ref->mBbaMem[BBA_NAFR_PAR0], 6);
                  memcpy(&hwdata->src_mac, self->fake_mac, 6);
                  ipdata->crc = 0;
                  ipdata->dest_ip = 0xc701a8c0;
                  ipdata->src_ip = self->NetRef[i].ip;
                  ipdata->header = 0x45;
                  ipdata->seqId = htons(++self->ip_frame_id);
                  ipdata->size = htons((u16)(datasize + 8 + 20));
                  ipdata->ttl = 64;
                  ipdata->protocol = 17;
                  ipdata->crc = CalcCRC((u16*)ipdata, 10);
                  udpdata->size = htons((u16)(datasize + 8));
                  udpdata->dest_port = self->NetRef[i].local;
                  udpdata->src_port = self->NetRef[i].remote;
                  udpdata->crc = CalcIPCRC((u16*)udpdata, (u16)(datasize + 8), ipdata->dest_ip,
                                           ipdata->src_ip, 17);
                  datasize += 0x2a;
                  memcpy(self->m_eth_ref->mRecvBuffer.get(), &buf[0],
                         datasize);
                  break;
                }
                
              }
              else if (self->NetRef[i].type == 6)
              {
                sf::Socket::Status st = self->tcp_socket[i].receive(&buf[0x36], 1460, datasize);
                
                if (datasize > 0)
                {
                  DEBUG_LOG_FMT(SP1, "data recv: {:x}", datasize);
                  memset(&buf[0], 0, 0x36);
                  net_hw_lvl* hwpart = (net_hw_lvl*)&buf[0];
                  net_ipv4_lvl* ippart = (net_ipv4_lvl*)&buf[14];
                  net_tcp_lvl* tcppart = (net_tcp_lvl*)&buf[0x22];
                  memcpy(&hwpart->dest_mac, &self->m_eth_ref->mBbaMem[BBA_NAFR_PAR0], 6);
                  memcpy(&hwpart->src_mac, self->fake_mac, 6);
                  hwpart->protocol = 0x8;

                  ippart->dest_ip = 0xc701a8c0;
                  ippart->src_ip = self->NetRef[i].ip;
                  ippart->header = 0x45;
                  ippart->protocol = 6;
                  ippart->seqId = htons(++self->ip_frame_id);
                  ippart->size = htons((u16)(datasize + 20 + 20));
                  ippart->ttl = 64;
                  ippart->crc = CalcCRC((u16*)ippart, 10);

                  tcppart->src_port = self->NetRef[i].remote;
                  tcppart->dest_port = self->NetRef[i].local;
                  tcppart->seq_num = Common::swap32(self->NetRef[i].seq_num);
                  tcppart->ack_num = Common::swap32(self->NetRef[i].ack_num);
                  tcppart->flag_length = (0x50 | TCP_FLAG_ACK);
                  tcppart->win_size = 0x7c;
                  tcppart->crc =
                      CalcIPCRC((u16*)tcppart, (u16)(datasize+20), ippart->dest_ip, ippart->src_ip, 6);

                  self->NetRef[i].seq_num += (u32)datasize;
                  datasize += 0x36;
                  memcpy(self->m_eth_ref->mRecvBuffer.get(), &buf[0], datasize);
                  break;
                }
                if (self->NetRef[i].delay == 0)
                {
                  if (st == sf::Socket::Disconnected || st == sf::Socket::Error)
                  {
                    self->NetRef[i].ip = 0;
                    self->tcp_socket[i].disconnect();
                    datasize = self->BuildFINFrame((char*)&buf[0], false, i);
                    break;
                  }
                }
                else
                {
                  self->NetRef[i].delay--;
                }
              }
            }
          }
        }

        if (datasize > 0)
        {
          self->mtx.lock();
          self->m_eth_ref->mRecvBufferLength = (u32)datasize;
          self->m_eth_ref->RecvHandlePacket();
          retrigger = 0xf;
          ready = false;
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
  for (int i = 0; i < 10; i++)
  {
    if (NetRef[i].ip != 0)
    {
      if (NetRef[i].type == 6)
      {
        tcp_socket[i].disconnect();
      }
      else
      {
        udp_socket[i].unbind();
      }
    }
    NetRef[i].ip = 0;
    queue_read = 0;
    queue_write = 0;
  }
}
}  // namespace ExpansionInterface
