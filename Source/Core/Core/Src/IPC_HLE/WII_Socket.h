// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#ifndef _WII_SOCKET_H_
#define _WII_SOCKET_H_


#ifdef _WIN32
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <iphlpapi.h>

#include "fakepoll.h"
#define MALLOC(x) HeapAlloc(GetProcessHeap(), 0, (x))
#define FREE(x) HeapFree(GetProcessHeap(), 0, (x))

#elif defined(__linux__) or defined(__APPLE__)
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#if defined(ANDROID)
#include <fcntl.h>
#else
#include <sys/fcntl.h>
#endif
#include <netinet/in.h>
#include <net/if.h>
#include <errno.h>
#include <poll.h>
#include <string.h>

typedef struct pollfd pollfd_t;
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/fcntl.h>
#include <netinet/in.h>
#include <errno.h>
#endif

#include <algorithm>    // std::for_each
#include <unordered_map>
#include <stdio.h>
#include <string>
#include <list>

#include "FileUtil.h"
#include "WII_IPC_HLE.h"
#include "WII_IPC_HLE_Device_net.h"
#include "WII_IPC_HLE_Device_net_ssl.h"

enum {
	SO_MSG_OOB		= 0x01,
	SO_MSG_PEEK		= 0x02,
	SO_MSG_NONBLOCK = 0x04,
};
enum {
	SO_SUCCESS,
	SO_E2BIG = 1,
	SO_EACCES,
	SO_EADDRINUSE,
	SO_EADDRNOTAVAIL,
	SO_EAFNOSUPPORT,
	SO_EAGAIN,
	SO_EALREADY,
	SO_EBADF,
	SO_EBADMSG,
	SO_EBUSY,
	SO_ECANCELED,
	SO_ECHILD,
	SO_ECONNABORTED,
	SO_ECONNREFUSED,
	SO_ECONNRESET,
	SO_EDEADLK,
	SO_EDESTADDRREQ,
	SO_EDOM,
	SO_EDQUOT,
	SO_EEXIST,
	SO_EFAULT,
	SO_EFBIG,
	SO_EHOSTUNREACH,
	SO_EIDRM,
	SO_EILSEQ,
	SO_EINPROGRESS,
	SO_EINTR,
	SO_EINVAL,
	SO_EIO,
	SO_EISCONN,
	SO_EISDIR,
	SO_ELOOP,
	SO_EMFILE,
	SO_EMLINK,
	SO_EMSGSIZE,
	SO_EMULTIHOP,
	SO_ENAMETOOLONG,
	SO_ENETDOWN,
	SO_ENETRESET,
	SO_ENETUNREACH,
	SO_ENFILE,
	SO_ENOBUFS,
	SO_ENODATA,
	SO_ENODEV,
	SO_ENOENT,
	SO_ENOEXEC,
	SO_ENOLCK,
	SO_ENOLINK,
	SO_ENOMEM,
	SO_ENOMSG,
	SO_ENOPROTOOPT,
	SO_ENOSPC,
	SO_ENOSR,
	SO_ENOSTR,
	SO_ENOSYS,
	SO_ENOTCONN,
	SO_ENOTDIR,
	SO_ENOTEMPTY,
	SO_ENOTSOCK,
	SO_ENOTSUP,
	SO_ENOTTY,
	SO_ENXIO,
	SO_EOPNOTSUPP,
	SO_EOVERFLOW,
	SO_EPERM,
	SO_EPIPE,
	SO_EPROTO,
	SO_EPROTONOSUPPORT,
	SO_EPROTOTYPE,
	SO_ERANGE,
	SO_EROFS,
	SO_ESPIPE,
	SO_ESRCH,
	SO_ESTALE,
	SO_ETIME,
	SO_ETIMEDOUT,
	SO_ETXTBSY,
	SO_EXDEV
};

#pragma pack(push, 1)
struct WiiInAddr
{
	u32 addr;
};

struct WiiSockAddr
{
    u8 len;
    u8 family;
    u8 data[6];
};

struct WiiSockAddrIn
{
    u8 len;
    u8 family;
    u16 port;
    WiiInAddr addr;
};
#pragma pack(pop)

class WiiSocket
{
	struct sockop{
		u32 _CommandAddress;
		bool is_ssl;
		union
		{
			NET_IOCTL net_type;
			SSL_IOCTL ssl_type;
		};
	};
private:
	s32 fd;
	bool nonBlock;
	std::list<sockop> pending_sockops;

	friend class WiiSockMan;
	void setFd(s32 s);
	s32 closeFd();
	s32 _fcntl(u32 cmd, u32 arg);

	void doSock(u32 _CommandAddress, NET_IOCTL type);
	void doSock(u32 _CommandAddress, SSL_IOCTL type);
	void update(bool read, bool write, bool except);
	bool valid() {return fd >= 0;}
public:
	WiiSocket() : fd(-1), nonBlock(false) {}
	~WiiSocket();
	void operator=(WiiSocket const&);	// Don't implement

};

class WiiSockMan
{
public:
	static s32 getNetErrorCode(s32 ret, std::string caller, bool isRW);
	static char* DecodeError(s32 ErrorCode);

	static WiiSockMan& getInstance()
	{
		static WiiSockMan instance; // Guaranteed to be destroyed.
		return instance;			// Instantiated on first use.
	}
	void Update();
	static void EnqueueReply(u32 CommandAddress, s32 ReturnValue);
	static void Convert(WiiSockAddrIn const & from, sockaddr_in& to);
	static void Convert(sockaddr_in const & from, WiiSockAddrIn& to, s32 addrlen=-1);
	// NON-BLOCKING FUNCTIONS
	s32 newSocket(s32 af, s32 type, s32 protocol);
	void addSocket(s32 fd);
	s32 delSocket(s32 s);
	s32 getLastNetError() {return errono_last;}
	void setLastNetError(s32 error) {errono_last = error;}

	void clean()
	{
		WiiSockets.clear();
	}

	template <typename T>
	void doSock(s32 sock, u32 CommandAddress, T type)
	{
		if (WiiSockets.find(sock) == WiiSockets.end())
		{
			ERROR_LOG(WII_IPC_NET,
				"doSock: Error, fd not found (%08x, %08X, %08X)",
				sock, CommandAddress, type);
			EnqueueReply(CommandAddress, -SO_EBADF);
		}
		else
		{
			WiiSockets[sock].doSock(CommandAddress, type);
		}
	}

private:
	WiiSockMan() {};                   // Constructor? (the {} brackets) are needed here.
	WiiSockMan(WiiSockMan const&);     // Don't Implement
	void operator=(WiiSockMan const&); // Don't implement
	std::unordered_map<s32, WiiSocket> WiiSockets;

	s32 errono_last;
};

#endif
