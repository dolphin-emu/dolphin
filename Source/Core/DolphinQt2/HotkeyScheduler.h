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
  void TogglePauseHotkey();
  void ScreenShotHotkey();
  void SetStateSlotHotkey(int slot);
  void StateLoadSlotHotkey();
  void StateSaveSlotHotkey();
  void StartRecording();
  void ExportRecording();
  void ToggleReadOnlyMode();
  void ConnectWiiRemote(int id);

  void Step();
  void StepOver();
  void StepOut();
  void Skip();

  void ShowPC();
  void SetPC();

  void ToggleBreakpoint();
  void AddBreakpoint();

private:
  void Run();

  Common::Flag m_stop_requested;
  std::thread m_thread;
};
