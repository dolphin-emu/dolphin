// Copyright 2011 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cstring>
#include <memory>
#include <vector>

#include <mbedtls/md.h>

#include "Common/FileUtil.h"
#include "Common/Logging/Log.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/HW/Memmap.h"
#include "Core/IOS/Network/SSL.h"
#include "Core/IOS/Network/Socket.h"

namespace IOS
{
namespace HLE
{
namespace Device
{
WII_SSL NetSSL::_SSL[NET_SSL_MAXINSTANCES];

static constexpr mbedtls_x509_crt_profile mbedtls_x509_crt_profile_wii = {
    /* Hashes from SHA-1 and above */
    MBEDTLS_X509_ID_FLAG(MBEDTLS_MD_SHA1) | MBEDTLS_X509_ID_FLAG(MBEDTLS_MD_RIPEMD160) |
        MBEDTLS_X509_ID_FLAG(MBEDTLS_MD_SHA224) | MBEDTLS_X509_ID_FLAG(MBEDTLS_MD_SHA256) |
        MBEDTLS_X509_ID_FLAG(MBEDTLS_MD_SHA384) | MBEDTLS_X509_ID_FLAG(MBEDTLS_MD_SHA512),
    0xFFFFFFF, /* Any PK alg          */
    0xFFFFFFF, /* Any curve           */
    0,         /* No RSA min key size */
};

NetSSL::NetSSL(u32 device_id, const std::string& device_name) : Device(device_id, device_name)
{
  for (WII_SSL& ssl : _SSL)
  {
    ssl.active = false;
  }
}

NetSSL::~NetSSL()
{
  // Cleanup sessions
  for (WII_SSL& ssl : _SSL)
  {
    if (ssl.active)
    {
      mbedtls_ssl_close_notify(&ssl.ctx);

      mbedtls_x509_crt_free(&ssl.cacert);
      mbedtls_x509_crt_free(&ssl.clicert);

      mbedtls_ssl_session_free(&ssl.session);
      mbedtls_ssl_free(&ssl.ctx);
      mbedtls_ssl_config_free(&ssl.config);
      mbedtls_ctr_drbg_free(&ssl.ctr_drbg);
      mbedtls_entropy_free(&ssl.entropy);

      ssl.hostname.clear();

      ssl.active = false;
    }
  }
}

int NetSSL::GetSSLFreeID() const
{
  for (int i = 0; i < NET_SSL_MAXINSTANCES; i++)
  {
    if (!_SSL[i].active)
    {
      return i + 1;
    }
  }
  return 0;
}

IPCCommandResult NetSSL::IOCtl(const IOSIOCtlRequest& request)
{
  request.Log(GetDeviceName(), LogTypes::WII_IPC_SSL, LogTypes::LINFO);
  return GetDefaultReply(IPC_SUCCESS);
}

IPCCommandResult NetSSL::IOCtlV(const IOSIOCtlVRequest& request)
{
  u32 BufferIn = 0, BufferIn2 = 0, BufferIn3 = 0;
  u32 BufferInSize = 0, BufferInSize2 = 0, BufferInSize3 = 0;

  u32 BufferOut = 0, BufferOut2 = 0, BufferOut3 = 0;
  u32 BufferOutSize = 0, BufferOutSize2 = 0, BufferOutSize3 = 0;

  if (request.in_vectors.size() > 0)
  {
    BufferIn = request.in_vectors.at(0).address;
    BufferInSize = request.in_vectors.at(0).size;
  }
  if (request.in_vectors.size() > 1)
  {
    BufferIn2 = request.in_vectors.at(1).address;
    BufferInSize2 = request.in_vectors.at(1).size;
  }
  if (request.in_vectors.size() > 2)
  {
    BufferIn3 = request.in_vectors.at(2).address;
    BufferInSize3 = request.in_vectors.at(2).size;
  }

  if (request.io_vectors.size() > 0)
  {
    BufferOut = request.io_vectors.at(0).address;
    BufferOutSize = request.io_vectors.at(0).size;
  }
  if (request.io_vectors.size() > 1)
  {
    BufferOut2 = request.io_vectors.at(1).address;
    BufferOutSize2 = request.io_vectors.at(1).size;
  }
  if (request.io_vectors.size() > 2)
  {
    BufferOut3 = request.io_vectors.at(2).address;
    BufferOutSize3 = request.io_vectors.at(2).size;
  }

  // I don't trust SSL to be deterministic, and this is never going to sync
  // as such (as opposed to forwarding IPC results or whatever), so -
  if (Core::g_want_determinism)
    return GetDefaultReply(IPC_EACCES);

  switch (request.request)
  {
  case IOCTLV_NET_SSL_NEW:
  {
    int verifyOption = Memory::Read_U32(BufferOut);
    std::string hostname = Memory::GetString(BufferOut2, BufferOutSize2);

    int freeSSL = GetSSLFreeID();
    if (freeSSL)
    {
      int sslID = freeSSL - 1;
      WII_SSL* ssl = &_SSL[sslID];
      mbedtls_ssl_init(&ssl->ctx);
      mbedtls_entropy_init(&ssl->entropy);
      static constexpr const char* pers = "dolphin-emu";
      mbedtls_ctr_drbg_init(&ssl->ctr_drbg);
      int ret = mbedtls_ctr_drbg_seed(&ssl->ctr_drbg, mbedtls_entropy_func, &ssl->entropy,
                                      (const unsigned char*)pers, strlen(pers));
      if (ret)
      {
        mbedtls_ssl_free(&ssl->ctx);
        mbedtls_ctr_drbg_free(&ssl->ctr_drbg);
        mbedtls_entropy_free(&ssl->entropy);
        goto _SSL_NEW_ERROR;
      }

      mbedtls_ssl_config_init(&ssl->config);
      mbedtls_ssl_config_defaults(&ssl->config, MBEDTLS_SSL_IS_CLIENT, MBEDTLS_SSL_TRANSPORT_STREAM,
                                  MBEDTLS_SSL_PRESET_DEFAULT);
      mbedtls_ssl_conf_rng(&ssl->config, mbedtls_ctr_drbg_random, &ssl->ctr_drbg);

      // For some reason we can't use TLSv1.2, v1.1 and below are fine!
      mbedtls_ssl_conf_max_version(&ssl->config, MBEDTLS_SSL_MAJOR_VERSION_3,
                                   MBEDTLS_SSL_MINOR_VERSION_2);
      mbedtls_ssl_conf_cert_profile(&ssl->config, &mbedtls_x509_crt_profile_wii);
      mbedtls_ssl_set_session(&ssl->ctx, &ssl->session);

      if (SConfig::GetInstance().m_SSLVerifyCert && verifyOption)
        mbedtls_ssl_conf_authmode(&ssl->config, MBEDTLS_SSL_VERIFY_REQUIRED);
      else
        mbedtls_ssl_conf_authmode(&ssl->config, MBEDTLS_SSL_VERIFY_NONE);
      mbedtls_ssl_conf_renegotiation(&ssl->config, MBEDTLS_SSL_RENEGOTIATION_ENABLED);

      ssl->hostname = hostname;
      mbedtls_ssl_set_hostname(&ssl->ctx, ssl->hostname.c_str());

      ssl->active = true;
      Memory::Write_U32(freeSSL, BufferIn);
    }
    else
    {
    _SSL_NEW_ERROR:
      Memory::Write_U32(SSL_ERR_FAILED, BufferIn);
    }

    INFO_LOG(WII_IPC_SSL, "IOCTLV_NET_SSL_NEW (%d, %s) "
                          "BufferIn: (%08x, %i), BufferIn2: (%08x, %i), "
                          "BufferIn3: (%08x, %i), BufferOut: (%08x, %i), "
                          "BufferOut2: (%08x, %i), BufferOut3: (%08x, %i)",
             verifyOption, hostname.c_str(), BufferIn, BufferInSize, BufferIn2, BufferInSize2,
             BufferIn3, BufferInSize3, BufferOut, BufferOutSize, BufferOut2, BufferOutSize2,
             BufferOut3, BufferOutSize3);
    break;
  }
  case IOCTLV_NET_SSL_SHUTDOWN:
  {
    int sslID = Memory::Read_U32(BufferOut) - 1;
    if (SSLID_VALID(sslID))
    {
      WII_SSL* ssl = &_SSL[sslID];

      mbedtls_ssl_close_notify(&ssl->ctx);

      mbedtls_x509_crt_free(&ssl->cacert);
      mbedtls_x509_crt_free(&ssl->clicert);

      mbedtls_ssl_session_free(&ssl->session);
      mbedtls_ssl_free(&ssl->ctx);
      mbedtls_ssl_config_free(&ssl->config);
      mbedtls_ctr_drbg_free(&ssl->ctr_drbg);
      mbedtls_entropy_free(&ssl->entropy);

      ssl->hostname.clear();

      ssl->active = false;

      Memory::Write_U32(SSL_OK, BufferIn);
    }
    else
    {
      Memory::Write_U32(SSL_ERR_ID, BufferIn);
    }
    INFO_LOG(WII_IPC_SSL, "IOCTLV_NET_SSL_SHUTDOWN "
                          "BufferIn: (%08x, %i), BufferIn2: (%08x, %i), "
                          "BufferIn3: (%08x, %i), BufferOut: (%08x, %i), "
                          "BufferOut2: (%08x, %i), BufferOut3: (%08x, %i)",
             BufferIn, BufferInSize, BufferIn2, BufferInSize2, BufferIn3, BufferInSize3, BufferOut,
             BufferOutSize, BufferOut2, BufferOutSize2, BufferOut3, BufferOutSize3);
    break;
  }
  case IOCTLV_NET_SSL_SETROOTCA:
  {
    INFO_LOG(WII_IPC_SSL, "IOCTLV_NET_SSL_SETROOTCA "
                          "BufferIn: (%08x, %i), BufferIn2: (%08x, %i), "
                          "BufferIn3: (%08x, %i), BufferOut: (%08x, %i), "
                          "BufferOut2: (%08x, %i), BufferOut3: (%08x, %i)",
             BufferIn, BufferInSize, BufferIn2, BufferInSize2, BufferIn3, BufferInSize3, BufferOut,
             BufferOutSize, BufferOut2, BufferOutSize2, BufferOut3, BufferOutSize3);

    int sslID = Memory::Read_U32(BufferOut) - 1;
    if (SSLID_VALID(sslID))
    {
      WII_SSL* ssl = &_SSL[sslID];
      int ret =
          mbedtls_x509_crt_parse_der(&ssl->cacert, Memory::GetPointer(BufferOut2), BufferOutSize2);

      if (SConfig::GetInstance().m_SSLDumpRootCA)
      {
        std::string filename = File::GetUserPath(D_DUMPSSL_IDX) + ssl->hostname + "_rootca.der";
        File::IOFile(filename, "wb").WriteBytes(Memory::GetPointer(BufferOut2), BufferOutSize2);
      }

      if (ret)
      {
        Memory::Write_U32(SSL_ERR_FAILED, BufferIn);
      }
      else
      {
        mbedtls_ssl_conf_ca_chain(&ssl->config, &ssl->cacert, nullptr);
        Memory::Write_U32(SSL_OK, BufferIn);
      }

      INFO_LOG(WII_IPC_SSL, "IOCTLV_NET_SSL_SETROOTCA = %d", ret);
    }
    else
    {
      Memory::Write_U32(SSL_ERR_ID, BufferIn);
    }
    break;
  }
  case IOCTLV_NET_SSL_SETBUILTINCLIENTCERT:
  {
    INFO_LOG(WII_IPC_SSL, "IOCTLV_NET_SSL_SETBUILTINCLIENTCERT "
                          "BufferIn: (%08x, %i), BufferIn2: (%08x, %i), "
                          "BufferIn3: (%08x, %i), BufferOut: (%08x, %i), "
                          "BufferOut2: (%08x, %i), BufferOut3: (%08x, %i)",
             BufferIn, BufferInSize, BufferIn2, BufferInSize2, BufferIn3, BufferInSize3, BufferOut,
             BufferOutSize, BufferOut2, BufferOutSize2, BufferOut3, BufferOutSize3);

    int sslID = Memory::Read_U32(BufferOut) - 1;
    if (SSLID_VALID(sslID))
    {
      WII_SSL* ssl = &_SSL[sslID];
      std::string cert_base_path = File::GetUserPath(D_SESSION_WIIROOT_IDX);
      int ret =
          mbedtls_x509_crt_parse_file(&ssl->clicert, (cert_base_path + "/clientca.pem").c_str());
      int pk_ret = mbedtls_pk_parse_keyfile(&ssl->pk, (cert_base_path + "/clientcakey.pem").c_str(),
                                            nullptr);
      if (ret || pk_ret)
      {
        mbedtls_x509_crt_free(&ssl->clicert);
        mbedtls_pk_free(&ssl->pk);
        Memory::Write_U32(SSL_ERR_FAILED, BufferIn);
      }
      else
      {
        mbedtls_ssl_conf_own_cert(&ssl->config, &ssl->clicert, &ssl->pk);
        Memory::Write_U32(SSL_OK, BufferIn);
      }

      INFO_LOG(WII_IPC_SSL, "IOCTLV_NET_SSL_SETBUILTINCLIENTCERT = (%d, %d)", ret, pk_ret);
    }
    else
    {
      Memory::Write_U32(SSL_ERR_ID, BufferIn);
      INFO_LOG(WII_IPC_SSL, "IOCTLV_NET_SSL_SETBUILTINCLIENTCERT invalid sslID = %d", sslID);
    }
    break;
  }
  case IOCTLV_NET_SSL_REMOVECLIENTCERT:
  {
    INFO_LOG(WII_IPC_SSL, "IOCTLV_NET_SSL_REMOVECLIENTCERT "
                          "BufferIn: (%08x, %i), BufferIn2: (%08x, %i), "
                          "BufferIn3: (%08x, %i), BufferOut: (%08x, %i), "
                          "BufferOut2: (%08x, %i), BufferOut3: (%08x, %i)",
             BufferIn, BufferInSize, BufferIn2, BufferInSize2, BufferIn3, BufferInSize3, BufferOut,
             BufferOutSize, BufferOut2, BufferOutSize2, BufferOut3, BufferOutSize3);

    int sslID = Memory::Read_U32(BufferOut) - 1;
    if (SSLID_VALID(sslID))
    {
      WII_SSL* ssl = &_SSL[sslID];
      mbedtls_x509_crt_free(&ssl->clicert);
      mbedtls_pk_free(&ssl->pk);

      mbedtls_ssl_conf_own_cert(&ssl->config, nullptr, nullptr);
      Memory::Write_U32(SSL_OK, BufferIn);
    }
    else
    {
      Memory::Write_U32(SSL_ERR_ID, BufferIn);
      INFO_LOG(WII_IPC_SSL, "IOCTLV_NET_SSL_SETBUILTINCLIENTCERT invalid sslID = %d", sslID);
    }
    break;
  }
  case IOCTLV_NET_SSL_SETBUILTINROOTCA:
  {
    int sslID = Memory::Read_U32(BufferOut) - 1;
    if (SSLID_VALID(sslID))
    {
      WII_SSL* ssl = &_SSL[sslID];

      int ret = mbedtls_x509_crt_parse_file(
          &ssl->cacert, (File::GetUserPath(D_SESSION_WIIROOT_IDX) + "/rootca.pem").c_str());
      if (ret)
      {
        mbedtls_x509_crt_free(&ssl->clicert);
        Memory::Write_U32(SSL_ERR_FAILED, BufferIn);
      }
      else
      {
        mbedtls_ssl_conf_ca_chain(&ssl->config, &ssl->cacert, nullptr);
        Memory::Write_U32(SSL_OK, BufferIn);
      }
      INFO_LOG(WII_IPC_SSL, "IOCTLV_NET_SSL_SETBUILTINROOTCA = %d", ret);
    }
    else
    {
      Memory::Write_U32(SSL_ERR_ID, BufferIn);
    }
    INFO_LOG(WII_IPC_SSL, "IOCTLV_NET_SSL_SETBUILTINROOTCA "
                          "BufferIn: (%08x, %i), BufferIn2: (%08x, %i), "
                          "BufferIn3: (%08x, %i), BufferOut: (%08x, %i), "
                          "BufferOut2: (%08x, %i), BufferOut3: (%08x, %i)",
             BufferIn, BufferInSize, BufferIn2, BufferInSize2, BufferIn3, BufferInSize3, BufferOut,
             BufferOutSize, BufferOut2, BufferOutSize2, BufferOut3, BufferOutSize3);
    break;
  }
  case IOCTLV_NET_SSL_CONNECT:
  {
    int sslID = Memory::Read_U32(BufferOut) - 1;
    if (SSLID_VALID(sslID))
    {
      WII_SSL* ssl = &_SSL[sslID];
      mbedtls_ssl_setup(&ssl->ctx, &ssl->config);
      ssl->sockfd = Memory::Read_U32(BufferOut2);
      INFO_LOG(WII_IPC_SSL, "IOCTLV_NET_SSL_CONNECT socket = %d", ssl->sockfd);
      mbedtls_ssl_set_bio(&ssl->ctx, &ssl->sockfd, mbedtls_net_send, mbedtls_net_recv, nullptr);
      Memory::Write_U32(SSL_OK, BufferIn);
    }
    else
    {
      Memory::Write_U32(SSL_ERR_ID, BufferIn);
    }
    INFO_LOG(WII_IPC_SSL, "IOCTLV_NET_SSL_CONNECT "
                          "BufferIn: (%08x, %i), BufferIn2: (%08x, %i), "
                          "BufferIn3: (%08x, %i), BufferOut: (%08x, %i), "
                          "BufferOut2: (%08x, %i), BufferOut3: (%08x, %i)",
             BufferIn, BufferInSize, BufferIn2, BufferInSize2, BufferIn3, BufferInSize3, BufferOut,
             BufferOutSize, BufferOut2, BufferOutSize2, BufferOut3, BufferOutSize3);
    break;
  }
  case IOCTLV_NET_SSL_DOHANDSHAKE:
  {
    int sslID = Memory::Read_U32(BufferOut) - 1;
    if (SSLID_VALID(sslID))
    {
      WiiSockMan& sm = WiiSockMan::GetInstance();
      sm.DoSock(_SSL[sslID].sockfd, request, IOCTLV_NET_SSL_DOHANDSHAKE);
      return GetNoReply();
    }
    else
    {
      Memory::Write_U32(SSL_ERR_ID, BufferIn);
    }
    break;
  }
  case IOCTLV_NET_SSL_WRITE:
  {
    int sslID = Memory::Read_U32(BufferOut) - 1;
    if (SSLID_VALID(sslID))
    {
      WiiSockMan& sm = WiiSockMan::GetInstance();
      sm.DoSock(_SSL[sslID].sockfd, request, IOCTLV_NET_SSL_WRITE);
      return GetNoReply();
    }
    else
    {
      Memory::Write_U32(SSL_ERR_ID, BufferIn);
    }
    INFO_LOG(WII_IPC_SSL, "IOCTLV_NET_SSL_WRITE "
                          "BufferIn: (%08x, %i), BufferIn2: (%08x, %i), "
                          "BufferIn3: (%08x, %i), BufferOut: (%08x, %i), "
                          "BufferOut2: (%08x, %i), BufferOut3: (%08x, %i)",
             BufferIn, BufferInSize, BufferIn2, BufferInSize2, BufferIn3, BufferInSize3, BufferOut,
             BufferOutSize, BufferOut2, BufferOutSize2, BufferOut3, BufferOutSize3);
    INFO_LOG(WII_IPC_SSL, "%s", Memory::GetString(BufferOut2).c_str());
    break;
  }
  case IOCTLV_NET_SSL_READ:
  {
    int ret = 0;
    int sslID = Memory::Read_U32(BufferOut) - 1;
    if (SSLID_VALID(sslID))
    {
      WiiSockMan& sm = WiiSockMan::GetInstance();
      sm.DoSock(_SSL[sslID].sockfd, request, IOCTLV_NET_SSL_READ);
      return GetNoReply();
    }
    else
    {
      Memory::Write_U32(SSL_ERR_ID, BufferIn);
    }

    INFO_LOG(WII_IPC_SSL, "IOCTLV_NET_SSL_READ(%d)"
                          "BufferIn: (%08x, %i), BufferIn2: (%08x, %i), "
                          "BufferIn3: (%08x, %i), BufferOut: (%08x, %i), "
                          "BufferOut2: (%08x, %i), BufferOut3: (%08x, %i)",
             ret, BufferIn, BufferInSize, BufferIn2, BufferInSize2, BufferIn3, BufferInSize3,
             BufferOut, BufferOutSize, BufferOut2, BufferOutSize2, BufferOut3, BufferOutSize3);
    break;
  }
  case IOCTLV_NET_SSL_SETROOTCADEFAULT:
  {
    int sslID = Memory::Read_U32(BufferOut) - 1;
    if (SSLID_VALID(sslID))
    {
      Memory::Write_U32(SSL_OK, BufferIn);
    }
    else
    {
      Memory::Write_U32(SSL_ERR_ID, BufferIn);
    }
    INFO_LOG(WII_IPC_SSL, "IOCTLV_NET_SSL_SETROOTCADEFAULT "
                          "BufferIn: (%08x, %i), BufferIn2: (%08x, %i), "
                          "BufferIn3: (%08x, %i), BufferOut: (%08x, %i), "
                          "BufferOut2: (%08x, %i), BufferOut3: (%08x, %i)",
             BufferIn, BufferInSize, BufferIn2, BufferInSize2, BufferIn3, BufferInSize3, BufferOut,
             BufferOutSize, BufferOut2, BufferOutSize2, BufferOut3, BufferOutSize3);
    break;
  }
  case IOCTLV_NET_SSL_SETCLIENTCERTDEFAULT:
  {
    INFO_LOG(WII_IPC_SSL, "IOCTLV_NET_SSL_SETCLIENTCERTDEFAULT "
                          "BufferIn: (%08x, %i), BufferIn2: (%08x, %i), "
                          "BufferIn3: (%08x, %i), BufferOut: (%08x, %i), "
                          "BufferOut2: (%08x, %i), BufferOut3: (%08x, %i)",
             BufferIn, BufferInSize, BufferIn2, BufferInSize2, BufferIn3, BufferInSize3, BufferOut,
             BufferOutSize, BufferOut2, BufferOutSize2, BufferOut3, BufferOutSize3);

    int sslID = Memory::Read_U32(BufferOut) - 1;
    if (SSLID_VALID(sslID))
    {
      Memory::Write_U32(SSL_OK, BufferIn);
    }
    else
    {
      Memory::Write_U32(SSL_ERR_ID, BufferIn);
    }
    break;
  }
  default:
    request.DumpUnknown(GetDeviceName(), LogTypes::WII_IPC_SSL);
  }

  // SSL return codes are written to BufferIn
  return GetDefaultReply(IPC_SUCCESS);
}
}  // namespace Device
}  // namespace HLE
}  // namespace IOS
