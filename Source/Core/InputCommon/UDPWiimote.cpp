// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <functional>
#include <list>
#include <string>

#include "Common/Thread.h"
#include "Common/Timer.h"

#include "InputCommon/UDPWiimote.h"

#ifdef _WIN32

#include <winsock2.h>
#include <ws2tcpip.h>
#define sock_t SOCKET
#define ERRNO WSAGetLastError()
#undef EWOULDBLOCK
#define EWOULDBLOCK WSAEWOULDBLOCK
#define BAD_SOCK INVALID_SOCKET
#define close(x) closesocket(x)
#define cleanup do {noinst--; if (noinst==0) WSACleanup();} while (0)
#define blockingoff(sock) ioctlsocket(sock, FIONBIO, &iMode)
#define dataz char*
#ifdef _MSC_VER
#pragma comment (lib, "Ws2_32.lib")
#endif

#else

#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#define BAD_SOCK -1
#define ERRNO errno
#define cleanup noinst--
#define blockingoff(sock) fcntl(sock, F_SETFL, O_NONBLOCK)
#define dataz void*
#define sock_t int

#endif


struct UDPWiimote::_d
{
	std::thread thread;
	std::list<sock_t> sockfds;
	std::mutex termLock, mutex, nameMutex;
	volatile bool exit;
	sock_t bipv4_fd, bipv6_fd;
};

int UDPWiimote::noinst = 0;

UDPWiimote::UDPWiimote(const std::string& _port, const std::string& name, int _index) :
	port(_port), displayName(name),	d(new _d),
	waX(0), waY(0), waZ(1), naX(0), naY(0), naZ(-1), nunX(0), nunY(0),
	pointerX(1001.0f / 2), pointerY(0), nunMask(0), wiimoteMask(0), index(_index), int_port(atoi(_port.c_str()))
{

	static bool sranded=false;
	if (!sranded)
	{
		srand((unsigned int)time(nullptr));
		sranded=true;
	}
	bcastMagic=rand() & 0xFFFF;

	#ifdef _WIN32
	u_long iMode = 1;
	#endif
	struct addrinfo hints, *servinfo, *p;
	int rv;

	#ifdef _WIN32
	if (noinst==0)
	{
		WORD sockVersion;
		WSADATA wsaData;
		sockVersion = MAKEWORD(2, 2);
		WSAStartup(sockVersion, &wsaData);
	}
	#endif

	noinst++;

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_flags = AI_PASSIVE; // use my IP

	if (!int_port)
	{
		cleanup;
		err=-1;
		return;
	}

	if ((rv = getaddrinfo(nullptr, _port.c_str(), &hints, &servinfo)) != 0)
	{
		cleanup;
		err=-1;
		return;
	}

	// loop through all the results and bind to everything we can
	for (p = servinfo; p != nullptr; p = p->ai_next)
	{
		sock_t sock;
		if ((sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == BAD_SOCK)
		{
			continue;
		}

		if (bind(sock, p->ai_addr, (int)p->ai_addrlen) == -1)
		{
			close(sock);
			continue;
		}
		d->sockfds.push_back(sock);
	}

	if (d->sockfds.empty())
	{
		cleanup;
		err=-2;
		return;
	}

	freeaddrinfo(servinfo);
	err=0;
	d->exit=false;
	initBroadcastIPv4();
	initBroadcastIPv6();

	std::lock_guard<std::mutex> lk(d->termLock);
	d->thread = std::thread(std::mem_fn(&UDPWiimote::mainThread), this);

	return;
}

void UDPWiimote::mainThread()
{
	std::unique_lock<std::mutex> lk(d->termLock);

	Common::Timer time;
	fd_set fds;
	struct timeval timeout;
	timeout.tv_sec=0;
	timeout.tv_usec=0;
	time.Update();
	do
	{
		int maxfd=0;
		FD_ZERO(&fds);
		for (auto& fd : d->sockfds)
		{
			FD_SET(fd,&fds);
#ifndef _WIN32
			if (fd>=maxfd)
				maxfd=(fd)+1;
#endif
		}

		u64 tleft=timeout.tv_sec*1000+timeout.tv_usec/1000;
		u64 telapsed=time.GetTimeDifference();
		time.Update();
		if (tleft<=telapsed)
		{
			timeout.tv_sec=1;
			timeout.tv_usec=500000;
			broadcastPresence();
		}
		else
		{
			tleft-=telapsed;
			timeout.tv_sec=(long)(tleft/1000);
			timeout.tv_usec=(tleft%1000)*1000;
		}

		lk.unlock(); //VERY hacky. don't like it
		if (d->exit) return;
		int rt=select(maxfd,&fds,nullptr,nullptr,&timeout);
		if (d->exit) return;
		lk.lock();
		if (d->exit) return;

		if (rt)
		{
			for (sock_t fd : d->sockfds)
			{
				if (FD_ISSET(fd,&fds))
				{
					u8 bf[64];
					int size=60;
					size_t addr_len;
					struct sockaddr_storage their_addr;
					addr_len = sizeof their_addr;
					if ((size = recvfrom(fd,
										 (dataz)bf,
										 size , 0,(struct sockaddr *)&their_addr, (socklen_t*)&addr_len)) == -1)
					{
						ERROR_LOG(WIIMOTE,"UDPWii Packet error");
					}
					else
					{
						std::lock_guard<std::mutex> lkm(d->mutex);
						if (pharsePacket(bf,size)==0)
						{
							//NOTICE_LOG(WIIMOTE,"UDPWII New pack");
						}
						else
						{
							//NOTICE_LOG(WIIMOTE,"UDPWII Wrong pack format... ignoring");
						}
					}
				}
			}
		}
	} while (!(d->exit));
}

UDPWiimote::~UDPWiimote()
{
	d->exit = true;
	{
	std::lock_guard<std::mutex> lk(d->termLock);
	d->thread.join();
	}
	for (auto& elem : d->sockfds)
		close(elem);
	close(d->bipv4_fd);
	close(d->bipv6_fd);
	cleanup;
	delete d;
}

#define ACCEL_FLAG    (1 << 0)
#define BUTT_FLAG     (1 << 1)
#define IR_FLAG       (1 << 2)
#define NUN_FLAG      (1 << 3)
#define NUNACCEL_FLAG (1 << 4)

int UDPWiimote::pharsePacket(u8 * bf, size_t size)
{
	if (size < 3)
		return -1;

	if (bf[0] != 0xde)
		return -1;
	//if (bf[1]==0)
	//    time=0;
	//if (bf[1]<time) //NOT LONGER NEEDED TO ALLOW MULTIPLE IPHONES ON A SINGLE PORT
	//    return -1;
	//time=bf[1];
	u32 *p=(u32*)(&bf[3]);
	if (bf[2] & ACCEL_FLAG)
	{
		if ((size-(((u8*)p)-bf)) < 12)
			return -1;

		double ux,uy,uz;
		ux=(double)((s32)ntohl(*p)); p++;
		uy=(double)((s32)ntohl(*p)); p++;
		uz=(double)((s32)ntohl(*p)); p++;
		waX=ux/1048576; //packet accel data
		waY=uy/1048576;
		waZ=uz/1048576;
	}

	if (bf[2] & BUTT_FLAG)
	{
		if ((size-(((u8*)p)-bf)) < 4)
			return -1;

		wiimoteMask = ntohl(*p); p++;
	}

	if (bf[2] & IR_FLAG)
	{
		if ((size-(((u8*)p)-bf)) < 8)
			return -1;

		pointerX=((double)((s32)ntohl(*p)))/1048576; p++;
		pointerY=((double)((s32)ntohl(*p)))/1048576; p++;
	}

	if (bf[2] & NUN_FLAG)
	{
		if ((size-(((u8*)p)-bf)) < 9)
			return -1;

		nunMask=*((u8*)p); p=(u32*)(((u8*)p)+1);
		nunX=((double)((s32)ntohl(*p)))/1048576; p++;
		nunY=((double)((s32)ntohl(*p)))/1048576; p++;
	}

	if (bf[2] & NUNACCEL_FLAG)
	{
		if ((size-(((u8*)p)-bf)) < 12)
			return -1;

		double ux,uy,uz;
		ux=(double)((s32)ntohl(*p)); p++;
		uy=(double)((s32)ntohl(*p)); p++;
		uz=(double)((s32)ntohl(*p)); p++;
		naX=ux/1048576; //packet accel data
		naY=uy/1048576;
		naZ=uz/1048576;
	}

	return 0;
}

void UDPWiimote::initBroadcastIPv4()
{
	d->bipv4_fd=socket(AF_INET, SOCK_DGRAM, 0);
	if (d->bipv4_fd == BAD_SOCK)
	{
		WARN_LOG(WIIMOTE,"socket() failed");
		return;
	}

	int broad=1;
	if (setsockopt(d->bipv4_fd,SOL_SOCKET,SO_BROADCAST, (const dataz)(&broad), sizeof broad) == -1)
	{
		WARN_LOG(WIIMOTE,"setsockopt(SO_BROADCAST) failed");
		return;
	}
}

void UDPWiimote::broadcastIPv4(const void * data, size_t size)
{

	struct sockaddr_in their_addr;
	their_addr.sin_family = AF_INET;
	their_addr.sin_port = htons(4431);
	their_addr.sin_addr.s_addr = INADDR_BROADCAST;
	memset(their_addr.sin_zero, '\0', sizeof their_addr.sin_zero);

	int num;
	if ((num=sendto(d->bipv4_fd,(const dataz)data,(int)size,0,(struct sockaddr *) &their_addr, sizeof their_addr)) == -1)
	{
		WARN_LOG(WIIMOTE,"sendto() failed");
		return;
	}
}

void UDPWiimote::initBroadcastIPv6()
{
	//TODO: IPv6 support
}

void UDPWiimote::broadcastIPv6(const void * data, size_t size)
{
	//TODO: IPv6 support
}

void UDPWiimote::broadcastPresence()
{
	size_t slen;
	u8 bf[512];
	bf[0]=0xdf; //magic number
	*((u16*)(&(bf[1])))=htons(bcastMagic); //unique per-wiimote 16-bit ID
	bf[3]=(u8)index; //wiimote index
	*((u16*)(&(bf[4])))=htons(int_port); //port
	{
	std::lock_guard<std::mutex> lk(d->nameMutex);
	slen=displayName.size();
	if (slen>=256)
		slen=255;
	bf[6]=(u8)slen; //display name size (max 255)
	memcpy(&(bf[7]),displayName.c_str(),slen); //display name
	}
	broadcastIPv4(bf,7+slen);
	broadcastIPv6(bf,7+slen);
}

void UDPWiimote::getAccel(float* x, float* y, float* z)
{
	std::lock_guard<std::mutex> lk(d->mutex);
	*x = (float)waX;
	*y = (float)waY;
	*z = (float)waZ;
}

u32 UDPWiimote::getButtons()
{
	u32 msk;
	std::lock_guard<std::mutex> lk(d->mutex);
	msk = wiimoteMask;
	return msk;
}

void UDPWiimote::getIR(float* x, float* y)
{
	std::lock_guard<std::mutex> lk(d->mutex);
	*x = (float)pointerX;
	*y = (float)pointerY;
}

void UDPWiimote::getNunchuck(float* x, float* y, u8* mask)
{
	std::lock_guard<std::mutex> lk(d->mutex);
	*x = (float)nunX;
	*y = (float)nunY;
	*mask = nunMask;
}

void UDPWiimote::getNunchuckAccel(float* x, float* y, float* z)
{
	std::lock_guard<std::mutex> lk(d->mutex);
	*x = (float)naX;
	*y = (float)naY;
	*z = (float)naZ;
}

const std::string& UDPWiimote::getPort()
{
	return port;
}

void UDPWiimote::changeName(const std::string& name)
{
	std::lock_guard<std::mutex> lk(d->nameMutex);
	displayName = name;
}
