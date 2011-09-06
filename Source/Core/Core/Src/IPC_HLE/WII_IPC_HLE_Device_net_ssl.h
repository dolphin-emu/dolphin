// Copyright (C) 2003 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

#ifndef _WII_IPC_HLE_DEVICE_NET_SSL_H_
#define _WII_IPC_HLE_DEVICE_NET_SSL_H_

#ifdef _MSC_VER
#pragma warning(disable: 4748)
#pragma optimize("",off)
#endif

#include "WII_IPC_HLE_Device.h"
#include <openssl/ssl.h>
#include <openssl/evp.h>
#include <openssl/pkcs12.h>
#include <openssl/x509v3.h>

#define NET_SSL_MAXINSTANCES 4

class CWII_IPC_HLE_Device_net_ssl : public IWII_IPC_HLE_Device
{
public:

    CWII_IPC_HLE_Device_net_ssl(u32 _DeviceID, const std::string& _rDeviceName);    

    virtual ~CWII_IPC_HLE_Device_net_ssl();

    virtual bool Open(u32 _CommandAddress, u32 _Mode);

    virtual bool Close(u32 _CommandAddress, bool _bForce);

	virtual bool IOCtl(u32 _CommandAddress);
    virtual bool IOCtlV(u32 _CommandAddress);
	int getSSLFreeID();

private:
	SSL * sslfds[NET_SSL_MAXINSTANCES]; 
    enum
    {
        IOCTLV_NET_SSL_NEW							= 0x01,
        IOCTLV_NET_SSL_CONNECT						= 0x02,
        IOCTLV_NET_SSL_DOHANDSHAKE					= 0x03,
        IOCTLV_NET_SSL_READ							= 0x04,
        IOCTLV_NET_SSL_WRITE						= 0x05,
        IOCTLV_NET_SSL_SHUTDOWN						= 0x06,
        IOCTLV_NET_SSL_SETCLIENTCERT				= 0x07,
        IOCTLV_NET_SSL_SETCLIENTCERTDEFAULT			= 0x08,
        IOCTLV_NET_SSL_REMOVECLIENTCERT				= 0x09,
        IOCTLV_NET_SSL_SETROOTCA					= 0x0A,
        IOCTLV_NET_SSL_SETROOTCADEFAULT				= 0x0B,
        IOCTLV_NET_SSL_DOHANDSHAKEEX				= 0x0C,
        IOCTLV_NET_SSL_SETBUILTINROOTCA				= 0x0D,
        IOCTLV_NET_SSL_SETBUILTINCLIENTCERT			= 0x0E,
        IOCTLV_NET_SSL_DISABLEVERIFYOPTIONFORDEBUG	= 0x0F,
        IOCTLV_NET_SSL_DEBUGGETVERSION				= 0x14,
        IOCTLV_NET_SSL_DEBUGGETTIME					= 0x15,
    };

	
	u32 ExecuteCommand(u32 _Parameter, u32 _BufferIn, u32 _BufferInSize, u32 _BufferOut, u32 _BufferOutSize);
	u32 ExecuteCommandV(u32 _Parameter, SIOCtlVBuffer CommandBuffer);
};

#ifdef _MSC_VER
#pragma optimize("",off)
#endif

#endif
