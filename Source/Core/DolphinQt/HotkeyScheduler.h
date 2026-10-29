// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <thread>

#include <QObject>

#include "Common/CommonTypes.h"
#include "Common/Flag.h"
#include "InputCommon/InputProfile.h"

class HotkeyScheduler : public QObject
{
  Q_OBJECT
public:
  explicit HotkeyScheduler();
  ~HotkeyScheduler() override;

  void Start();
  void Stop();
signals:
  void Open();
  void EjectDisc();
  void ChangeDisc();

  void ExitHotkey();
  void UnlockCursor();
  void ActivateChat();
  void RequestGolfControl();
  void FullScreenHotkey();
  void StopHotkey();
  void ResetHotkey();
  void TogglePauseHotkey();
  void ScreenShotHotkey();
  void RefreshGameListHotkey();
  void SetStateSlotHotkey(int slot);
  void IncrementSelectedStateSlotHotkey();
  void DecrementSelectedStateSlotHotkey();
  void StateLoadSlotHotkey();
  void StateSaveSlotHotkey();
  void StateLoadSlot(int state);
  void StateSaveSlot(int state);
  void StateLoadLastSaved(int state);
  void StateSaveOldest();
  void StateLoadFile();
  void StateSaveFile();
  void StateLoadUndo();
  void StateSaveUndo();
  void StartRecording();
  void PlayRecording();
  void ExportRecording();
  void ToggleReadOnlyMode();
  void ConnectWiiRemote(int id);
#ifdef USE_RETRO_ACHIEVEMENTS
  void OpenAchievements();
#endif  // USE_RETRO_ACHIEVEMENTS

  void Step();
  void StepOver();
  void StepOut();
  void Skip();

  void ShowPC();
  void SetPC();

  void ToggleBreakpoint();
  void AddBreakpoint();

  void SkylandersPortalHotkey();
  void InfinityBaseHotkey();

  // A Qt::Key from a configured controller while no game is running: an arrow key or Key_Return.
  void GameListKey(int key);

private:
  void Run();
  void CheckDebuggingHotkeys();
  void CheckGBAHotkeys();
  void CheckGameListController();

  Common::Flag m_stop_requested;
  std::thread m_thread;

  // The last game-list controller poll, so a press is sent once. Only the hotkey thread uses these.
  bool m_game_list_synced = false;
  int m_game_list_direction = 0;
  bool m_game_list_a = false;

  InputProfile::ProfileCycler m_profile_cycler;
};
