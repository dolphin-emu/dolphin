// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "WII_Socket.h"
#include "WII_IPC_HLE.h"
#include "WII_IPC_HLE_Device.h"
// No Wii socket support while using NetPlay or TAS
#include "NetPlayClient.h"
#include "Movie.h"

using WII_IPC_HLE_Interface::ECommandType;
using WII_IPC_HLE_Interface::COMMAND_IOCTL;
using WII_IPC_HLE_Interface::COMMAND_IOCTLV;

#ifdef _WIN32
#define ERRORCODE(name) WSA ## name
#define EITHER(win32, posix) win32
#else
#define ERRORCODE(name) name
#define EITHER(win32, posix) posix
#endif

char* WiiSockMan::DecodeError(s32 ErrorCode)
{
#ifdef _WIN32
	static char Message[1024];
	// If this program was multi-threaded, we'd want to use FORMAT_MESSAGE_ALLOCATE_BUFFER
	// instead of a static buffer here.
	// (And of course, free the buffer when we were done with it)
	FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS |
		FORMAT_MESSAGE_MAX_WIDTH_MASK, NULL, ErrorCode,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPWSTR)Message, 1024, NULL);
	return Message;
#else
	return strerror(ErrorCode);
#endif
}


s32 WiiSockMan::getNetErrorCode(s32 ret, std::string caller, bool isRW)
{
#ifdef _WIN32
	s32 errorCode = WSAGetLastError();
#else
	s32 errorCode = errno;
#endif
	if (ret >= 0)
		return ret;

	DEBUG_LOG(WII_IPC_NET, "%s failed with error %d: %s, ret= %d",
		caller.c_str(), errorCode, DecodeError(errorCode), ret);

	switch (errorCode)
	{
	case ERRORCODE(EMSGSIZE):
		ERROR_LOG(WII_IPC_NET, "Find out why this happened, looks like PEEK failure?");
		return -1; // Should be -SO_EMSGSIZE
	case EITHER(WSAENOTSOCK, EBADF):
		return -SO_EBADF;
	case ERRORCODE(EADDRINUSE):
		return -SO_EADDRINUSE;
	case ERRORCODE(ECONNRESET):
		return -SO_ECONNRESET;
	case ERRORCODE(EISCONN):
		return -SO_EISCONN;
	case ERRORCODE(ENOTCONN):
		return -SO_EAGAIN; // After proper blocking SO_EAGAIN shouldn't be needed...
	case ERRORCODE(EINPROGRESS):
		return -SO_EINPROGRESS;
	case ERRORCODE(EALREADY):
		return -SO_EALREADY;
	case ERRORCODE(EACCES):
		return -SO_EACCES;
	case ERRORCODE(ECONNREFUSED):
		return -SO_ECONNREFUSED;
	case ERRORCODE(ENETUNREACH):
		return -SO_ENETUNREACH;
	case ERRORCODE(EHOSTUNREACH):
		return -SO_EHOSTUNREACH;
	case EITHER(WSAEWOULDBLOCK, EAGAIN):
		if (isRW){
			return -SO_EAGAIN;  // EAGAIN
		}else{
			return -SO_EINPROGRESS; // EINPROGRESS
		}
	default:
		return -1;
	}

}

WiiSocket::~WiiSocket()
{
	if (fd >= 0)
	{
		(void)closeFd();
	}
}

void WiiSocket::setFd(s32 s)
{
	if (fd >= 0)
		(void)closeFd();

	nonBlock = false;
	fd = s;

	// Set socket to NON-BLOCK
#ifdef _WIN32
	u_long iMode = 1;
	ioctlsocket(fd, FIONBIO, &iMode);
#else
	int flags;
	if (-1 == (flags = fcntl(fd, F_GETFL, 0)))
		flags = 0;
	fcntl(fd, F_SETFL, flags | O_NONBLOCK);
#endif
}

s32 WiiSocket::closeFd()
{
	s32 ReturnValue = 0;
	if (fd >= 0)
	{
#ifdef _WIN32
		s32 ret = closesocket(fd);
#else
		s32 ret = close(fd);
#endif
		ReturnValue = WiiSockMan::getNetErrorCode(ret, "delSocket", false);
	}
	else
	{
		ReturnValue = WiiSockMan::getNetErrorCode(EITHER(WSAENOTSOCK, EBADF), "delSocket", false);
	}
	fd = -1;
	return ReturnValue;
}

s32 WiiSocket::_fcntl(u32 cmd, u32 arg)
{
#define F_GETFL			3
#define F_SETFL			4
#define F_NONBLOCK		4
	s32 ret = 0;
	if (cmd == F_GETFL)
	{
		ret = nonBlock ? F_NONBLOCK : 0;
	}
	else if (cmd == F_SETFL)
	{
		nonBlock = (arg & F_NONBLOCK) == F_NONBLOCK;
	}
	else
	{
		ERROR_LOG(WII_IPC_NET, "SO_FCNTL unknown command");
	}

	WARN_LOG(WII_IPC_NET, "IOCTL_SO_FCNTL(%08x, %08X, %08X)",
		fd, cmd, arg);

	return ret;
}

void WiiSocket::update(bool read, bool write, bool except)
{
	auto it = pending_sockops.begin();
	while (it != pending_sockops.end())
	{
		s32 ReturnValue = 0;
		bool forceNonBlock = false;
		ECommandType ct = static_cast<ECommandType>(Memory::Read_U32(it->_CommandAddress));
		if (!it->is_ssl && ct == COMMAND_IOCTL)
		{
			u32 BufferIn = Memory::Read_U32(it->_CommandAddress + 0x10);
			switch(it->net_type)
			{
			case IOCTL_SO_FCNTL:
			{
				u32 cmd	= Memory::Read_U32(BufferIn + 4);
				u32 arg	= Memory::Read_U32(BufferIn + 8);
				ReturnValue = _fcntl(cmd, arg);
				break;
			}
			case IOCTL_SO_BIND:
			{
				//TODO: tidy
				bind_params *addr = (bind_params*)Memory::GetPointer(BufferIn);
				GC_sockaddr_in addrPC;
				memcpy(&addrPC, addr->name, sizeof(GC_sockaddr_in));
				sockaddr_in address;
				address.sin_family = addrPC.sin_family;
				address.sin_addr.s_addr = addrPC.sin_addr.s_addr_;
				address.sin_port = addrPC.sin_port;
				
				int ret = bind(fd, (sockaddr*)&address, sizeof(address));
				ReturnValue = WiiSockMan::getNetErrorCode(ret, "SO_BIND", false);

				WARN_LOG(WII_IPC_NET, "IOCTL_SO_BIND (%08X %s:%d) = %d ", fd, 
					inet_ntoa(address.sin_addr), Common::swap16(address.sin_port), ret);
				break;
			}
			case IOCTL_SO_CONNECT:
			{
				
				//struct sockaddr_in echoServAddr;
				u32 has_addr = Memory::Read_U32(BufferIn + 0x04);
				sockaddr_in serverAddr;

				u8 addr[28];
				Memory::ReadBigEData(addr, BufferIn + 0x08, sizeof(addr));

				if (has_addr != 1)
				{
					ReturnValue = -1;
					break;
				}

				memset(&serverAddr, 0, sizeof(serverAddr));
				memcpy(&serverAddr, addr, addr[0]);
				// GC/Wii sockets have a length param as well, we dont really care :)
				serverAddr.sin_family = serverAddr.sin_family >> 8;
				
				int ret = connect(fd, (sockaddr*)&serverAddr, sizeof(serverAddr));
				ReturnValue = WiiSockMan::getNetErrorCode(ret, "SO_CONNECT", false);

				WARN_LOG(WII_IPC_NET,"IOCTL_SO_CONNECT (%08x, %s:%d)",
					fd, inet_ntoa(serverAddr.sin_addr), Common::swap16(serverAddr.sin_port));
				break;
			}
			default:
				break;
			}

			// Fix blocking error codes
			if (!nonBlock)
			{
				if (it->net_type == IOCTL_SO_CONNECT 
					&& ReturnValue == -SO_EISCONN)
				{
					ReturnValue = SO_SUCCESS;
				}
			}
		}
		else if (ct == COMMAND_IOCTLV)
		{
			SIOCtlVBuffer CommandBuffer(it->_CommandAddress);
			u32 BufferIn = 0, BufferIn2 = 0;
			u32 BufferInSize = 0, BufferInSize2 = 0;
			u32 BufferOut = 0, BufferOut2 = 0;
			u32 BufferOutSize = 0, BufferOutSize2 = 0;
			
			if (CommandBuffer.InBuffer.size() > 0)
			{
				BufferIn = CommandBuffer.InBuffer.at(0).m_Address;
				BufferInSize = CommandBuffer.InBuffer.at(0).m_Size;
			}
			
			if (CommandBuffer.PayloadBuffer.size() > 0)
			{
				BufferOut = CommandBuffer.PayloadBuffer.at(0).m_Address;
				BufferOutSize = CommandBuffer.PayloadBuffer.at(0).m_Size;
			}
			
			if (CommandBuffer.PayloadBuffer.size() > 1)
			{
				BufferOut2 = CommandBuffer.PayloadBuffer.at(1).m_Address;
				BufferOutSize2 = CommandBuffer.PayloadBuffer.at(1).m_Size;
			}
			
			if (CommandBuffer.InBuffer.size() > 1)
			{
				BufferIn2 = CommandBuffer.InBuffer.at(1).m_Address;
				BufferInSize2 = CommandBuffer.InBuffer.at(1).m_Size;
			}

			if (it->is_ssl)
			{
				int sslID = Memory::Read_U32(BufferOut) - 1;
				if (SSLID_VALID(sslID))
				{
					switch(it->ssl_type)
					{
					case IOCTLV_NET_SSL_DOHANDSHAKE:
					{
					
						int ret = ssl_handshake(&CWII_IPC_HLE_Device_net_ssl::_SSL[sslID].ctx);
						switch (ret)
						{
						case 0:
							Memory::Write_U32(SSL_OK, BufferIn);
							break;
						case POLARSSL_ERR_NET_WANT_READ:
							Memory::Write_U32(SSL_ERR_RAGAIN, BufferIn);
							if (!nonBlock)
								ReturnValue = SSL_ERR_RAGAIN;
							break;
						case POLARSSL_ERR_NET_WANT_WRITE:
							Memory::Write_U32(SSL_ERR_WAGAIN, BufferIn);
							if (!nonBlock)
								ReturnValue = SSL_ERR_WAGAIN;
							break;
						default:
							Memory::Write_U32(SSL_ERR_FAILED, BufferIn);
							break;
						}
					
						WARN_LOG(WII_IPC_SSL, "IOCTLV_NET_SSL_DOHANDSHAKE = (%d) "
							"BufferIn: (%08x, %i), BufferIn2: (%08x, %i), "
							"BufferOut: (%08x, %i), BufferOut2: (%08x, %i)",
							ret,
							BufferIn, BufferInSize, BufferIn2, BufferInSize2,
							BufferOut, BufferOutSize, BufferOut2, BufferOutSize2);
						break;
					}
					case IOCTLV_NET_SSL_WRITE:
					{
						int ret = ssl_write(&CWII_IPC_HLE_Device_net_ssl::_SSL[sslID].ctx, Memory::GetPointer(BufferOut2), BufferOutSize2);
			
#ifdef DEBUG_SSL
						File::IOFile("ssl_write.bin", "ab").WriteBytes(Memory::GetPointer(BufferOut2), BufferOutSize2);
#endif
						if (ret >= 0)
						{
							// Return bytes written or SSL_ERR_ZERO if none
							Memory::Write_U32((ret == 0) ? SSL_ERR_ZERO : ret, BufferIn);
						}
						else 
						{
							switch (ret)
							{
							case POLARSSL_ERR_NET_WANT_READ:
								Memory::Write_U32(SSL_ERR_RAGAIN, BufferIn);
								if (!nonBlock)
									ReturnValue = SSL_ERR_RAGAIN;
								break;
							case POLARSSL_ERR_NET_WANT_WRITE:
								Memory::Write_U32(SSL_ERR_WAGAIN, BufferIn);
								if (!nonBlock)
									ReturnValue = SSL_ERR_WAGAIN;
								break;
							default:
								Memory::Write_U32(SSL_ERR_FAILED, BufferIn);
								break;
							}
						}
						break;
					}
					case IOCTLV_NET_SSL_READ:
					{
						int ret = ssl_read(&CWII_IPC_HLE_Device_net_ssl::_SSL[sslID].ctx, Memory::GetPointer(BufferIn2), BufferInSize2);
#ifdef DEBUG_SSL
						if (ret > 0)
						{
							File::IOFile("ssl_read.bin", "ab").WriteBytes(Memory::GetPointer(BufferIn2), ret);
						}
#endif
						if (ret >= 0)
						{
							// Return bytes read or SSL_ERR_ZERO if none
							Memory::Write_U32((ret == 0) ? SSL_ERR_ZERO : ret, BufferIn);
						}
						else 
						{
							switch (ret)
							{
							case POLARSSL_ERR_NET_WANT_READ:
								Memory::Write_U32(SSL_ERR_RAGAIN, BufferIn);
								if (!nonBlock)
									ReturnValue = SSL_ERR_RAGAIN;
								break;
							case POLARSSL_ERR_NET_WANT_WRITE:
								Memory::Write_U32(SSL_ERR_WAGAIN, BufferIn);
								if (!nonBlock)
									ReturnValue = SSL_ERR_WAGAIN;
								break;
							default:
								Memory::Write_U32(SSL_ERR_FAILED, BufferIn);
								break;
							}
						}
						break;
					}
					default:
						break;
					}
				}
				else
				{
					Memory::Write_U32(SSL_ERR_ID, BufferIn);
				}
			} 
			else
			{
				switch (it->net_type)
				{
				case IOCTLV_SO_SENDTO:
				{

					char * data = (char*)Memory::GetPointer(BufferIn);
					u32 flags = Common::swap32(BufferIn2 + 0x04);
					u32 has_destaddr = Common::swap32(BufferIn2 + 0x08);
					// Act as non blocking when SO_MSG_NONBLOCK is specified
					forceNonBlock = ((flags & SO_MSG_NONBLOCK) == SO_MSG_NONBLOCK);

					// send/sendto only handles PEEK
					flags &= SO_MSG_PEEK | SO_MSG_OOB;
					
					u8 destaddr[28];
					struct sockaddr_in* addr = (struct sockaddr_in*)&destaddr;
					if (has_destaddr)
					{
						Memory::ReadBigEData((u8*)&destaddr, BufferIn2 + 0x0C, BufferInSize2 - 0x0C);
						addr->sin_family = addr->sin_family >> 8;
					}

					int ret = sendto(fd, data, BufferInSize, flags, 
						has_destaddr ? (struct sockaddr*)addr : NULL, 
						has_destaddr ? sizeof(sockaddr) :  0);
					ReturnValue = WiiSockMan::getNetErrorCode(ret, "SO_SENDTO", true);

					WARN_LOG(WII_IPC_NET,
						"%s = %d Socket: %08x, BufferIn: (%08x, %i), BufferIn2: (%08x, %i), %u.%u.%u.%u",
						has_destaddr ? "IOCTLV_SO_SENDTO " : "IOCTLV_SO_SEND ",
						ReturnValue, fd, BufferIn, BufferInSize,
						BufferIn2, BufferInSize2,
						addr->sin_addr.s_addr & 0xFF,
						(addr->sin_addr.s_addr >> 8) & 0xFF,
						(addr->sin_addr.s_addr >> 16) & 0xFF,
						(addr->sin_addr.s_addr >> 24) & 0xFF
						);
					break;
				}
				case IOCTLV_SO_RECVFROM:
				{
					u32 sock	= Memory::Read_U32(BufferIn);
					u32 flags	= Memory::Read_U32(BufferIn + 4);
			
					char *buf	= (char *)Memory::GetPointer(BufferOut);
					int len		= BufferOutSize;
					struct sockaddr_in addr;
					memset(&addr, 0, sizeof(sockaddr_in));
					socklen_t fromlen = 0;
			
					if (BufferOutSize2 != 0)
					{
						fromlen = BufferOutSize2 >= sizeof(struct sockaddr) ? BufferOutSize2 : sizeof(struct sockaddr);
					}

					// Act as non blocking when SO_MSG_NONBLOCK is specified
					forceNonBlock = ((flags & SO_MSG_NONBLOCK) == SO_MSG_NONBLOCK);
			
					// recv/recvfrom only handles PEEK
					flags &= SO_MSG_PEEK | SO_MSG_OOB;
#ifdef _WIN32
					if (flags & MSG_PEEK){
						unsigned long totallen = 0;
						ioctlsocket(sock, FIONREAD, &totallen);
						ReturnValue = totallen;
						break;
					}
#endif
					int ret = recvfrom(sock, buf, len, flags,
									fromlen ? (struct sockaddr*) &addr : NULL,
									fromlen ? &fromlen : 0);
					ReturnValue = WiiSockMan::getNetErrorCode(ret, fromlen ? "SO_RECVFROM" : "SO_RECV", true);

			
					WARN_LOG(WII_IPC_NET, "%s(%d, %p) Socket: %08X, Flags: %08X, "
					"BufferIn: (%08x, %i), BufferIn2: (%08x, %i), "
					"BufferOut: (%08x, %i), BufferOut2: (%08x, %i)", 
					fromlen ? "IOCTLV_SO_RECVFROM " : "IOCTLV_SO_RECV ",
					ReturnValue, buf, sock, flags,
					BufferIn, BufferInSize, BufferIn2, BufferInSize2,
					BufferOut, BufferOutSize, BufferOut2, BufferOutSize2);

					if (BufferOutSize2 != 0)
					{
						addr.sin_family = (addr.sin_family << 8) | (BufferOutSize2&0xFF);
						Memory::WriteBigEData((u8*)&addr, BufferOut2, BufferOutSize2);
					}
					break;
				}
				default:
					break;
				}
			}
			
		}

		if ( nonBlock || forceNonBlock
			|| (!it->is_ssl && ReturnValue != -SO_EAGAIN && ReturnValue != -SO_EINPROGRESS && ReturnValue != -SO_EALREADY)
			|| (it->is_ssl && ReturnValue != SSL_ERR_WAGAIN && ReturnValue != SSL_ERR_RAGAIN))
		{
			DEBUG_LOG(WII_IPC_NET, "IOCTL(V) Sock: %d ioctl/v: %d returned: %d nonBlock: %d forceNonBlock: %d", 
				fd, it->is_ssl ? it->ssl_type : it->net_type, ReturnValue, nonBlock, forceNonBlock);
			WiiSockMan::EnqueueReply(it->_CommandAddress, ReturnValue);
			it = pending_sockops.erase(it);
		}
		else
		{
			++it;
		}
	}
}

void WiiSocket::doSock(u32 _CommandAddress, NET_IOCTL type)
{
	sockop so = {_CommandAddress, false};
	so.net_type = type;
	pending_sockops.push_back(so);
}

void WiiSocket::doSock(u32 _CommandAddress, SSL_IOCTL type)
{
	sockop so = {_CommandAddress, true};
	so.ssl_type = type;
	pending_sockops.push_back(so);
}

s32 WiiSockMan::newSocket(s32 af, s32 type, s32 protocol)
{
	if (NetPlay::IsNetPlayRunning()
		|| Movie::IsRecordingInput()
		|| Movie::IsPlayingInput())
	{
		return SO_ENOMEM;
	}

	s32 fd = (s32)socket(af, type, protocol);
	s32 ret = getNetErrorCode(fd, "newSocket", false);
	if (ret >= 0)
	{
		WiiSocket& sock = WiiSockets[ret];
		sock.setFd(ret);
	}
	return ret;
}

s32 WiiSockMan::delSocket(s32 s)
{
	s32 ReturnValue = WiiSockets[s].closeFd();
	WiiSockets.erase(s);
	return ReturnValue;
}

void WiiSockMan::Update()
{
	s32 nfds = 0;
	fd_set read_fds, write_fds, except_fds;
	struct timeval t = {0,0};
	FD_ZERO(&read_fds);
	FD_ZERO(&write_fds);
	FD_ZERO(&except_fds);
	for (auto it = WiiSockets.begin(); it != WiiSockets.end(); ++it)
	{
		WiiSocket& sock = it->second;
		if (sock.valid())
		{
			FD_SET(sock.fd, &read_fds);
			FD_SET(sock.fd, &write_fds);
			FD_SET(sock.fd, &except_fds);
			nfds = max(nfds, sock.fd+1);
		}
		else
		{
			// Good time to clean up invalid sockets.
			WiiSockets.erase(sock.fd);
		}
	}
	s32 ret = select(nfds, &read_fds, &write_fds, &except_fds, &t);

	if (ret >= 0)
	{
		for (auto it = WiiSockets.begin(); it != WiiSockets.end(); ++it)
		{
			WiiSocket& sock = it->second;
			sock.update(
				FD_ISSET(sock.fd, &read_fds) != 0, 
				FD_ISSET(sock.fd, &write_fds) != 0, 
				FD_ISSET(sock.fd, &except_fds) != 0
			);
		}
	}
	else
	{
		for (auto it = WiiSockets.begin(); it != WiiSockets.end(); ++it)
		{
			it->second.update(false, false, false);
		}
	}
}

void WiiSockMan::EnqueueReply(u32 CommandAddress, s32 ReturnValue)
{
	Memory::Write_U32(8, CommandAddress);
	// IOS seems to write back the command that was responded to
	Memory::Write_U32(Memory::Read_U32(CommandAddress), CommandAddress + 8);
	
	// Return value
	Memory::Write_U32(ReturnValue, CommandAddress + 4);
	
	WII_IPC_HLE_Interface::EnqReply(CommandAddress);
}


#undef ERRORCODE
#undef EITHER
