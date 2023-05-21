// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "SteamHelperCommon/IpcConnection.h"

namespace Steam
{
class HelperServer : public IpcConnection
{
public:
  HelperServer(PipeHandle in_handle, PipeHandle out_handle) : IpcConnection(in_handle, out_handle)
  {
  }

  bool GetSteamInited() const { return m_steam_inited; }

private:
  virtual void Receive(sf::Packet& packet) override;

  void ReceiveInitRequest(uint32_t call_id);
  void ReceiveShutdownRequest();

  bool m_steam_inited = false;
};
}  // namespace Steam
