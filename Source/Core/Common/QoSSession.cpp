// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Common/QoSSession.h"

#if defined(_WIN32)
#include <Qos2.h>
#pragma comment(lib, "qwave")
#endif

#include "Core/ConfigManager.h"

namespace Common
{
#if defined(_WIN32)
QoSSession::QoSSession(ENetPeer* peer, int tos_val) : m_peer(peer)
{
  QOS_VERSION ver = {1, 0};

  if (!QOSCreateHandle(&ver, &m_qos_handle))
    return;

  sockaddr_in sin = {};

  sin.sin_family = AF_INET;
  sin.sin_port = ENET_HOST_TO_NET_16(peer->host->address.port);
  sin.sin_addr.s_addr = peer->host->address.host;

  if (QOSAddSocketToFlow(m_qos_handle, peer->host->socket, reinterpret_cast<PSOCKADDR>(&sin),
                         QOSTrafficTypeControl, QOS_NON_ADAPTIVE_FLOW, &m_qos_flow_id))
  {
    // We shift the complete ToS value by 3 to get rid of the 3 bit ECN field
    DWORD dscp = static_cast<DWORD>(tos_val >> 3);

    // Sets DSCP to the same as Linux
    // This will fail if we're not admin, but we ignore it
    QOSSetFlow(m_qos_handle, m_qos_flow_id, QOSSetOutgoingDSCPValue, sizeof(DWORD), &dscp, 0,
               nullptr);

    m_success = true;
  }
}

QoSSession::~QoSSession()
{
  if (m_qos_handle == nullptr)
    return;

  if (m_qos_flow_id != 0)
    QOSRemoveSocketFromFlow(m_qos_handle, m_peer->host->socket, m_qos_flow_id, 0);

  QOSCloseHandle(m_qos_handle);
}
#else
QoSSession::QoSSession(ENetPeer* peer, int tos_val)
{
#if defined(__linux__)
  constexpr int priority = 7;
  setsockopt(peer->host->socket, SOL_SOCKET, SO_PRIORITY, &priority, sizeof(priority));
#endif

  m_success = setsockopt(peer->host->socket, IPPROTO_IP, IP_TOS, &tos_val, sizeof(tos_val)) == 0;
}

QoSSession::~QoSSession() = default;
#endif

QoSSession& QoSSession::operator=(QoSSession&& session)
{
  if (this != &session)
  {
#if defined(_WIN32)
    m_qos_handle = session.m_qos_handle;
    m_qos_flow_id = session.m_qos_flow_id;
    m_peer = session.m_peer;

    session.m_qos_handle = nullptr;
    session.m_qos_flow_id = 0;
    session.m_peer = nullptr;
#endif
    m_success = session.m_success;
    session.m_success = false;
  }

  return *this;
}
}  // namespace Common
