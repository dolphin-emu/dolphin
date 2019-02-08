// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/BBADebug.h"

#include <QDateTime>

#include "DolphinQt/BBAServer.h"

BBADebug::BBADebug(BBAServer &server) :
  QDebug(&m_log_line),
  m_server(server)
{
}

BBADebug::~BBADebug()
{
  m_server.Information(QDateTime::currentDateTime(), m_log_line);
}
