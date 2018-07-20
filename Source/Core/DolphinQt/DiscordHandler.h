// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

// Note using a ifdef around this class causes link issues with qt

#include <list>
#include <thread>

#include <QObject>

#include "Common/Flag.h"

#include "DolphinQt/DiscordJoinRequestDialog.h"

#include "UICommon/DiscordPresence.h"

class DiscordHandler : public QObject, public Discord::Handler
{
  Q_OBJECT
public:
  explicit DiscordHandler(QWidget* parent);
  ~DiscordHandler();

  void Start();
  void Stop();
  void DiscordJoin() override;
  void DiscordJoinRequest(const char* id, const std::string& discord_tag,
                          const char* avatar) override;
  void ShowNewJoinRequest();
signals:
  void Join();
  void JoinRequest();

private:
  void Run();
  QWidget* m_parent;
  Common::Flag m_stop_requested;
  std::thread m_thread;
  std::list<DiscordJoinRequestDialog> m_request_dialogs;
};
