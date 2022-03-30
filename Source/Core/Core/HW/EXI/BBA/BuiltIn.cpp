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

#include <SFML/Network.hpp>

#include <cstring>

// BBA implementation with UDP interface to XLink Kai PC/MAC/RaspberryPi client
// For more information please see: https://www.teamxlink.co.uk/wiki/Emulator_Integration_Protocol
// Still have questions? Please email crunchbite@teamxlink.co.uk
// When editing this file please maintain proper capitalization of "XLink Kai"

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

  active = false;

  //kill all active socket


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

  for (int i = 0; i < size/2; i++)
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
  queue_write++;
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
    AddDHCPOption(reply, 1, 53, request->options[2] == 1 ? 2 : 4, 0, 0, 0);
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

void CEXIETHERNET::BuiltInBBAInterface::HandleTCPFrame(net_hw_lvl* hwdata, net_ipv4_lvl* ipdata,
                                                       net_tcp_lvl* tcpdata, char * data)
{
  if (tcpdata->flag_length & TCP_FLAG_SIN)
  {
    //new connection
    int i = GetAvaibleSlot(0);
    NetRef[i].ip = 
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

  memmove(m_out_frame, frame, size);

  INFO_LOG_FMT(SP1, "SendFrame {:x}\n{}", size, ArrayToString((u8 *) &m_out_frame, size, 16));

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

      }
      else 
      {
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
            return false;
          }
        }
        if (htons(udpdata->dest_port) == 53)
        {
          target = sf::IpAddress("149.56.167.128");  // dns server ip
        }
        else
        {
          target = sf::IpAddress(ipdata->dest_ip);
        }

        udp_socket[i].send(&m_out_frame[offset], htons(udpdata->size) - 8, target,
                           htons(udpdata->dest_port));
      }


      break;

    case 6:   //tcp
      tcpdata = (net_tcp_lvl*)&m_out_frame[14 + ((ipdata->header & 0xf) * 4)];
      offset = 14 + ((ipdata->header & 0xf) * 4) + (((tcpdata->flag_length >> 4) & 0xf)*4);
      HandleTCPFrame(hwdata, ipdata, tcpdata, &m_out_frame[offset]);
      break;
    }

  }
  if (hwdata->protocol == 0x608) //arp
  {
    net_arp_lvl* arpdata = (net_arp_lvl*)&m_out_frame[14];
    HandleARP(hwdata, arpdata);
  }

  m_eth_ref->SendComplete();
  return true;
}

void CEXIETHERNET::BuiltInBBAInterface::ReadThreadHandler(CEXIETHERNET::BuiltInBBAInterface* self)
{
  sf::IpAddress sender;
  unsigned char buf[2048];
  net_hw_lvl* hwdata;
  net_ipv4_lvl* ipdata;
  net_udp_lvl* udpdata;

  while (!self->m_read_thread_shutdown.IsSet())
  {
    if (!self->IsActivated())
      break;
    //test every open connection for incomming data / disconnection
    if (self->m_read_enabled.IsSet())
    {
      size_t datasize = 0;
      // process queue file first
      if (self->queue_read != self->queue_write)
      {
        datasize = self->queue_data_size[self->queue_read];
        memcpy(self->m_eth_ref->mRecvBuffer.get(), &self->queue_data[self->queue_read], datasize);
        self->queue_read++;
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
                memset(&buf, 0, 0x2a);
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
                memcpy(self->m_eth_ref->mRecvBuffer.get(), &buf,
                       datasize);
                break;
              }
                
            }
          }
        }
      }

      if (datasize > 0)
      {
        self->m_eth_ref->mRecvBufferLength = (u32)datasize;
        self->m_eth_ref->RecvHandlePacket();
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
}
}  // namespace ExpansionInterface
