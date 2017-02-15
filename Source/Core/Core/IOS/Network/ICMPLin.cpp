// Copyright 2012 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/IOS/Network/ICMP.h"

// Currently stubbed. AFAIK (delroth) there is no way to send ICMP echo
// requests without being root on current Linux versions.

int icmp_echo_req(const u32 s, const sockaddr_in* addr, const u8* data, const u32 data_length)
{
  // TODO
  return -1;
}

int icmp_echo_rep(const u32 s, sockaddr_in* addr, const u32 timeout, const u32 data_length)
{
  // TODO
  return -1;
}
