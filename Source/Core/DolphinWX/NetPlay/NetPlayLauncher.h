// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>
#include "Common/CommonTypes.h"
#include "Common/IniFile.h"

class GameListCtrl;
class wxRect;
class wxWindow;

class NetPlayLaunchConfig
{
public:
  static std::string GetTraversalHostFromIniConfig(const IniFile::Section& netplay_section);
  static u16 GetTraversalPortFromIniConfig(const IniFile::Section& netplay_section);
  void SetDialogInfo(const IniFile::Section& section, wxWindow* parent);

  static const std::string DEFAULT_TRAVERSAL_HOST;
  static constexpr u16 DEFAULT_TRAVERSAL_PORT = 6262;
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
  void FromIniConfig(IniFile::Section& netplay_section);

  static constexpr u16 DEFAULT_LISTEN_PORT = 2626;

  std::string game_name;
  u16 listen_port = 0;
#ifdef USE_UPNP
  bool forward_port;
#endif
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
