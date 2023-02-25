// Copyright 2019 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <deque>
#include <functional>
#include <memory>
#include <string>
#include <utility>

class NetPlayChatUI
{
public:
  explicit NetPlayChatUI(std::function<void(const std::string&)> callback);
  ~NetPlayChatUI();

  using Color = std::array<float, 3>;

  void Display();
  void AppendChat(std::string message, Color color);
  void SendMessage();
  void Activate();

private:
  char m_message_buf[256] = {};
  bool m_scroll_to_bottom = false;
  bool m_activate = false;
  bool m_is_scrolled_to_bottom = true;

  std::deque<std::pair<std::string, Color>> m_messages;
  std::function<void(const std::string&)> m_message_callback;
};

extern std::unique_ptr<NetPlayChatUI> g_netplay_chat_ui;
