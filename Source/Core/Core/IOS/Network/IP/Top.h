// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <queue>
#include <string>

#include "Common/CommonTypes.h"
#include "Common/SocketContext.h"
#include "Common/WorkQueueThread.h"
#include "Core/IOS/Device.h"

#ifdef _WIN32
#include <ws2tcpip.h>
#endif

// WSAPoll doesn't support POLLPRI and POLLWRBAND flags
#ifdef _WIN32
constexpr int UNSUPPORTED_WSAPOLL = POLLPRI | POLLWRBAND;
#else
constexpr int UNSUPPORTED_WSAPOLL = 0;
#endif

namespace IOS::HLE
{
enum NET_IOCTL
{
  IOCTL_SO_ACCEPT = 1,
  IOCTL_SO_BIND,
  IOCTL_SO_CLOSE,
  IOCTL_SO_CONNECT,
  IOCTL_SO_FCNTL,
  IOCTL_SO_GETPEERNAME,
  IOCTL_SO_GETSOCKNAME,
  IOCTL_SO_GETSOCKOPT,
  IOCTL_SO_SETSOCKOPT,
  IOCTL_SO_LISTEN,
  IOCTL_SO_POLL,
  IOCTLV_SO_RECVFROM,
  IOCTLV_SO_SENDTO,
  IOCTL_SO_SHUTDOWN,
  IOCTL_SO_SOCKET,
  IOCTL_SO_GETHOSTID,
  IOCTL_SO_GETHOSTBYNAME,
  IOCTL_SO_GETHOSTBYADDR,
  IOCTLV_SO_GETNAMEINFO,
  IOCTL_SO_GETLASTERROR,
  IOCTL_SO_INETATON,
  IOCTL_SO_INETPTON,
  IOCTL_SO_INETNTOP,
  IOCTLV_SO_GETADDRINFO,
  IOCTL_SO_SOCKATMARK,
  IOCTLV_SO_STARTUP,
  IOCTLV_SO_CLEANUP,
  IOCTLV_SO_GETINTERFACEOPT,
  IOCTLV_SO_SETINTERFACEOPT,
  IOCTL_SO_SETINTERFACE,
  IOCTL_SO_INITINTERFACE,
  IOCTL_SO_ICMPSOCKET = 0x30,
  IOCTLV_SO_ICMPPING,
  IOCTL_SO_ICMPCANCEL,
  IOCTL_SO_ICMPCLOSE
};

class NetIPTopDevice : public EmulationDevice
{
public:
  NetIPTopDevice(EmulationKernel& ios, const std::string& device_name);

  void DoState(PointerWrap& p) override;
  std::optional<IPCReply> IOCtl(const IOCtlRequest& request) override;
  std::optional<IPCReply> IOCtlV(const IOCtlVRequest& request) override;

  void Update() override;

private:
  struct AsyncTask
  {
    IOS::HLE::Request request;
    std::function<IPCReply()> handler;
  };

  struct AsyncReply
  {
    IOS::HLE::Request request;
    s32 return_value;
  };

  template <typename Method, typename Request>
  std::optional<IPCReply> LaunchAsyncTask(Method method, const Request& request)
  {
    m_work_queue.EmplaceItem(AsyncTask{request, std::bind(method, this, request)});
    return std::nullopt;
  }

  IPCReply HandleInitInterfaceRequest(const IOCtlRequest& request);
  IPCReply HandleSocketRequest(const IOCtlRequest& request);
  IPCReply HandleICMPSocketRequest(const IOCtlRequest& request);
  IPCReply HandleCloseRequest(const IOCtlRequest& request);
  std::optional<IPCReply> HandleDoSockRequest(const IOCtlRequest& request);
  IPCReply HandleShutdownRequest(const IOCtlRequest& request);
  IPCReply HandleListenRequest(const IOCtlRequest& request);
  IPCReply HandleGetSockOptRequest(const IOCtlRequest& request);
  IPCReply HandleSetSockOptRequest(const IOCtlRequest& request);
  IPCReply HandleGetSockNameRequest(const IOCtlRequest& request);
  IPCReply HandleGetPeerNameRequest(const IOCtlRequest& request);
  IPCReply HandleGetHostIDRequest(const IOCtlRequest& request);
  IPCReply HandleInetAToNRequest(const IOCtlRequest& request);
  IPCReply HandleInetPToNRequest(const IOCtlRequest& request);
  IPCReply HandleInetNToPRequest(const IOCtlRequest& request);
  std::optional<IPCReply> HandlePollRequest(const IOCtlRequest& request);
  IPCReply HandleGetHostByNameRequest(const IOCtlRequest& request);
  IPCReply HandleICMPCancelRequest(const IOCtlRequest& request);

  IPCReply HandleGetInterfaceOptRequest(const IOCtlVRequest& request);
  std::optional<IPCReply> HandleSendToRequest(const IOCtlVRequest& request);
  std::optional<IPCReply> HandleRecvFromRequest(const IOCtlVRequest& request);
  IPCReply HandleGetAddressInfoRequest(const IOCtlVRequest& request);
  IPCReply HandleICMPPingRequest(const IOCtlVRequest& request);

  Common::SocketContext m_socket_context;
  Common::WorkQueueThread<AsyncTask> m_work_queue;
  std::mutex m_async_reply_lock;
  std::queue<AsyncReply> m_async_replies;
};
}  // namespace IOS::HLE
