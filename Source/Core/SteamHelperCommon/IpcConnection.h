// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <atomic>
#include <mutex>
#include <thread>

#include <SFML/Network/Packet.hpp>

#include "SteamHelperCommon/PipeEnd.h"

namespace Steam
{
class IpcConnection
{
public:
  IpcConnection(PipeHandle in_handle, PipeHandle out_handle);
  virtual ~IpcConnection();

  bool IsRunning() const;
  void RequestStop();

protected:
  void Send(sf::Packet& packet);
  virtual void Receive(sf::Packet& packet) = 0;

  virtual void HandleRequestedStop() {}

private:
  void ReceiveThreadFunc();

  PipeEnd m_in_end;
  PipeEnd m_out_end;
  std::mutex m_out_mutex;

  std::atomic_bool m_is_running;

  std::thread m_receive_thread;
};
}  // namespace Steam
