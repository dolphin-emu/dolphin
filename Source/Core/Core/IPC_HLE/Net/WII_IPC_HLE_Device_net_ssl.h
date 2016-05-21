// Copyright 2011 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <mbedtls/net.h>
#include <mbedtls/ssl.h>

#include "Core/IPC_HLE/WII_IPC_HLE_Device.h"

#define NET_SSL_MAXINSTANCES 4

#define SSLID_VALID(x) (x >= 0 && x < NET_SSL_MAXINSTANCES && CWII_IPC_HLE_Device_net_ssl::_SSL[x].active)

enum ssl_err_t
{
	SSL_OK              =   0,
	SSL_ERR_FAILED      =  -1,
	SSL_ERR_RAGAIN      =  -2,
	SSL_ERR_WAGAIN      =  -3,
	SSL_ERR_SYSCALL     =  -5,
	SSL_ERR_ZERO        =  -6,  // read or write returned 0
	SSL_ERR_CAGAIN      =  -7,  // BIO not connected
	SSL_ERR_ID          =  -8,  // invalid SSL id
	SSL_ERR_VCOMMONNAME =  -9,  // verify failed: common name
	SSL_ERR_VROOTCA     =  -10, // verify failed: root ca
	SSL_ERR_VCHAIN      =  -11, // verify failed: certificate chain
	SSL_ERR_VDATE       =  -12, // verify failed: date invalid
	SSL_ERR_SERVER_CERT =  -13, // certificate cert invalid
};

enum SSL_IOCTL
{
	IOCTLV_NET_SSL_NEW                         = 0x01,
	IOCTLV_NET_SSL_CONNECT                     = 0x02,
	IOCTLV_NET_SSL_DOHANDSHAKE                 = 0x03,
	IOCTLV_NET_SSL_READ                        = 0x04,
	IOCTLV_NET_SSL_WRITE                       = 0x05,
	IOCTLV_NET_SSL_SHUTDOWN                    = 0x06,
	IOCTLV_NET_SSL_SETCLIENTCERT               = 0x07,
	IOCTLV_NET_SSL_SETCLIENTCERTDEFAULT        = 0x08,
	IOCTLV_NET_SSL_REMOVECLIENTCERT            = 0x09,
	IOCTLV_NET_SSL_SETROOTCA                   = 0x0A,
	IOCTLV_NET_SSL_SETROOTCADEFAULT            = 0x0B,
	IOCTLV_NET_SSL_DOHANDSHAKEEX               = 0x0C,
	IOCTLV_NET_SSL_SETBUILTINROOTCA            = 0x0D,
	IOCTLV_NET_SSL_SETBUILTINCLIENTCERT        = 0x0E,
	IOCTLV_NET_SSL_DISABLEVERIFYOPTIONFORDEBUG = 0x0F,
	IOCTLV_NET_SSL_DEBUGGETVERSION             = 0x14,
	IOCTLV_NET_SSL_DEBUGGETTIME                = 0x15,
};

struct WII_SSL
{
	mbedtls_ssl_context ctx;
	mbedtls_ssl_config config;
	mbedtls_ssl_session session;
	mbedtls_entropy_context entropy;
	mbedtls_ctr_drbg_context ctr_drbg;
	mbedtls_x509_crt cacert;
	mbedtls_x509_crt clicert;
	mbedtls_pk_context pk;
	int sockfd;
	std::string hostname;
	bool active;
};

class CWII_IPC_HLE_Device_net_ssl : public IWII_IPC_HLE_Device
{
public:

	CWII_IPC_HLE_Device_net_ssl(u32 _DeviceID, const std::string& _rDeviceName);

	virtual ~CWII_IPC_HLE_Device_net_ssl();

	IPCCommandResult Open(u32 _CommandAddress, u32 _Mode) override;
	IPCCommandResult Close(u32 _CommandAddress, bool _bForce) override;

	IPCCommandResult IOCtl(u32 _CommandAddress) override;
	IPCCommandResult IOCtlV(u32 _CommandAddress) override;

	int GetSSLFreeID() const;

	static WII_SSL _SSL[NET_SSL_MAXINSTANCES];
};
