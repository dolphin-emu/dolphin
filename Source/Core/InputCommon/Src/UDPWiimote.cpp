#include "UDPWiimote.h"

#ifdef _WIN32

#include <winsock2.h>
#include <ws2tcpip.h>
#define sock_t SOCKET
#define ERRNO WSAGetLastError()
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

#include <stdlib.h>
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

#include "Thread.h"
#include <stdio.h>
#include <string.h>
#include <list>

struct UDPWiimote::_d
{
	Common::Thread * thread;
	std::list<sock_t> sockfds;
	Common::CriticalSection termLock,mutex;
	volatile bool exit;
};

int UDPWiimote::noinst=0;

void _UDPWiiThread(void* arg)
{
	((UDPWiimote*)arg)->mainThread();
	//NOTICE_LOG(WIIMOTE,"UDPWii thread stopped");
}

THREAD_RETURN UDPWiiThread(void* arg)
{
	_UDPWiiThread(arg);
	return 0;
}

UDPWiimote::UDPWiimote(const char *_port) : 
	port(_port),
	d(new _d) ,x(0),y(0),z(0),naX(0),naY(0),naZ(0),nunX(0),nunY(0),
	pointerX(-0.1),pointerY(-0.1),nunMask(0),mask(0),time(0)
{
	#ifdef _WIN32
	u_long iMode = 1;
	#endif
	struct addrinfo hints, *servinfo, *p;
	int rv;
	d->thread=NULL;

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
	//PanicAlert("UDPWii instantiated");

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_flags = AI_PASSIVE; // use my IP
	
	if ((rv = getaddrinfo(NULL, _port, &hints, &servinfo)) != 0) {
		cleanup;
		err=-1;
		return;
	}

	// loop through all the results and bind to everything we can
	for(p = servinfo; p != NULL; p = p->ai_next) {
		sock_t sock;
		if ((sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == BAD_SOCK) {
			continue;
		}
		
		if (bind(sock, p->ai_addr, p->ai_addrlen) == -1) {
			close(sock);
			continue;
		}
		

		//NOTICE_LOG(WIIMOTE,"UDPWii new listening sock");
		d->sockfds.push_back(sock);
	}
	
	if (d->sockfds.empty()) {
		cleanup;
		err=-2;
		return;
	}
	
	freeaddrinfo(servinfo);
	err=0;
	d->exit=false;
//	NOTICE_LOG(WIIMOTE,"UDPWii thread starting");
	d->termLock.Enter();
	d->thread = new Common::Thread(UDPWiiThread,this);
	d->termLock.Leave();
	return;
}

void UDPWiimote::mainThread()
{
	d->termLock.Enter();
//	NOTICE_LOG(WIIMOTE,"UDPWii thread started");
	fd_set fds;
	struct timeval timeout;
	timeout.tv_sec=1;
	timeout.tv_usec=500000;
	//Common::Thread * thisthr= d->thread;
	do
	{
		int maxfd=0;
		FD_ZERO(&fds);
		for (std::list<sock_t>::iterator i=d->sockfds.begin(); i!=d->sockfds.end(); i++)
		{
			FD_SET(*i,&fds);
#ifndef _WIN32
			if (*i>=maxfd)
				maxfd=(*i)+1;
#endif
		}
		d->termLock.Leave();
		if (d->exit) return;
		int rt=select(maxfd,&fds,NULL,NULL,&timeout);
		if (d->exit) return;
		d->termLock.Enter();
		if (d->exit) return;
		if (rt)
		{
			for (std::list<sock_t>::iterator i=d->sockfds.begin(); i!=d->sockfds.end(); i++)
				if (FD_ISSET(*i,&fds))
				{
					sock_t fd=*i;
					u8 bf[64];
					int size=60;
					size_t addr_len;
					struct sockaddr_storage their_addr;
					addr_len = sizeof their_addr;
					if ((size = recvfrom(fd, (dataz)bf, size , 0,(struct sockaddr *)&their_addr, (socklen_t*)&addr_len)) == -1) 
					{
						ERROR_LOG(WIIMOTE,"UDPWii Packet error");
					}
					else
					{
						d->mutex.Enter();
						if (pharsePacket(bf,size)==0)
						{
							//NOTICE_LOG(WIIMOTE,"UDPWII New pack");
						} else {
							//NOTICE_LOG(WIIMOTE,"UDPWII Wrong pack format... ignoring");
						}
						d->mutex.Leave();
					}
				}
		} else {
			broadcastPresence();
		}
	} while (!(d->exit));
	d->termLock.Leave();
	//delete thisthr;
}

UDPWiimote::~UDPWiimote()
{
	d->exit=true;
	d->thread->WaitForDeath();
	d->termLock.Enter();
	d->termLock.Leave();
	for (std::list<sock_t>::iterator i=d->sockfds.begin(); i!=d->sockfds.end(); i++)
		close(*i);
	cleanup;
	delete d;
	//PanicAlert("UDPWii destructed");
}

#define ACCEL_FLAG (1<<0)
#define BUTT_FLAG (1<<1)
#define IR_FLAG (1<<2)
#define NUN_FLAG (1<<3)
#define NUNACCEL_FLAG (1<<4)

int UDPWiimote::pharsePacket(u8 * bf, size_t size)
{
	if (size<3) return -1;
	if (bf[0]!=0xde)
		return -1;
	if (bf[1]==0)
		time=0;
	if (bf[1]<time)
		return -1;
	time=bf[1];
	u32 *p=(u32*)(&bf[3]);
	if (bf[2]&ACCEL_FLAG)
	{
		if ((size-(((u8*)p)-bf))<12) return -1;
		double ux,uy,uz;
		ux=(double)((s32)ntohl(*p)); p++;
		uy=(double)((s32)ntohl(*p)); p++;
		uz=(double)((s32)ntohl(*p)); p++;
		x=ux/1048576; //packet accel data
		y=uy/1048576;
		z=uz/1048576;
	}
	if (bf[2]&BUTT_FLAG)
	{
		if ((size-(((u8*)p)-bf))<4) return -1;
		mask=ntohl(*p); p++;
	}
	if (bf[2]&IR_FLAG)
	{
		if ((size-(((u8*)p)-bf))<8) return -1;
		pointerX=((double)((s32)ntohl(*p)))/1048576; p++;
		pointerY=((double)((s32)ntohl(*p)))/1048576; p++;
	}
	if (bf[2]&NUN_FLAG)
	{
		if ((size-(((u8*)p)-bf))<9) return -1;
		nunMask=*((u8*)p); p=(u32*)(((u8*)p)+1);
		nunX=((double)((s32)ntohl(*p)))/1048576; p++;
		nunY=((double)((s32)ntohl(*p)))/1048576; p++;
	}
	if (bf[2]&NUNACCEL_FLAG)
	{
		if ((size-(((u8*)p)-bf))<12) return -1;
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


void UDPWiimote::broadcastPresence()
{
//	NOTICE_LOG(WIIMOTE,"UDPWii broadcasting presence");
}

void UDPWiimote::getAccel(float &_x, float &_y, float &_z)
{
	d->mutex.Enter();
	_x=x;
	_y=y;
	_z=z;
	d->mutex.Leave();
	//NOTICE_LOG(WIIMOTE,"%lf %lf %lf",_x, _y, _z);
}

u32 UDPWiimote::getButtons()
{
	u32 msk;
	d->mutex.Enter();
	msk=mask;
	d->mutex.Leave();
	return msk;
}

void UDPWiimote::getIR(float &_x, float &_y)
{
	d->mutex.Enter();
	_x=(float)pointerX;
	_y=(float)pointerY;
	d->mutex.Leave();
}

void UDPWiimote::getNunchuck(float &_x, float &_y, u8 &_mask)
{
	d->mutex.Enter();
	_x=(float)nunX;
	_y=(float)nunY;
	_mask=nunMask;
	d->mutex.Leave();
}

void UDPWiimote::getNunchuckAccel(float &_x, float &_y, float &_z)
{
	d->mutex.Enter();
	_x=(float)naX;
	_y=(float)naY;
	_z=(float)naZ;
	d->mutex.Leave();
}


const char * UDPWiimote::getPort()
{
	return port.c_str();
}
