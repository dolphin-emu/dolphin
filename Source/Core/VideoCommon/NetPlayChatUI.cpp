// Copyright 2019 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Common/MsgHandler.h"

#include "VideoCommon/NetPlayChatUI.h"

#include <imgui.h>

constexpr float DEFAULT_WINDOW_WIDTH = 220.0f;
constexpr float DEFAULT_WINDOW_HEIGHT = 400.0f;

constexpr size_t MAX_BACKLOG_SIZE = 100;

std::unique_ptr<NetPlayChatUI> g_netplay_chat_ui;

NetPlayChatUI::NetPlayChatUI(std::function<void(const std::string&)> callback)
    : m_message_callback{std::move(callback)}
{
}

NetPlayChatUI::~NetPlayChatUI() = default;

void NetPlayChatUI::Display()
{
  const float scale = ImGui::GetIO().DisplayFramebufferScale.x;

  ImGui::SetNextWindowPos(ImVec2(10.0f * scale, 10.0f * scale), ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSizeConstraints(
      ImVec2(DEFAULT_WINDOW_WIDTH * scale, DEFAULT_WINDOW_HEIGHT * scale),
      ImGui::GetIO().DisplaySize);

  if (!ImGui::Begin(GetStringT("Chat").c_str(), nullptr, ImGuiWindowFlags_None))
  {
    ImGui::End();
    return;
  }

  ImGui::BeginChild("Scrolling", ImVec2(0, -30 * scale), true, ImGuiWindowFlags_None);
  for (const auto& msg : m_messages)
  {
    auto c = msg.second;
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextColored(ImVec4(c[0], c[1], c[2], 1.0f), "%s", msg.first.c_str());
    ImGui::PopTextWrapPos();
  }

  if (m_scroll_to_bottom)
  {
    ImGui::SetScrollHereY(1.0f);
    m_scroll_to_bottom = false;
  }

  m_is_scrolled_to_bottom = ImGui::GetScrollY() == ImGui::GetScrollMaxY();

  ImGui::EndChild();

  ImGui::Spacing();

  ImGui::PushItemWidth(-50.0f * scale);

  if (ImGui::InputText("##NetplayMessageBuffer", m_message_buf, IM_ARRAYSIZE(m_message_buf),
                       ImGuiInputTextFlags_EnterReturnsTrue))
  {
    SendMessage();
  }

  if (m_activate)
  {
    ImGui::SetKeyboardFocusHere(-1);
    m_activate = false;
  }

  ImGui::PopItemWidth();

  ImGui::SameLine();

  if (ImGui::Button(GetStringT("Send").c_str()))
    SendMessage();

  ImGui::End();
}

void NetPlayChatUI::AppendChat(std::string message, Color color)
{
  if (m_messages.size() > MAX_BACKLOG_SIZE)
    m_messages.pop_front();

  m_messages.emplace_back(std::move(message), color);

  // Only scroll to bottom, if we were at the bottom previously
  if (m_is_scrolled_to_bottom)
    m_scroll_to_bottom = true;
}

void NetPlayChatUI::SendMessage()
{
  // Check whether the input field is empty
  if (m_message_buf[0] != '\0')
  {
    if (m_message_callback)
      m_message_callback(m_message_buf);

    // 'Empty' the buffer
    m_message_buf[0] = '\0';
  }
}

void NetPlayChatUI::Activate()
{
  if (ImGui::IsItemFocused())
    ImGui::SetWindowFocus(nullptr);
  else
    m_activate = true;
}
