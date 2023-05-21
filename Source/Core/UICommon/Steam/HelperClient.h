// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <future>
#include <unordered_map>

#include "SteamHelperCommon/IpcConnection.h"
#include "SteamHelperCommon/MessageType.h"

namespace Steam
{
struct IpcResult
{
  // Whether we received a reply to the IPC request or not.
  bool ipc_success;
  sf::Packet payload;
};

class HelperClient : public IpcConnection
{
public:
  HelperClient(PipeHandle in_handle, PipeHandle out_handle) : IpcConnection(in_handle, out_handle)
  {
  }

  std::future<IpcResult> SendMessageWithReply(MessageType type, const sf::Packet& payload = {});
  void SendMessageNoReply(MessageType type, const sf::Packet& payload = {});

private:
  virtual void Receive(sf::Packet& packet) override;

  virtual void HandleRequestedStop() override;

  std::unordered_map<uint32_t, std::shared_ptr<std::promise<IpcResult>>> m_promises;
  std::mutex m_promises_mutex;
  uint32_t m_last_call_id = 0;
};
}  // namespace Steam
