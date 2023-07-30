// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#ifdef USE_DISCORD_PRESENCE

#include "DolphinQt/DiscordHandler.h"

#include <chrono>
#include <iterator>

#include <QApplication>

#include "Common/Thread.h"

#include "UICommon/DiscordPresence.h"

#include "DolphinQt/DiscordJoinRequestDialog.h"
#include "DolphinQt/QtUtils/RunOnObject.h"
#include "DolphinQt/QtUtils/SetWindowDecorations.h"

DiscordHandler::DiscordHandler(QWidget* parent) : QObject{parent}, m_parent{parent}
{
  connect(this, &DiscordHandler::JoinRequest, this, &DiscordHandler::ShowNewJoinRequest);
}

DiscordHandler::~DiscordHandler()
{
  Stop();
}

void DiscordHandler::Start()
{
  m_stop_requested.Set(false);
  m_thread = std::thread(&DiscordHandler::Run, this);
}

void DiscordHandler::Stop()
{
  if (!m_thread.joinable())
    return;

  m_stop_requested.Set(true);
  m_wakeup_event.Set();
  m_thread.join();
}

void DiscordHandler::DiscordJoinRequest(const char* id, const std::string& discord_tag,
                                        const char* avatar)
{
  emit DiscordHandler::JoinRequest(id, discord_tag, avatar);
}

void DiscordHandler::DiscordJoin()
{
  emit DiscordHandler::Join();
}

void DiscordHandler::ShowNewJoinRequest(const std::string& id, const std::string& discord_tag,
                                        const std::string& avatar)
{
  std::lock_guard<std::mutex> lock(m_request_dialogs_mutex);
  m_request_dialogs.emplace_front(m_parent, id, discord_tag, avatar);
  DiscordJoinRequestDialog& request_dialog = m_request_dialogs.front();
  SetQWidgetWindowDecorations(&request_dialog);
  request_dialog.show();
  request_dialog.raise();
  request_dialog.activateWindow();
  QApplication::alert(nullptr, DiscordJoinRequestDialog::s_max_lifetime_seconds * 1000);
}

void DiscordHandler::Run()
{
  Common::SetCurrentThreadName("DiscordHandler");

  while (!m_stop_requested.IsSet())
  {
    Discord::CallPendingCallbacks();

    // close and remove dead requests
    {
      std::lock_guard<std::mutex> lock(m_request_dialogs_mutex);
      for (auto request_dialog = m_request_dialogs.begin();
           request_dialog != m_request_dialogs.end();)
      {
        if (std::time(nullptr) < request_dialog->GetCloseTimestamp())
        {
          ++request_dialog;
          continue;
        }

        RunOnObject(m_parent, [this, &request_dialog] {
          request_dialog->close();
          request_dialog = m_request_dialogs.erase(request_dialog);
          return nullptr;
        });
      }
    }

    m_wakeup_event.WaitFor(std::chrono::seconds{2});
  }
}

#endif
