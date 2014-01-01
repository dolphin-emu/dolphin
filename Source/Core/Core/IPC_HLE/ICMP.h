#ifndef _ICMP_H_
#define _ICMP_H_

#ifdef _WIN32
#include <winsock2.h>
#else
#include <netinet/in.h>
#endif

#include "Common.h"

int icmp_echo_req(const u32 s, const sockaddr_in *addr, const u8 *data, const u32 data_length);
int icmp_echo_rep(const u32 s, sockaddr_in *addr, const u32 timeout, const u32 data_length);

#endif
