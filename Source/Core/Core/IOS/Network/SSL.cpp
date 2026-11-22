// Copyright 2011 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/IOS/Network/SSL.h"

#include <array>
#include <vector>

#include <wolfssl/ssl.h>
#include <wolfssl/wolfcrypt/sha256.h>

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

namespace
{
int SSLSend(WOLFSSL* ssl, char* buf, int len, void* ctx)
{
  auto* wii_ssl = static_cast<WII_SSL*>(ctx);
  const int ret = EmbedSend(ssl, buf, len, &wii_ssl->hostfd);

  // Log raw SSL packets if we don't dump unencrypted SSL writes
  if (!Config::Get(Config::MAIN_NETWORK_SSL_DUMP_WRITE) && ret > 0)
  {
    Core::System::GetInstance().GetPowerPC().GetDebugInterface().NetworkLogger()->LogWrite(
        reinterpret_cast<const u8*>(buf), ret, wii_ssl->hostfd, nullptr);
  }

  return ret;
}

int SSLRecv(WOLFSSL* ssl, char* buf, int len, void* ctx)
{
  auto* wii_ssl = static_cast<WII_SSL*>(ctx);
  const int ret = EmbedReceive(ssl, buf, len, &wii_ssl->hostfd);

  // Log raw SSL packets if we don't dump unencrypted SSL reads
  if (!Config::Get(Config::MAIN_NETWORK_SSL_DUMP_READ) && ret > 0)
  {
    Core::System::GetInstance().GetPowerPC().GetDebugInterface().NetworkLogger()->LogRead(
        reinterpret_cast<const u8*>(buf), ret, wii_ssl->hostfd, nullptr);
  }

  return ret;
}

// Called for every certificate of the server chain, valid or not, even when verification is
// disabled (WOLFSSL_ALWAYS_VERIFY_CB, set by OPENSSL_EXTRA). Depth 0 is the server certificate.
int SSLVerify(int preverify, WOLFSSL_X509_STORE_CTX* store)
{
  auto* wii_ssl = static_cast<WII_SSL*>(store->userCtx);
  const int depth = wolfSSL_X509_STORE_CTX_get_error_depth(store);

  // wolfSSL does not check the name when the host name is empty, which would accept a valid
  // certificate issued for any domain: report a name mismatch instead.
  if (preverify && depth == 0 && wii_ssl != nullptr && wii_ssl->hostname.empty() &&
      (wolfSSL_get_verify_mode(wii_ssl->ssl) & WOLFSSL_VERIFY_PEER) != 0)
  {
    wolfSSL_X509_STORE_CTX_set_error(store, DOMAIN_NAME_MISMATCH);
    preverify = 0;
  }

  if (!preverify)
  {
    const int error = wolfSSL_X509_STORE_CTX_get_error(store);
    ERROR_LOG_FMT(IOS_SSL, "Certificate verification failed (depth {}): {} ({})", depth,
                  wolfSSL_ERR_reason_error_string(error), error);
    if (wii_ssl != nullptr && wii_ssl->verify_error == 0)
      wii_ssl->verify_error = (error != 0) ? error : VERIFY_CERT_ERROR;
  }

  if (depth == 0 && wii_ssl != nullptr && Config::Get(Config::MAIN_NETWORK_SSL_DUMP_PEER_CERT))
  {
    int der_size = 0;
    const u8* der = wolfSSL_X509_get_der(wolfSSL_X509_STORE_CTX_get_current_cert(store), &der_size);
    if (der != nullptr && der_size > 0)
    {
      const std::string filename =
          File::GetUserPath(D_DUMPSSL_IDX) + wii_ssl->hostname + "_peercert.der";
      File::IOFile(filename, "wb").WriteBytes(der, der_size);
    }
  }

  return preverify;
}

void FreeSSL(WII_SSL* ssl)
{
  if (ssl->ssl != nullptr)
  {
    if (wolfSSL_is_init_finished(ssl->ssl))
      wolfSSL_shutdown(ssl->ssl);
    wolfSSL_free(ssl->ssl);
    ssl->ssl = nullptr;
  }
  if (ssl->ctx != nullptr)
  {
    wolfSSL_CTX_free(ssl->ctx);
    ssl->ctx = nullptr;
  }

  ssl->sockfd = -1;
  ssl->hostfd = -1;
  ssl->hostname.clear();
  ssl->verify_error = 0;
  ssl->active = false;
}

bool CreateSSL(WII_SSL* ssl, bool verify)
{
  ssl->ctx = wolfSSL_CTX_new(wolfSSLv23_client_method());
  if (ssl->ctx == nullptr)
    return false;

  // The real Wii negotiates TLS 1.0 at most; servers still in use accept TLS 1.2.
  // TLS 1.3 is compiled in for curl and must not be offered here.
  if (wolfSSL_CTX_set_min_proto_version(ssl->ctx, TLS1_VERSION) != WOLFSSL_SUCCESS ||
      wolfSSL_CTX_set_max_proto_version(ssl->ctx, TLS1_2_VERSION) != WOLFSSL_SUCCESS ||
      wolfSSL_CTX_UseSecureRenegotiation(ssl->ctx) != WOLFSSL_SUCCESS ||
      wolfSSL_CTX_SetMinRsaKey_Sz(ssl->ctx, 0) != WOLFSSL_SUCCESS ||
      wolfSSL_CTX_SetMinEccKey_Sz(ssl->ctx, 0) != WOLFSSL_SUCCESS)
  {
    return false;
  }

  ssl->ssl = wolfSSL_new(ssl->ctx);
  if (ssl->ssl == nullptr)
    return false;

  wolfSSL_set_verify(ssl->ssl, verify ? WOLFSSL_VERIFY_PEER : WOLFSSL_VERIFY_NONE, SSLVerify);
  wolfSSL_SetCertCbCtx(ssl->ssl, ssl);
  // Checked against the subject alternative names and the common name. Unlike
  // wolfSSL_check_domain_name(), any string is accepted (IP addresses, single labels...), as with
  // mbedTLS. An empty name is not checked.
  if (verify && wolfSSL_X509_VERIFY_PARAM_set1_host(wolfSSL_get0_param(ssl->ssl),
                                                    ssl->hostname.c_str(), 0) != WOLFSSL_SUCCESS)
  {
    return false;
  }

  wolfSSL_SSLSetIORecv(ssl->ssl, SSLRecv);
  wolfSSL_SSLSetIOSend(ssl->ssl, SSLSend);
  wolfSSL_SetIOReadCtx(ssl->ssl, ssl);
  wolfSSL_SetIOWriteCtx(ssl->ssl, ssl);
  return true;
}
}  // namespace

NetSSLDevice::NetSSLDevice(EmulationKernel& ios, const std::string& device_name)
    : EmulationDevice(ios, device_name)
{
  [[maybe_unused]] static const int init_ret = wolfSSL_Init();

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
      FreeSSL(&ssl);
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
  if (wc_Sha256Hash(bytes.data(), static_cast<word32>(bytes.size()), hash.data()) != 0 ||
      hash != correct_hash)
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
      ssl->hostname = hostname;
      const bool verify = Config::Get(Config::MAIN_NETWORK_SSL_VERIFY_CERTIFICATES) && verifyOption;
      if (!CreateSSL(ssl, verify))
      {
        FreeSSL(ssl);
        goto _SSL_NEW_ERROR;
      }

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
      FreeSSL(&_SSL[sslID]);

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
      int ret = wolfSSL_CTX_load_verify_buffer_ex(
          ssl->ctx, memory.GetPointerForRange(BufferOut2, BufferOutSize2), BufferOutSize2,
          WOLFSSL_FILETYPE_ASN1, 0, WOLFSSL_LOAD_FLAG_DATE_ERR_OKAY);

      if (Config::Get(Config::MAIN_NETWORK_SSL_DUMP_ROOT_CA))
      {
        std::string filename = File::GetUserPath(D_DUMPSSL_IDX) + ssl->hostname + "_rootca.der";
        File::IOFile(filename, "wb")
            .WriteBytes(memory.GetPointerForRange(BufferOut2, BufferOutSize2), BufferOutSize2);
      }

      WriteReturnValue(memory, ret == WOLFSSL_SUCCESS ? SSL_OK : SSL_ERR_FAILED, BufferIn);
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

      const int ret = wolfSSL_use_certificate_buffer(ssl->ssl, client_cert.data(),
                                                     static_cast<long>(client_cert.size()),
                                                     WOLFSSL_FILETYPE_ASN1);
      const int pk_ret = wolfSSL_use_PrivateKey_buffer(
          ssl->ssl, client_key.data(), static_cast<long>(client_key.size()), WOLFSSL_FILETYPE_ASN1);
      if (ret != WOLFSSL_SUCCESS || pk_ret != WOLFSSL_SUCCESS)
      {
        wolfSSL_UnloadCertsKeys(ssl->ssl);
        WriteReturnValue(memory, SSL_ERR_FAILED, BufferIn);
      }
      else
      {
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
      wolfSSL_UnloadCertsKeys(ssl->ssl);
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

      const int ret = wolfSSL_CTX_load_verify_buffer_ex(
          ssl->ctx, root_ca.data(), static_cast<long>(root_ca.size()), WOLFSSL_FILETYPE_ASN1, 0,
          WOLFSSL_LOAD_FLAG_DATE_ERR_OKAY);
      WriteReturnValue(memory, ret == WOLFSSL_SUCCESS ? SSL_OK : SSL_ERR_FAILED, BufferIn);
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
      ssl->sockfd = memory.Read_U32(BufferOut2);
      ssl->hostfd = GetEmulationKernel().GetSocketManager()->GetHostSocket(ssl->sockfd);
      INFO_LOG_FMT(IOS_SSL, "IOCTLV_NET_SSL_CONNECT socket = {}", ssl->sockfd);
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
