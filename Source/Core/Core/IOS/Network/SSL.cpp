// Copyright 2011 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/IOS/Network/SSL.h"

#include <array>
#include <cstring>
#include <memory>
#include <vector>

#include <mbedtls/md.h>
#include <mbedtls/sha256.h>

#include "Common/FileUtil.h"
#include "Common/IOFile.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Core/Config/MainSettings.h"
#include "Core/Core.h"
#include "Core/HW/Memmap.h"
#include "Core/IOS/Network/Socket.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/System.h"

namespace IOS::HLE
{
WII_SSL NetSSLDevice::_SSL[NET_SSL_MAXINSTANCES];

static constexpr mbedtls_x509_crt_profile mbedtls_x509_crt_profile_wii = {
    /* Hashes from SHA-1 and above */
    MBEDTLS_X509_ID_FLAG(MBEDTLS_MD_SHA1) | MBEDTLS_X509_ID_FLAG(MBEDTLS_MD_RIPEMD160) |
        MBEDTLS_X509_ID_FLAG(MBEDTLS_MD_SHA224) | MBEDTLS_X509_ID_FLAG(MBEDTLS_MD_SHA256) |
        MBEDTLS_X509_ID_FLAG(MBEDTLS_MD_SHA384) | MBEDTLS_X509_ID_FLAG(MBEDTLS_MD_SHA512),
    0xFFFFFFF, /* Any PK alg          */
    0xFFFFFFF, /* Any curve           */
    0,         /* No RSA min key size */
};

namespace
{
// Dirty workaround to disable SNI which isn't supported by the Wii.
//
// This SSL extension can ONLY be disabled by undefining
// MBEDTLS_SSL_SERVER_NAME_INDICATION and recompiling the library. When
// enabled and if the hostname is set, it uses the SNI extension which is sent
// with the Client Hello message.
//
// This workaround doesn't require recompiling the library. It does so by
// deferring mbedtls_ssl_set_hostname after the Client Hello message. The send
// callback is used as it's the (only?) hook called at the beginning of
// each step of the handshake by the mbedtls_ssl_flush_output function.
//
// The hostname still needs to be set as it is checked against the Common Name
// field during the certificate verification process.
int SSLSendWithoutSNI(void* ctx, const unsigned char* buf, size_t len)
{
  auto* ssl = static_cast<WII_SSL*>(ctx);
  auto* fd = &ssl->hostfd;

  if (ssl->ctx.state == MBEDTLS_SSL_SERVER_HELLO)
    mbedtls_ssl_set_hostname(&ssl->ctx, ssl->hostname.c_str());
  const int ret = mbedtls_net_send(fd, buf, len);

  // Log raw SSL packets if we don't dump unencrypted SSL writes
  if (!Config::Get(Config::MAIN_NETWORK_SSL_DUMP_WRITE) && ret > 0)
  {
    Core::System::GetInstance().GetPowerPC().GetDebugInterface().NetworkLogger()->LogWrite(
        buf, ret, *fd, nullptr);
  }

  return ret;
}

int SSLRecv(void* ctx, unsigned char* buf, size_t len)
{
  auto* ssl = static_cast<WII_SSL*>(ctx);
  auto* fd = &ssl->hostfd;
  const int ret = mbedtls_net_recv(fd, buf, len);

  // Log raw SSL packets if we don't dump unencrypted SSL reads
  if (!Config::Get(Config::MAIN_NETWORK_SSL_DUMP_READ) && ret > 0)
  {
    Core::System::GetInstance().GetPowerPC().GetDebugInterface().NetworkLogger()->LogRead(
        buf, ret, *fd, nullptr);
  }

  return ret;
}
}  // namespace

NetSSLDevice::NetSSLDevice(EmulationKernel& ios, const std::string& device_name)
    : EmulationDevice(ios, device_name)
{
  for (WII_SSL& ssl : _SSL)
  {
    ssl.active = false;
  }
}

NetSSLDevice::~NetSSLDevice()
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

int NetSSLDevice::GetSSLFreeID() const
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

std::optional<IPCReply> NetSSLDevice::IOCtl(const IOCtlRequest& request)
{
  request.Log(GetDeviceName(), Common::Log::LogType::IOS_SSL, Common::Log::LogLevel::LINFO);
  return IPCReply(IPC_SUCCESS);
}

constexpr std::array<u8, 32> s_client_cert_hash = {
    {0x22, 0x9e, 0xc6, 0x78, 0x52, 0x5e, 0x06, 0x05, 0x88, 0xe8, 0xea,
     0x23, 0xe5, 0x45, 0x9e, 0xc1, 0x4a, 0xf3, 0xc2, 0xeb, 0xb7, 0xb9,
     0xe6, 0x9e, 0xc4, 0x6b, 0x0f, 0xaf, 0x01, 0x17, 0x30, 0xd9}};
constexpr std::array<u8, 32> s_client_key_hash = {{0x72, 0x3b, 0xe9, 0xb3, 0x2c, 0x3a, 0xfb, 0x83,
                                                   0xa4, 0xa3, 0x75, 0x7a, 0xdf, 0x35, 0x25, 0x29,
                                                   0xe9, 0x0c, 0x0a, 0xd6, 0xfa, 0xd5, 0x25, 0x09,
                                                   0x96, 0x3b, 0xa8, 0x94, 0x2a, 0xe6, 0x25, 0xdf}};
constexpr std::array<u8, 32> s_root_ca_hash = {{0xc5, 0xb0, 0xf8, 0xdf, 0xce, 0xc6, 0xb9, 0xed,
                                                0x2a, 0xc3, 0x8b, 0x8b, 0xc6, 0x9a, 0x4d, 0xb7,
                                                0xc2, 0x09, 0xdc, 0x17, 0x7d, 0x24, 0x3c, 0x8d,
                                                0xf2, 0xbd, 0xdf, 0x9e, 0x39, 0x17, 0x1e, 0x5f}};

static std::vector<u8> ReadCertFile(const std::string& path, const std::array<u8, 32>& correct_hash,
                                    bool silent)
{
  File::IOFile file(path, "rb");
  std::vector<u8> bytes(file.GetSize());
  if (!file.ReadBytes(bytes.data(), bytes.size()))
  {
    ERROR_LOG_FMT(IOS_SSL, "Failed to read {}", path);
    if (!silent)
    {
      PanicAlertFmtT("IOS: Could not read a file required for SSL services ({0}). Please refer to "
                     "https://dolphin-emu.org/docs/guides/wii-network-guide/ for "
                     "instructions on setting up Wii networking.",
                     path);
    }
    return {};
  }

  std::array<u8, 32> hash;
  mbedtls_sha256_ret(bytes.data(), bytes.size(), hash.data(), 0);
  if (hash != correct_hash)
  {
    ERROR_LOG_FMT(IOS_SSL, "Wrong hash for {}", path);
    if (!silent)
    {
      PanicAlertFmtT("IOS: A file required for SSL services ({0}) is invalid. Please refer to "
                     "https://dolphin-emu.org/docs/guides/wii-network-guide/ for "
                     "instructions on setting up Wii networking.",
                     path);
    }
    return {};
  }
  return bytes;
}

std::optional<IPCReply> NetSSLDevice::IOCtlV(const IOCtlVRequest& request)
{
  u32 BufferIn = 0, BufferIn2 = 0, BufferIn3 = 0;
  u32 BufferInSize = 0, BufferInSize2 = 0, BufferInSize3 = 0;

  u32 BufferOut = 0, BufferOut2 = 0, BufferOut3 = 0;
  u32 BufferOutSize = 0, BufferOutSize2 = 0, BufferOutSize3 = 0;

  if (!request.in_vectors.empty())
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

  if (!request.io_vectors.empty())
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
  if (Core::WantsDeterminism())
    return IPCReply(IPC_EACCES);

  auto& system = Core::System::GetInstance();
  auto& memory = system.GetMemory();

  switch (request.request)
  {
  case IOCTLV_NET_SSL_NEW:
  {
    int verifyOption = memory.Read_U32(BufferOut);
    std::string hostname = memory.GetString(BufferOut2, BufferOutSize2);

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

      if (Config::Get(Config::MAIN_NETWORK_SSL_VERIFY_CERTIFICATES) && verifyOption)
        mbedtls_ssl_conf_authmode(&ssl->config, MBEDTLS_SSL_VERIFY_REQUIRED);
      else
        mbedtls_ssl_conf_authmode(&ssl->config, MBEDTLS_SSL_VERIFY_NONE);
      mbedtls_ssl_conf_renegotiation(&ssl->config, MBEDTLS_SSL_RENEGOTIATION_ENABLED);

      ssl->hostname = hostname;
      ssl->active = true;
      WriteReturnValue(memory, freeSSL, BufferIn);
    }
    else
    {
    _SSL_NEW_ERROR:
      WriteReturnValue(memory, SSL_ERR_FAILED, BufferIn);
    }

    INFO_LOG_FMT(IOS_SSL,
                 "IOCTLV_NET_SSL_NEW ({}, {}) "
                 "BufferIn: ({:08x}, {}), BufferIn2: ({:08x}, {}), "
                 "BufferIn3: ({:08x}, {}), BufferOut: ({:08x}, {}), "
                 "BufferOut2: ({:08x}, {}), BufferOut3: ({:08x}, {})",
                 verifyOption, hostname, BufferIn, BufferInSize, BufferIn2, BufferInSize2,
                 BufferIn3, BufferInSize3, BufferOut, BufferOutSize, BufferOut2, BufferOutSize2,
                 BufferOut3, BufferOutSize3);
    break;
  }
  case IOCTLV_NET_SSL_SHUTDOWN:
  {
    int sslID = memory.Read_U32(BufferOut) - 1;
    if (IsSSLIDValid(sslID))
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

      WriteReturnValue(memory, SSL_OK, BufferIn);
    }
    else
    {
      WriteReturnValue(memory, SSL_ERR_ID, BufferIn);
    }
    INFO_LOG_FMT(IOS_SSL,
                 "IOCTLV_NET_SSL_SHUTDOWN "
                 "BufferIn: ({:08x}, {}), BufferIn2: ({:08x}, {}), "
                 "BufferIn3: ({:08x}, {}), BufferOut: ({:08x}, {}), "
                 "BufferOut2: ({:08x}, {}), BufferOut3: ({:08x}, {})",
                 BufferIn, BufferInSize, BufferIn2, BufferInSize2, BufferIn3, BufferInSize3,
                 BufferOut, BufferOutSize, BufferOut2, BufferOutSize2, BufferOut3, BufferOutSize3);
    break;
  }
  case IOCTLV_NET_SSL_SETROOTCA:
  {
    INFO_LOG_FMT(IOS_SSL,
                 "IOCTLV_NET_SSL_SETROOTCA "
                 "BufferIn: ({:08x}, {}), BufferIn2: ({:08x}, {}), "
                 "BufferIn3: ({:08x}, {}), BufferOut: ({:08x}, {}), "
                 "BufferOut2: ({:08x}, {}), BufferOut3: ({:08x}, {})",
                 BufferIn, BufferInSize, BufferIn2, BufferInSize2, BufferIn3, BufferInSize3,
                 BufferOut, BufferOutSize, BufferOut2, BufferOutSize2, BufferOut3, BufferOutSize3);

    int sslID = memory.Read_U32(BufferOut) - 1;
    if (IsSSLIDValid(sslID))
    {
      WII_SSL* ssl = &_SSL[sslID];
      int ret =
          mbedtls_x509_crt_parse_der(&ssl->cacert, memory.GetPointer(BufferOut2), BufferOutSize2);

      if (Config::Get(Config::MAIN_NETWORK_SSL_DUMP_ROOT_CA))
      {
        std::string filename = File::GetUserPath(D_DUMPSSL_IDX) + ssl->hostname + "_rootca.der";
        File::IOFile(filename, "wb").WriteBytes(memory.GetPointer(BufferOut2), BufferOutSize2);
      }

      if (ret)
      {
        WriteReturnValue(memory, SSL_ERR_FAILED, BufferIn);
      }
      else
      {
        mbedtls_ssl_conf_ca_chain(&ssl->config, &ssl->cacert, nullptr);
        WriteReturnValue(memory, SSL_OK, BufferIn);
      }

      INFO_LOG_FMT(IOS_SSL, "IOCTLV_NET_SSL_SETROOTCA = {}", ret);
    }
    else
    {
      WriteReturnValue(memory, SSL_ERR_ID, BufferIn);
    }
    break;
  }
  case IOCTLV_NET_SSL_SETBUILTINCLIENTCERT:
  {
    INFO_LOG_FMT(IOS_SSL,
                 "IOCTLV_NET_SSL_SETBUILTINCLIENTCERT "
                 "BufferIn: ({:08x}, {}), BufferIn2: ({:08x}, {}), "
                 "BufferIn3: ({:08x}, {}), BufferOut: ({:08x}, {}), "
                 "BufferOut2: ({:08x}, {}), BufferOut3: ({:08x}, {})",
                 BufferIn, BufferInSize, BufferIn2, BufferInSize2, BufferIn3, BufferInSize3,
                 BufferOut, BufferOutSize, BufferOut2, BufferOutSize2, BufferOut3, BufferOutSize3);

    int sslID = memory.Read_U32(BufferOut) - 1;
    if (IsSSLIDValid(sslID))
    {
      WII_SSL* ssl = &_SSL[sslID];
      const std::string cert_base_path = File::GetUserPath(D_SESSION_WIIROOT_IDX);
      const std::vector<u8> client_cert =
          ReadCertFile(cert_base_path + "/clientca.pem", s_client_cert_hash, m_cert_error_shown);
      const std::vector<u8> client_key =
          ReadCertFile(cert_base_path + "/clientcakey.pem", s_client_key_hash, m_cert_error_shown);
      // If any of the required files fail to load, show a panic alert, but only once
      // per IOS instance (usually once per emulation session).
      if (client_cert.empty() || client_key.empty())
        m_cert_error_shown = true;

      int ret = mbedtls_x509_crt_parse(&ssl->clicert, client_cert.data(), client_cert.size());
      int pk_ret = mbedtls_pk_parse_key(&ssl->pk, client_key.data(), client_key.size(), nullptr, 0);
      if (ret || pk_ret)
      {
        mbedtls_x509_crt_free(&ssl->clicert);
        mbedtls_pk_free(&ssl->pk);
        WriteReturnValue(memory, SSL_ERR_FAILED, BufferIn);
      }
      else
      {
        mbedtls_ssl_conf_own_cert(&ssl->config, &ssl->clicert, &ssl->pk);
        WriteReturnValue(memory, SSL_OK, BufferIn);
      }

      INFO_LOG_FMT(IOS_SSL, "IOCTLV_NET_SSL_SETBUILTINCLIENTCERT = ({}, {})", ret, pk_ret);
    }
    else
    {
      WriteReturnValue(memory, SSL_ERR_ID, BufferIn);
      INFO_LOG_FMT(IOS_SSL, "IOCTLV_NET_SSL_SETBUILTINCLIENTCERT invalid sslID = {}", sslID);
    }
    break;
  }
  case IOCTLV_NET_SSL_REMOVECLIENTCERT:
  {
    INFO_LOG_FMT(IOS_SSL,
                 "IOCTLV_NET_SSL_REMOVECLIENTCERT "
                 "BufferIn: ({:08x}, {}), BufferIn2: ({:08x}, {}), "
                 "BufferIn3: ({:08x}, {}), BufferOut: ({:08x}, {}), "
                 "BufferOut2: ({:08x}, {}), BufferOut3: ({:08x}, {})",
                 BufferIn, BufferInSize, BufferIn2, BufferInSize2, BufferIn3, BufferInSize3,
                 BufferOut, BufferOutSize, BufferOut2, BufferOutSize2, BufferOut3, BufferOutSize3);

    int sslID = memory.Read_U32(BufferOut) - 1;
    if (IsSSLIDValid(sslID))
    {
      WII_SSL* ssl = &_SSL[sslID];
      mbedtls_x509_crt_free(&ssl->clicert);
      mbedtls_pk_free(&ssl->pk);

      mbedtls_ssl_conf_own_cert(&ssl->config, nullptr, nullptr);
      WriteReturnValue(memory, SSL_OK, BufferIn);
    }
    else
    {
      WriteReturnValue(memory, SSL_ERR_ID, BufferIn);
      INFO_LOG_FMT(IOS_SSL, "IOCTLV_NET_SSL_SETBUILTINCLIENTCERT invalid sslID = {}", sslID);
    }
    break;
  }
  case IOCTLV_NET_SSL_SETBUILTINROOTCA:
  {
    int sslID = memory.Read_U32(BufferOut) - 1;
    if (IsSSLIDValid(sslID))
    {
      WII_SSL* ssl = &_SSL[sslID];
      const std::string cert_base_path = File::GetUserPath(D_SESSION_WIIROOT_IDX);
      const std::vector<u8> root_ca =
          ReadCertFile(cert_base_path + "/rootca.pem", s_root_ca_hash, m_cert_error_shown);
      if (root_ca.empty())
        m_cert_error_shown = true;

      int ret = mbedtls_x509_crt_parse(&ssl->cacert, root_ca.data(), root_ca.size());
      if (ret)
      {
        mbedtls_x509_crt_free(&ssl->clicert);
        WriteReturnValue(memory, SSL_ERR_FAILED, BufferIn);
      }
      else
      {
        mbedtls_ssl_conf_ca_chain(&ssl->config, &ssl->cacert, nullptr);
        WriteReturnValue(memory, SSL_OK, BufferIn);
      }
      INFO_LOG_FMT(IOS_SSL, "IOCTLV_NET_SSL_SETBUILTINROOTCA = {}", ret);
    }
    else
    {
      WriteReturnValue(memory, SSL_ERR_ID, BufferIn);
    }
    INFO_LOG_FMT(IOS_SSL,
                 "IOCTLV_NET_SSL_SETBUILTINROOTCA "
                 "BufferIn: ({:08x}, {}), BufferIn2: ({:08x}, {}), "
                 "BufferIn3: ({:08x}, {}), BufferOut: ({:08x}, {}), "
                 "BufferOut2: ({:08x}, {}), BufferOut3: ({:08x}, {})",
                 BufferIn, BufferInSize, BufferIn2, BufferInSize2, BufferIn3, BufferInSize3,
                 BufferOut, BufferOutSize, BufferOut2, BufferOutSize2, BufferOut3, BufferOutSize3);
    break;
  }
  case IOCTLV_NET_SSL_CONNECT:
  {
    int sslID = memory.Read_U32(BufferOut) - 1;
    if (IsSSLIDValid(sslID))
    {
      WII_SSL* ssl = &_SSL[sslID];
      mbedtls_ssl_setup(&ssl->ctx, &ssl->config);
      ssl->sockfd = memory.Read_U32(BufferOut2);
      ssl->hostfd = GetEmulationKernel().GetSocketManager()->GetHostSocket(ssl->sockfd);
      INFO_LOG_FMT(IOS_SSL, "IOCTLV_NET_SSL_CONNECT socket = {}", ssl->sockfd);
      mbedtls_ssl_set_bio(&ssl->ctx, ssl, SSLSendWithoutSNI, SSLRecv, nullptr);
      WriteReturnValue(memory, SSL_OK, BufferIn);
    }
    else
    {
      WriteReturnValue(memory, SSL_ERR_ID, BufferIn);
    }
    INFO_LOG_FMT(IOS_SSL,
                 "IOCTLV_NET_SSL_CONNECT "
                 "BufferIn: ({:08x}, {}), BufferIn2: ({:08x}, {}), "
                 "BufferIn3: ({:08x}, {}), BufferOut: ({:08x}, {}), "
                 "BufferOut2: ({:08x}, {}), BufferOut3: ({:08x}, {})",
                 BufferIn, BufferInSize, BufferIn2, BufferInSize2, BufferIn3, BufferInSize3,
                 BufferOut, BufferOutSize, BufferOut2, BufferOutSize2, BufferOut3, BufferOutSize3);
    break;
  }
  case IOCTLV_NET_SSL_DOHANDSHAKE:
  {
    int sslID = memory.Read_U32(BufferOut) - 1;
    if (IsSSLIDValid(sslID))
    {
      GetEmulationKernel().GetSocketManager()->DoSock(_SSL[sslID].sockfd, request,
                                                      IOCTLV_NET_SSL_DOHANDSHAKE);
      return std::nullopt;
    }
    else
    {
      WriteReturnValue(memory, SSL_ERR_ID, BufferIn);
    }
    break;
  }
  case IOCTLV_NET_SSL_WRITE:
  {
    const int sslID = memory.Read_U32(BufferOut) - 1;
    if (IsSSLIDValid(sslID))
    {
      GetEmulationKernel().GetSocketManager()->DoSock(_SSL[sslID].sockfd, request,
                                                      IOCTLV_NET_SSL_WRITE);
      return std::nullopt;
    }
    else
    {
      WriteReturnValue(memory, SSL_ERR_ID, BufferIn);
    }
    INFO_LOG_FMT(IOS_SSL,
                 "IOCTLV_NET_SSL_WRITE "
                 "BufferIn: ({:08x}, {}), BufferIn2: ({:08x}, {}), "
                 "BufferIn3: ({:08x}, {}), BufferOut: ({:08x}, {}), "
                 "BufferOut2: ({:08x}, {}), BufferOut3: ({:08x}, {})",
                 BufferIn, BufferInSize, BufferIn2, BufferInSize2, BufferIn3, BufferInSize3,
                 BufferOut, BufferOutSize, BufferOut2, BufferOutSize2, BufferOut3, BufferOutSize3);
    INFO_LOG_FMT(IOS_SSL, "{}", memory.GetString(BufferOut2));
    break;
  }
  case IOCTLV_NET_SSL_READ:
  {
    int ret = 0;
    int sslID = memory.Read_U32(BufferOut) - 1;
    if (IsSSLIDValid(sslID))
    {
      GetEmulationKernel().GetSocketManager()->DoSock(_SSL[sslID].sockfd, request,
                                                      IOCTLV_NET_SSL_READ);
      return std::nullopt;
    }
    else
    {
      WriteReturnValue(memory, SSL_ERR_ID, BufferIn);
    }

    INFO_LOG_FMT(IOS_SSL,
                 "IOCTLV_NET_SSL_READ({})"
                 "BufferIn: ({:08x}, {}), BufferIn2: ({:08x}, {}), "
                 "BufferIn3: ({:08x}, {}), BufferOut: ({:08x}, {}), "
                 "BufferOut2: ({:08x}, {}), BufferOut3: ({:08x}, {})",
                 ret, BufferIn, BufferInSize, BufferIn2, BufferInSize2, BufferIn3, BufferInSize3,
                 BufferOut, BufferOutSize, BufferOut2, BufferOutSize2, BufferOut3, BufferOutSize3);
    break;
  }
  case IOCTLV_NET_SSL_SETROOTCADEFAULT:
  {
    int sslID = memory.Read_U32(BufferOut) - 1;
    if (IsSSLIDValid(sslID))
    {
      WriteReturnValue(memory, SSL_OK, BufferIn);
    }
    else
    {
      WriteReturnValue(memory, SSL_ERR_ID, BufferIn);
    }
    INFO_LOG_FMT(IOS_SSL,
                 "IOCTLV_NET_SSL_SETROOTCADEFAULT "
                 "BufferIn: ({:08x}, {}), BufferIn2: ({:08x}, {}), "
                 "BufferIn3: ({:08x}, {}), BufferOut: ({:08x}, {}), "
                 "BufferOut2: ({:08x}, {}), BufferOut3: ({:08x}, {})",
                 BufferIn, BufferInSize, BufferIn2, BufferInSize2, BufferIn3, BufferInSize3,
                 BufferOut, BufferOutSize, BufferOut2, BufferOutSize2, BufferOut3, BufferOutSize3);
    break;
  }
  case IOCTLV_NET_SSL_SETCLIENTCERTDEFAULT:
  {
    INFO_LOG_FMT(IOS_SSL,
                 "IOCTLV_NET_SSL_SETCLIENTCERTDEFAULT "
                 "BufferIn: ({:08x}, {}), BufferIn2: ({:08x}, {}), "
                 "BufferIn3: ({:08x}, {}), BufferOut: ({:08x}, {}), "
                 "BufferOut2: ({:08x}, {}), BufferOut3: ({:08x}, {})",
                 BufferIn, BufferInSize, BufferIn2, BufferInSize2, BufferIn3, BufferInSize3,
                 BufferOut, BufferOutSize, BufferOut2, BufferOutSize2, BufferOut3, BufferOutSize3);

    int sslID = memory.Read_U32(BufferOut) - 1;
    if (IsSSLIDValid(sslID))
    {
      WriteReturnValue(memory, SSL_OK, BufferIn);
    }
    else
    {
      WriteReturnValue(memory, SSL_ERR_ID, BufferIn);
    }
    break;
  }
  default:
    request.DumpUnknown(system, GetDeviceName(), Common::Log::LogType::IOS_SSL);
  }

  // SSL return codes are written to BufferIn
  return IPCReply(IPC_SUCCESS);
}
}  // namespace IOS::HLE
