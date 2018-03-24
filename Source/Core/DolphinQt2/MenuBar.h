// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <memory>
#include <string>

#include <QMenu>
#include <QMenuBar>

namespace Core
{
enum class State;
}

namespace DiscIO
{
enum class Region;
};

namespace UICommon
{
class GameFile;
}

class MenuBar final : public QMenuBar
{
  Q_OBJECT

public:
  explicit MenuBar(QWidget* parent = nullptr);

  void UpdateStateSlotMenu();
  void UpdateToolsMenu(bool emulation_started);

signals:
  // File
  void Open();
  void Exit();
  void ChangeDisc();
  void BootDVDBackup(const QString& backup);
  void EjectDisc();

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
  void ShowMemcardManager();
  void BootGameCubeIPL(DiscIO::Region region);
  void ShowFIFOPlayer();
  void ShowAboutDialog();
  void ConnectWiiRemote(int id);

  // Options
  void Configure();
  void ConfigureGraphics();
  void ConfigureAudio();
  void ConfigureControllers();
  void ConfigureHotkeys();

  // View
  void ShowList();
  void ShowGrid();
  void ToggleSearch();
  void ColumnVisibilityToggled(const QString& row, bool visible);
  void GameListPlatformVisibilityToggled(const QString& row, bool visible);
  void GameListRegionVisibilityToggled(const QString& row, bool visible);

  // Movie
  void PlayRecording();
  void StartRecording();
  void StopRecording();
  void ExportRecording();
  void ShowTASInput();

  void SelectionChanged(std::shared_ptr<const UICommon::GameFile> game_file);
  void RecordingStatusChanged(bool recording);
  void ReadOnlyModeChanged(bool read_only);

private:
  void OnEmulationStateChanged(Core::State state);

  void AddFileMenu();
  void AddDVDBackupMenu(QMenu* file_menu);

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
  void AddMovieMenu();
  void AddSymbolsMenu();

  void InstallWAD();
  void ImportWiiSave();
  void ExportWiiSaves();
  void CheckNAND();
  void NANDExtractCertificates();
  void ChangeDebugFont();

  // Debugging UI
  void ClearSymbols();
  void GenerateSymbolsFromAddress();
  void GenerateSymbolsFromSignatureDB();
  void GenerateSymbolsFromRSO();
  void LoadSymbolMap();
  void LoadOtherSymbolMap();
  void SaveSymbolMap();
  void SaveSymbolMapAs();
  void SaveCode();
  void CreateSignatureFile();
  void PatchHLEFunctions();

  void OnSelectionChanged(std::shared_ptr<const UICommon::GameFile> game_file);
  void OnRecordingStatusChanged(bool recording);
  void OnReadOnlyModeChanged(bool read_only);
  void OnDebugModeToggled(bool enabled);

  // File
  QAction* m_open_action;
  QAction* m_exit_action;
  QAction* m_change_disc;
  QAction* m_eject_disc;
  QMenu* m_backup_menu;

  // Tools
  QAction* m_wad_install_action;
  QMenu* m_perform_online_update_menu;
  QAction* m_perform_online_update_for_current_region;
  QAction* m_ntscj_ipl;
  QAction* m_ntscu_ipl;
  QAction* m_pal_ipl;
  QAction* m_import_backup;
  QAction* m_check_nand;
  QAction* m_extract_certificates;
  std::array<QAction*, 5> m_wii_remotes;

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

  // Movie
  QAction* m_recording_export;
  QAction* m_recording_play;
  QAction* m_recording_start;
  QAction* m_recording_stop;
  QAction* m_recording_read_only;

  // Options
  QAction* m_boot_to_pause;
  QAction* m_automatic_start;
  QAction* m_change_font;

  // View
  QAction* m_show_code;
  QAction* m_show_registers;
  QAction* m_show_watch;
  QAction* m_show_breakpoints;

  // Symbols
  QMenu* m_symbols;
};
