// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

// No Wii socket support while using NetPlay or TAS
#include "Core/IOS/Network/Socket.h"

#include <algorithm>
#include <mbedtls/error.h>
#ifndef _WIN32
#include <arpa/inet.h>
#include <unistd.h>
#endif
#ifdef __HAIKU__
#include <sys/select.h>
#endif

#include "Common/File.h"
#include "Common/FileUtil.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/IOS/Device.h"
#include "Core/IOS/IOS.h"

#ifdef _WIN32
#define ERRORCODE(name) WSA##name
#define EITHER(win32, posix) win32
#else
#define ERRORCODE(name) name
#define EITHER(win32, posix) posix
#define closesocket close
#endif

namespace IOS
{
namespace HLE
{
constexpr int WII_SOCKET_FD_MAX = 24;

char* WiiSockMan::DecodeError(s32 ErrorCode)
{
#ifdef _WIN32
  // NOT THREAD SAFE
  static char Message[1024];

  FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS |
                     FORMAT_MESSAGE_MAX_WIDTH_MASK,
                 nullptr, ErrorCode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), Message,
                 sizeof(Message), nullptr);

  return Message;
#else
  return strerror(ErrorCode);
#endif
}

static s32 TranslateErrorCode(s32 native_error, bool isRW)
{
  switch (native_error)
  {
  case ERRORCODE(EMSGSIZE):
    ERROR_LOG(IOS_NET, "Find out why this happened, looks like PEEK failure?");
    return -1;  // Should be -SO_EMSGSIZE
  case EITHER(WSAENOTSOCK, EBADF):
    return -SO_EBADF;
  case ERRORCODE(EADDRINUSE):
    return -SO_EADDRINUSE;
  case ERRORCODE(ECONNRESET):
    return -SO_ECONNRESET;
  case ERRORCODE(EISCONN):
    return -SO_EISCONN;
  case ERRORCODE(ENOTCONN):
    return -SO_EAGAIN;  // After proper blocking SO_EAGAIN shouldn't be needed...
  case ERRORCODE(EINPROGRESS):
    return -SO_EINPROGRESS;
  case ERRORCODE(EALREADY):
    return -SO_EALREADY;
  case ERRORCODE(EACCES):
    return -SO_EACCES;
  case ERRORCODE(ECONNREFUSED):
    return -SO_ECONNREFUSED;
  case ERRORCODE(ENETUNREACH):
    return -SO_ENETUNREACH;
  case ERRORCODE(EHOSTUNREACH):
    return -SO_EHOSTUNREACH;
  case ENOMEM:  // See man (7) ip
  case ERRORCODE(ENOBUFS):
    return -SO_ENOMEM;
  case ERRORCODE(ENETRESET):
    return -SO_ENETRESET;
  case EITHER(WSAEWOULDBLOCK, EAGAIN):
    if (isRW)
    {
      return -SO_EAGAIN;  // EAGAIN
    }
    else
    {
      return -SO_EINPROGRESS;  // EINPROGRESS
    }
  default:
    return -1;
  }
}

// Don't use string! (see https://github.com/dolphin-emu/dolphin/pull/3143)
s32 WiiSockMan::GetNetErrorCode(s32 ret, const char* caller, bool isRW)
{
#ifdef _WIN32
  s32 errorCode = WSAGetLastError();
#else
  s32 errorCode = errno;
#endif

  if (ret >= 0)
  {
    WiiSockMan::GetInstance().SetLastNetError(ret);
    return ret;
  }

  ERROR_LOG(IOS_NET, "%s failed with error %d: %s, ret= %d", caller, errorCode,
            DecodeError(errorCode), ret);

  s32 ReturnValue = TranslateErrorCode(errorCode, isRW);
  WiiSockMan::GetInstance().SetLastNetError(ReturnValue);

  return ReturnValue;
}

WiiSocket::~WiiSocket()
{
  if (fd >= 0)
  {
    (void)CloseFd();
  }
}

void WiiSocket::SetFd(s32 s)
{
  if (fd >= 0)
    (void)CloseFd();

  nonBlock = false;
  fd = s;

// Set socket to NON-BLOCK
#ifdef _WIN32
  u_long iMode = 1;
  ioctlsocket(fd, FIONBIO, &iMode);
#else
  int flags;
  if (-1 == (flags = fcntl(fd, F_GETFL, 0)))
    flags = 0;
  fcntl(fd, F_SETFL, flags | O_NONBLOCK);
#endif
}

void WiiSocket::SetWiiFd(s32 s)
{
  wii_fd = s;
}

s32 WiiSocket::CloseFd()
{
  s32 ReturnValue = 0;
  if (fd >= 0)
  {
    s32 ret = closesocket(fd);
    ReturnValue = WiiSockMan::GetNetErrorCode(ret, "CloseFd", false);
  }
  else
  {
    ReturnValue = WiiSockMan::GetNetErrorCode(EITHER(WSAENOTSOCK, EBADF), "CloseFd", false);
  }
  fd = -1;
  return ReturnValue;
}

s32 WiiSocket::FCntl(u32 cmd, u32 arg)
{
#ifndef F_GETFL
#define F_GETFL 3
#endif
#ifndef F_SETFL
#define F_SETFL 4
#endif
#define F_NONBLOCK 4
  s32 ret = 0;
  if (cmd == F_GETFL)
  {
    ret = nonBlock ? F_NONBLOCK : 0;
  }
  else if (cmd == F_SETFL)
  {
    nonBlock = (arg & F_NONBLOCK) == F_NONBLOCK;
  }
  else
  {
    ERROR_LOG(IOS_NET, "SO_FCNTL unknown command");
  }

  INFO_LOG(IOS_NET, "IOCTL_SO_FCNTL(%08x, %08X, %08X)", wii_fd, cmd, arg);

  return ret;
}

void WiiSocket::Update(bool read, bool write, bool except)
{
  auto it = pending_sockops.begin();
  while (it != pending_sockops.end())
  {
    s32 ReturnValue = 0;
    bool forceNonBlock = false;
    IPCCommandType ct = it->request.command;
    if (!it->is_ssl && ct == IPC_CMD_IOCTL)
    {
      IOCtlRequest ioctl{it->request.address};
      switch (it->net_type)
      {
      case IOCTL_SO_FCNTL:
      {
        u32 cmd = Memory::Read_U32(ioctl.buffer_in + 4);
        u32 arg = Memory::Read_U32(ioctl.buffer_in + 8);
        ReturnValue = FCntl(cmd, arg);
        break;
      }
      case IOCTL_SO_BIND:
      {
        sockaddr_in local_name;
        WiiSockAddrIn* wii_name = (WiiSockAddrIn*)Memory::GetPointer(ioctl.buffer_in + 8);
        WiiSockMan::Convert(*wii_name, local_name);

        int ret = bind(fd, (sockaddr*)&local_name, sizeof(local_name));
        ReturnValue = WiiSockMan::GetNetErrorCode(ret, "SO_BIND", false);

        INFO_LOG(IOS_NET, "IOCTL_SO_BIND (%08X, %s:%d) = %d", wii_fd,
                 inet_ntoa(local_name.sin_addr), Common::swap16(local_name.sin_port), ret);
        break;
      }
      case IOCTL_SO_CONNECT:
      {
        sockaddr_in local_name;
        WiiSockAddrIn* wii_name = (WiiSockAddrIn*)Memory::GetPointer(ioctl.buffer_in + 8);
        WiiSockMan::Convert(*wii_name, local_name);

        int ret = connect(fd, (sockaddr*)&local_name, sizeof(local_name));
        ReturnValue = WiiSockMan::GetNetErrorCode(ret, "SO_CONNECT", false);

        INFO_LOG(IOS_NET, "IOCTL_SO_CONNECT (%08x, %s:%d) = %d", wii_fd,
                 inet_ntoa(local_name.sin_addr), Common::swap16(local_name.sin_port), ret);
        break;
      }
      case IOCTL_SO_ACCEPT:
      {
        s32 ret;
        if (ioctl.buffer_out_size > 0)
        {
          sockaddr_in local_name;
          WiiSockAddrIn* wii_name = (WiiSockAddrIn*)Memory::GetPointer(ioctl.buffer_out);
          WiiSockMan::Convert(*wii_name, local_name);

          socklen_t addrlen = sizeof(sockaddr_in);
          ret = static_cast<s32>(accept(fd, (sockaddr*)&local_name, &addrlen));

          WiiSockMan::Convert(local_name, *wii_name, addrlen);
        }
        else
        {
          ret = static_cast<s32>(accept(fd, nullptr, nullptr));
        }

        ReturnValue = WiiSockMan::GetInstance().AddSocket(ret, true);

        ioctl.Log("IOCTL_SO_ACCEPT", LogTypes::IOS_NET);
        break;
      }
      default:
        break;
      }

      // Fix blocking error codes
      if (!nonBlock)
      {
        if (it->net_type == IOCTL_SO_CONNECT && ReturnValue == -SO_EISCONN)
        {
          ReturnValue = SO_SUCCESS;
        }
      }
    }
    else if (ct == IPC_CMD_IOCTLV)
    {
      IOCtlVRequest ioctlv{it->request.address};
      u32 BufferIn = 0, BufferIn2 = 0;
      u32 BufferInSize = 0, BufferInSize2 = 0;
      u32 BufferOut = 0, BufferOut2 = 0;
      u32 BufferOutSize = 0, BufferOutSize2 = 0;

      if (ioctlv.in_vectors.size() > 0)
      {
        BufferIn = ioctlv.in_vectors.at(0).address;
        BufferInSize = ioctlv.in_vectors.at(0).size;
      }

      if (ioctlv.io_vectors.size() > 0)
      {
        BufferOut = ioctlv.io_vectors.at(0).address;
        BufferOutSize = ioctlv.io_vectors.at(0).size;
      }

      if (ioctlv.io_vectors.size() > 1)
      {
        BufferOut2 = ioctlv.io_vectors.at(1).address;
        BufferOutSize2 = ioctlv.io_vectors.at(1).size;
      }

      if (ioctlv.in_vectors.size() > 1)
      {
        BufferIn2 = ioctlv.in_vectors.at(1).address;
        BufferInSize2 = ioctlv.in_vectors.at(1).size;
      }

      if (it->is_ssl)
      {
        int sslID = Memory::Read_U32(BufferOut) - 1;
        if (SSLID_VALID(sslID))
        {
          switch (it->ssl_type)
          {
          case IOCTLV_NET_SSL_DOHANDSHAKE:
          {
            mbedtls_ssl_context* ctx = &Device::NetSSL::_SSL[sslID].ctx;
            int ret = mbedtls_ssl_handshake(ctx);
            if (ret)
            {
              char error_buffer[256] = "";
              mbedtls_strerror(ret, error_buffer, sizeof(error_buffer));
              ERROR_LOG(IOS_SSL, "IOCTLV_NET_SSL_DOHANDSHAKE: %s", error_buffer);
            }
            switch (ret)
            {
            case 0:
              WriteReturnValue(SSL_OK, BufferIn);
              break;
            case MBEDTLS_ERR_SSL_WANT_READ:
              WriteReturnValue(SSL_ERR_RAGAIN, BufferIn);
              if (!nonBlock)
                ReturnValue = SSL_ERR_RAGAIN;
              break;
            case MBEDTLS_ERR_SSL_WANT_WRITE:
              WriteReturnValue(SSL_ERR_WAGAIN, BufferIn);
              if (!nonBlock)
                ReturnValue = SSL_ERR_WAGAIN;
              break;
            case MBEDTLS_ERR_X509_CERT_VERIFY_FAILED:
            {
              char error_buffer[256] = "";
              int res = mbedtls_ssl_get_verify_result(ctx);
              mbedtls_x509_crt_verify_info(error_buffer, sizeof(error_buffer), "", res);
              ERROR_LOG(IOS_SSL, "MBEDTLS_ERR_X509_CERT_VERIFY_FAILED (verify_result = %d): %s",
                        res, error_buffer);

              if (res & MBEDTLS_X509_BADCERT_CN_MISMATCH)
                res = SSL_ERR_VCOMMONNAME;
              else if (res & MBEDTLS_X509_BADCERT_NOT_TRUSTED)
                res = SSL_ERR_VROOTCA;
              else if (res & MBEDTLS_X509_BADCERT_REVOKED)
                res = SSL_ERR_VCHAIN;
              else if (res & MBEDTLS_X509_BADCERT_EXPIRED || res & MBEDTLS_X509_BADCERT_FUTURE)
                res = SSL_ERR_VDATE;
              else
                res = SSL_ERR_FAILED;

              WriteReturnValue(res, BufferIn);
              if (!nonBlock)
                ReturnValue = res;
              break;
            }
            default:
              WriteReturnValue(SSL_ERR_FAILED, BufferIn);
              break;
            }

            // mbedtls_ssl_get_peer_cert(ctx) seems not to work if handshake failed
            // Below is an alternative to dump the peer certificate
            if (SConfig::GetInstance().m_SSLDumpPeerCert && ctx->session_negotiate != nullptr)
            {
              const mbedtls_x509_crt* cert = ctx->session_negotiate->peer_cert;
              if (cert != nullptr)
              {
                std::string filename = File::GetUserPath(D_DUMPSSL_IDX) +
                                       ((ctx->hostname != nullptr) ? ctx->hostname : "") +
                                       "_peercert.der";
                File::IOFile(filename, "wb").WriteBytes(cert->raw.p, cert->raw.len);
              }
            }

            INFO_LOG(IOS_SSL, "IOCTLV_NET_SSL_DOHANDSHAKE = (%d) "
                              "BufferIn: (%08x, %i), BufferIn2: (%08x, %i), "
                              "BufferOut: (%08x, %i), BufferOut2: (%08x, %i)",
                     ret, BufferIn, BufferInSize, BufferIn2, BufferInSize2, BufferOut,
                     BufferOutSize, BufferOut2, BufferOutSize2);
            break;
          }
          case IOCTLV_NET_SSL_WRITE:
          {
            int ret = mbedtls_ssl_write(&Device::NetSSL::_SSL[sslID].ctx,
                                        Memory::GetPointer(BufferOut2), BufferOutSize2);

            if (SConfig::GetInstance().m_SSLDumpWrite && ret > 0)
            {
              std::string filename = File::GetUserPath(D_DUMPSSL_IDX) +
                                     SConfig::GetInstance().GetGameID() + "_write.bin";
              File::IOFile(filename, "ab").WriteBytes(Memory::GetPointer(BufferOut2), ret);
            }

            if (ret >= 0)
            {
              // Return bytes written or SSL_ERR_ZERO if none
              WriteReturnValue((ret == 0) ? SSL_ERR_ZERO : ret, BufferIn);
            }
            else
            {
              switch (ret)
              {
              case MBEDTLS_ERR_SSL_WANT_READ:
                WriteReturnValue(SSL_ERR_RAGAIN, BufferIn);
                if (!nonBlock)
                  ReturnValue = SSL_ERR_RAGAIN;
                break;
              case MBEDTLS_ERR_SSL_WANT_WRITE:
                WriteReturnValue(SSL_ERR_WAGAIN, BufferIn);
                if (!nonBlock)
                  ReturnValue = SSL_ERR_WAGAIN;
                break;
              default:
                WriteReturnValue(SSL_ERR_FAILED, BufferIn);
                break;
              }
            }
            break;
          }
          case IOCTLV_NET_SSL_READ:
          {
            int ret = mbedtls_ssl_read(&Device::NetSSL::_SSL[sslID].ctx,
                                       Memory::GetPointer(BufferIn2), BufferInSize2);

            if (SConfig::GetInstance().m_SSLDumpRead && ret > 0)
            {
              std::string filename = File::GetUserPath(D_DUMPSSL_IDX) +
                                     SConfig::GetInstance().GetGameID() + "_read.bin";
              File::IOFile(filename, "ab").WriteBytes(Memory::GetPointer(BufferIn2), ret);
            }

            if (ret >= 0)
            {
              // Return bytes read or SSL_ERR_ZERO if none
              WriteReturnValue((ret == 0) ? SSL_ERR_ZERO : ret, BufferIn);
            }
            else
            {
              switch (ret)
              {
              case MBEDTLS_ERR_SSL_WANT_READ:
                WriteReturnValue(SSL_ERR_RAGAIN, BufferIn);
                if (!nonBlock)
                  ReturnValue = SSL_ERR_RAGAIN;
                break;
              case MBEDTLS_ERR_SSL_WANT_WRITE:
                WriteReturnValue(SSL_ERR_WAGAIN, BufferIn);
                if (!nonBlock)
                  ReturnValue = SSL_ERR_WAGAIN;
                break;
              default:
                WriteReturnValue(SSL_ERR_FAILED, BufferIn);
                break;
              }
            }
            break;
          }
          default:
            break;
          }
        }
        else
        {
          WriteReturnValue(SSL_ERR_ID, BufferIn);
        }
      }
      else
      {
        switch (it->net_type)
        {
        case IOCTLV_SO_SENDTO:
        {
          u32 flags = Memory::Read_U32(BufferIn2 + 0x04);
          u32 has_destaddr = Memory::Read_U32(BufferIn2 + 0x08);

          // Not a string, Windows requires a const char* for sendto
          const char* data = (const char*)Memory::GetPointer(BufferIn);

          // Act as non blocking when SO_MSG_NONBLOCK is specified
          forceNonBlock = ((flags & SO_MSG_NONBLOCK) == SO_MSG_NONBLOCK);
          // send/sendto only handles MSG_OOB
          flags &= SO_MSG_OOB;

          sockaddr_in local_name = {0};
          if (has_destaddr)
          {
            WiiSockAddrIn* wii_name = (WiiSockAddrIn*)Memory::GetPointer(BufferIn2 + 0x0C);
            WiiSockMan::Convert(*wii_name, local_name);
          }

          int ret = sendto(fd, data, BufferInSize, flags,
                           has_destaddr ? (struct sockaddr*)&local_name : nullptr,
                           has_destaddr ? sizeof(sockaddr) : 0);
          ReturnValue = WiiSockMan::GetNetErrorCode(ret, "SO_SENDTO", true);

          DEBUG_LOG(
              IOS_NET,
              "%s = %d Socket: %08x, BufferIn: (%08x, %i), BufferIn2: (%08x, %i), %u.%u.%u.%u",
              has_destaddr ? "IOCTLV_SO_SENDTO " : "IOCTLV_SO_SEND ", ReturnValue, wii_fd, BufferIn,
              BufferInSize, BufferIn2, BufferInSize2, local_name.sin_addr.s_addr & 0xFF,
              (local_name.sin_addr.s_addr >> 8) & 0xFF, (local_name.sin_addr.s_addr >> 16) & 0xFF,
              (local_name.sin_addr.s_addr >> 24) & 0xFF);
          break;
        }
        case IOCTLV_SO_RECVFROM:
        {
          u32 flags = Memory::Read_U32(BufferIn + 0x04);
          // Not a string, Windows requires a char* for recvfrom
          char* data = (char*)Memory::GetPointer(BufferOut);
          int data_len = BufferOutSize;

          sockaddr_in local_name;
          memset(&local_name, 0, sizeof(sockaddr_in));

          if (BufferOutSize2 != 0)
          {
            WiiSockAddrIn* wii_name = (WiiSockAddrIn*)Memory::GetPointer(BufferOut2);
            WiiSockMan::Convert(*wii_name, local_name);
          }

          // Act as non blocking when SO_MSG_NONBLOCK is specified
          forceNonBlock = ((flags & SO_MSG_NONBLOCK) == SO_MSG_NONBLOCK);

          // recv/recvfrom only handles PEEK/OOB
          flags &= SO_MSG_PEEK | SO_MSG_OOB;
#ifdef _WIN32
          if (flags & SO_MSG_PEEK)
          {
            unsigned long totallen = 0;
            ioctlsocket(fd, FIONREAD, &totallen);
            ReturnValue = totallen;
            break;
          }
#endif
          socklen_t addrlen = sizeof(sockaddr_in);
          int ret = recvfrom(fd, data, data_len, flags,
                             BufferOutSize2 ? (struct sockaddr*)&local_name : nullptr,
                             BufferOutSize2 ? &addrlen : nullptr);
          ReturnValue =
              WiiSockMan::GetNetErrorCode(ret, BufferOutSize2 ? "SO_RECVFROM" : "SO_RECV", true);

          INFO_LOG(IOS_NET, "%s(%d, %p) Socket: %08X, Flags: %08X, "
                            "BufferIn: (%08x, %i), BufferIn2: (%08x, %i), "
                            "BufferOut: (%08x, %i), BufferOut2: (%08x, %i)",
                   BufferOutSize2 ? "IOCTLV_SO_RECVFROM " : "IOCTLV_SO_RECV ", ReturnValue, data,
                   wii_fd, flags, BufferIn, BufferInSize, BufferIn2, BufferInSize2, BufferOut,
                   BufferOutSize, BufferOut2, BufferOutSize2);

          if (BufferOutSize2 != 0)
          {
            WiiSockAddrIn* wii_name = (WiiSockAddrIn*)Memory::GetPointer(BufferOut2);
            WiiSockMan::Convert(local_name, *wii_name, addrlen);
          }
          break;
        }
        default:
          break;
        }
      }
    }

    if (nonBlock || forceNonBlock ||
        (!it->is_ssl && ReturnValue != -SO_EAGAIN && ReturnValue != -SO_EINPROGRESS &&
         ReturnValue != -SO_EALREADY) ||
        (it->is_ssl && ReturnValue != SSL_ERR_WAGAIN && ReturnValue != SSL_ERR_RAGAIN))
    {
      DEBUG_LOG(IOS_NET,
                "IOCTL(V) Sock: %08x ioctl/v: %d returned: %d nonBlock: %d forceNonBlock: %d",
                wii_fd, it->is_ssl ? (int)it->ssl_type : (int)it->net_type, ReturnValue, nonBlock,
                forceNonBlock);

      // TODO: remove the dependency on a running IOS instance.
      GetIOS()->EnqueueIPCReply(it->request, ReturnValue);
      it = pending_sockops.erase(it);
    }
    else
    {
      ++it;
    }
  }
}

void WiiSocket::DoSock(Request request, NET_IOCTL type)
{
  sockop so = {request, false};
  so.net_type = type;
  pending_sockops.push_back(so);
}

void WiiSocket::DoSock(Request request, SSL_IOCTL type)
{
  sockop so = {request, true};
  so.ssl_type = type;
  pending_sockops.push_back(so);
}

s32 WiiSockMan::AddSocket(s32 fd, bool is_rw)
{
  const char* caller = is_rw ? "SO_ACCEPT" : "NewSocket";

  if (fd < 0)
    return GetNetErrorCode(fd, caller, is_rw);

  s32 wii_fd;
  for (wii_fd = 0; wii_fd < WII_SOCKET_FD_MAX; ++wii_fd)
  {
    // Find an available socket fd
    if (WiiSockets.count(wii_fd) == 0)
      break;
  }

  if (wii_fd == WII_SOCKET_FD_MAX)
  {
    // Close host socket
    closesocket(fd);
    wii_fd = -SO_EMFILE;
    ERROR_LOG(IOS_NET, "%s failed: Too many open sockets, ret=%d", caller, wii_fd);
  }
  else
  {
    WiiSocket& sock = WiiSockets[wii_fd];
    sock.SetFd(fd);
    sock.SetWiiFd(wii_fd);
  }

  SetLastNetError(wii_fd);
  return wii_fd;
}

s32 WiiSockMan::NewSocket(s32 af, s32 type, s32 protocol)
{
  if (af != 2 && af != 23)  // AF_INET && AF_INET6
    return -SO_EAFNOSUPPORT;
  if (protocol != 0)  // IPPROTO_IP
    return -SO_EPROTONOSUPPORT;
  if (type != 1 && type != 2)  // SOCK_STREAM && SOCK_DGRAM
    return -SO_EPROTOTYPE;
  s32 fd = static_cast<s32>(socket(af, type, protocol));
  return AddSocket(fd, false);
}

s32 WiiSockMan::GetHostSocket(s32 wii_fd) const
{
  if (WiiSockets.count(wii_fd) > 0)
    return WiiSockets.at(wii_fd).fd;
  return -EBADF;
}

s32 WiiSockMan::DeleteSocket(s32 s)
{
  s32 ReturnValue = -SO_EBADF;
  auto socket_entry = WiiSockets.find(s);
  if (socket_entry != WiiSockets.end())
  {
    ReturnValue = socket_entry->second.CloseFd();
    WiiSockets.erase(socket_entry);
  }
  return ReturnValue;
}

void WiiSockMan::Update()
{
  s32 nfds = 0;
  fd_set read_fds, write_fds, except_fds;
  struct timeval t = {0, 0};
  FD_ZERO(&read_fds);
  FD_ZERO(&write_fds);
  FD_ZERO(&except_fds);

  auto socket_iter = WiiSockets.begin();
  auto end_socks = WiiSockets.end();

  while (socket_iter != end_socks)
  {
    WiiSocket& sock = socket_iter->second;
    if (sock.IsValid())
    {
      FD_SET(sock.fd, &read_fds);
      FD_SET(sock.fd, &write_fds);
      FD_SET(sock.fd, &except_fds);
      nfds = std::max(nfds, sock.fd + 1);
      ++socket_iter;
    }
    else
    {
      // Good time to clean up invalid sockets.
      socket_iter = WiiSockets.erase(socket_iter);
    }
  }
  s32 ret = select(nfds, &read_fds, &write_fds, &except_fds, &t);

  if (ret >= 0)
  {
    for (auto& pair : WiiSockets)
    {
      WiiSocket& sock = pair.second;
      sock.Update(FD_ISSET(sock.fd, &read_fds) != 0, FD_ISSET(sock.fd, &write_fds) != 0,
                  FD_ISSET(sock.fd, &except_fds) != 0);
    }
  }
  else
  {
    for (auto& elem : WiiSockets)
    {
      elem.second.Update(false, false, false);
    }
  }
}

void WiiSockMan::Convert(WiiSockAddrIn const& from, sockaddr_in& to)
{
  to.sin_addr.s_addr = from.addr.addr;
  to.sin_family = from.family;
  to.sin_port = from.port;
}

void WiiSockMan::Convert(sockaddr_in const& from, WiiSockAddrIn& to, s32 addrlen)
{
  to.addr.addr = from.sin_addr.s_addr;
  to.family = from.sin_family & 0xFF;
  to.port = from.sin_port;
  if (addrlen < 0 || addrlen > static_cast<s32>(sizeof(WiiSockAddrIn)))
    to.len = sizeof(WiiSockAddrIn);
  else
    to.len = addrlen;
}

void WiiSockMan::UpdateWantDeterminism(bool want)
{
  // If we switched into movie recording, kill existing sockets.
  if (want)
    Clean();
}

#undef ERRORCODE
#undef EITHER
}  // namespace HLE
}  // namespace IOS
