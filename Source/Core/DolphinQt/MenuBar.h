// Copyright 2015 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <memory>
#include <string>
#include <vector>

#include <QMenuBar>
#include <QPointer>

#include "Common/CommonTypes.h"

class QMenu;
class ParallelProgressDialog;

namespace Core
{
enum class State;
}

namespace DiscIO
{
enum class Region;
}

namespace UICommon
{
class GameFile;
}

using RSOPairEntry = std::pair<u32, std::string>;
using RSOVector = std::vector<RSOPairEntry>;

class MenuBar final : public QMenuBar
{
  Q_OBJECT

public:
  static MenuBar* GetMenuBar() { return s_menu_bar; }

  explicit MenuBar(QWidget* parent = nullptr);

  void UpdateToolsMenu(bool emulation_started);

  QMenu* GetListColumnsMenu() const { return m_cols_menu; }

  void InstallUpdateManually();

signals:
  // File
  void Open();
  void Exit();
  void ChangeDisc();
  void EjectDisc();
  void OpenUserFolder();

  // Emulation
  void Play();
  void Pause();
  void Stop();
  void Reset();
  void Fullscreen();
  void FrameAdvance();
  void Screenshot();
  void StartNetPlay();
  void BrowseNetPlay();
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
  void ShowCheatsManager();
  void ShowResourcePackManager();
  void ShowSkylanderPortal();
  void ShowInfinityBase();
  void ShowWiiSpeakWindow();
  void ConnectWiiRemote(int id);

#ifdef USE_RETRO_ACHIEVEMENTS
  void ShowAchievementsWindow();
#endif  // USE_RETRO_ACHIEVEMENTS

  // Options
  void Configure();
  void ConfigureGraphics();
  void ConfigureAudio();
  void ConfigureControllers();
  void ConfigureHotkeys();
  void ConfigureFreelook();

  // View
  void ShowList();
  void ShowGrid();
  void PurgeGameListCache();
  void ShowSearch();
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
  void AddJITMenu();
  void AddSymbolsMenu();

  void UpdateStateSlotMenu();

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
  void GenerateSymbolsFromRSOAuto();
  RSOVector DetectRSOModules(ParallelProgressDialog& progress);
  void LoadSymbolMap();
  void LoadOtherSymbolMap();
  void LoadBadSymbolMap();
  void SaveSymbolMap();
  void SaveSymbolMapAs();
  void SaveCode();
  bool TryLoadMapFile(const QString& path, const bool bad = false);
  void TrySaveSymbolMap(const QString& path);
  void CreateSignatureFile();
  void AppendSignatureFile();
  void ApplySignatureFile();
  void CombineSignatureFiles();
  void PatchHLEFunctions();
  void ClearCache();
  void LogInstructions();
  void SearchInstruction();

  void OnSelectionChanged(std::shared_ptr<const UICommon::GameFile> game_file);
  void OnRecordingStatusChanged(bool recording);
  void OnReadOnlyModeChanged(bool read_only);
  void OnDebugModeToggled(bool enabled);
  void OnWriteJitBlockLogDump();

  QString GetSignatureSelector() const;

  static QPointer<MenuBar> s_menu_bar;

  // File
  QAction* m_open_action;
  QAction* m_exit_action;
  QAction* m_change_disc;
  QAction* m_eject_disc;
  QMenu* m_backup_menu;
  QAction* m_open_user_folder;

  // Tools
  QAction* m_wad_install_action;
  QMenu* m_perform_online_update_menu;
  QAction* m_perform_online_update_for_current_region;
  QAction* m_ntscj_ipl;
  QAction* m_ntscu_ipl;
  QAction* m_pal_ipl;
  QMenu* m_manage_nand_menu;
  QAction* m_import_backup;
  QAction* m_check_nand;
  QAction* m_extract_certificates;
  std::array<QAction*, 5> m_wii_remotes;
  QAction* m_import_wii_save;
  QAction* m_export_wii_saves;

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
  QAction* m_reset_ignore_panic_handler;
  QAction* m_change_font;
  QAction* m_controllers_action;

  // View
  QAction* m_show_code;
  QAction* m_show_registers;
  QAction* m_show_threads;
  QAction* m_show_watch;
  QAction* m_show_breakpoints;
  QAction* m_show_memory;
  QAction* m_show_network;
  QAction* m_show_jit;
  QAction* m_show_assembler;
  QMenu* m_cols_menu;

  // JIT
  QMenu* m_jit;

  // Symbols
  QMenu* m_symbols;
  QAction* m_jit_interpreter_core;
  QAction* m_jit_block_linking;
  QAction* m_jit_disable_cache;
  QAction* m_jit_disable_fastmem;
  QAction* m_jit_disable_fastmem_arena;
  QAction* m_jit_disable_large_entry_points_map;
  QAction* m_jit_clear_cache;
  QAction* m_jit_log_coverage;
  QAction* m_jit_search_instruction;
  QAction* m_jit_profile_blocks;
  QAction* m_jit_write_cache_log_dump;
  QAction* m_jit_off;
  QAction* m_jit_loadstore_off;
  QAction* m_jit_loadstore_lbzx_off;
  QAction* m_jit_loadstore_lxz_off;
  QAction* m_jit_loadstore_lwz_off;
  QAction* m_jit_loadstore_floating_off;
  QAction* m_jit_loadstore_paired_off;
  QAction* m_jit_floatingpoint_off;
  QAction* m_jit_integer_off;
  QAction* m_jit_paired_off;
  QAction* m_jit_systemregisters_off;
  QAction* m_jit_branch_off;
  QAction* m_jit_register_cache_off;

  bool m_game_selected = false;
};
