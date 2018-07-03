// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <thread>

#include <QObject>

#include "Common/Flag.h"

class DiscordHandler : public QObject
{
  Q_OBJECT
public:
  explicit DiscordHandler();
  ~DiscordHandler();

  void Start();
  void Stop();
  void DiscordJoin();
signals:
  void Join();

private:
  void Run();
  Common::Flag m_stop_requested;
  std::thread m_thread;
};
