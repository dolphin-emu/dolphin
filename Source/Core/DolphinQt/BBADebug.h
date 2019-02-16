// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QDebug>

class BBAServer;

class BBADebug : public QDebug
{
public:
  explicit BBADebug(BBAServer& server);
  ~BBADebug();

private:
  BBAServer& m_server;
  QString m_log_line;
};
