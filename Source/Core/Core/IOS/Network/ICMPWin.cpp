// Copyright 2012 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/IOS/Network/ICMP.h"

enum
{
  ICMP_ECHOREPLY = 0,
  ICMP_ECHOREQ = 8
};

enum
{
  ICMP_HDR_LEN = 4,
  IP_HDR_LEN = 20
};

#pragma pack(push, 1)
struct icmp_hdr
{
  u8 type;
  u8 code;
  u16 checksum;
  u16 id;
  u16 seq;
  char data[1];
};
#pragma pack(pop)

static u8 workspace[56];

/*
* Description:
* Calculate Internet checksum for data buffer and length (one's
* complement sum of 16-bit words). Used in IP, ICMP, UDP, IGMP.
*
* NOTE: to handle odd number of bytes, last (even) byte in
* buffer have a value of 0 (we assume that it does)
*/
u16 cksum(const u16* buffer, int length)
{
  u32 sum = 0;

  while (length > 0)
  {
    sum += *(buffer++);
    length -= 2;
  }

  sum = (sum & 0xffff) + (sum >> 16);
  sum += sum >> 16;

  return (u16)~sum;
}

int icmp_echo_req(const u32 s, const sockaddr_in* addr, const u8* data, const u32 data_length)
{
  memset(workspace, 0, sizeof(workspace));
  icmp_hdr* header = (icmp_hdr*)workspace;
  header->type = ICMP_ECHOREQ;
  header->code = 0;
  header->checksum = 0;
  memcpy(&header->id, data, data_length);

  header->checksum = cksum((u16*)header, ICMP_HDR_LEN + data_length);

  int num_bytes = sendto((SOCKET)s, (LPSTR)header, ICMP_HDR_LEN + data_length, 0, (sockaddr*)addr,
                         sizeof(sockaddr));

  if (num_bytes >= ICMP_HDR_LEN)
    num_bytes -= ICMP_HDR_LEN;

  return num_bytes;
}

int icmp_echo_rep(const u32 s, sockaddr_in* addr, const u32 timeout, const u32 data_length)
{
  memset(workspace, 0, sizeof(workspace));
  int addr_length = sizeof(sockaddr_in);
  int num_bytes = 0;

  fd_set read_fds;
  FD_ZERO(&read_fds);
  FD_SET(s, &read_fds);

  timeval t;
  t.tv_sec = timeout / 1000;
  if (select(0, &read_fds, nullptr, nullptr, &t) > 0)
  {
    num_bytes = recvfrom((SOCKET)s, (LPSTR)workspace, IP_HDR_LEN + ICMP_HDR_LEN + data_length, 0,
                         (sockaddr*)addr, &addr_length);

    // TODO do we need to memcmp the data?

    if (num_bytes >= IP_HDR_LEN + ICMP_HDR_LEN)
      num_bytes -= IP_HDR_LEN + ICMP_HDR_LEN;
  }

  return num_bytes;
}
