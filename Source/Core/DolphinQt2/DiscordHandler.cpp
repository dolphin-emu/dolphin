// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt2/DiscordHandler.h"

#include "Common/Thread.h"

#include "UICommon/DiscordPresence.h"

DiscordHandler::DiscordHandler() = default;

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

void DiscordHandler::DiscordJoin()
{
  emit DiscordHandler::Join();
}

void DiscordHandler::Run()
{
  while (!m_stop_requested.IsSet())
  {
    Common::SleepCurrentThread(1000 * 2);

    Discord::CallPendingCallbacks();
  }
}
