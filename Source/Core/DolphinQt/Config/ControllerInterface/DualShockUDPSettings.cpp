// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DualShockUDPSettings.h"

#include <fmt/format.h>

#include "Common/Config/Config.h"
#include "InputCommon/ControllerInterface/DualShockUDPClient/DualShockUDPClient.h"

namespace DualShockUDPSettings
{
  std::string SerializeServer(DualShockUDPServer server)
  {
    return fmt::format("{}{}{}{}{}{}",
                       server.description,
                       FIELD_SEPARATOR,
                       server.server_address,
                       FIELD_SEPARATOR,
                       server.server_port,
                       SERVER_SEPARATOR);
  }

  void MigrateIfNeeded()
  {
    const auto server_address_setting =
        Config::Get(ciface::DualShockUDPClient::Settings::SERVER_ADDRESS);
    const auto server_port_setting = Config::Get(ciface::DualShockUDPClient::Settings::SERVER_PORT);

    // Update our servers setting if the user is using old configuration
    if (!server_address_setting.empty() && server_port_setting != 0)
    {
      const auto& servers_setting = Config::Get(ciface::DualShockUDPClient::Settings::SERVERS);
      Config::SetBaseOrCurrent(ciface::DualShockUDPClient::Settings::SERVERS,
                               servers_setting + SerializeServer(DualShockUDPServer("DS4", server_address_setting, server_port_setting)));
      Config::SetBase(ciface::DualShockUDPClient::Settings::SERVER_ADDRESS, "");
      Config::SetBase(ciface::DualShockUDPClient::Settings::SERVER_PORT, 0);
    }
  }

  const std::vector<DualShockUDPServer> GetServers() {
    MigrateIfNeeded();

    const auto& servers_setting = Config::Get(ciface::DualShockUDPClient::Settings::SERVERS);
    const auto server_details = SplitString(servers_setting, SERVER_SEPARATOR);

    std::vector<DualShockUDPServer> result(server_details.size());
    for (const std::string& server_detail : server_details)
    {
      const auto server_info = SplitString(server_detail, FIELD_SEPARATOR);
      if (server_info.size() < 3)
        continue;

      result.push_back(DualShockUDPServer(server_info[0], server_info[1], std::stoi(server_info[2])));
    }

    return result;
  }

  void AddServer(DualShockUDPServer server)
  {
    MigrateIfNeeded();

    const auto& servers_setting = Config::Get(ciface::DualShockUDPClient::Settings::SERVERS);
    Config::SetBaseOrCurrent(ciface::DualShockUDPClient::Settings::SERVERS,
                             servers_setting + SerializeServer(server));
  }

  void ReplaceServer(size_t index, DualShockUDPServer server)
  {
    MigrateIfNeeded();

    const auto& servers_setting = Config::Get(ciface::DualShockUDPClient::Settings::SERVERS);
    const auto server_details = SplitString(servers_setting, SERVER_SEPARATOR);

    std::string new_servers_setting;
    for (size_t i = 0; i < server_details.size(); i++)
    {
      if (i == index)
      {
        new_servers_setting += SerializeServer(server);
      }
      else
      {
        new_servers_setting += server_details[i] + SERVER_SEPARATOR;
      }

    }

    Config::SetBaseOrCurrent(ciface::DualShockUDPClient::Settings::SERVERS, new_servers_setting);
  }

  void RemoveServer(size_t index)
  {
    MigrateIfNeeded();

    const auto& servers_setting = Config::Get(ciface::DualShockUDPClient::Settings::SERVERS);
    const auto server_details = SplitString(servers_setting, SERVER_SEPARATOR);

    std::string new_servers_setting;
    for (size_t i = 0; i < server_details.size(); i++)
    {
      if (i == index)
      {
        continue;
      }

      new_servers_setting += server_details[i] + SERVER_SEPARATOR;
    }

    Config::SetBaseOrCurrent(ciface::DualShockUDPClient::Settings::SERVERS, new_servers_setting);
  }

  bool IsEnabled()
  {
    return Config::Get(ciface::DualShockUDPClient::Settings::SERVERS_ENABLED);
  }

  void SetEnabled(bool enabled)
  {
    Config::SetBaseOrCurrent(ciface::DualShockUDPClient::Settings::SERVERS_ENABLED, enabled);
  }
}
