/* $Id: miniupnpc.c,v 1.111 2012/10/09 17:53:14 nanard Exp $ */
/* Project : miniupnp
 * Web : http://miniupnp.free.fr/
 * Author : Thomas BERNARD
 * copyright (c) 2005-2012 Thomas Bernard
 * This software is subjet to the conditions detailed in the
 * provided LICENSE file. */
#define __EXTENSIONS__ 1
#if !defined(MACOSX) && !defined(__sun)
#if !defined(_XOPEN_SOURCE) && !defined(__OpenBSD__) && !defined(__NetBSD__)
#ifndef __cplusplus
#define _XOPEN_SOURCE 600
#endif
#endif
#ifndef __BSD_VISIBLE
#define __BSD_VISIBLE 1
#endif
#endif

#if !defined(__DragonFly__) && !defined(__OpenBSD__) && !defined(__NetBSD__) && !defined(MACOSX) && !defined(_WIN32) && !defined(__CYGWIN__) && !defined(__sun)
#define HAS_IP_MREQN
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
/* Win32 Specific includes and defines */
#include <winsock2.h>
#include <ws2tcpip.h>
#include <io.h>
#include <iphlpapi.h>
#define snprintf _snprintf
#define strdup _strdup
#ifndef strncasecmp
#if defined(_MSC_VER) && (_MSC_VER >= 1400)
#define strncasecmp _memicmp
#else /* defined(_MSC_VER) && (_MSC_VER >= 1400) */
#define strncasecmp memicmp
#endif /* defined(_MSC_VER) && (_MSC_VER >= 1400) */
#endif /* #ifndef strncasecmp */
#define MAXHOSTNAMELEN 64
#else /* #ifdef _WIN32 */
/* Standard POSIX includes */
#include <unistd.h>
#if defined(__amigaos__) && !defined(__amigaos4__)
/* Amiga OS 3 specific stuff */
#define socklen_t int
#else
#include <sys/select.h>
#endif
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/param.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <net/if.h>
#if !defined(__amigaos__) && !defined(__amigaos4__)
#include <poll.h>
#endif
#include <strings.h>
#include <errno.h>
#define closesocket close
#endif /* #else _WIN32 */
#ifdef MINIUPNPC_SET_SOCKET_TIMEOUT
#include <sys/time.h>
#endif
#if defined(__amigaos__) || defined(__amigaos4__)
/* Amiga OS specific stuff */
#define TIMEVAL struct timeval
#endif

#include "miniupnpc.h"
#include "minissdpc.h"
#include "miniwget.h"
#include "minisoap.h"
#include "minixml.h"
#include "upnpcommands.h"
#include "connecthostport.h"
#include "receivedata.h"

#ifdef _WIN32
#define PRINT_SOCKET_ERROR(x)    printf("Socket error: %s, %d\n", x, WSAGetLastError());
#else
#define PRINT_SOCKET_ERROR(x) perror(x)
#endif

#define SOAPPREFIX "s"
#define SERVICEPREFIX "u"
#define SERVICEPREFIX2 'u'

/* root description parsing */
LIBSPEC void parserootdesc(const char * buffer, int bufsize, struct IGDdatas * data)
{
	struct xmlparser parser;
	/* xmlparser object */
	parser.xmlstart = buffer;
	parser.xmlsize = bufsize;
	parser.data = data;
	parser.starteltfunc = IGDstartelt;
	parser.endeltfunc = IGDendelt;
	parser.datafunc = IGDdata;
	parser.attfunc = 0;
	parsexml(&parser);
#ifdef DEBUG
	printIGD(data);
#endif
}

/* simpleUPnPcommand2 :
 * not so simple !
 * return values :
 *   pointer - OK
 *   NULL - error */
char * simpleUPnPcommand2(int s, const char * url, const char * service,
		       const char * action, struct UPNParg * args,
		       int * bufsize, const char * httpversion)
{
	char hostname[MAXHOSTNAMELEN+1];
	unsigned short port = 0;
	char * path;
	char soapact[128];
	char soapbody[2048];
	char * buf;
    int n;

	*bufsize = 0;
	snprintf(soapact, sizeof(soapact), "%s#%s", service, action);
	if(args==NULL)
	{
		/*soapbodylen = */snprintf(soapbody, sizeof(soapbody),
						"<?xml version=\"1.0\"?>\r\n"
	    	              "<" SOAPPREFIX ":Envelope "
						  "xmlns:" SOAPPREFIX "=\"http://schemas.xmlsoap.org/soap/envelope/\" "
						  SOAPPREFIX ":encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">"
						  "<" SOAPPREFIX ":Body>"
						  "<" SERVICEPREFIX ":%s xmlns:" SERVICEPREFIX "=\"%s\">"
						  "</" SERVICEPREFIX ":%s>"
						  "</" SOAPPREFIX ":Body></" SOAPPREFIX ":Envelope>"
					 	  "\r\n", action, service, action);
	}
	else
	{
		char * p;
		const char * pe, * pv;
		int soapbodylen;
		soapbodylen = snprintf(soapbody, sizeof(soapbody),
						"<?xml version=\"1.0\"?>\r\n"
	    	            "<" SOAPPREFIX ":Envelope "
						"xmlns:" SOAPPREFIX "=\"http://schemas.xmlsoap.org/soap/envelope/\" "
						SOAPPREFIX ":encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">"
						"<" SOAPPREFIX ":Body>"
						"<" SERVICEPREFIX ":%s xmlns:" SERVICEPREFIX "=\"%s\">",
						action, service);
		p = soapbody + soapbodylen;
		while(args->elt)
		{
			/* check that we are never overflowing the string... */
			if(soapbody + sizeof(soapbody) <= p + 100)
			{
				/* we keep a margin of at least 100 bytes */
				return NULL;
			}
			*(p++) = '<';
			pe = args->elt;
			while(*pe)
				*(p++) = *(pe++);
			*(p++) = '>';
			if((pv = args->val))
			{
				while(*pv)
					*(p++) = *(pv++);
			}
			*(p++) = '<';
			*(p++) = '/';
			pe = args->elt;
			while(*pe)
				*(p++) = *(pe++);
			*(p++) = '>';
			args++;
		}
		*(p++) = '<';
		*(p++) = '/';
		*(p++) = SERVICEPREFIX2;
		*(p++) = ':';
		pe = action;
		while(*pe)
			*(p++) = *(pe++);
		strncpy(p, "></" SOAPPREFIX ":Body></" SOAPPREFIX ":Envelope>\r\n",
		        soapbody + sizeof(soapbody) - p);
	}
	if(!parseURL(url, hostname, &port, &path, NULL)) return NULL;
	if(s < 0) {
		s = connecthostport(hostname, port, 0);
		if(s < 0) {
			/* failed to connect */
			return NULL;
		}
	}

	n = soapPostSubmit(s, path, hostname, port, soapact, soapbody, httpversion);
	if(n<=0) {
#ifdef DEBUG
		printf("Error sending SOAP request\n");
#endif
		closesocket(s);
		return NULL;
	}

	buf = getHTTPResponse(s, bufsize);
#ifdef DEBUG
	if(*bufsize > 0 && buf)
	{
		printf("SOAP Response :\n%.*s\n", *bufsize, buf);
	}
#endif
	closesocket(s);
	return buf;
}

/* simpleUPnPcommand :
 * not so simple !
 * return values :
 *   pointer - OK
 *   NULL    - error */
char * simpleUPnPcommand(int s, const char * url, const char * service,
		       const char * action, struct UPNParg * args,
		       int * bufsize)
{
	char * buf;

#if 1
	buf = simpleUPnPcommand2(s, url, service, action, args, bufsize, "1.1");
#else
	buf = simpleUPnPcommand2(s, url, service, action, args, bufsize, "1.0");
	if (!buf || *bufsize == 0)
	{
#if DEBUG
	    printf("Error or no result from SOAP request; retrying with HTTP/1.1\n");
#endif
		buf = simpleUPnPcommand2(s, url, service, action, args, bufsize, "1.1");
	}
#endif
	return buf;
}

/* parseMSEARCHReply()
 * the last 4 arguments are filled during the parsing :
 *    - location/locationsize : "location:" field of the SSDP reply packet
 *    - st/stsize : "st:" field of the SSDP reply packet.
 * The strings are NOT null terminated */
static void
parseMSEARCHReply(const char * reply, int size,
                  const char * * location, int * locationsize,
			      const char * * st, int * stsize)
{
	int a, b, i;
	i = 0;
	a = i;	/* start of the line */
	b = 0;	/* end of the "header" (position of the colon) */
	while(i<size)
	{
		switch(reply[i])
		{
		case ':':
				if(b==0)
				{
					b = i; /* end of the "header" */
					/*for(j=a; j<b; j++)
					{
						putchar(reply[j]);
					}
					*/
				}
				break;
		case '\x0a':
		case '\x0d':
				if(b!=0)
				{
					/*for(j=b+1; j<i; j++)
					{
						putchar(reply[j]);
					}
					putchar('\n');*/
					/* skip the colon and white spaces */
					do { b++; } while(reply[b]==' ');
					if(0==strncasecmp(reply+a, "location", 8))
					{
						*location = reply+b;
						*locationsize = i-b;
					}
					else if(0==strncasecmp(reply+a, "st", 2))
					{
						*st = reply+b;
						*stsize = i-b;
					}
					b = 0;
				}
				a = i+1;
				break;
		default:
				break;
		}
		i++;
	}
}

/* port upnp discover : SSDP protocol */
#define PORT 1900
#define XSTR(s) STR(s)
#define STR(s) #s
#define UPNP_MCAST_ADDR "239.255.255.250"
/* for IPv6 */
#define UPNP_MCAST_LL_ADDR "FF02::C" /* link-local */
#define UPNP_MCAST_SL_ADDR "FF05::C" /* site-local */

/* upnpDiscover() :
 * return a chained list of all devices found or NULL if
 * no devices was found.
 * It is up to the caller to free the chained list
 * delay is in millisecond (poll) */
LIBSPEC struct UPNPDev *
upnpDiscover(int delay, const char * multicastif,
             const char * minissdpdsock, int sameport,
             int ipv6,
             int * error)
{
	struct UPNPDev * tmp;
	struct UPNPDev * devlist = 0;
	unsigned int scope_id = 0;
	int opt = 1;
	static const char MSearchMsgFmt[] =
	"M-SEARCH * HTTP/1.1\r\n"
	"HOST: %s:" XSTR(PORT) "\r\n"
	"ST: %s\r\n"
	"MAN: \"ssdp:discover\"\r\n"
	"MX: %u\r\n"
	"\r\n";
	static const char * const deviceList[] = {
#if 0
		"urn:schemas-upnp-org:device:InternetGatewayDevice:2",
		"urn:schemas-upnp-org:service:WANIPConnection:2",
#endif
		"urn:schemas-upnp-org:device:InternetGatewayDevice:1",
		"urn:schemas-upnp-org:service:WANIPConnection:1",
		"urn:schemas-upnp-org:service:WANPPPConnection:1",
		"upnp:rootdevice",
		0
	};
	int deviceIndex = 0;
	char bufr[1536];	/* reception and emission buffer */
	int sudp;
	int n;
	struct sockaddr_storage sockudp_r;
	unsigned int mx;
#ifdef NO_GETADDRINFO
	struct sockaddr_storage sockudp_w;
#else
	int rv;
	struct addrinfo hints, *servinfo, *p;
#endif
#ifdef _WIN32
	MIB_IPFORWARDROW ip_forward;
#endif
	int linklocal = 1;

	if(error)
		*error = UPNPDISCOVER_UNKNOWN_ERROR;
	/* fallback to direct discovery */
#ifdef _WIN32
	sudp = socket(ipv6 ? PF_INET6 : PF_INET, SOCK_DGRAM, IPPROTO_UDP);
#else
	sudp = socket(ipv6 ? PF_INET6 : PF_INET, SOCK_DGRAM, 0);
#endif
	if(sudp < 0)
	{
		if(error)
			*error = UPNPDISCOVER_SOCKET_ERROR;
		PRINT_SOCKET_ERROR("socket");
		return NULL;
	}
	/* reception */
	memset(&sockudp_r, 0, sizeof(struct sockaddr_storage));
	if(ipv6) {
		struct sockaddr_in6 * p = (struct sockaddr_in6 *)&sockudp_r;
		p->sin6_family = AF_INET6;
		if(sameport)
			p->sin6_port = htons(PORT);
		p->sin6_addr = in6addr_any; /* in6addr_any is not available with MinGW32 3.4.2 */
	} else {
		struct sockaddr_in * p = (struct sockaddr_in *)&sockudp_r;
		p->sin_family = AF_INET;
		if(sameport)
			p->sin_port = htons(PORT);
		p->sin_addr.s_addr = INADDR_ANY;
	}
#ifdef _WIN32
/* This code could help us to use the right Network interface for
 * SSDP multicast traffic */
/* Get IP associated with the index given in the ip_forward struct
 * in order to give this ip to setsockopt(sudp, IPPROTO_IP, IP_MULTICAST_IF) */
	if(!ipv6
	   && (GetBestRoute(inet_addr("223.255.255.255"), 0, &ip_forward) == NO_ERROR)) {
		DWORD dwRetVal = 0;
		PMIB_IPADDRTABLE pIPAddrTable;
		DWORD dwSize = 0;
#ifdef DEBUG
		IN_ADDR IPAddr;
#endif
		int i;
#ifdef DEBUG
		printf("ifIndex=%lu nextHop=%lx \n", ip_forward.dwForwardIfIndex, ip_forward.dwForwardNextHop);
#endif
		pIPAddrTable = (MIB_IPADDRTABLE *) malloc(sizeof (MIB_IPADDRTABLE));
		if (GetIpAddrTable(pIPAddrTable, &dwSize, 0) == ERROR_INSUFFICIENT_BUFFER) {
			free(pIPAddrTable);
			pIPAddrTable = (MIB_IPADDRTABLE *) malloc(dwSize);
		}
		if(pIPAddrTable) {
			dwRetVal = GetIpAddrTable( pIPAddrTable, &dwSize, 0 );
#ifdef DEBUG
			printf("\tNum Entries: %ld\n", pIPAddrTable->dwNumEntries);
#endif
			for (i=0; i < (int) pIPAddrTable->dwNumEntries; i++) {
#ifdef DEBUG
				printf("\n\tInterface Index[%d]:\t%ld\n", i, pIPAddrTable->table[i].dwIndex);
				IPAddr.S_un.S_addr = (u_long) pIPAddrTable->table[i].dwAddr;
				printf("\tIP Address[%d]:     \t%s\n", i, inet_ntoa(IPAddr) );
				IPAddr.S_un.S_addr = (u_long) pIPAddrTable->table[i].dwMask;
				printf("\tSubnet Mask[%d]:    \t%s\n", i, inet_ntoa(IPAddr) );
				IPAddr.S_un.S_addr = (u_long) pIPAddrTable->table[i].dwBCastAddr;
				printf("\tBroadCast[%d]:      \t%s (%ld)\n", i, inet_ntoa(IPAddr), pIPAddrTable->table[i].dwBCastAddr);
				printf("\tReassembly size[%d]:\t%ld\n", i, pIPAddrTable->table[i].dwReasmSize);
				printf("\tType and State[%d]:", i);
				printf("\n");
#endif
				if (pIPAddrTable->table[i].dwIndex == ip_forward.dwForwardIfIndex) {
					/* Set the address of this interface to be used */
					struct in_addr mc_if;
					memset(&mc_if, 0, sizeof(mc_if));
					mc_if.s_addr = pIPAddrTable->table[i].dwAddr;
					if(setsockopt(sudp, IPPROTO_IP, IP_MULTICAST_IF, (const char *)&mc_if, sizeof(mc_if)) < 0) {
						PRINT_SOCKET_ERROR("setsockopt");
					}
					((struct sockaddr_in *)&sockudp_r)->sin_addr.s_addr = pIPAddrTable->table[i].dwAddr;
#ifndef DEBUG
					break;
#endif
				}
			}
			free(pIPAddrTable);
			pIPAddrTable = NULL;
		}
	}
#endif

#ifdef _WIN32
	if (setsockopt(sudp, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof (opt)) < 0)
#else
	if (setsockopt(sudp, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof (opt)) < 0)
#endif
	{
		if(error)
			*error = UPNPDISCOVER_SOCKET_ERROR;
		PRINT_SOCKET_ERROR("setsockopt");
		return NULL;
	}

	if(multicastif)
	{
		if(ipv6) {
#if !defined(_WIN32)
			/* according to MSDN, if_nametoindex() is supported since
			 * MS Windows Vista and MS Windows Server 2008.
			 * http://msdn.microsoft.com/en-us/library/bb408409%28v=vs.85%29.aspx */
			unsigned int ifindex = if_nametoindex(multicastif); /* eth0, etc. */
			if(setsockopt(sudp, IPPROTO_IPV6, IPV6_MULTICAST_IF, &ifindex, sizeof(&ifindex)) < 0)
			{
				PRINT_SOCKET_ERROR("setsockopt");
			}
#else
#ifdef DEBUG
			printf("Setting of multicast interface not supported in IPv6 under Windows.\n");
#endif
#endif
		} else {
			struct in_addr mc_if;
			mc_if.s_addr = inet_addr(multicastif); /* ex: 192.168.x.x */
			if(mc_if.s_addr != INADDR_NONE)
			{
				((struct sockaddr_in *)&sockudp_r)->sin_addr.s_addr = mc_if.s_addr;
				if(setsockopt(sudp, IPPROTO_IP, IP_MULTICAST_IF, (const char *)&mc_if, sizeof(mc_if)) < 0)
				{
					PRINT_SOCKET_ERROR("setsockopt");
				}
			} else {
#ifdef HAS_IP_MREQN
				/* was not an ip address, try with an interface name */
				struct ip_mreqn reqn;	/* only defined with -D_BSD_SOURCE or -D_GNU_SOURCE */
				memset(&reqn, 0, sizeof(struct ip_mreqn));
				reqn.imr_ifindex = if_nametoindex(multicastif);
				if(setsockopt(sudp, IPPROTO_IP, IP_MULTICAST_IF, (const char *)&reqn, sizeof(reqn)) < 0)
				{
					PRINT_SOCKET_ERROR("setsockopt");
				}
#else
#ifdef DEBUG
				printf("Setting of multicast interface not supported with interface name.\n");
#endif
#endif
			}
		}
	}

	/* Avant d'envoyer le paquet on bind pour recevoir la reponse */
    if (bind(sudp, (const struct sockaddr *)&sockudp_r,
	         ipv6 ? sizeof(struct sockaddr_in6) : sizeof(struct sockaddr_in)) != 0)
	{
		if(error)
			*error = UPNPDISCOVER_SOCKET_ERROR;
        PRINT_SOCKET_ERROR("bind");
		closesocket(sudp);
		return NULL;
    }

	if(error)
		*error = UPNPDISCOVER_SUCCESS;
	/* Calculating maximum response time in seconds */
	mx = ((unsigned int)delay) / 1000u;
	/* receiving SSDP response packet */
	for(n = 0; deviceList[deviceIndex]; deviceIndex++)
	{
	if(n == 0)
	{
		/* sending the SSDP M-SEARCH packet */
		n = snprintf(bufr, sizeof(bufr),
		             MSearchMsgFmt,
		             ipv6 ?
		             (linklocal ? "[" UPNP_MCAST_LL_ADDR "]" :  "[" UPNP_MCAST_SL_ADDR "]")
		             : UPNP_MCAST_ADDR,
		             deviceList[deviceIndex], mx);
#ifdef DEBUG
		printf("Sending %s", bufr);
#endif
#ifdef NO_GETADDRINFO
		/* the following code is not using getaddrinfo */
		/* emission */
		memset(&sockudp_w, 0, sizeof(struct sockaddr_storage));
		if(ipv6) {
			struct sockaddr_in6 * p = (struct sockaddr_in6 *)&sockudp_w;
			p->sin6_family = AF_INET6;
			p->sin6_port = htons(PORT);
			inet_pton(AF_INET6,
			          linklocal ? UPNP_MCAST_LL_ADDR : UPNP_MCAST_SL_ADDR,
			          &(p->sin6_addr));
		} else {
			struct sockaddr_in * p = (struct sockaddr_in *)&sockudp_w;
			p->sin_family = AF_INET;
			p->sin_port = htons(PORT);
			p->sin_addr.s_addr = inet_addr(UPNP_MCAST_ADDR);
		}
		n = sendto(sudp, bufr, n, 0,
		           &sockudp_w,
		           ipv6 ? sizeof(struct sockaddr_in6) : sizeof(struct sockaddr_in));
		if (n < 0) {
			if(error)
				*error = UPNPDISCOVER_SOCKET_ERROR;
			PRINT_SOCKET_ERROR("sendto");
			break;
		}
#else /* #ifdef NO_GETADDRINFO */
		memset(&hints, 0, sizeof(hints));
		hints.ai_family = AF_UNSPEC; /* AF_INET6 or AF_INET */
		hints.ai_socktype = SOCK_DGRAM;
		/*hints.ai_flags = */
		if ((rv = getaddrinfo(ipv6
		                      ? (linklocal ? UPNP_MCAST_LL_ADDR : UPNP_MCAST_SL_ADDR)
		                      : UPNP_MCAST_ADDR,
		                      XSTR(PORT), &hints, &servinfo)) != 0) {
			if(error)
				*error = UPNPDISCOVER_SOCKET_ERROR;
#ifdef _WIN32
		    fprintf(stderr, "getaddrinfo() failed: %d\n", rv);
#else
		    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
#endif
			break;
		}
		for(p = servinfo; p; p = p->ai_next) {
			n = sendto(sudp, bufr, n, 0, p->ai_addr, p->ai_addrlen);
			if (n < 0) {
#ifdef DEBUG
				char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];
				if (getnameinfo(p->ai_addr, p->ai_addrlen, hbuf, sizeof(hbuf), sbuf,
						sizeof(sbuf), NI_NUMERICHOST | NI_NUMERICSERV) == 0) {
					fprintf(stderr, "host:%s port:%s\n", hbuf, sbuf);
				}
#endif
				PRINT_SOCKET_ERROR("sendto");
				continue;
			}
		}
		freeaddrinfo(servinfo);
		if(n < 0) {
			if(error)
				*error = UPNPDISCOVER_SOCKET_ERROR;
			break;
		}
#endif /* #ifdef NO_GETADDRINFO */
	}
	/* Waiting for SSDP REPLY packet to M-SEARCH */
	n = receivedata(sudp, bufr, sizeof(bufr), delay, &scope_id);
	if (n < 0) {
		/* error */
		if(error)
			*error = UPNPDISCOVER_SOCKET_ERROR;
		break;
	} else if (n == 0) {
		/* no data or Time Out */
		if (devlist) {
			/* no more device type to look for... */
			if(error)
				*error = UPNPDISCOVER_SUCCESS;
			break;
		}
		if(ipv6) {
			if(linklocal) {
				linklocal = 0;
				--deviceIndex;
			} else {
				linklocal = 1;
			}
		}
	} else {
		const char * descURL=NULL;
		int urlsize=0;
		const char * st=NULL;
		int stsize=0;
        /*printf("%d byte(s) :\n%s\n", n, bufr);*/ /* affichage du message */
		parseMSEARCHReply(bufr, n, &descURL, &urlsize, &st, &stsize);
		if(st&&descURL)
		{
#ifdef DEBUG
			printf("M-SEARCH Reply:\nST: %.*s\nLocation: %.*s\n",
			       stsize, st, urlsize, descURL);
#endif
			for(tmp=devlist; tmp; tmp = tmp->pNext) {
				if(memcmp(tmp->descURL, descURL, urlsize) == 0 &&
				   tmp->descURL[urlsize] == '\0' &&
				   memcmp(tmp->st, st, stsize) == 0 &&
				   tmp->st[stsize] == '\0')
					break;
			}
			/* at the exit of the loop above, tmp is null if
			 * no duplicate device was found */
			if(tmp)
				continue;
			tmp = (struct UPNPDev *)malloc(sizeof(struct UPNPDev)+urlsize+stsize);
			if(!tmp) {
				/* memory allocation error */
				if(error)
					*error = UPNPDISCOVER_MEMORY_ERROR;
				break;
			}
			tmp->pNext = devlist;
			tmp->descURL = tmp->buffer;
			tmp->st = tmp->buffer + 1 + urlsize;
			memcpy(tmp->buffer, descURL, urlsize);
			tmp->buffer[urlsize] = '\0';
			memcpy(tmp->buffer + urlsize + 1, st, stsize);
			tmp->buffer[urlsize+1+stsize] = '\0';
			tmp->scope_id = scope_id;
			devlist = tmp;
		}
	}
	}
	closesocket(sudp);
	return devlist;
}

/* freeUPNPDevlist() should be used to
 * free the chained list returned by upnpDiscover() */
LIBSPEC void freeUPNPDevlist(struct UPNPDev * devlist)
{
	struct UPNPDev * next;
	while(devlist)
	{
		next = devlist->pNext;
		free(devlist);
		devlist = next;
	}
}

static void
url_cpy_or_cat(char * dst, const char * src, int n)
{
	if(  (src[0] == 'h')
	   &&(src[1] == 't')
	   &&(src[2] == 't')
	   &&(src[3] == 'p')
	   &&(src[4] == ':')
	   &&(src[5] == '/')
	   &&(src[6] == '/'))
	{
		strncpy(dst, src, n);
	}
	else
	{
		int l = strlen(dst);
		if(src[0] != '/')
			dst[l++] = '/';
		if(l<=n)
			strncpy(dst + l, src, n - l);
	}
}

/* Prepare the Urls for usage...
 */
LIBSPEC void
GetUPNPUrls(struct UPNPUrls * urls, struct IGDdatas * data,
            const char * descURL, unsigned int scope_id)
{
	char * p;
	int n1, n2, n3, n4;
#ifdef IF_NAMESIZE
	char ifname[IF_NAMESIZE];
#else
	char scope_str[8];
#endif

	n1 = strlen(data->urlbase);
	if(n1==0)
		n1 = strlen(descURL);
	if(scope_id != 0) {
#ifdef IF_NAMESIZE
		if(if_indextoname(scope_id, ifname)) {
			n1 += 3 + strlen(ifname);	/* 3 == strlen(%25) */
		}
#else
	/* under windows, scope is numerical */
	snprintf(scope_str, sizeof(scope_str), "%u", scope_id);
#endif
	}
	n1 += 2;	/* 1 byte more for Null terminator, 1 byte for '/' if needed */
	n2 = n1; n3 = n1; n4 = n1;
	n1 += strlen(data->first.scpdurl);
	n2 += strlen(data->first.controlurl);
	n3 += strlen(data->CIF.controlurl);
	n4 += strlen(data->IPv6FC.controlurl);

	/* allocate memory to store URLs */
	urls->ipcondescURL = (char *)malloc(n1);
	urls->controlURL = (char *)malloc(n2);
	urls->controlURL_CIF = (char *)malloc(n3);
	urls->controlURL_6FC = (char *)malloc(n4);

	/* strdup descURL */
	urls->rootdescURL = strdup(descURL);

	/* get description of WANIPConnection */
	if(data->urlbase[0] != '\0')
		strncpy(urls->ipcondescURL, data->urlbase, n1);
	else
		strncpy(urls->ipcondescURL, descURL, n1);
	p = strchr(urls->ipcondescURL+7, '/');
	if(p) p[0] = '\0';
	if(scope_id != 0) {
		if(0 == memcmp(urls->ipcondescURL, "http://[fe80:", 13)) {
			/* this is a linklocal IPv6 address */
			p = strchr(urls->ipcondescURL, ']');
			if(p) {
				/* insert %25<scope> into URL */
#ifdef IF_NAMESIZE
				memmove(p + 3 + strlen(ifname), p, strlen(p) + 1);
				memcpy(p, "%25", 3);
				memcpy(p + 3, ifname, strlen(ifname));
#else
				memmove(p + 3 + strlen(scope_str), p, strlen(p) + 1);
				memcpy(p, "%25", 3);
				memcpy(p + 3, scope_str, strlen(scope_str));
#endif
			}
		}
	}
	strncpy(urls->controlURL, urls->ipcondescURL, n2);
	strncpy(urls->controlURL_CIF, urls->ipcondescURL, n3);
	strncpy(urls->controlURL_6FC, urls->ipcondescURL, n4);

	url_cpy_or_cat(urls->ipcondescURL, data->first.scpdurl, n1);

	url_cpy_or_cat(urls->controlURL, data->first.controlurl, n2);

	url_cpy_or_cat(urls->controlURL_CIF, data->CIF.controlurl, n3);

	url_cpy_or_cat(urls->controlURL_6FC, data->IPv6FC.controlurl, n4);

#ifdef DEBUG
	printf("urls->ipcondescURL='%s' %u n1=%d\n", urls->ipcondescURL,
	       (unsigned)strlen(urls->ipcondescURL), n1);
	printf("urls->controlURL='%s' %u n2=%d\n", urls->controlURL,
	       (unsigned)strlen(urls->controlURL), n2);
	printf("urls->controlURL_CIF='%s' %u n3=%d\n", urls->controlURL_CIF,
	       (unsigned)strlen(urls->controlURL_CIF), n3);
	printf("urls->controlURL_6FC='%s' %u n4=%d\n", urls->controlURL_6FC,
	       (unsigned)strlen(urls->controlURL_6FC), n4);
#endif
}

LIBSPEC void
FreeUPNPUrls(struct UPNPUrls * urls)
{
	if(!urls)
		return;
	free(urls->controlURL);
	urls->controlURL = 0;
	free(urls->ipcondescURL);
	urls->ipcondescURL = 0;
	free(urls->controlURL_CIF);
	urls->controlURL_CIF = 0;
	free(urls->controlURL_6FC);
	urls->controlURL_6FC = 0;
	free(urls->rootdescURL);
	urls->rootdescURL = 0;
}

int
UPNPIGD_IsConnected(struct UPNPUrls * urls, struct IGDdatas * data)
{
	char status[64];
	unsigned int uptime;
	status[0] = '\0';
	UPNP_GetStatusInfo(urls->controlURL, data->first.servicetype,
	                   status, &uptime, NULL);
	if(0 == strcmp("Connected", status))
	{
		return 1;
	}
	else
		return 0;
}


/* UPNP_GetValidIGD() :
 * return values :
 *    -1 = Internal error
 *     0 = NO IGD found
 *     1 = A valid connected IGD has been found
 *     2 = A valid IGD has been found but it reported as
 *         not connected
 *     3 = an UPnP device has been found but was not recognized as an IGD
 *
 * In any non zero return case, the urls and data structures
 * passed as parameters are set. Donc forget to call FreeUPNPUrls(urls) to
 * free allocated memory.
 */
LIBSPEC int
UPNP_GetValidIGD(struct UPNPDev * devlist,
                 struct UPNPUrls * urls,
				 struct IGDdatas * data,
				 char * lanaddr, int lanaddrlen)
{
	struct xml_desc {
		char * xml;
		int size;
	} * desc = NULL;
	struct UPNPDev * dev;
	int ndev = 0;
	int i;
	int state = -1; /* state 1 : IGD connected. State 2 : IGD. State 3 : anything */
	if(!devlist)
	{
#ifdef DEBUG
		printf("Empty devlist\n");
#endif
		return 0;
	}
	for(dev = devlist; dev; dev = dev->pNext)
		ndev++;
	if(ndev > 0)
	{
		desc = calloc(ndev, sizeof(struct xml_desc));
		if(!desc)
			return -1; /* memory allocation error */
	}
	for(state = 1; state <= 3; state++)
	{
		for(dev = devlist, i = 0; dev; dev = dev->pNext, i++)
		{
			/* we should choose an internet gateway device.
		 	* with st == urn:schemas-upnp-org:device:InternetGatewayDevice:1 */
			if(state == 1)
			{
				desc[i].xml = miniwget_getaddr(dev->descURL, &(desc[i].size),
				   	                           lanaddr, lanaddrlen,
				                               dev->scope_id);
#ifdef DEBUG
				if(!desc[i].xml)
				{
					printf("error getting XML description %s\n", dev->descURL);
				}
#endif
			}
			if(desc[i].xml)
			{
				memset(data, 0, sizeof(struct IGDdatas));
				memset(urls, 0, sizeof(struct UPNPUrls));
				parserootdesc(desc[i].xml, desc[i].size, data);
				if(0==strcmp(data->CIF.servicetype,
				   "urn:schemas-upnp-org:service:WANCommonInterfaceConfig:1")
				   || state >= 3 )
				{
				  GetUPNPUrls(urls, data, dev->descURL, dev->scope_id);

#ifdef DEBUG
				  printf("UPNPIGD_IsConnected(%s) = %d\n",
				     urls->controlURL,
			         UPNPIGD_IsConnected(urls, data));
#endif
				  if((state >= 2) || UPNPIGD_IsConnected(urls, data))
					goto free_and_return;
				  FreeUPNPUrls(urls);
				  if(data->second.servicetype[0] != '\0') {
#ifdef DEBUG
				    printf("We tried %s, now we try %s !\n",
				           data->first.servicetype, data->second.servicetype);
#endif
				    /* swaping WANPPPConnection and WANIPConnection ! */
				    memcpy(&data->tmp, &data->first, sizeof(struct IGDdatas_service));
				    memcpy(&data->first, &data->second, sizeof(struct IGDdatas_service));
				    memcpy(&data->second, &data->tmp, sizeof(struct IGDdatas_service));
				    GetUPNPUrls(urls, data, dev->descURL, dev->scope_id);
#ifdef DEBUG
				    printf("UPNPIGD_IsConnected(%s) = %d\n",
				       urls->controlURL,
			           UPNPIGD_IsConnected(urls, data));
#endif
				    if((state >= 2) || UPNPIGD_IsConnected(urls, data))
					  goto free_and_return;
				    FreeUPNPUrls(urls);
				  }
				}
				memset(data, 0, sizeof(struct IGDdatas));
			}
		}
	}
	state = 0;
free_and_return:
	if(desc) {
		for(i = 0; i < ndev; i++) {
			if(desc[i].xml) {
				free(desc[i].xml);
			}
		}
		free(desc);
	}
	return state;
}

/* UPNP_GetIGDFromUrl()
 * Used when skipping the discovery process.
 * return value :
 *   0 - Not ok
 *   1 - OK */
int
UPNP_GetIGDFromUrl(const char * rootdescurl,
                   struct UPNPUrls * urls,
                   struct IGDdatas * data,
                   char * lanaddr, int lanaddrlen)
{
	char * descXML;
	int descXMLsize = 0;
	descXML = miniwget_getaddr(rootdescurl, &descXMLsize,
	   	                       lanaddr, lanaddrlen, 0);
	if(descXML) {
		memset(data, 0, sizeof(struct IGDdatas));
		memset(urls, 0, sizeof(struct UPNPUrls));
		parserootdesc(descXML, descXMLsize, data);
		free(descXML);
		descXML = NULL;
		GetUPNPUrls(urls, data, rootdescurl, 0);
		return 1;
	} else {
		return 0;
	}
}

