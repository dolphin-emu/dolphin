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

#include "EmuMain.h"
#include <stdio.h>
#include <string.h>

struct UDPWiimote::_d
{
	sock_t sockfd;
};

int UDPWiimote::noinst=0;

UDPWiimote::UDPWiimote(const char *port) : d(new _d) ,x(0),y(0),z(0),nunX(0),nunY(0),pointerX(-0.1),pointerY(-0.1),nunMask(0),mask(0)
{
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
	
    if ((rv = getaddrinfo(NULL, port, &hints, &servinfo)) != 0) {
//        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		cleanup;
        err=-1;
		return;
    }
	
    // loop through all the results and bind to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if (((d->sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)),d->sockfd) == BAD_SOCK) {
			continue;
        }
		
        if (bind(d->sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(d->sockfd);
			continue;
        }
		
        break;
    }
	
    if (p == NULL) {
		cleanup;
		err=-2;
        return;
    }
	
    freeaddrinfo(servinfo);
	blockingoff(d->sockfd);
	err=0;
	return;
}

UDPWiimote::~UDPWiimote()
{
	close(d->sockfd);
	cleanup;
	delete d;
}

int UDPWiimote::readPack(void * data, int *size)
{
    int numbytes;
    size_t addr_len;
    struct sockaddr_storage their_addr;
	addr_len = sizeof their_addr;
    if ((numbytes = recvfrom(d->sockfd, (dataz)data, (*size) , 0,
							 (struct sockaddr *)&their_addr, (socklen_t*)&addr_len)) == -1) {
		if (ERRNO==EWOULDBLOCK)
			return -1;
        return -2;
    } else
		(*size)=numbytes;
	return 0;
}

#define ACCEL_FLAG (1<<0)
#define BUTT_FLAG (1<<1)
#define IR_FLAG (1<<2)
#define NUN_FLAG (1<<3)

void UDPWiimote::update()
{
	u8 bf[64];
	int size=60;
	int res=0;
	u8 time=0;
	int nopack=0;
	for (int i=0; (res=readPack(&bf,&size)),(i<100)&&(res!=-1); (res<-1)?i++:0)
	{
		if (res==0)
		{
			if (bf[0]==0xde)
			{
				if (bf[1]==0)
					time=0;
				if (bf[1]>=time) //packet timestamp. assures order is maintained
				{
					nopack++;
					time=bf[1];
					u32 *p=(u32*)(&bf[3]);
					if (bf[2]&ACCEL_FLAG)
					{
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
						mask=ntohl(*p); p++;
					}
					if (bf[2]&IR_FLAG)
					{
						pointerX=((double)((s32)ntohl(*p)))/1048576; p++;
						pointerY=((double)((s32)ntohl(*p)))/1048576; p++;
					}
					if (bf[2]&NUN_FLAG)
					{
						nunMask=*((u8*)p); p=(u32*)(((u8*)p)+1);
						nunX=((double)((s32)ntohl(*p)))/1048576; p++;
						nunY=((double)((s32)ntohl(*p)))/1048576; p++;
					}
				}
			}
		}
		if (res==-2)
		{
			ERROR_LOG(WIIMOTE,"UDPWii Packet error");
		} 
	}
	//NOTICE_LOG(WIIMOTE,"UDPWii update result:np:%d x:%f y:%f z:%f nx:%f ny:%f px:%f py:%f bmask:%x nmask:%x",
	//	nopack, x, y, z, nunX, nunY, pointerX, pointerY, mask, nunMask);
}

void UDPWiimote::getAccel(int &_x, int &_y, int &_z)
{
	//NOTICE_LOG(WIIMOTE,"%lf %lf %lf",_x, _y, _z);
	float xg = WiiMoteEmu::g_wm.cal_g.x;
	float yg = WiiMoteEmu::g_wm.cal_g.y;
	float zg = WiiMoteEmu::g_wm.cal_g.z;
	_x = WiiMoteEmu::g_wm.cal_zero.x + (int)(xg * x);
	_y = WiiMoteEmu::g_wm.cal_zero.y + (int)(yg * y);
	_z = WiiMoteEmu::g_wm.cal_zero.z + (int)(zg * z);
}

u32 UDPWiimote::getButtons()
{
	return mask;
}

void UDPWiimote::getIR(float &_x, float &_y)
{
	_x=(float)pointerX;
	_y=(float)pointerY;
}

void UDPWiimote::getNunchuck(float &_x, float &_y, u8 &_mask)
{
	_x=(float)nunX;
	_y=(float)nunY;
	_mask=nunMask;
}
