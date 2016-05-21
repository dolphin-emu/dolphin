// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Core/IPC_HLE/WII_IPC_HLE_Device.h"

#ifdef _WIN32
#include <ws2tcpip.h>
#endif

enum NET_IOCTL
{
	IOCTL_SO_ACCEPT = 1,
	IOCTL_SO_BIND,
	IOCTL_SO_CLOSE,
	IOCTL_SO_CONNECT,
	IOCTL_SO_FCNTL,
	IOCTL_SO_GETPEERNAME,
	IOCTL_SO_GETSOCKNAME,
	IOCTL_SO_GETSOCKOPT,
	IOCTL_SO_SETSOCKOPT,
	IOCTL_SO_LISTEN,
	IOCTL_SO_POLL,
	IOCTLV_SO_RECVFROM,
	IOCTLV_SO_SENDTO,
	IOCTL_SO_SHUTDOWN,
	IOCTL_SO_SOCKET,
	IOCTL_SO_GETHOSTID,
	IOCTL_SO_GETHOSTBYNAME,
	IOCTL_SO_GETHOSTBYADDR,
	IOCTLV_SO_GETNAMEINFO,
	IOCTL_SO_UNK14,
	IOCTL_SO_INETATON,
	IOCTL_SO_INETPTON,
	IOCTL_SO_INETNTOP,
	IOCTLV_SO_GETADDRINFO,
	IOCTL_SO_SOCKATMARK,
	IOCTLV_SO_UNK1A,
	IOCTLV_SO_UNK1B,
	IOCTLV_SO_GETINTERFACEOPT,
	IOCTLV_SO_SETINTERFACEOPT,
	IOCTL_SO_SETINTERFACE,
	IOCTL_SO_STARTUP,
	IOCTL_SO_ICMPSOCKET = 0x30,
	IOCTLV_SO_ICMPPING,
	IOCTL_SO_ICMPCANCEL,
	IOCTL_SO_ICMPCLOSE
};

class CWII_IPC_HLE_Device_net_ip_top final : public IWII_IPC_HLE_Device
{
public:
	CWII_IPC_HLE_Device_net_ip_top(u32 device_id, const std::string& name);
	virtual ~CWII_IPC_HLE_Device_net_ip_top();

	IPCCommandResult Open(u32 command_address, u32 mode) override;
	IPCCommandResult Close(u32 command_address, bool force) override;
	IPCCommandResult IOCtl(u32 command_address) override;
	IPCCommandResult IOCtlV(u32 command_address) override;

	u32 Update() override;

private:
#ifdef _WIN32
	WSADATA InitData;
#endif
};

