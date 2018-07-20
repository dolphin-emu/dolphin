// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#ifdef USE_DISCORD_PRESENCE

#include <iterator>

#include <QApplication>

#include "DolphinQt/DiscordHandler.h"

#include "Common/Thread.h"

#include "UICommon/DiscordPresence.h"

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
  m_stop_requested.Set(true);

  if (m_thread.joinable())
    m_thread.join();
}

void DiscordHandler::DiscordJoinRequest(const char* id, const std::string& discord_tag,
                                        const char* avatar)
{
  m_request_dialogs.emplace_front(m_parent, id, discord_tag, avatar);
  emit DiscordHandler::JoinRequest();
}

void DiscordHandler::DiscordJoin()
{
  emit DiscordHandler::Join();
}

void DiscordHandler::ShowNewJoinRequest()
{
  m_request_dialogs.front().show();
  QApplication::alert(nullptr, DiscordJoinRequestDialog::s_max_lifetime_seconds * 1000);
}

void DiscordHandler::Run()
{
  while (!m_stop_requested.IsSet())
  {
    if (m_thread.joinable())
      Discord::CallPendingCallbacks();

    // close and remove dead requests
    for (auto request_dialog = m_request_dialogs.rbegin();
         request_dialog != m_request_dialogs.rend(); ++request_dialog)
    {
      if (std::time(nullptr) < request_dialog->GetCloseTimestamp())
        continue;
      request_dialog->close();
      std::advance(request_dialog, 1);
      m_request_dialogs.erase(request_dialog.base());
    }

    Common::SleepCurrentThread(1000 * 2);
  }
}

#endif
