// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <memory>
#include <string>

#include <QMenuBar>
#include <QPointer>

class QMenu;

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
  static MenuBar* GetMenuBar() { return s_menu_bar; }

  explicit MenuBar(QWidget* parent = nullptr);

  void UpdateMenu_Tools(bool emulation_started);

  void InstallUpdateManually();

  QMenu* GetGameListColumnsSubMenu() const { return m_gamelist_columns_submenu; }

signals:
  // File Menu
  void Open();
  void ChangeDisc();
  void EjectDisc();
  void BootDVDBackup(const QString& backup);
  void Exit();

  // View Menu
  void ShowList();
  void ShowGrid();
  void PurgeGameListCache();
  void ShowSearch();

  void ColumnVisibilityToggled(const QString& row, bool visible);
  void GameListPlatformVisibilityToggled(const QString& row, bool visible);
  void GameListRegionVisibilityToggled(const QString& row, bool visible);

  // Emulation Menu
  void Play();
  void Pause();
  void Stop();
  void Reset();
  void Fullscreen();
  void FrameAdvance();
  void Screenshot();

  // State Sub Menus
  void StateLoadFile();
  void StateSaveFile();
  void StateLoadSlot();
  void StateSaveSlot();
  void StateLoadSlotAt(int slot);
  void StateSaveSlotAt(int slot);
  void StateLoadUndo();
  void StateSaveUndo();
  void StateSaveSlotOldest();
  void SetStateSlot(int slot);

  // Tools Menu
  void ShowResourcePackManager();
  void ShowCheatsManager();
  void ShowFIFOPlayer();
  void StartNetPlay();
  void BrowseNetPlay();
  void BootGameCubeIPL(DiscIO::Region region);
  void ShowMemcardManager();
  void BootWiiSystemMenu();
  void ImportMergeSecondaryNAND();
  void PerformOnlineUpdate(const std::string& region);
  void ConnectWiiRemote(int id);

  // Input Recording System Sub Menu
  void INRECSPlayRecordedInputTrack();
  void INRECSStartRecording();
  void INRECSStopRecording();
  void INRECSExportRecording();
  void INRECSShowTASInputConfig();

  void INRECSStatusChanged(bool recording);
  void INRECSReadOnlyModeChanged(bool read_only);

  // Options Menu
  void Configure();
  void ConfigureGraphics();
  void ConfigureAudio();
  void ConfigureControllers();
  void ConfigureHotkeys();

  // Symbols Menu
  void NotifySymbolsUpdated();

  // Help Menu
  void ShowAboutDialog();

  // Other
  void GameSelectionChanged(std::shared_ptr<const UICommon::GameFile> game_file);

private:
  // File Menu
  void AddFileMainMenu();
  void AddBootDVDBackupSubMenu(QMenu* mainmenu_file);

  // View Menu
  void AddViewMainMenu();
  void AddGameListTypeSection(QMenu* mainmenu_view);
  void AddListColumnsSubMenu(QMenu* mainmenu_view);
  void AddShowPlatformsSubMenu(QMenu* mainmenu_view);
  void AddShowRegionsSubMenu(QMenu* mainmenu_view);

  // Emulation Menu
  void AddEmulationMainMenu();
  void AddStateLoadSubMenu(QMenu* mainmenu_emu);
  void AddStateSaveSubMenu(QMenu* mainmenu_emu);
  void AddStateSlotSelectSubMenu(QMenu* mainmenu_emu);

  void UpdateStateSlotSubMenuAction();

  void OnEmulationStateChanged(Core::State state);

  // Tools Menu
  void AddToolsMainMenu();
  void AddLoadGameCubeIPLSubMenu(QMenu* mainmenu_tools);
  void AddNANDManagementSubMenu(QMenu* mainmenu_tools);
  void AddPerformOnlineSystemUpdateSubMenu(QMenu* mainmenu_tools);
  void AddINRECSSubMenu(QMenu* mainmenu_tools);
  void AddConnectWiiRemotesSubMenu(QMenu* const mainmenu_tools);

  void ExportWiiSaves();
  void ImportWiiSave();
  void InstallWAD();
  void NAND_Check();
  void NAND_ExtractCertificates();

  void OnINRECSStatusChanged(bool recording);
  void OnINRECSReadOnlyModeChanged(bool read_only);

  // Options Menu
  void AddOptionsMainMenu();

  void ChangeDebugFont();

  // JIT Menu
  void AddDebugJITMainMenu();

  void ClearCache();
  void LogInstructions();
  void SearchInstruction();

  // Symbols Menu
  void AddDebugSymbolsMainMenu();

  void AppendSignatureFile();
  void ApplySignatureFile();
  void CreateSignatureFile();
  void CombineSignatureFiles();
  void ClearSymbols();
  void GenerateSymbolsFromAddress();
  void GenerateSymbolsFromSignatureDB();
  void GenerateSymbolsFromRSO();
  void GenerateSymbolsFromRSOAuto();
  void LoadSymbolMap();
  void LoadOtherSymbolMap();
  void LoadBadSymbolMap();
  void PatchHLEFunctions();
  void SaveSymbolMap();
  void SaveSymbolMapAs();
  void SaveCode();
  bool TryLoadMapFile(const QString& path, const bool bad = false);
  void TrySaveSymbolMap(const QString& path);

  // Help Menu
  void AddHelpMainMenu();

  // Other
  void OnGameSelectionChanged(std::shared_ptr<const UICommon::GameFile> game_file);
  void OnDebugModeToggled(bool enabled);

  QString GetSignatureSelector() const;

  static QPointer<MenuBar> s_menu_bar;

  // File Menu
  QAction* m_open_action;
  QAction* m_exit_action;
  QAction* m_change_disc;
  QAction* m_eject_disc;
  QMenu* m_boot_dvd_backup_submenu;

  // View Menu
  QAction* m_debug__show_code;
  QAction* m_debug__show_registers;
  QAction* m_debug__show_threads;
  QAction* m_debug__show_watch;
  QAction* m_debug__show_breakpoints;
  QAction* m_debug__show_memory;
  QAction* m_debug__show_network;
  QAction* m_debug__show_jit;
  QMenu* m_gamelist_columns_submenu;

  // Emulation Menu
  QAction* m_emu_play_action;
  QAction* m_emu_pause_action;
  QAction* m_emu_stop_action;
  QAction* m_emu_reset_action;
  QAction* m_emu_fullscreen_action;
  QAction* m_emu_frame_advance_action;
  QAction* m_emu_screenshot_action;
  QMenu* m_state_load_submenu;
  QMenu* m_state_save_submenu;
  QMenu* m_state_slot_select_submenu;
  QActionGroup* m_state_slots;
  QMenu* m_state_load_slots_submenu;
  QMenu* m_state_save_slots_submenu;

  // Tools Menu
  QAction* m_show_cheat_manager;
  QAction* m_gc_load_ipl_ntscj;
  QAction* m_gc_load_ipl_ntscu;
  QAction* m_gc_load_ipl_pal;
  QAction* m_wii_load_sysmenu_action;
  QMenu* m_perform_online_update_submenu;
  QAction* m_perform_online_update_for_current_region;
  QAction* m_nand_import_merge_secondary;
  QAction* m_nand_check;
  QAction* m_nand_extract_certificates;
  QAction* m_inrecs_export;
  QAction* m_inrecs_replay;
  QAction* m_inrecs_start;
  QAction* m_inrecs_stop;
  QAction* m_inrecs_read_only;
  std::array<QAction*, 5> m_wii_remotes;

  // Options Menu
  QAction* m_debug__boot_to_pause;
  QAction* m_debug__automatic_start;
  QAction* m_debug__change_font;
  QAction* m_controllers_action;

  // JIT Menu
  QMenu* m_debug__jit_mainmenu;
  QAction* m_debug__jit_interpreter_core;
  QAction* m_debug__jit_block_linking;
  QAction* m_debug__jit_disable_cache;
  QAction* m_debug__jit_disable_fastmem;
  QAction* m_debug__jit_clear_cache;
  QAction* m_debug__jit_log_coverage;
  QAction* m_debug__jit_search_instruction;
  QAction* m_debug__jit_off;
  QAction* m_debug__jit_loadstore_off;
  QAction* m_debug__jit_loadstore_lbzx_off;
  QAction* m_debug__jit_loadstore_lxz_off;
  QAction* m_debug__jit_loadstore_lwz_off;
  QAction* m_debug__jit_loadstore_floating_off;
  QAction* m_debug__jit_loadstore_paired_off;
  QAction* m_debug__jit_floatingpoint_off;
  QAction* m_debug__jit_integer_off;
  QAction* m_debug__jit_paired_off;
  QAction* m_debug__jit_systemregisters_off;
  QAction* m_debug__jit_branch_off;
  QAction* m_debug__jit_register_cache_off;

  // Symbols Menu
  QMenu* m_debug__symbols_mainmenu;

  // Other
  bool m_game_selected = false;
};
