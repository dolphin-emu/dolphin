// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <thread>

#include <QObject>

#include "Common/Flag.h"

class HotkeyScheduler : public QObject
{
  Q_OBJECT
public:
  explicit HotkeyScheduler();
  ~HotkeyScheduler();

  void Start();
  void Stop();
signals:
  void ExitHotkey();
  void FullScreenHotkey();
  void StopHotkey();
  void PauseHotkey();
  void ScreenShotHotkey();
  void SetStateSlotHotkey(int slot);
  void StateLoadSlotHotkey();
  void StateSaveSlotHotkey();
  void StartRecording();
  void ExportRecording();
  void ToggleReadOnlyMode();

private:
  void Run();

  Common::Flag m_stop_requested;
  std::thread m_thread;
};
