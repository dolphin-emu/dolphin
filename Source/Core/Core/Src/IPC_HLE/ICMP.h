#pragma once

#ifdef _WIN32
#include <winsock2.h>
#endif

#include "Common.h"

int icmp_echo_req(u32 s, sockaddr_in *addr, u8 *data, u32 data_length);
int icmp_echo_rep(u32 s, sockaddr_in *addr, const u8 *data, u32 data_length);
