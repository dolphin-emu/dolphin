/* $Id: portlistingparse.h,v 1.7 2012/09/27 15:42:10 nanard Exp $ */
/* MiniUPnP project
 * http://miniupnp.free.fr/ or http://miniupnp.tuxfamily.org/
 * (c) 2011-2012 Thomas Bernard
 * This software is subject to the conditions detailed
 * in the LICENCE file provided within the distribution */
#ifndef PORTLISTINGPARSE_H_INCLUDED
#define PORTLISTINGPARSE_H_INCLUDED

#include "declspec.h"
/* for the definition of UNSIGNED_INTEGER */
#include "miniupnpctypes.h"

#if defined(NO_SYS_QUEUE_H) || defined(_WIN32) || defined(__HAIKU__)
#include "bsdqueue.h"
#else
#include <sys/queue.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* sample of PortMappingEntry :
  <p:PortMappingEntry>
    <p:NewRemoteHost>202.233.2.1</p:NewRemoteHost>
    <p:NewExternalPort>2345</p:NewExternalPort>
    <p:NewProtocol>TCP</p:NewProtocol>
    <p:NewInternalPort>2345</p:NewInternalPort>
    <p:NewInternalClient>192.168.1.137</p:NewInternalClient>
    <p:NewEnabled>1</p:NewEnabled>
    <p:NewDescription>dooom</p:NewDescription>
    <p:NewLeaseTime>345</p:NewLeaseTime>
  </p:PortMappingEntry>
 */
typedef enum { PortMappingEltNone,
       PortMappingEntry, NewRemoteHost,
       NewExternalPort, NewProtocol,
       NewInternalPort, NewInternalClient,
       NewEnabled, NewDescription,
       NewLeaseTime } portMappingElt;

struct PortMapping {
	LIST_ENTRY(PortMapping) entries;
	UNSIGNED_INTEGER leaseTime;
	unsigned short externalPort;
	unsigned short internalPort;
	char remoteHost[64];
	char internalClient[64];
	char description[64];
	char protocol[4];
	unsigned char enabled;
};

struct PortMappingParserData {
	LIST_HEAD(portmappinglisthead, PortMapping) head;
	portMappingElt curelt;
};

LIBSPEC void
ParsePortListing(const char * buffer, int bufsize,
                 struct PortMappingParserData * pdata);

LIBSPEC void
FreePortListing(struct PortMappingParserData * pdata);

#ifdef __cplusplus
}
#endif

#endif
