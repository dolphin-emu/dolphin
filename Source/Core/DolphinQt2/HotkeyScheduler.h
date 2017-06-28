// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <wobjectdefs.h>
#include <thread>

#include <QObject>

#include "Common/Flag.h"

class HotkeyScheduler : public QObject
{
  W_OBJECT(HotkeyScheduler)
public:
  explicit HotkeyScheduler();
  ~HotkeyScheduler();

  void Start();
  void Stop();

  void ExitHotkey() W_SIGNAL(ExitHotkey);
  void FullScreenHotkey() W_SIGNAL(FullScreenHotkey);
  void StopHotkey() W_SIGNAL(StopHotkey);
  void PauseHotkey() W_SIGNAL(PauseHotkey);
  void ScreenShotHotkey() W_SIGNAL(ScreenShotHotkey);
  void SetStateSlotHotkey(int slot) W_SIGNAL(SetStateSlotHotkey, (int), slot);
  void StateLoadSlotHotkey() W_SIGNAL(StateLoadSlotHotkey);
  void StateSaveSlotHotkey() W_SIGNAL(StateSaveSlotHotkey);

private:
  void Run();

  Common::Flag m_stop_requested;
  std::thread m_thread;
};
