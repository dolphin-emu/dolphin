// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <enet/enet.h>
#include <utility>

namespace Common
{
class QoSSession
{
public:
  // 1 0 1 1 1 0 0 0
  // DSCP      ECN
  static constexpr int ef_tos = 0b10111000;

  QoSSession() = default;
  QoSSession(ENetPeer* peer, int tos_val = ef_tos);

  ~QoSSession();

  QoSSession& operator=(const QoSSession&) = delete;
  QoSSession(const QoSSession&) = delete;

  QoSSession& operator=(QoSSession&& session);
  QoSSession(QoSSession&& session) { *this = std::move(session); }
  bool Successful() const { return m_success; }

private:
#if defined(_WIN32)
  void* m_qos_handle = nullptr;
  unsigned long m_qos_flow_id = 0;

  ENetPeer* m_peer = nullptr;
#endif

  bool m_success = false;
};
}  // namespace Common
