// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>

#include <QMenu>
#include <QMenuBar>

namespace DiscIO
{
enum class Region;
};

class MenuBar final : public QMenuBar
{
  Q_OBJECT

public:
  explicit MenuBar(QWidget* parent = nullptr);

  void EmulationStarted();
  void EmulationPaused();
  void EmulationStopped();
  void UpdateStateSlotMenu();
  void UpdateToolsMenu(bool emulation_started);

signals:
  // File
  void Open();
  void Exit();

  // Emulation
  void Play();
  void Pause();
  void Stop();
  void Reset();
  void Fullscreen();
  void FrameAdvance();
  void Screenshot();
  void StartNetPlay();
  void StateLoad();
  void StateSave();
  void StateLoadSlot();
  void StateSaveSlot();
  void StateLoadSlotAt(int slot);
  void StateSaveSlotAt(int slot);
  void StateLoadUndo();
  void StateSaveUndo();
  void StateSaveOldest();
  void SetStateSlot(int slot);
  void BootWiiSystemMenu();
  void ImportNANDBackup();

  void PerformOnlineUpdate(const std::string& region);

  // Tools
  void BootGameCubeIPL(DiscIO::Region region);

  // Options
  void Configure();
  void ConfigureGraphics();
  void ConfigureAudio();
  void ConfigureControllers();
  void ConfigureHotkeys();

  // View
  void ShowList();
  void ShowGrid();
  void ColumnVisibilityToggled(const QString& row, bool visible);
  void GameListPlatformVisibilityToggled(const QString& row, bool visible);
  void GameListRegionVisibilityToggled(const QString& row, bool visible);

  void ShowAboutDialog();

private:
  void AddFileMenu();

  void AddEmulationMenu();
  void AddStateLoadMenu(QMenu* emu_menu);
  void AddStateSaveMenu(QMenu* emu_menu);
  void AddStateSlotMenu(QMenu* emu_menu);

  void AddViewMenu();
  void AddGameListTypeSection(QMenu* view_menu);
  void AddListColumnsMenu(QMenu* view_menu);
  void AddShowPlatformsMenu(QMenu* view_menu);
  void AddShowRegionsMenu(QMenu* view_menu);

  void AddOptionsMenu();
  void AddToolsMenu();
  void AddHelpMenu();

  void InstallWAD();
  void ImportWiiSave();
  void ExportWiiSaves();
  void NANDExtractCertificates();

  // File
  QAction* m_open_action;
  QAction* m_exit_action;

  // Tools
  QAction* m_wad_install_action;
  QMenu* m_perform_online_update_menu;
  QAction* m_perform_online_update_for_current_region;
  QAction* m_ntscj_ipl;
  QAction* m_ntscu_ipl;
  QAction* m_pal_ipl;
  QAction* m_import_backup;
  QAction* m_extract_certificates;

  // Emulation
  QAction* m_play_action;
  QAction* m_pause_action;
  QAction* m_stop_action;
  QAction* m_reset_action;
  QAction* m_fullscreen_action;
  QAction* m_frame_advance_action;
  QAction* m_screenshot_action;
  QAction* m_boot_sysmenu;
  QMenu* m_state_load_menu;
  QMenu* m_state_save_menu;
  QMenu* m_state_slot_menu;
  QActionGroup* m_state_slots;
  QMenu* m_state_load_slots_menu;
  QMenu* m_state_save_slots_menu;
};
