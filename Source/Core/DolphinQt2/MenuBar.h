// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>
#include <wobjectdefs.h>

#include <QMenu>
#include <QMenuBar>

W_REGISTER_ARGTYPE(std::string)

class MenuBar final : public QMenuBar
{
  W_OBJECT(MenuBar)

public:
  explicit MenuBar(QWidget* parent = nullptr);

  // File
  void Open() W_SIGNAL(Open);
  void Exit() W_SIGNAL(Exit);

  // Emulation
  void Play() W_SIGNAL(Play);
  void Pause() W_SIGNAL(Pause);
  void Stop() W_SIGNAL(Stop);
  void Reset() W_SIGNAL(Reset);
  void Fullscreen() W_SIGNAL(Fullscreen);
  void FrameAdvance() W_SIGNAL(FrameAdvance);
  void Screenshot() W_SIGNAL(Screenshot);
  void StateLoad() W_SIGNAL(StateLoad);
  void StateSave() W_SIGNAL(StateSave);
  void StateLoadSlot() W_SIGNAL(StateLoadSlot);
  void StateSaveSlot() W_SIGNAL(StateSaveSlot);
  void StateLoadSlotAt(int slot) W_SIGNAL(StateLoadSlotAt, (int), slot);
  void StateSaveSlotAt(int slot) W_SIGNAL(StateSaveSlotAt, (int), slot);
  void StateLoadUndo() W_SIGNAL(StateLoadUndo);
  void StateSaveUndo() W_SIGNAL(StateSaveUndo);
  void StateSaveOldest() W_SIGNAL(StateSaveOldest);
  void SetStateSlot(int slot) W_SIGNAL(SetStateSlot, (int), slot);

  void PerformOnlineUpdate(const std::string& region)
      W_SIGNAL(PerformOnlineUpdate, (const std::string&), region);

  // Options
  void ConfigureHotkeys() W_SIGNAL(ConfigureHotkeys);

  // View
  void ShowTable() W_SIGNAL(ShowTable);
  void ShowList() W_SIGNAL(ShowList);
  void ColumnVisibilityToggled(const QString& row, bool visible)
      W_SIGNAL(ColumnVisibilityToggled, (const QString&, bool), row, visible);

  void ShowAboutDialog() W_SIGNAL(ShowAboutDialog);

public:
  void EmulationStarted();
  W_SLOT(EmulationStarted);
  void EmulationPaused();
  W_SLOT(EmulationPaused);
  void EmulationStopped();
  W_SLOT(EmulationStopped);
  void UpdateStateSlotMenu();
  W_SLOT(UpdateStateSlotMenu);
  void UpdateToolsMenu(bool emulation_started);
  W_SLOT(UpdateToolsMenu, (bool));

  // Tools
  void InstallWAD();
  W_SLOT(InstallWAD);

private:
  void AddFileMenu();

  void AddEmulationMenu();
  void AddStateLoadMenu(QMenu* emu_menu);
  void AddStateSaveMenu(QMenu* emu_menu);
  void AddStateSlotMenu(QMenu* emu_menu);

  void AddViewMenu();
  void AddGameListTypeSection(QMenu* view_menu);
  void AddTableColumnsMenu(QMenu* view_menu);

  void AddOptionsMenu();
  void AddToolsMenu();
  void AddHelpMenu();

  // File
  QAction* m_open_action;
  QAction* m_exit_action;

  // Tools
  QAction* m_wad_install_action;
  QMenu* m_perform_online_update_menu;
  QAction* m_perform_online_update_for_current_region;

  // Emulation
  QAction* m_play_action;
  QAction* m_pause_action;
  QAction* m_stop_action;
  QAction* m_reset_action;
  QAction* m_fullscreen_action;
  QAction* m_frame_advance_action;
  QAction* m_screenshot_action;
  QMenu* m_state_load_menu;
  QMenu* m_state_save_menu;
  QMenu* m_state_slot_menu;
  QActionGroup* m_state_slots;
  QMenu* m_state_load_slots_menu;
  QMenu* m_state_save_slots_menu;
};
