// Copyright 2011 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

// These imports need to be in this order for mbed to be included correctly.
// clang-format off

#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <mbedtls/net_sockets.h>
#include <mbedtls/pk.h>
#include <mbedtls/platform.h>
#include <mbedtls/ssl.h>
#include <mbedtls/x509_crt.h>
#include <string>

// clang-format on

#include "Common/CommonTypes.h"
#include "Core/IOS/IOS.h"
#include "Core/IOS/Device.h"

namespace IOS::HLE
{
#define NET_SSL_MAXINSTANCES 4

// TODO: remove this macro.
#define SSLID_VALID(x)                                                                             \
  (x >= 0 && x < NET_SSL_MAXINSTANCES && ::IOS::HLE::Device::NetSSL::_SSL[x].active)

enum ssl_err_t : s32
{
  SSL_OK = 0,
  SSL_ERR_FAILED = -1,
  SSL_ERR_RAGAIN = -2,
  SSL_ERR_WAGAIN = -3,
  SSL_ERR_SYSCALL = -5,
  SSL_ERR_ZERO = -6,          // read or write returned 0
  SSL_ERR_CAGAIN = -7,        // BIO not connected
  SSL_ERR_ID = -8,            // invalid SSL id
  SSL_ERR_VCOMMONNAME = -9,   // verify failed: common name
  SSL_ERR_VROOTCA = -10,      // verify failed: root ca
  SSL_ERR_VCHAIN = -11,       // verify failed: certificate chain
  SSL_ERR_VDATE = -12,        // verify failed: date invalid
  SSL_ERR_SERVER_CERT = -13,  // certificate cert invalid
};

enum SSL_IOCTL
{
  IOCTLV_NET_SSL_NEW = 0x01,
  IOCTLV_NET_SSL_CONNECT = 0x02,
  IOCTLV_NET_SSL_DOHANDSHAKE = 0x03,
  IOCTLV_NET_SSL_READ = 0x04,
  IOCTLV_NET_SSL_WRITE = 0x05,
  IOCTLV_NET_SSL_SHUTDOWN = 0x06,
  IOCTLV_NET_SSL_SETCLIENTCERT = 0x07,
  IOCTLV_NET_SSL_SETCLIENTCERTDEFAULT = 0x08,
  IOCTLV_NET_SSL_REMOVECLIENTCERT = 0x09,
  IOCTLV_NET_SSL_SETROOTCA = 0x0A,
  IOCTLV_NET_SSL_SETROOTCADEFAULT = 0x0B,
  IOCTLV_NET_SSL_DOHANDSHAKEEX = 0x0C,
  IOCTLV_NET_SSL_SETBUILTINROOTCA = 0x0D,
  IOCTLV_NET_SSL_SETBUILTINCLIENTCERT = 0x0E,
  IOCTLV_NET_SSL_DISABLEVERIFYOPTIONFORDEBUG = 0x0F,
  IOCTLV_NET_SSL_DEBUGGETVERSION = 0x14,
  IOCTLV_NET_SSL_DEBUGGETTIME = 0x15,
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
  int hostfd;
  std::string hostname;
  bool active;
};

namespace Device
{
class NetSSL : public Device
{
public:
  NetSSL(Kernel& ios, const std::string& device_name);

  virtual ~NetSSL();

  IPCCommandResult IOCtl(const IOCtlRequest& request) override;
  IPCCommandResult IOCtlV(const IOCtlVRequest& request) override;

  int GetSSLFreeID() const;

  static WII_SSL _SSL[NET_SSL_MAXINSTANCES];

private:
  bool m_cert_error_shown = false;
};
}  // namespace Device
}  // namespace IOS::HLE
