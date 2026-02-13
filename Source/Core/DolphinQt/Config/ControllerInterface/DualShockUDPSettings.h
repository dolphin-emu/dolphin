// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>
#include <vector>

struct DualShockUDPServer
{
  std::string description;
  std::string server_address;
  int server_port;
};

namespace DualShockUDPSettings
{
constexpr char FIELD_SEPARATOR = ':';
constexpr char SERVER_SEPARATOR = ';';

const std::vector<DualShockUDPServer> GetServers();

void AddServer(DualShockUDPServer server);

void ReplaceServer(size_t index, DualShockUDPServer server);

void RemoveServer(size_t index);

bool IsEnabled();

void SetEnabled(bool enabled);
}  // namespace DualShockUDPSettings
