// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "NetPlayLauncher.h"
#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/IniFile.h"
#include "Common/StringUtil.h"
#include "DolphinWX/WxUtils.h"
#include "NetWindow.h"

bool NetPlayLauncher::Host(const NetPlayHostConfig& config)
{
  NetPlayDialog*& npd = NetPlayDialog::GetInstance();
  NetPlayServer*& netplay_server = NetPlayDialog::GetNetPlayServer();

  if (npd)
  {
    WxUtils::ShowErrorDialog(_("A NetPlay window is already open!"));
    return false;
  }

  netplay_server = new NetPlayServer(config.listen_port, config.use_traversal,
                                     config.traversal_host, config.traversal_port);

  if (!netplay_server->is_connected)
  {
    WxUtils::ShowErrorDialog(
        _("Failed to listen. Is another instance of the NetPlay server running?"));
    return false;
  }

  netplay_server->ChangeGame(config.game_name);

#ifdef USE_UPNP
  if (config.forward_port)
  {
    netplay_server->TryPortmapping(config.listen_port);
  }
#endif

  npd = new NetPlayDialog(config.parent_window, config.game_list_ctrl, config.game_name, true);

  NetPlayClient*& netplay_client = NetPlayDialog::GetNetPlayClient();
  netplay_client =
      new NetPlayClient("127.0.0.1", netplay_server->GetPort(), npd, config.player_name, false,
                        config.traversal_host, config.traversal_port);

  if (netplay_client->IsConnected())
  {
    npd->Show();
    netplay_server->SetNetPlayUI(NetPlayDialog::GetInstance());
    return true;
  }
  else
  {
    npd->Destroy();
    return false;
  }
}

bool NetPlayLauncher::Join(const NetPlayJoinConfig& config)
{
  NetPlayDialog*& npd = NetPlayDialog::GetInstance();
  NetPlayClient*& netplay_client = NetPlayDialog::GetNetPlayClient();

  npd = new NetPlayDialog(config.parent_window, config.game_list_ctrl, "", false);

  std::string host;
  if (config.use_traversal)
    host = config.connect_hash_code;
  else
    host = config.connect_host;

  netplay_client =
      new NetPlayClient(host, config.connect_port, npd, config.player_name, config.use_traversal,
                        config.traversal_host, config.traversal_port);
  if (netplay_client->IsConnected())
  {
    npd->Show();
    return true;
  }
  else
  {
    npd->Destroy();
    return false;
  }
}

const std::string NetPlayLaunchConfig::DEFAULT_TRAVERSAL_HOST = "stun.dolphin-emu.org";
const u16 NetPlayLaunchConfig::DEFAULT_TRAVERSAL_PORT = 6262;
const u16 NetPlayHostConfig::DEFAULT_LISTEN_PORT = 2626;

std::string NetPlayLaunchConfig::GetTraversalHostFromIniConfig(IniFile::Section& netplay_section)
{
  std::string host;

  netplay_section.Get("TraversalServer", &host, DEFAULT_TRAVERSAL_HOST);
  host = StripSpaces(host);

  if (host.empty())
    return DEFAULT_TRAVERSAL_HOST;

  return host;
}

u16 NetPlayLaunchConfig::GetTraversalPortFromIniConfig(IniFile::Section& netplay_section)
{
  std::string port_str;
  unsigned long port;

  netplay_section.Get("TraversalPort", &port_str, std::to_string(DEFAULT_TRAVERSAL_PORT));
  StrToWxStr(port_str).ToULong(&port);

  if (port == 0)
    port = DEFAULT_TRAVERSAL_PORT;

  return static_cast<u16>(port);
}

void NetPlayHostConfig::FromIniConfig(IniFile::Section& netplay_section)
{
  std::string traversal_choice_setting;
  netplay_section.Get("TraversalChoice", &traversal_choice_setting, "direct");
  use_traversal = traversal_choice_setting == "traversal";

  if (!use_traversal)
  {
    unsigned long lport = 0;
    std::string port_setting;
    netplay_section.Get("HostPort", &port_setting, std::to_string(DEFAULT_LISTEN_PORT));
    StrToWxStr(port_setting).ToULong(&lport);

    listen_port = static_cast<u16>(lport);
  }
  else
  {
    traversal_port = GetTraversalPortFromIniConfig(netplay_section);
    traversal_host = GetTraversalHostFromIniConfig(netplay_section);
  }
}