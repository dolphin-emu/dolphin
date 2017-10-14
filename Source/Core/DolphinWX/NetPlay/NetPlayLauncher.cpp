// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <wx/config.h>
#include <wx/gdicmn.h>

#include "Common/CommonTypes.h"
#include "Common/StringUtil.h"
#include "Core/Config/NetplaySettings.h"
#include "DolphinWX/NetPlay/NetPlayLauncher.h"
#include "DolphinWX/NetPlay/NetWindow.h"
#include "DolphinWX/WxUtils.h"

bool NetPlayLauncher::Host(const NetPlayHostConfig& config)
{
  NetPlayDialog*& npd = NetPlayDialog::GetInstance();
  NetPlayServer*& netplay_server = NetPlayDialog::GetNetPlayServer();

  if (npd)
  {
    WxUtils::ShowErrorDialog(_("A NetPlay window is already open!"));
    return false;
  }

  netplay_server = new NetPlayServer(
      config.listen_port, config.forward_port,
      NetTraversalConfig{config.use_traversal, config.traversal_host, config.traversal_port});

  if (!netplay_server->is_connected)
  {
    WxUtils::ShowErrorDialog(
        _("Failed to listen. Is another instance of the NetPlay server running?"));
    return false;
  }

  netplay_server->ChangeGame(config.game_name);

  npd = new NetPlayDialog(config.parent_window, config.game_list_ctrl, config.game_name, true);

  NetPlayClient*& netplay_client = NetPlayDialog::GetNetPlayClient();
  netplay_client = new NetPlayClient("127.0.0.1", netplay_server->GetPort(), npd,
                                     config.player_name, NetTraversalConfig{});

  if (netplay_client->IsConnected())
  {
    npd->SetSize(config.window_pos);
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

  netplay_client = new NetPlayClient(
      host, config.connect_port, npd, config.player_name,
      NetTraversalConfig{config.use_traversal, config.traversal_host, config.traversal_port});
  if (netplay_client->IsConnected())
  {
    npd->SetSize(config.window_pos);
    npd->Show();
    return true;
  }
  else
  {
    npd->Destroy();
    return false;
  }
}

void NetPlayLaunchConfig::SetDialogInfo(wxWindow* parent)
{
  parent_window = parent;

  wxConfig::Get()->Read("NetWindowPosX", &window_pos.x, window_defaults.GetX());
  wxConfig::Get()->Read("NetWindowPosY", &window_pos.y, window_defaults.GetY());
  wxConfig::Get()->Read("NetWindowWidth", &window_pos.width, window_defaults.GetWidth());
  wxConfig::Get()->Read("NetWindowHeight", &window_pos.height, window_defaults.GetHeight());

  if (window_pos.GetX() == window_defaults.GetX() || window_pos.GetY() == window_defaults.GetY())
  {
    // Center over toplevel dolphin window
    window_pos = window_defaults.CenterIn(parent_window->GetScreenRect());
  }
}

void NetPlayHostConfig::FromConfig()
{
  player_name = Config::Get(Config::NETPLAY_NICKNAME);

  const std::string traversal_choice_setting = Config::Get(Config::NETPLAY_TRAVERSAL_CHOICE);
  use_traversal = traversal_choice_setting == "traversal";

  if (!use_traversal)
  {
    listen_port = Config::Get(Config::NETPLAY_HOST_PORT);
  }
  else
  {
    traversal_port = Config::Get(Config::NETPLAY_TRAVERSAL_PORT);
    traversal_host = Config::Get(Config::NETPLAY_TRAVERSAL_SERVER);
  }
}
