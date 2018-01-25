// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>
#include "Common/CommonTypes.h"

class GameListCtrl;
class wxRect;
class wxWindow;

class NetPlayLaunchConfig
{
public:
  void SetDialogInfo(wxWindow* parent);

  const wxRect window_defaults{wxDefaultCoord, wxDefaultCoord, 768, 768 - 128};

  std::string player_name;
  const GameListCtrl* game_list_ctrl;
  wxWindow* parent_window;
  bool use_traversal;
  std::string traversal_host;
  u16 traversal_port;
  wxRect window_pos{window_defaults};
};

class NetPlayHostConfig : public NetPlayLaunchConfig
{
public:
  void FromConfig();

  std::string game_name;
  u16 listen_port = 0;
  bool forward_port = false;
};

class NetPlayJoinConfig : public NetPlayLaunchConfig
{
public:
  std::string connect_host;
  u16 connect_port;
  std::string connect_hash_code;
};

class NetPlayLauncher
{
public:
  static bool Host(const NetPlayHostConfig& config);
  static bool Join(const NetPlayJoinConfig& config);
};
