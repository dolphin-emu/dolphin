// Copyright 2012 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#ifdef _WIN32
#include <winsock2.h>
#else
#include <netinet/in.h>
#endif

#include "Common/CommonTypes.h"

int icmp_echo_req(const u32 s, const sockaddr_in* addr, const u8* data, const u32 data_length);
int icmp_echo_rep(const u32 s, sockaddr_in* addr, const u32 timeout, const u32 data_length);
