// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <list>
#include <mutex>
#include <thread>

#include <QObject>

#include "Common/Event.h"
#include "Common/Flag.h"

#include "UICommon/DiscordPresence.h"

class DiscordJoinRequestDialog;

class DiscordHandler : public QObject, public Discord::Handler
{
  Q_OBJECT
#ifdef USE_DISCORD_PRESENCE
public:
  explicit DiscordHandler(QWidget* parent);
  ~DiscordHandler();

  void Start();
  void Stop();
  void DiscordJoin() override;
  void DiscordJoinRequest(const char* id, const std::string& discord_tag,
                          const char* avatar) override;
  void ShowNewJoinRequest(const std::string& id, const std::string& discord_tag,
                          const std::string& avatar);
#endif

signals:
  void Join();
  void JoinRequest(const std::string id, const std::string discord_tag, const std::string avatar);

#ifdef USE_DISCORD_PRESENCE
private:
  void Run();
  QWidget* m_parent;
  Common::Flag m_stop_requested;
  Common::Event m_wakeup_event;
  std::thread m_thread;
  std::list<DiscordJoinRequestDialog> m_request_dialogs;
  std::mutex m_request_dialogs_mutex;
#endif
};
