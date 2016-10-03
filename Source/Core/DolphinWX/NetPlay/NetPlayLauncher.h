// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <string>
#include "Common/CommonTypes.h"
#include "Common/IniFile.h"

class CGameListCtrl;
class wxWindow;

class NetPlayLaunchConfig
{
public:
  static std::string GetTraversalHostFromIniConfig(IniFile::Section& netplay_section);
  static u16 GetTraversalPortFromIniConfig(IniFile::Section& netplay_section);

  static const std::string DEFAULT_TRAVERSAL_HOST;
  static const u16 DEFAULT_TRAVERSAL_PORT;

  std::string player_name;
  const CGameListCtrl* game_list_ctrl;
  wxWindow* parent_window;
  bool use_traversal;
  std::string traversal_host;
  u16 traversal_port;
};

class NetPlayHostConfig : public NetPlayLaunchConfig
{
public:
  void FromIniConfig(IniFile::Section& netplay_section);

  static const u16 DEFAULT_LISTEN_PORT;

  std::string game_name;
  u16 listen_port = 0;
  bool forward_port;
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