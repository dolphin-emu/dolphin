#pragma once
#include <SFML/Network.hpp>
#include "Common/CommonTypes.h"

#define TCP_FLAG_SIN 0x200
#define TCP_FLAG_ACK 0x1000
#define TCP_FLAG_PSH 0x800
#define TCP_FLAG_FIN 0x100
#define TCP_FLAG_RST 0x400

struct TcpBuffer
{
  bool used;
  unsigned long long tick;
  u32 seq_id;
  u16 data_size;
  char data[2048];
};
struct StackRef
{
  unsigned int ip;
  unsigned short local;
  unsigned short remote;
  unsigned short type;
  sf::IpAddress target;
  u32 seq_num;
  u32 ack_num;
  u32 ack_base;
  u16 window_size;
  u16 delay;
  TcpBuffer TcpBuffers[4];
  bool ready;
};

struct net_hw_lvl
{
  unsigned char dest_mac[6];
  unsigned char src_mac[6];
  u16 protocol;
};

#pragma pack(push, 1)
struct net_arp_lvl  // 0x0806
{
  u16 hw_type;
  u16 p_type;
  u8 wh_size;
  u8 p_size;
  u16 opcode;
  unsigned char send_mac[6];
  u32 send_ip;
  unsigned char targ_mac[6];
  u32 targ_ip;
};
#pragma pack(pop)

struct net_ipv4_lvl  // 0x0800
{
  u16 header;
  u16 size;
  u16 seqId;
  u16 flag_frag;
  u8 ttl;
  u8 protocol;
  u16 crc;
  u32 src_ip;
  u32 dest_ip;
};

struct net_udp_lvl  // 17
{
  u16 src_port;
  u16 dest_port;
  u16 size;
  u16 crc;
};

struct net_tcp_lvl  // 6
{
  u16 src_port;
  u16 dest_port;
  u32 seq_num;
  u32 ack_num;
  u16 flag_length;
  u16 win_size;
  u16 crc;
  u16 ptr;
  u8 options[32];
};

struct net_dhcp
{
  u8 msg_type;
  u8 hw_type;
  u8 hw_addr;
  u8 hops;
  u32 transaction_id;
  u16 secondes;
  u16 boot_flag;
  u32 client_ip;
  u32 your_ip;
  u32 server_ip;
  u32 relay_ip;
  unsigned char client_mac[6];
  unsigned char padding[10];
  unsigned char hostname[0x40];
  unsigned char boot_file[0x80];
  u32 magic_cookie;  // 0x63538263
  u8 options[300];
};
