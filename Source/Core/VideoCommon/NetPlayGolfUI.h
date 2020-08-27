// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>

namespace NetPlay
{
class NetPlayClient;
}

class NetPlayGolfUI
{
public:
  explicit NetPlayGolfUI(std::shared_ptr<NetPlay::NetPlayClient> netplay_client);
  ~NetPlayGolfUI();

  void Display();

private:
  std::weak_ptr<NetPlay::NetPlayClient> m_netplay_client;
};

extern std::unique_ptr<NetPlayGolfUI> g_netplay_golf_ui;
