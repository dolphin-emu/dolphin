// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <array>
#include <chrono>
#include <cinttypes>
#include <cstdarg>
#include <cstdio>
#include <future>
#include <mutex>
#include <string>
#include <vector>
#include <wx/app.h>
#include <wx/aui/framemanager.h>
#include <wx/bitmap.h>
#include <wx/filedlg.h>
#include <wx/filefn.h>
#include <wx/menu.h>
#include <wx/msgdlg.h>
#include <wx/panel.h>
#include <wx/progdlg.h>
#include <wx/statusbr.h>
#include <wx/toolbar.h>
#include <wx/toplevel.h>

#include "Common/CDUtils.h"
#include "Common/CommonPaths.h"
#include "Common/CommonTypes.h"
#include "Common/FileSearch.h"
#include "Common/FileUtil.h"
#include "Common/NandPaths.h"
#include "Common/StringUtil.h"
#include "Common/Version.h"

#include "Core/ARBruteForcer.h"
#include "Core/Boot/Boot.h"
#include "Core/BootManager.h"
#include "Core/CommonTitles.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/Boot/Boot.h"
#include "Core/HW/CPU.h"
#include "Core/HW/DVD/DVDInterface.h"
#include "Core/HW/GCKeyboard.h"
#include "Core/HW/GCPad.h"
#include "Core/HW/ProcessorInterface.h"
#include "Core/HW/SI/SI_Device.h"
#include "Core/HW/WiiSaveCrypted.h"
#include "Core/HW/Wiimote.h"
#include "Core/Host.h"
#include "Core/HotkeyManager.h"
#include "Core/IOS/ES/ES.h"
#include "Core/IOS/IOS.h"
#include "Core/IOS/STM/STM.h"
#include "Core/IOS/USB/Bluetooth/BTEmu.h"
#include "Core/IOS/USB/Bluetooth/WiimoteDevice.h"
#include "Core/Movie.h"
#include "Core/PowerPC/PPCAnalyst.h"
#include "Core/PowerPC/PPCSymbolDB.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/PowerPC/SignatureDB/SignatureDB.h"
#include "Core/State.h"
#include "Core/TitleDatabase.h"
#include "Core/WiiUtils.h"

#include "DiscIO/Enums.h"
#include "DiscIO/NANDImporter.h"
#include "DiscIO/VolumeWad.h"
#include "DiscIO/WiiSaveBanner.h"

#include "DolphinWX/AboutDolphin.h"
#include "DolphinWX/Cheats/CheatsWindow.h"
#include "DolphinWX/Config/ConfigMain.h"
#include "DolphinWX/ConfigVR.h"
#include "DolphinWX/ControllerConfigDiag.h"
#include "DolphinWX/Debugger/BreakpointWindow.h"
#include "DolphinWX/Debugger/CodeWindow.h"
#include "DolphinWX/Debugger/WatchWindow.h"
#include "DolphinWX/FifoPlayerDlg.h"
#include "DolphinWX/Frame.h"
#include "DolphinWX/GameListCtrl.h"
#include "DolphinWX/Globals.h"
#include "DolphinWX/ISOFile.h"
#include "DolphinWX/Input/HotkeyInputConfigDiag.h"
#include "DolphinWX/Input/InputConfigDiag.h"
#include "DolphinWX/LogWindow.h"
#include "DolphinWX/MainMenuBar.h"
#include "DolphinWX/MainToolBar.h"
#include "DolphinWX/MemcardManager.h"
#include "DolphinWX/NetPlay/NetPlaySetupFrame.h"
#include "DolphinWX/NetPlay/NetWindow.h"
#include "DolphinWX/TASInputDlg.h"
#include "DolphinWX/WxEventUtils.h"
#include "DolphinWX/WxUtils.h"

#include "InputCommon/ControllerInterface/ControllerInterface.h"

#include "UICommon/UICommon.h"

#include "VideoCommon/RenderBase.h"
#include "VideoCommon/VR.h"
#include "VideoCommon/VideoBackendBase.h"
#include "VideoCommon/VideoConfig.h"

class InputConfig;
class wxFrame;

// This override allows returning a fake menubar object while removing the real one from the screen
wxMenuBar* CFrame::GetMenuBar() const
{
  if (m_frameMenuBar)
  {
    return m_frameMenuBar;
  }
  else
  {
    return m_menubar_shadow;
  }
}

// Create menu items
// ---------------------
wxMenuBar* CFrame::CreateMenuBar() const
{
  const auto menu_type =
      m_use_debugger ? MainMenuBar::MenuType::Debug : MainMenuBar::MenuType::Regular;

  return new MainMenuBar{menu_type};
}

void CFrame::BindMenuBarEvents()
{
  // File menu
  Bind(wxEVT_MENU, &CFrame::OnOpen, this, wxID_OPEN);
  Bind(wxEVT_MENU, &CFrame::OnChangeDisc, this, IDM_CHANGE_DISC);
  Bind(wxEVT_MENU, &CFrame::OnEjectDisc, this, IDM_EJECT_DISC);
  Bind(wxEVT_MENU, &CFrame::OnBootDrive, this, IDM_DRIVE1, IDM_DRIVE24);
  Bind(wxEVT_MENU, &CFrame::OnRefresh, this, wxID_REFRESH);
  Bind(wxEVT_MENU, &CFrame::OnQuit, this, wxID_EXIT);

  // Emulation menu
  Bind(wxEVT_MENU, &CFrame::OnPlay, this, IDM_PLAY);
  Bind(wxEVT_MENU, &CFrame::OnStop, this, IDM_STOP);
  Bind(wxEVT_MENU, &CFrame::OnReset, this, IDM_RESET);
  Bind(wxEVT_MENU, &CFrame::OnToggleFullscreen, this, IDM_TOGGLE_FULLSCREEN);
  Bind(wxEVT_MENU, &CFrame::OnFrameStep, this, IDM_FRAMESTEP);
  Bind(wxEVT_MENU, &CFrame::OnScreenshot, this, IDM_SCREENSHOT);
  Bind(wxEVT_MENU, &CFrame::OnLoadStateFromFile, this, IDM_LOAD_STATE_FILE);
  Bind(wxEVT_MENU, &CFrame::OnLoadCurrentSlot, this, IDM_LOAD_SELECTED_SLOT);
  Bind(wxEVT_MENU, &CFrame::OnUndoLoadState, this, IDM_UNDO_LOAD_STATE);
  Bind(wxEVT_MENU, &CFrame::OnLoadState, this, IDM_LOAD_SLOT_1, IDM_LOAD_SLOT_10);
  Bind(wxEVT_MENU, &CFrame::OnLoadLastState, this, IDM_LOAD_LAST_1, IDM_LOAD_LAST_10);
  Bind(wxEVT_MENU, &CFrame::OnSaveStateToFile, this, IDM_SAVE_STATE_FILE);
  Bind(wxEVT_MENU, &CFrame::OnSaveCurrentSlot, this, IDM_SAVE_SELECTED_SLOT);
  Bind(wxEVT_MENU, &CFrame::OnSaveFirstState, this, IDM_SAVE_FIRST_STATE);
  Bind(wxEVT_MENU, &CFrame::OnUndoSaveState, this, IDM_UNDO_SAVE_STATE);
  Bind(wxEVT_MENU, &CFrame::OnSaveState, this, IDM_SAVE_SLOT_1, IDM_SAVE_SLOT_10);
  Bind(wxEVT_MENU, &CFrame::OnSelectSlot, this, IDM_SELECT_SLOT_1, IDM_SELECT_SLOT_10);

  // Movie menu
  Bind(wxEVT_MENU, &CFrame::OnRecord, this, IDM_RECORD);
  Bind(wxEVT_MENU, &CFrame::OnPlayRecording, this, IDM_PLAY_RECORD);
  Bind(wxEVT_MENU, &CFrame::OnStopRecording, this, IDM_STOP_RECORD);
  Bind(wxEVT_MENU, &CFrame::OnRecordExport, this, IDM_RECORD_EXPORT);
  Bind(wxEVT_MENU, &CFrame::OnRecordReadOnly, this, IDM_RECORD_READ_ONLY);
  Bind(wxEVT_MENU, &CFrame::OnTASInput, this, IDM_TAS_INPUT);
  Bind(wxEVT_MENU, &CFrame::OnTogglePauseMovie, this, IDM_TOGGLE_PAUSE_MOVIE);
  Bind(wxEVT_MENU, &CFrame::OnShowLag, this, IDM_SHOW_LAG);
  Bind(wxEVT_MENU, &CFrame::OnShowFrameCount, this, IDM_SHOW_FRAME_COUNT);
  Bind(wxEVT_MENU, &CFrame::OnShowInputDisplay, this, IDM_SHOW_INPUT_DISPLAY);
  Bind(wxEVT_MENU, &CFrame::OnShowRTCDisplay, this, IDM_SHOW_RTC_DISPLAY);
  Bind(wxEVT_MENU, &CFrame::OnToggleDumpFrames, this, IDM_TOGGLE_DUMP_FRAMES);
  Bind(wxEVT_MENU, &CFrame::OnToggleDumpAudio, this, IDM_TOGGLE_DUMP_AUDIO);

  // Options menu
  Bind(wxEVT_MENU, &CFrame::OnConfigMain, this, wxID_PREFERENCES);
  Bind(wxEVT_MENU, &CFrame::OnConfigGFX, this, IDM_CONFIG_GFX_BACKEND);
  Bind(wxEVT_MENU, &CFrame::OnConfigAudio, this, IDM_CONFIG_AUDIO);
  Bind(wxEVT_MENU, &CFrame::OnConfigControllers, this, IDM_CONFIG_CONTROLLERS);
  Bind(wxEVT_MENU, &CFrame::OnConfigVR, this, IDM_CONFIG_VR);
  Bind(wxEVT_MENU, &CFrame::OnConfigHotkey, this, IDM_CONFIG_HOTKEYS);

  // Tools menu
  Bind(wxEVT_MENU, &CFrame::OnMemcard, this, IDM_MEMCARD);
  Bind(wxEVT_MENU, &CFrame::OnImportSave, this, IDM_IMPORT_SAVE);
  Bind(wxEVT_MENU, &CFrame::OnExportAllSaves, this, IDM_EXPORT_ALL_SAVE);
  Bind(wxEVT_MENU, &CFrame::OnLoadGameCubeIPLJAP, this, IDM_LOAD_GC_IPL_JAP);
  Bind(wxEVT_MENU, &CFrame::OnLoadGameCubeIPLUSA, this, IDM_LOAD_GC_IPL_USA);
  Bind(wxEVT_MENU, &CFrame::OnLoadGameCubeIPLEUR, this, IDM_LOAD_GC_IPL_EUR);
  Bind(wxEVT_MENU, &CFrame::OnShowCheatsWindow, this, IDM_CHEATS);
  Bind(wxEVT_MENU, &CFrame::OnNetPlay, this, IDM_NETPLAY);
  Bind(wxEVT_MENU, &CFrame::OnInstallWAD, this, IDM_MENU_INSTALL_WAD);
  Bind(wxEVT_MENU, &CFrame::OnLoadWiiMenu, this, IDM_LOAD_WII_MENU);
  Bind(wxEVT_MENU, &CFrame::OnImportBootMiiBackup, this, IDM_IMPORT_NAND);
  Bind(wxEVT_MENU, &CFrame::OnCheckNAND, this, IDM_CHECK_NAND);
  Bind(wxEVT_MENU, &CFrame::OnExtractCertificates, this, IDM_EXTRACT_CERTIFICATES);
  for (const int idm : {IDM_PERFORM_ONLINE_UPDATE_CURRENT, IDM_PERFORM_ONLINE_UPDATE_EUR,
                        IDM_PERFORM_ONLINE_UPDATE_JPN, IDM_PERFORM_ONLINE_UPDATE_KOR,
                        IDM_PERFORM_ONLINE_UPDATE_USA})
  {
    Bind(wxEVT_MENU, &CFrame::OnPerformOnlineWiiUpdate, this, idm);
  }
  Bind(wxEVT_MENU, &CFrame::OnFifoPlayer, this, IDM_FIFOPLAYER);
  Bind(wxEVT_MENU, &CFrame::OnConnectWiimote, this, IDM_CONNECT_WIIMOTE1, IDM_CONNECT_BALANCEBOARD);

  // View menu
  Bind(wxEVT_MENU, &CFrame::OnToggleToolbar, this, IDM_TOGGLE_TOOLBAR);
  Bind(wxEVT_MENU, &CFrame::OnToggleStatusbar, this, IDM_TOGGLE_STATUSBAR);
  Bind(wxEVT_MENU, &CFrame::OnToggleWindow, this, IDM_LOG_WINDOW, IDM_VIDEO_WINDOW);
  Bind(wxEVT_MENU, &CFrame::GameListChanged, this, IDM_LIST_WAD, IDM_LIST_DRIVES);
  Bind(wxEVT_MENU, &CFrame::GameListChanged, this, IDM_PURGE_GAME_LIST_CACHE);
  Bind(wxEVT_MENU, &CFrame::OnChangeColumnsVisible, this, IDM_SHOW_SYSTEM, IDM_SHOW_STATE);

  // Help menu
  Bind(wxEVT_MENU, &CFrame::OnHelp, this, IDM_HELP_WEBSITE);
  Bind(wxEVT_MENU, &CFrame::OnHelp, this, IDM_HELP_ONLINE_DOCS);
  Bind(wxEVT_MENU, &CFrame::OnHelp, this, IDM_HELP_GITHUB);
  Bind(wxEVT_MENU, &CFrame::OnHelp, this, wxID_ABOUT);

  if (m_use_debugger)
    BindDebuggerMenuBarEvents();
}

void CFrame::BindDebuggerMenuBarEvents()
{
  // Debug menu
  Bind(wxEVT_MENU, &CFrame::OnPerspectiveMenu, this, IDM_SAVE_PERSPECTIVE);
  Bind(wxEVT_MENU, &CFrame::OnPerspectiveMenu, this, IDM_EDIT_PERSPECTIVES);
  Bind(wxEVT_MENU, &CFrame::OnPerspectiveMenu, this, IDM_PERSPECTIVES_ADD_PANE_TOP);
  Bind(wxEVT_MENU, &CFrame::OnPerspectiveMenu, this, IDM_PERSPECTIVES_ADD_PANE_BOTTOM);
  Bind(wxEVT_MENU, &CFrame::OnPerspectiveMenu, this, IDM_PERSPECTIVES_ADD_PANE_LEFT);
  Bind(wxEVT_MENU, &CFrame::OnPerspectiveMenu, this, IDM_PERSPECTIVES_ADD_PANE_RIGHT);
  Bind(wxEVT_MENU, &CFrame::OnPerspectiveMenu, this, IDM_PERSPECTIVES_ADD_PANE_CENTER);
  Bind(wxEVT_MENU, &CFrame::OnSelectPerspective, this, IDM_PERSPECTIVES_0, IDM_PERSPECTIVES_100);
  Bind(wxEVT_MENU, &CFrame::OnPerspectiveMenu, this, IDM_ADD_PERSPECTIVE);
  Bind(wxEVT_MENU, &CFrame::OnPerspectiveMenu, this, IDM_TAB_SPLIT);
  Bind(wxEVT_MENU, &CFrame::OnPerspectiveMenu, this, IDM_NO_DOCKING);

  BindDebuggerMenuBarUpdateEvents();
}

void CFrame::BindDebuggerMenuBarUpdateEvents()
{
  Bind(wxEVT_UPDATE_UI, &WxEventUtils::OnEnableIfCPUCanStep, IDM_STEP);
  Bind(wxEVT_UPDATE_UI, &WxEventUtils::OnEnableIfCPUCanStep, IDM_STEPOUT);
  Bind(wxEVT_UPDATE_UI, &WxEventUtils::OnEnableIfCPUCanStep, IDM_STEPOVER);

  Bind(wxEVT_UPDATE_UI, &CFrame::OnUpdateInterpreterMenuItem, this, IDM_INTERPRETER);

  Bind(wxEVT_UPDATE_UI, &WxEventUtils::OnEnableIfCoreRunning, IDM_JIT_OFF);
  Bind(wxEVT_UPDATE_UI, &WxEventUtils::OnEnableIfCoreRunning, IDM_JIT_LS_OFF);
  Bind(wxEVT_UPDATE_UI, &WxEventUtils::OnEnableIfCoreRunning, IDM_JIT_LSLXZ_OFF);
  Bind(wxEVT_UPDATE_UI, &WxEventUtils::OnEnableIfCoreRunning, IDM_JIT_LSLWZ_OFF);
  Bind(wxEVT_UPDATE_UI, &WxEventUtils::OnEnableIfCoreRunning, IDM_JIT_LSLBZX_OFF);
  Bind(wxEVT_UPDATE_UI, &WxEventUtils::OnEnableIfCoreRunning, IDM_JIT_LSF_OFF);
  Bind(wxEVT_UPDATE_UI, &WxEventUtils::OnEnableIfCoreRunning, IDM_JIT_LSP_OFF);
  Bind(wxEVT_UPDATE_UI, &WxEventUtils::OnEnableIfCoreRunning, IDM_JIT_FP_OFF);
  Bind(wxEVT_UPDATE_UI, &WxEventUtils::OnEnableIfCoreRunning, IDM_JIT_I_OFF);
  Bind(wxEVT_UPDATE_UI, &WxEventUtils::OnEnableIfCoreRunning, IDM_JIT_P_OFF);
  Bind(wxEVT_UPDATE_UI, &WxEventUtils::OnEnableIfCoreRunning, IDM_JIT_SR_OFF);
  Bind(wxEVT_UPDATE_UI, &WxEventUtils::OnEnableIfCoreRunning, IDM_CLEAR_CODE_CACHE);

  Bind(wxEVT_UPDATE_UI, &WxEventUtils::OnEnableIfCoreInitialized, IDM_SEARCH_INSTRUCTION);
  Bind(wxEVT_UPDATE_UI, &WxEventUtils::OnEnableIfCoreInitialized, IDM_CLEAR_SYMBOLS);
  Bind(wxEVT_UPDATE_UI, &WxEventUtils::OnEnableIfCoreInitialized, IDM_SCAN_FUNCTIONS);
  Bind(wxEVT_UPDATE_UI, &WxEventUtils::OnEnableIfCoreInitialized, IDM_SCAN_SIGNATURES);
  Bind(wxEVT_UPDATE_UI, &WxEventUtils::OnEnableIfCoreInitialized, IDM_SCAN_RSO);
  Bind(wxEVT_UPDATE_UI, &WxEventUtils::OnEnableIfCoreInitialized, IDM_LOAD_MAP_FILE);
  Bind(wxEVT_UPDATE_UI, &WxEventUtils::OnEnableIfCoreInitialized, IDM_SAVEMAPFILE);
  Bind(wxEVT_UPDATE_UI, &WxEventUtils::OnEnableIfCoreInitialized, IDM_LOAD_MAP_FILE_AS);
  Bind(wxEVT_UPDATE_UI, &WxEventUtils::OnEnableIfCoreInitialized, IDM_SAVE_MAP_FILE_AS);
  Bind(wxEVT_UPDATE_UI, &WxEventUtils::OnEnableIfCoreInitialized, IDM_LOAD_BAD_MAP_FILE);
  Bind(wxEVT_UPDATE_UI, &WxEventUtils::OnEnableIfCorePaused, IDM_SAVE_MAP_FILE_WITH_CODES);
  Bind(wxEVT_UPDATE_UI, &WxEventUtils::OnEnableIfCoreInitialized, IDM_CREATE_SIGNATURE_FILE);
  Bind(wxEVT_UPDATE_UI, &WxEventUtils::OnEnableIfCoreInitialized, IDM_APPEND_SIGNATURE_FILE);
  Bind(wxEVT_UPDATE_UI, &WxEventUtils::OnEnableIfCoreInitialized, IDM_COMBINE_SIGNATURE_FILES);
  Bind(wxEVT_UPDATE_UI, &WxEventUtils::OnEnableIfCoreInitialized, IDM_RENAME_SYMBOLS);
  Bind(wxEVT_UPDATE_UI, &WxEventUtils::OnEnableIfCoreInitialized, IDM_USE_SIGNATURE_FILE);
  Bind(wxEVT_UPDATE_UI, &WxEventUtils::OnEnableIfCoreInitialized, IDM_PATCH_HLE_FUNCTIONS);

  Bind(wxEVT_UPDATE_UI, &WxEventUtils::OnEnableIfCoreUninitialized, IDM_JIT_NO_BLOCK_CACHE);
}

wxToolBar* CFrame::OnCreateToolBar(long style, wxWindowID id, const wxString& name)
{
  const auto type =
      m_use_debugger ? MainToolBar::ToolBarType::Debug : MainToolBar::ToolBarType::Regular;

  return new MainToolBar{type, this, id, wxDefaultPosition, wxDefaultSize, style};
}

void CFrame::OpenGeneralConfiguration(wxWindowID tab_id)
{
  if (!m_main_config_dialog)
    m_main_config_dialog = new CConfigMain(this);
  if (tab_id > wxID_ANY)
    m_main_config_dialog->SetSelectedTab(tab_id);

  m_main_config_dialog->Show();
  m_main_config_dialog->SetFocus();
}

// Menu items

// Start the game or change the disc.
// Boot priority:
// 1. Show the game list and boot the selected game.
// 2. Default ISO
// 3. Boot last selected game
void CFrame::BootGame(const std::string& filename)
{
  std::string bootfile = filename;
  SConfig& StartUp = SConfig::GetInstance();

  if (Core::GetState() != Core::State::Uninitialized)
    return;

  // Start filename if non empty.
  // Start the selected ISO, or try one of the saved paths.
  // If all that fails, ask to add a dir and don't boot
  if (bootfile.empty())
  {
    if (m_game_list_ctrl->GetSelectedISO() != nullptr)
    {
      if (m_game_list_ctrl->GetSelectedISO()->IsValid())
        bootfile = m_game_list_ctrl->GetSelectedISO()->GetFileName();
    }
    else if (!StartUp.m_strDefaultISO.empty() && File::Exists(StartUp.m_strDefaultISO))
    {
      bootfile = StartUp.m_strDefaultISO;
    }
    else
    {
      if (!SConfig::GetInstance().m_LastFilename.empty() &&
          File::Exists(SConfig::GetInstance().m_LastFilename))
      {
        bootfile = SConfig::GetInstance().m_LastFilename;
      }
      else
      {
        m_game_list_ctrl->BrowseForDirectory();
        return;
      }
    }
  }
  if (!bootfile.empty())
  {
	StartUp.m_LastFilename = bootfile;
	StartUp.SaveSettings();
	StartGame(BootParameters::GenerateFromFile(bootfile));
  }
}

// Open file to boot
void CFrame::OnOpen(wxCommandEvent& WXUNUSED(event))
{
  if (Core::GetState() == Core::State::Uninitialized)
    DoOpen(true);
}

void CFrame::DoOpen(bool Boot)
{
  std::string currentDir = File::GetCurrentDir();

  wxString path = wxFileSelector(
      _("Select the file to load"), wxEmptyString, wxEmptyString, wxEmptyString,
      _("All GC/Wii files (elf, dol, gcm, iso, tgc, wbfs, ciso, gcz, wad, dff)") +
          wxString::Format("|*.elf;*.dol;*.gcm;*.iso;*.tgc;*.wbfs;*.ciso;*.gcz;*.wad;*.dff|%s",
                           wxGetTranslation(wxALL_FILES)),
      wxFD_OPEN | wxFD_FILE_MUST_EXIST, this);

  if (path.IsEmpty())
    return;

  std::string currentDir2 = File::GetCurrentDir();

  if (currentDir != currentDir2)
  {
    PanicAlertT("Current directory changed from %s to %s after wxFileSelector!", currentDir.c_str(),
                currentDir2.c_str());
    File::SetCurrentDir(currentDir);
  }

  // Should we boot a new game or just change the disc?
  if (Boot && !path.IsEmpty())
  {
    BootGame(WxStrToStr(path));
  }
  else
  {
    Core::RunAsCPUThread([&path] { DVDInterface::ChangeDisc(WxStrToStr(path)); });
  }
}

void CFrame::OnRecordReadOnly(wxCommandEvent& event)
{
  Movie::SetReadOnly(event.IsChecked());
}

void CFrame::OnTASInput(wxCommandEvent& event)
{
  for (int i = 0; i < 4; ++i)
  {
    if (SConfig::GetInstance().m_SIDevice[i] != SerialInterface::SIDEVICE_NONE &&
        SConfig::GetInstance().m_SIDevice[i] != SerialInterface::SIDEVICE_GC_GBA)
    {
      m_tas_input_dialogs[i]->CreateGCLayout();
      m_tas_input_dialogs[i]->Show();
      m_tas_input_dialogs[i]->SetTitle(
          wxString::Format(_("TAS Input - GameCube Controller %d"), i + 1));
    }

    if (g_wiimote_sources[i] == WIIMOTE_SRC_EMU &&
        !(Core::IsRunning() && !SConfig::GetInstance().bWii))
    {
      m_tas_input_dialogs[i + 4]->CreateWiiLayout(i);
      m_tas_input_dialogs[i + 4]->Show();
      m_tas_input_dialogs[i + 4]->SetTitle(wxString::Format(_("TAS Input - Wii Remote %d"), i + 1));
    }
  }
}

void CFrame::OnTogglePauseMovie(wxCommandEvent& WXUNUSED(event))
{
  SConfig::GetInstance().m_PauseMovie = !SConfig::GetInstance().m_PauseMovie;
  SConfig::GetInstance().SaveSettings();
}

void CFrame::OnToggleDumpFrames(wxCommandEvent& WXUNUSED(event))
{
  SConfig::GetInstance().m_DumpFrames = !SConfig::GetInstance().m_DumpFrames;
  SConfig::GetInstance().SaveSettings();
}

void CFrame::OnToggleDumpAudio(wxCommandEvent& WXUNUSED(event))
{
  SConfig::GetInstance().m_DumpAudio = !SConfig::GetInstance().m_DumpAudio;
}

void CFrame::OnShowLag(wxCommandEvent& WXUNUSED(event))
{
  SConfig::GetInstance().m_ShowLag = !SConfig::GetInstance().m_ShowLag;
  SConfig::GetInstance().SaveSettings();
}

void CFrame::OnShowFrameCount(wxCommandEvent& WXUNUSED(event))
{
  SConfig::GetInstance().m_ShowFrameCount = !SConfig::GetInstance().m_ShowFrameCount;
  SConfig::GetInstance().SaveSettings();
}

void CFrame::OnShowInputDisplay(wxCommandEvent& WXUNUSED(event))
{
  SConfig::GetInstance().m_ShowInputDisplay = !SConfig::GetInstance().m_ShowInputDisplay;
  SConfig::GetInstance().SaveSettings();
}

void CFrame::OnShowRTCDisplay(wxCommandEvent& WXUNUSED(event))
{
  SConfig::GetInstance().m_ShowRTC = !SConfig::GetInstance().m_ShowRTC;
  SConfig::GetInstance().SaveSettings();
}

void CFrame::OnFrameStep(wxCommandEvent& event)
{
  bool wasPaused = Core::GetState() == Core::State::Paused;

  Core::DoFrameStep();

  bool isPaused = Core::GetState() == Core::State::Paused;
  if (isPaused && !wasPaused)  // don't update on unpause, otherwise the status would be wrong when
                               // pausing next frame
    UpdateGUI();
}

void CFrame::OnChangeDisc(wxCommandEvent& WXUNUSED(event))
{
  DoOpen(false);
}

void CFrame::OnEjectDisc(wxCommandEvent& WXUNUSED(event))
{
  Core::RunAsCPUThread(DVDInterface::EjectDisc);
}

void CFrame::OnRecord(wxCommandEvent& WXUNUSED(event))
{
  if ((!Core::IsRunningAndStarted() && Core::IsRunning()) || Movie::IsRecordingInput() ||
      Movie::IsPlayingInput())
    return;

  int controllers = 0;

  if (Movie::IsReadOnly())
  {
    // The user just chose to record a movie, so that should take precedence
    Movie::SetReadOnly(false);
    GetMenuBar()->FindItem(IDM_RECORD_READ_ONLY)->Check(false);
  }

  for (int i = 0; i < 4; i++)
  {
    if (SerialInterface::SIDevice_IsGCController(SConfig::GetInstance().m_SIDevice[i]))
      controllers |= (1 << i);

    if (g_wiimote_sources[i] != WIIMOTE_SRC_NONE)
      controllers |= (1 << (i + 4));
  }

  if (Movie::BeginRecordingInput(controllers))
    BootGame("");
}

void CFrame::OnPlayRecording(wxCommandEvent& WXUNUSED(event))
{
  wxString path =
      wxFileSelector(_("Select the Recording File"), wxEmptyString, wxEmptyString, wxEmptyString,
                     _("Dolphin TAS Movies (*.dtm)") +
                         wxString::Format("|*.dtm|%s", wxGetTranslation(wxALL_FILES)),
                     wxFD_OPEN | wxFD_PREVIEW | wxFD_FILE_MUST_EXIST, this);

  if (path.IsEmpty())
    return;

  if (!Movie::IsReadOnly())
  {
    // let's make the read-only flag consistent at the start of a movie.
    Movie::SetReadOnly(true);
    GetMenuBar()->FindItem(IDM_RECORD_READ_ONLY)->Check();
  }

  if (Movie::PlayInput(WxStrToStr(path)))
    BootGame("");
}

void CFrame::OnStopRecording(wxCommandEvent& WXUNUSED(event))
{
  if (Movie::IsRecordingInput())
  {
    const bool was_paused = Core::GetState() == Core::State::Paused;
    DoRecordingSave();
    const bool is_paused = Core::GetState() == Core::State::Paused;
    if (is_paused && !was_paused)
      CPU::EnableStepping(false);
  }

  Movie::EndPlayInput(false);

  GetMenuBar()->FindItem(IDM_STOP_RECORD)->Enable(Movie::IsMovieActive());
  GetMenuBar()->FindItem(IDM_RECORD)->Enable(!Movie::IsMovieActive());
}

void CFrame::OnRecordExport(wxCommandEvent& WXUNUSED(event))
{
  DoRecordingSave();
}

void CFrame::OnPlay(wxCommandEvent& event)
{
  if (Core::IsRunning())
  {
    // Core is initialized and emulator is running
    if (m_use_debugger)
    {
      bool was_stopped = CPU::IsStepping();
      CPU::EnableStepping(!was_stopped);
      // When the CPU stops it generates a IDM_UPDATE_DISASM_DIALOG which automatically refreshes
      // the UI, the UI only needs to be refreshed manually when unpausing.
      if (was_stopped)
      {
        m_code_window->Repopulate();
        UpdateGUI();
      }
    }
    else
    {
      DoPause();
    }
  }
  else
  {
    // Core is uninitialized, start the game
    BootGame(WxStrToStr(event.GetString()));
  }
}

void CFrame::OnRenderParentClose(wxCloseEvent& event)
{
  // Before closing the window we need to shut down the emulation core.
  // We'll try to close this window again once that is done.
  if (Core::GetState() != Core::State::Uninitialized)
  {
    DoStop();
    if (event.CanVeto())
    {
      event.Veto();
    }
    return;
  }

  event.Skip();
}

void CFrame::OnRenderParentMove(wxMoveEvent& event)
{
  if (Core::GetState() != Core::State::Uninitialized && !RendererIsFullscreen() &&
      !m_render_frame->IsMaximized() && !m_render_frame->IsIconized())
  {
    SConfig::GetInstance().iRenderWindowXPos = m_render_frame->GetPosition().x;
    SConfig::GetInstance().iRenderWindowYPos = m_render_frame->GetPosition().y;
  }
  event.Skip();
}

void CFrame::OnRenderParentResize(wxSizeEvent& event)
{
  if (Core::GetState() != Core::State::Uninitialized)
  {
    int width, height;
    if (!SConfig::GetInstance().bRenderToMain && !RendererIsFullscreen() &&
        !m_render_frame->IsMaximized() && !m_render_frame->IsIconized())
    {
      m_render_frame->GetClientSize(&width, &height);
      SConfig::GetInstance().iRenderWindowWidth = width;
      SConfig::GetInstance().iRenderWindowHeight = height;
    }
    m_log_window->Refresh();
    m_log_window->Update();

    // We call Renderer::ChangeSurface here to indicate the size has changed,
    // but pass the same window handle. This is needed for the Vulkan backend,
    // otherwise it cannot tell that the window has been resized on some drivers.
    if (g_renderer)
      g_renderer->ChangeSurface(GetRenderHandle());
  }
  event.Skip();
}

void CFrame::ToggleDisplayMode(bool bFullscreen)
{
#ifdef _WIN32
  if (bFullscreen && SConfig::GetInstance().strFullscreenResolution != "Auto")
  {
    DEVMODE dmScreenSettings;
    memset(&dmScreenSettings, 0, sizeof(dmScreenSettings));
    dmScreenSettings.dmSize = sizeof(dmScreenSettings);
    sscanf(SConfig::GetInstance().strFullscreenResolution.c_str(), "%dx%d",
           &dmScreenSettings.dmPelsWidth, &dmScreenSettings.dmPelsHeight);
    dmScreenSettings.dmBitsPerPel = 32;
    dmScreenSettings.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT;

    // Try To Set Selected Mode And Get Results.  NOTE: CDS_FULLSCREEN Gets Rid Of Start Bar.
    ChangeDisplaySettings(&dmScreenSettings, CDS_FULLSCREEN);
  }
  else if (bFullscreen && g_has_hmd)
  {
    DEVMODE dmScreenSettings;
    memset(&dmScreenSettings, 0, sizeof(dmScreenSettings));
    dmScreenSettings.dmSize = sizeof(dmScreenSettings);
    dmScreenSettings.dmPelsWidth = g_hmd_window_width;
    dmScreenSettings.dmPelsHeight = g_hmd_window_height;
    dmScreenSettings.dmBitsPerPel = 32;
    dmScreenSettings.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT;

    // Try To Set Selected Mode And Get Results.  NOTE: CDS_FULLSCREEN Gets Rid Of Start Bar.
    ChangeDisplaySettings(&dmScreenSettings, CDS_FULLSCREEN);
  }
  else
  {
    // Change to default resolution
    ChangeDisplaySettings(nullptr, CDS_FULLSCREEN);
  }
#elif defined(HAVE_XRANDR) && HAVE_XRANDR
  if (SConfig::GetInstance().strFullscreenResolution != "Auto")
    m_xrr_config->ToggleDisplayMode(bFullscreen);
#endif
}

// Prepare the GUI to start the game.
void CFrame::StartGame(std::unique_ptr<BootParameters> boot)
{
  if (m_is_game_loading)
    return;
  m_is_game_loading = true;
  wxPostEvent(GetMenuBar(), wxCommandEvent{DOLPHIN_EVT_UPDATE_LOAD_WII_MENU_ITEM});

  GetToolBar()->EnableTool(IDM_PLAY, false);
  GetMenuBar()->FindItem(IDM_PLAY)->Enable(false);

  VR_Init();  // Must be done before g_has_hmd is used below.

  VR_RecenterHMD();

  if (g_has_rift && g_vr_can_disable_hsw)
  {
    wxMessageDialog HealthDlg(this,
      _("HEALTH & SAFETY WARNING\n\nRead and follow all warnings and "
        "instructions\nincluded with the Headset before use. Headset\n"
        "should be calibrated for each user. Not for use by\nchildren "
        "under 13. Stop use if you experience any\n"
        "discomfort or health reactions.\n\n"
        "More: www.oculus.com/warnings\n\n"
        "Do you acknowledge?\n"),
      _("HEALTH & SAFETY WARNING"),
      wxYES_NO | wxSTAY_ON_TOP | wxICON_EXCLAMATION, wxDefaultPosition);

    int Ret = HealthDlg.ShowModal();
    if (Ret != wxID_YES)
      return;
  }

  if (SConfig::GetInstance().bRenderToMain)
  {
    // Game has been started, hide the game list
    m_game_list_ctrl->Disable();
    m_game_list_ctrl->Hide();

    m_renderer_has_focus = true;
    m_render_parent = m_panel;
    m_render_frame = this;
    if (SConfig::GetInstance().bKeepWindowOnTop)
      m_render_frame->SetWindowStyle(m_render_frame->GetWindowStyle() | wxSTAY_ON_TOP);
    else
      m_render_frame->SetWindowStyle(m_render_frame->GetWindowStyle() & ~wxSTAY_ON_TOP);

    // No, I really don't want TAB_TRAVERSAL being set behind my back,
    // thanks.  (Note that calling DisableSelfFocus would prevent this flag
    // from being set for new children, but wouldn't reset the existing
    // flag.)
    m_render_parent->SetWindowStyle(m_render_parent->GetWindowStyle() & ~wxTAB_TRAVERSAL);
  }
  else
  {
    wxRect window_geometry(
        SConfig::GetInstance().iRenderWindowXPos, SConfig::GetInstance().iRenderWindowYPos,
        SConfig::GetInstance().iRenderWindowWidth, SConfig::GetInstance().iRenderWindowHeight);
    // Set window size in framebuffer pixels since the 3D rendering will be operating at
    // that level.
    wxSize default_size{wxSize(640, 480) * (1.0 / GetContentScaleFactor())};

    wxSize size(SConfig::GetInstance().iRenderWindowWidth,
                SConfig::GetInstance().iRenderWindowHeight);
    // VR window must be a certain size for Head Mounted Displays
    if (g_has_hmd)
    {
      wxPoint position;
      size = wxSize(g_hmd_window_width, g_hmd_window_height);
      position.x = g_hmd_window_x;
      position.y = g_hmd_window_y;
      if (!SConfig::GetInstance().bFullscreen || g_is_direct_mode)
      {
        // shift window so border is off-screen
        position.x -= 4;
        position.y -= 4;
      }
      m_render_frame = new CRenderFrame(nullptr, wxID_ANY, _("Dolphin"), position);
      m_render_frame->SetClientSize(size.GetWidth(), size.GetHeight());
    }
    else
    {
      m_render_frame = new CRenderFrame(nullptr, wxID_ANY, _("Dolphin"), wxDefaultPosition, default_size);
      // Convert ClientSize coordinates to frame sizes.
      wxSize decoration_fudge = m_render_frame->GetSize() - m_render_frame->GetClientSize();
      default_size += decoration_fudge;
      if (!window_geometry.IsEmpty())
        window_geometry.SetSize(window_geometry.GetSize() + decoration_fudge);

      WxUtils::SetWindowSizeAndFitToScreen(m_render_frame, window_geometry.GetPosition(),
                                           window_geometry.GetSize(), default_size);
    }
    if (SConfig::GetInstance().bKeepWindowOnTop)
      m_render_frame->SetWindowStyle(m_render_frame->GetWindowStyle() | wxSTAY_ON_TOP);
    else
      m_render_frame->SetWindowStyle(m_render_frame->GetWindowStyle() & ~wxSTAY_ON_TOP);

    m_render_frame->SetBackgroundColour(*wxBLACK);
    m_render_frame->Bind(wxEVT_CLOSE_WINDOW, &CFrame::OnRenderParentClose, this);
    m_render_frame->Bind(wxEVT_ACTIVATE, &CFrame::OnActive, this);
    m_render_frame->Bind(wxEVT_MOVE, &CFrame::OnRenderParentMove, this);
#ifdef _WIN32
    // The renderer should use a top-level window for exclusive fullscreen support.
    m_render_parent = m_render_frame;
#else
    // To capture key events on Linux and Mac OS X the frame needs at least one child.
    m_render_parent = new wxPanel(m_render_frame, IDM_MPANEL, wxDefaultPosition, wxDefaultSize, 0);
#endif
    m_render_frame->Show();
  }

#if defined(__APPLE__)
  m_render_frame->EnableFullScreenView(true);
#endif

  wxBusyCursor hourglass;

  if (g_is_direct_mode)  // If Rift is in Direct Mode, start the mirror windowed.
    DoFullscreen(FALSE);
  else
    DoFullscreen(SConfig::GetInstance().bFullscreen);

  SetDebuggerStartupParameters();

  if (!BootManager::BootCore(std::move(boot)))
  {
    DoFullscreen(false);

    // Destroy the renderer frame when not rendering to main
    if (!SConfig::GetInstance().bRenderToMain)
      m_render_frame->Destroy();

    m_render_frame = nullptr;
    m_render_parent = nullptr;
    m_is_game_loading = false;
    UpdateGUI();
  }
  else
  {
    InhibitScreensaver();
    SConfig& StartUp = SConfig::GetInstance();
    VR_SetGame(StartUp.bWii, StartUp.m_is_nand, StartUp.GetGameID());

    // We need this specifically to support setting the focus properly when using
    // the 'render to main window' feature on Windows
    if (auto panel = wxDynamicCast(m_render_parent, wxPanel))
    {
      panel->SetFocusIgnoringChildren();
    }
    else
    {
      m_render_parent->SetFocus();
    }

    wxTheApp->Bind(wxEVT_KEY_DOWN, &CFrame::OnKeyDown, this);
    wxTheApp->Bind(wxEVT_RIGHT_DOWN, &CFrame::OnMouse, this);
    wxTheApp->Bind(wxEVT_RIGHT_UP, &CFrame::OnMouse, this);
    wxTheApp->Bind(wxEVT_MIDDLE_DOWN, &CFrame::OnMouse, this);
    wxTheApp->Bind(wxEVT_MIDDLE_UP, &CFrame::OnMouse, this);
    wxTheApp->Bind(wxEVT_MOTION, &CFrame::OnMouse, this);
    m_render_parent->Bind(wxEVT_SIZE, &CFrame::OnRenderParentResize, this);

    m_render_parent->SetCursor(wxCURSOR_BLANK);
  }
}

void CFrame::SetDebuggerStartupParameters() const
{
  SConfig& config = SConfig::GetInstance();

  if (m_use_debugger)
  {
    const wxMenuBar* const menu_bar = GetMenuBar();

    config.bBootToPause = menu_bar->IsChecked(IDM_BOOT_TO_PAUSE);
    config.bAutomaticStart = menu_bar->IsChecked(IDM_AUTOMATIC_START);
    config.bJITNoBlockCache = menu_bar->IsChecked(IDM_JIT_NO_BLOCK_CACHE);
    config.bJITNoBlockLinking = menu_bar->IsChecked(IDM_JIT_NO_BLOCK_LINKING);
    config.bEnableDebugging = true;
  }
  else
  {
    config.bBootToPause = false;
    config.bEnableDebugging = false;
  }
}

void CFrame::OnBootDrive(wxCommandEvent& event)
{
  const auto* menu = static_cast<wxMenu*>(event.GetEventObject());
  BootGame(WxStrToStr(menu->GetLabelText(event.GetId())));
}

void CFrame::OnRefresh(wxCommandEvent& WXUNUSED(event))
{
  GameListRescan();
}

void CFrame::OnScreenshot(wxCommandEvent& WXUNUSED(event))
{
  Core::SaveScreenShot();
}

// Pause the emulation
void CFrame::DoPause()
{
  if (Core::GetState() == Core::State::Running)
  {
    Core::SetState(Core::State::Paused);
    if (SConfig::GetInstance().bHideCursor)
      m_render_parent->SetCursor(wxNullCursor);
    Core::UpdateTitle();
  }
  else
  {
    Core::SetState(Core::State::Running);
    if (SConfig::GetInstance().bHideCursor && RendererHasFocus())
      m_render_parent->SetCursor(wxCURSOR_BLANK);
  }
  UpdateGUI();
}

// Stop the emulation
void CFrame::DoStop()
{
  if (!Core::IsRunningAndStarted())
    return;
  if (m_confirm_stop)
    return;

  // don't let this function run again until it finishes, or is aborted.
  m_confirm_stop = true;

  if (Core::GetState() != Core::State::Uninitialized || m_render_parent != nullptr)
  {
#if defined __WXGTK__
    wxMutexGuiLeave();
    std::lock_guard<std::recursive_mutex> lk(m_keystate_lock);
    wxMutexGuiEnter();
#endif

    // Pause the state during confirmation and restore it afterwards
    Core::State state = Core::GetState();

    // Ask for confirmation in case the user accidentally clicked Stop / Escape
    if (SConfig::GetInstance().bConfirmStop)
    {
      // Exit fullscreen to ensure it does not cover the stop dialog.
      DoFullscreen(false);

      // Do not pause if netplay is running as CPU thread might be blocked
      // waiting on inputs
      bool should_pause = !NetPlayDialog::GetNetPlayClient();

      if (should_pause)
      {
        Core::SetState(Core::State::Paused);
      }

      wxMessageDialog m_StopDlg(
          this, !m_tried_graceful_shutdown ? _("Do you want to stop the current emulation?") :
                                             _("A shutdown is already in progress. Unsaved data "
                                               "may be lost if you stop the current emulation "
                                               "before it completes. Force stop?"),
          _("Please confirm..."), wxYES_NO | wxSTAY_ON_TOP | wxICON_EXCLAMATION, wxDefaultPosition);

      HotkeyManagerEmu::Enable(false);
      int Ret = m_StopDlg.ShowModal();
      HotkeyManagerEmu::Enable(true);
      if (Ret != wxID_YES)
      {
        if (should_pause)
          Core::SetState(state);

        m_confirm_stop = false;
        return;
      }
    }
    // Save VR game-specific settings
    if (g_has_hmd && g_Config.VRSettingsModified())
    {
      // state = Core::GetState();
      Core::SetState(Core::State::Paused);
      wxMessageDialog m_StopDlg(
          this, _("Do you want to save these VR settings for this game?") +
                    wxString::Format(
                        "\n3D: %5.2f units per metre"
                        "\nHUD %5.1fm away, %5.1fm thick, 3D HUD %5.2f closer, aim distance %5.1fm"
                        "\nCamera %5.1fm forward, %5.0f degrees up."
                        "\n\n2D: Screen is %5.1fm high, %5.1fm away, %5.1fm thick, %5.1fm up"
                        "\n2D Camera %5.0f degrees up.",
                        g_Config.fUnitsPerMetre, g_Config.fHudDistance, g_Config.fHudThickness,
                        g_Config.fHud3DCloser, g_Config.fAimDistance, g_Config.fCameraForward,
                        g_Config.fCameraPitch, g_Config.fScreenHeight, g_Config.fScreenDistance,
                        g_Config.fScreenThickness, g_Config.fScreenUp, g_Config.fScreenPitch),
          _("Please confirm..."), wxYES_NO | wxSTAY_ON_TOP | wxICON_EXCLAMATION, wxDefaultPosition);

      int Ret = m_StopDlg.ShowModal();
      if (Ret == wxID_YES)
      {
        // save VR settings
        g_Config.GameIniSave();
      }
    }

    if (m_use_debugger && m_code_window)
    {
      PowerPC::watches.Clear();
      PowerPC::breakpoints.Clear();
      PowerPC::memchecks.Clear();
      if (m_code_window->HasPanel<CBreakPointWindow>())
        m_code_window->GetPanel<CBreakPointWindow>()->NotifyUpdate();
      g_symbolDB.Clear();
      Host_NotifyMapLoaded();
      Core::SetState(state);
    }

    if (NetPlayDialog::GetNetPlayClient())
      NetPlayDialog::GetNetPlayClient()->Stop();

    // TODO: Show the author/description dialog here
    if (Movie::IsRecordingInput())
      DoRecordingSave();
    if (Movie::IsMovieActive())
      Movie::EndPlayInput(false);

    if (!m_tried_graceful_shutdown && UICommon::TriggerSTMPowerEvent())
    {
      m_tried_graceful_shutdown = true;
      m_confirm_stop = false;

      // Unpause because gracefully shutting down needs the game to actually request a shutdown.
      // Do not unpause in debug mode to allow debugging until the complete shutdown.
      if (Core::GetState() == Core::State::Paused && !m_use_debugger)
        Core::SetState(Core::State::Running);

      return;
    }

    // Reshow the cursor on the parent frame after successful stop.
    m_render_parent->SetCursor(wxNullCursor);

    Core::Stop();
    UpdateGUI();
  }
}

void CFrame::OnStopped()
{
  m_confirm_stop = false;
  m_is_game_loading = false;
  m_tried_graceful_shutdown = false;
  wxPostEvent(GetMenuBar(), wxCommandEvent{DOLPHIN_EVT_UPDATE_LOAD_WII_MENU_ITEM});

  UninhibitScreensaver();

  m_render_frame->SetTitle(StrToWxStr(Common::scm_rev_str));

  // Destroy the renderer frame when not rendering to main
  m_render_parent->Unbind(wxEVT_SIZE, &CFrame::OnRenderParentResize, this);

  // Keyboard
  wxTheApp->Unbind(wxEVT_KEY_DOWN, &CFrame::OnKeyDown, this);

  // Mouse
  wxTheApp->Unbind(wxEVT_RIGHT_DOWN, &CFrame::OnMouse, this);
  wxTheApp->Unbind(wxEVT_RIGHT_UP, &CFrame::OnMouse, this);
  wxTheApp->Unbind(wxEVT_MIDDLE_DOWN, &CFrame::OnMouse, this);
  wxTheApp->Unbind(wxEVT_MIDDLE_UP, &CFrame::OnMouse, this);
  wxTheApp->Unbind(wxEVT_MOTION, &CFrame::OnMouse, this);
  if (SConfig::GetInstance().bHideCursor)
    m_render_parent->SetCursor(wxNullCursor);
  DoFullscreen(false);
  if (!SConfig::GetInstance().bRenderToMain)
  {
    m_render_frame->Destroy();
  }
  else
  {
#if defined(__APPLE__)
    // Disable the full screen button when not in a game.
    m_render_frame->EnableFullScreenView(false);
#endif

    // Make sure the window is not longer set to stay on top
    m_render_frame->SetWindowStyle(m_render_frame->GetWindowStyle() & ~wxSTAY_ON_TOP);
  }
  m_render_parent = nullptr;
  m_renderer_has_focus = false;
  m_render_frame = nullptr;

  // Clean framerate indications from the status bar.
  GetStatusBar()->SetStatusText(" ", 0);

  // Clear Wii Remote connection status from the status bar.
  GetStatusBar()->SetStatusText(" ", 1);

  // If batch mode was specified on the command-line or we were already closing, exit now.
  if (m_batch_mode || m_is_closing)
    Close(true);

  // If using auto size with render to main, reset the application size.
  if (SConfig::GetInstance().bRenderToMain && SConfig::GetInstance().bRenderWindowAutoSize)
    SetSize(SConfig::GetInstance().iWidth, SConfig::GetInstance().iHeight);

  m_game_list_ctrl->Enable();
  m_game_list_ctrl->Show();
  m_game_list_ctrl->SetFocus();
  UpdateGUI();
}

void CFrame::DoRecordingSave()
{
  bool paused = Core::GetState() == Core::State::Paused;

  if (!paused)
    DoPause();

  wxString path =
      wxFileSelector(_("Select the Recording File"), wxEmptyString, wxEmptyString, wxEmptyString,
                     _("Dolphin TAS Movies (*.dtm)") +
                         wxString::Format("|*.dtm|%s", wxGetTranslation(wxALL_FILES)),
                     wxFD_SAVE | wxFD_PREVIEW | wxFD_OVERWRITE_PROMPT, this);

  if (path.IsEmpty())
    return;

  Movie::SaveRecording(WxStrToStr(path));

  if (!paused)
    DoPause();
}

void CFrame::OnStop(wxCommandEvent& WXUNUSED(event))
{
  DoStop();
}

void CFrame::OnReset(wxCommandEvent& WXUNUSED(event))
{
  if (Movie::IsRecordingInput())
    Movie::SetReset(true);
  ProcessorInterface::ResetButton_Tap();
}

void CFrame::OnConfigMain(wxCommandEvent& WXUNUSED(event))
{
  OpenGeneralConfiguration();
}

void CFrame::OnConfigGFX(wxCommandEvent& WXUNUSED(event))
{
  HotkeyManagerEmu::Enable(false);
  if (g_video_backend)
    g_video_backend->ShowConfig(this);
  HotkeyManagerEmu::Enable(true);
}

void CFrame::OnConfigAudio(wxCommandEvent& WXUNUSED(event))
{
  OpenGeneralConfiguration(CConfigMain::ID_AUDIOPAGE);
}

void CFrame::OnConfigControllers(wxCommandEvent& WXUNUSED(event))
{
  ControllerConfigDiag config_dlg(this);
  HotkeyManagerEmu::Enable(false);
  config_dlg.ShowModal();
  HotkeyManagerEmu::Enable(true);
}

void CFrame::OnConfigVR(wxCommandEvent& WXUNUSED(event))
{
  // InputConfig* const pad_plugin =
  Pad::GetConfig();
  bool was_init = false;
  if (g_controller_interface.IsInit())  // check if game is running
  {
    was_init = true;
  }
  else
  {
#if defined(HAVE_X11) && HAVE_X11
    Window win = X11Utils::XWindowFromHandle(GetHandle());
    Pad::Initialize(reinterpret_cast<void*>(win));
#else
    Pad::Initialize();
#endif
  }

  CConfigVR ConfigVR(this);
  ConfigVR.ShowModal();
  ConfigVR.Destroy();
  if (!was_init)  // if game isn't running
  {
    Pad::Shutdown();
  }
}

void CFrame::OnConfigHotkey(wxCommandEvent& WXUNUSED(event))
{
  InputConfig* const hotkey_plugin = HotkeyManagerEmu::GetConfig();

  // check if game is running
  bool game_running = false;
  if (Core::GetState() == Core::State::Running)
  {
    Core::SetState(Core::State::Paused);
    game_running = true;
  }

  HotkeyManagerEmu::Enable(false);

  HotkeyInputConfigDialog m_ConfigFrame(this, *hotkey_plugin, _("Dolphin Hotkeys"), m_use_debugger);
  m_ConfigFrame.ShowModal();

  // Update references in case controllers were refreshed
  Wiimote::LoadConfig();
  Keyboard::LoadConfig();
  Pad::LoadConfig();
  HotkeyManagerEmu::LoadConfig();

  HotkeyManagerEmu::Enable(true);

  // if game isn't running
  if (game_running)
  {
    Core::SetState(Core::State::Running);
  }

  // Update the GUI in case menu accelerators were changed
  UpdateGUI();
}

void CFrame::OnHelp(wxCommandEvent& event)
{
  switch (event.GetId())
  {
  case wxID_ABOUT:
  {
    AboutDolphin frame(this);
    HotkeyManagerEmu::Enable(false);
    frame.ShowModal();
    HotkeyManagerEmu::Enable(true);
  }
  break;
  case IDM_HELP_WEBSITE:
    WxUtils::Launch("https://dolphinvr.wordpress.com/");
    break;
  case IDM_HELP_ONLINE_DOCS:
    WxUtils::Launch("https://dolphin-emu.org/docs/guides/");
    break;
  case IDM_HELP_GITHUB:
    WxUtils::Launch("https://github.com/CarlKenner/dolphin");
    break;
  }
}

void CFrame::OnReloadThemeBitmaps(wxCommandEvent& WXUNUSED(event))
{
  wxCommandEvent reload_event{DOLPHIN_EVT_RELOAD_TOOLBAR_BITMAPS};
  reload_event.SetEventObject(this);
  wxPostEvent(GetToolBar(), reload_event);

  GameListRefresh();
}

void CFrame::OnRefreshGameList(wxCommandEvent& WXUNUSED(event))
{
  GameListRefresh();
}

void CFrame::OnRescanGameList(wxCommandEvent& WXUNUSED(event))
{
  GameListRescan();
}

void CFrame::OnUpdateInterpreterMenuItem(wxUpdateUIEvent& event)
{
  WxEventUtils::OnEnableIfCoreRunning(event);

  if (GetMenuBar()->FindItem(IDM_INTERPRETER)->IsChecked())
    return;

  event.Check(SConfig::GetInstance().iCPUCore == PowerPC::CORE_INTERPRETER);
}

void CFrame::ClearStatusBar()
{
  if (this->GetStatusBar()->IsEnabled())
  {
    this->GetStatusBar()->SetStatusText("", 0);
  }
}

void CFrame::StatusBarMessage(const char* format, ...)
{
  va_list args;
  va_start(args, format);
  std::string msg = StringFromFormatV(format, args);
  va_end(args);

  if (this->GetStatusBar()->IsEnabled())
  {
    this->GetStatusBar()->SetStatusText(StrToWxStr(msg), 0);
  }
}

// Miscellaneous menus
// ---------------------
// NetPlay stuff
void CFrame::OnNetPlay(wxCommandEvent& WXUNUSED(event))
{
  if (!m_netplay_setup_frame)
  {
    if (NetPlayDialog::GetInstance() != nullptr)
      NetPlayDialog::GetInstance()->Raise();
    else
      m_netplay_setup_frame = new NetPlaySetupFrame(this, m_game_list_ctrl);
  }
  else
  {
    m_netplay_setup_frame->Raise();
  }
}

void CFrame::OnMemcard(wxCommandEvent& WXUNUSED(event))
{
  CMemcardManager MemcardManager(this);
  HotkeyManagerEmu::Enable(false);
  MemcardManager.ShowModal();
  HotkeyManagerEmu::Enable(true);
}

void CFrame::OnLoadGameCubeIPLJAP(wxCommandEvent&)
{
  StartGame(std::make_unique<BootParameters>(BootParameters::IPL{DiscIO::Region::NTSC_J}));
}

void CFrame::OnLoadGameCubeIPLUSA(wxCommandEvent&)
{
  StartGame(std::make_unique<BootParameters>(BootParameters::IPL{DiscIO::Region::NTSC_U}));
}

void CFrame::OnLoadGameCubeIPLEUR(wxCommandEvent&)
{
  StartGame(std::make_unique<BootParameters>(BootParameters::IPL{DiscIO::Region::PAL}));
}

void CFrame::OnExportAllSaves(wxCommandEvent& WXUNUSED(event))
{
  CWiiSaveCrypted::ExportAllSaves();
}

void CFrame::OnImportSave(wxCommandEvent& WXUNUSED(event))
{
  wxString path =
      wxFileSelector(_("Select the save file"), wxEmptyString, wxEmptyString, wxEmptyString,
                     _("Wii save files (*.bin)") + "|*.bin|" + wxGetTranslation(wxALL_FILES),
                     wxFD_OPEN | wxFD_PREVIEW | wxFD_FILE_MUST_EXIST, this);

  if (!path.IsEmpty())
    CWiiSaveCrypted::ImportWiiSave(WxStrToStr(path));
}

void CFrame::OnShowCheatsWindow(wxCommandEvent& WXUNUSED(event))
{
  if (!m_cheats_window)
    m_cheats_window = new wxCheatsWindow(this);

  m_cheats_window->Show();
  m_cheats_window->Raise();
}

void CFrame::OnLoadWiiMenu(wxCommandEvent& WXUNUSED(event))
{
  StartGame(std::make_unique<BootParameters>(BootParameters::NANDTitle{Titles::SYSTEM_MENU}));
}

void CFrame::OnInstallWAD(wxCommandEvent& event)
{
  std::string fileName;

  switch (event.GetId())
  {
  case IDM_LIST_INSTALL_WAD:
  {
    const GameListItem* iso = m_game_list_ctrl->GetSelectedISO();
    if (!iso)
      return;
    fileName = iso->GetFileName();
    break;
  }
  case IDM_MENU_INSTALL_WAD:
  {
    wxString path = wxFileSelector(
        _("Select a Wii WAD file to install"), wxEmptyString, wxEmptyString, wxEmptyString,
        _("Wii WAD files (*.wad)") + "|*.wad|" + wxGetTranslation(wxALL_FILES),
        wxFD_OPEN | wxFD_PREVIEW | wxFD_FILE_MUST_EXIST, this);
    fileName = WxStrToStr(path);
    break;
  }
  default:
    return;
  }

  wxProgressDialog dialog(_("Installing WAD..."), _("Working..."), 1000, this,
                          wxPD_APP_MODAL | wxPD_ELAPSED_TIME | wxPD_ESTIMATED_TIME |
                              wxPD_REMAINING_TIME | wxPD_SMOOTH);

  if (WiiUtils::InstallWAD(fileName))
    wxPostEvent(GetMenuBar(), wxCommandEvent{DOLPHIN_EVT_UPDATE_LOAD_WII_MENU_ITEM});
}

void CFrame::OnUninstallWAD(wxCommandEvent&)
{
  const GameListItem* file = m_game_list_ctrl->GetSelectedISO();
  if (!file)
    return;

  if (!AskYesNoT("Uninstalling the WAD will remove the currently installed version "
                 "of this title from the NAND without deleting its save data. Continue?"))
  {
    return;
  }

  u64 title_id = file->GetTitleID();
  IOS::HLE::Kernel ios;
  if (ios.GetES()->DeleteTitleContent(title_id) < 0)
  {
    PanicAlertT("Failed to remove this title from the NAND.");
    return;
  }

  if (title_id == Titles::SYSTEM_MENU)
    wxPostEvent(GetMenuBar(), wxCommandEvent{DOLPHIN_EVT_UPDATE_LOAD_WII_MENU_ITEM});
}

void CFrame::OnImportBootMiiBackup(wxCommandEvent& WXUNUSED(event))
{
  if (!AskYesNoT("Merging a new NAND over your currently selected NAND will overwrite any channels "
                 "and savegames that already exist. This process is not reversible, so it is "
                 "recommended that you keep backups of both NANDs. Are you sure you want to "
                 "continue?"))
    return;

  wxString path = wxFileSelector(
      _("Select a BootMii NAND backup to import"), wxEmptyString, wxEmptyString, wxEmptyString,
      _("BootMii NAND backup file (*.bin)") + "|*.bin|" + wxGetTranslation(wxALL_FILES),
      wxFD_OPEN | wxFD_PREVIEW | wxFD_FILE_MUST_EXIST, this);
  const std::string file_name = WxStrToStr(path);
  if (file_name.empty())
    return;

  wxProgressDialog dialog(_("Importing NAND backup"), _("Working..."), 100, this,
                          wxPD_APP_MODAL | wxPD_ELAPSED_TIME | wxPD_SMOOTH);
  DiscIO::NANDImporter().ImportNANDBin(
      file_name, [&dialog] { dialog.Pulse(); },
      [this] {
        return WxStrToStr(wxFileSelector(
            _("Select the OTP/SEEPROM dump"), wxEmptyString, wxEmptyString, wxEmptyString,
            _("BootMii OTP/SEEPROM dump (*.bin)") + "|*.bin|" + wxGetTranslation(wxALL_FILES),
            wxFD_OPEN | wxFD_PREVIEW | wxFD_FILE_MUST_EXIST, this));
      });
  wxPostEvent(GetMenuBar(), wxCommandEvent{DOLPHIN_EVT_UPDATE_LOAD_WII_MENU_ITEM});
}

void CFrame::OnCheckNAND(wxCommandEvent&)
{
  IOS::HLE::Kernel ios;
  WiiUtils::NANDCheckResult result = WiiUtils::CheckNAND(ios);
  if (!result.bad)
  {
    wxMessageBox(_("No issues have been detected."), _("NAND Check"), wxOK | wxICON_INFORMATION);
    return;
  }

  wxString message = _("The emulated NAND is damaged. System titles such as the Wii Menu and "
                       "the Wii Shop Channel may not work correctly.\n\n"
                       "Do you want to try to repair the NAND?");
  if (!result.titles_to_remove.empty())
  {
    std::string title_listings;
    Core::TitleDatabase title_db;
    for (const u64 title_id : result.titles_to_remove)
    {
      title_listings += StringFromFormat("%016" PRIx64, title_id);

      const std::string database_name = title_db.GetChannelName(title_id);
      if (!database_name.empty())
      {
        title_listings += " - " + database_name;
      }
      else
      {
        DiscIO::WiiSaveBanner banner(title_id);
        if (banner.IsValid())
        {
          title_listings += " - " + banner.GetName();
          const std::string description = banner.GetDescription();
          if (!StripSpaces(description).empty())
            title_listings += " - " + description;
        }
      }

      title_listings += "\n";
    }

    message += wxString::Format(
        _("\n\nWARNING: Fixing this NAND requires the deletion of titles that have "
          "incomplete data on the NAND, including all associated save data. "
          "By continuing, the following title(s) will be removed:\n\n"
          "%s"
          "\nLaunching these titles may also fix the issues."),
        StrToWxStr(title_listings));
  }

  if (wxMessageBox(message, _("NAND Check"), wxYES_NO) != wxYES)
    return;

  if (WiiUtils::RepairNAND(ios))
  {
    wxMessageBox(_("The NAND has been repaired."), _("NAND Check"), wxOK | wxICON_INFORMATION);
    return;
  }

  wxMessageBox(_("The NAND could not be repaired. It is recommended to back up "
                 "your current data and start over with a fresh NAND."),
               _("NAND Check"), wxOK | wxICON_ERROR);
}

void CFrame::OnExtractCertificates(wxCommandEvent& WXUNUSED(event))
{
  DiscIO::NANDImporter().ExtractCertificates(File::GetUserPath(D_WIIROOT_IDX));
}

static std::string GetUpdateRegionFromIdm(int idm)
{
  switch (idm)
  {
  case IDM_PERFORM_ONLINE_UPDATE_EUR:
    return "EUR";
  case IDM_PERFORM_ONLINE_UPDATE_JPN:
    return "JPN";
  case IDM_PERFORM_ONLINE_UPDATE_KOR:
    return "KOR";
  case IDM_PERFORM_ONLINE_UPDATE_USA:
    return "USA";
  case IDM_PERFORM_ONLINE_UPDATE_CURRENT:
  default:
    return "";
  }
}

static void ShowUpdateResult(WiiUtils::UpdateResult result)
{
  switch (result)
  {
  case WiiUtils::UpdateResult::Succeeded:
    wxMessageBox(_("The emulated Wii console has been updated."), _("Update completed"),
                 wxOK | wxICON_INFORMATION);
    DiscIO::NANDImporter().ExtractCertificates(File::GetUserPath(D_WIIROOT_IDX));
    break;
  case WiiUtils::UpdateResult::AlreadyUpToDate:
    wxMessageBox(_("The emulated Wii console is already up-to-date."), _("Update completed"),
                 wxOK | wxICON_INFORMATION);
    DiscIO::NANDImporter().ExtractCertificates(File::GetUserPath(D_WIIROOT_IDX));
    break;
  case WiiUtils::UpdateResult::ServerFailed:
    wxMessageBox(_("Could not download update information from Nintendo. "
                   "Please check your Internet connection and try again."),
                 _("Update failed"), wxOK | wxICON_ERROR);
    break;
  case WiiUtils::UpdateResult::DownloadFailed:
    wxMessageBox(_("Could not download update files from Nintendo. "
                   "Please check your Internet connection and try again."),
                 _("Update failed"), wxOK | wxICON_ERROR);
    break;
  case WiiUtils::UpdateResult::ImportFailed:
    wxMessageBox(_("Could not install an update to the Wii system memory. "
                   "Please refer to logs for more information."),
                 _("Update failed"), wxOK | wxICON_ERROR);
    break;
  case WiiUtils::UpdateResult::Cancelled:
    wxMessageBox(_("The update has been cancelled. It is strongly recommended to "
                   "finish it in order to avoid inconsistent system software versions."),
                 _("Update cancelled"), wxOK | wxICON_WARNING);
    break;
  case WiiUtils::UpdateResult::RegionMismatch:
    wxMessageBox(_("The game's region does not match your console's. "
                   "To avoid issues with the system menu, it is not possible to update "
                   "the emulated console using this disc."),
                 _("Update failed"), wxOK | wxICON_ERROR);
    break;
  case WiiUtils::UpdateResult::MissingUpdatePartition:
  case WiiUtils::UpdateResult::DiscReadFailed:
    wxMessageBox(_("The game disc does not contain any usable update information."),
                 _("Update failed"), wxOK | wxICON_ERROR);
    break;
  }
}

template <typename Callable, typename... Args>
static WiiUtils::UpdateResult ShowUpdateProgress(CFrame* frame, Callable function, Args&&... args)
{
  wxProgressDialog dialog(_("Updating"), _("Preparing to update...\nThis can take a while."), 1,
                          frame, wxPD_APP_MODAL | wxPD_AUTO_HIDE | wxPD_SMOOTH | wxPD_CAN_ABORT);

  std::future<WiiUtils::UpdateResult> result = std::async(std::launch::async, [&] {
    const WiiUtils::UpdateResult res = function(
        [&](size_t processed, size_t total, u64 title_id) {
          Core::QueueHostJob(
              [&dialog, processed, total, title_id] {
                dialog.SetRange(total);
                dialog.Update(processed, wxString::Format(_("Updating title %016" PRIx64 "...\n"
                                                            "This can take a while."),
                                                          title_id));
                dialog.Fit();
              },
              true);
          return !dialog.WasCancelled();
        },
        std::forward<Args>(args)...);
    Core::QueueHostJob([&dialog] { dialog.EndModal(0); }, true);
    return res;
  });

  dialog.ShowModal();
  return result.get();
}

void CFrame::OnPerformOnlineWiiUpdate(wxCommandEvent& event)
{
  int confirm = wxMessageBox(_("Connect to the Internet and perform an online system update?"),
                             _("System Update"), wxYES_NO, this);
  if (confirm != wxYES)
    return;

  const std::string region = GetUpdateRegionFromIdm(event.GetId());

  const WiiUtils::UpdateResult result = ShowUpdateProgress(this, WiiUtils::DoOnlineUpdate, region);
  ShowUpdateResult(result);
  wxPostEvent(GetMenuBar(), wxCommandEvent{DOLPHIN_EVT_UPDATE_LOAD_WII_MENU_ITEM});
}

void CFrame::OnPerformDiscWiiUpdate(wxCommandEvent&)
{
  const GameListItem* iso = m_game_list_ctrl->GetSelectedISO();
  if (!iso)
    return;

  const std::string file_name = iso->GetFileName();

  const WiiUtils::UpdateResult result = ShowUpdateProgress(this, WiiUtils::DoDiscUpdate, file_name);
  ShowUpdateResult(result);
  wxPostEvent(GetMenuBar(), wxCommandEvent{DOLPHIN_EVT_UPDATE_LOAD_WII_MENU_ITEM});
}

void CFrame::OnFifoPlayer(wxCommandEvent& WXUNUSED(event))
{
  if (m_fifo_player_dialog)
  {
    m_fifo_player_dialog->Show();
    m_fifo_player_dialog->SetFocus();
  }
  else
  {
    m_fifo_player_dialog = new FifoPlayerDlg(this);
  }
}

void CFrame::OnDebugger(wxCommandEvent& WXUNUSED(event))
{
  if (!m_use_debugger)
  {
    m_use_debugger = true;
    this->Maximize(true);
    m_code_window = new CCodeWindow(this, IDM_CODE_WINDOW);
    LoadIniPerspectives();
    m_code_window->Load();

    MainMenuBar* menu = (MainMenuBar*)GetMenuBar();
    menu->m_type = MainMenuBar::MenuType::Debug;
    wxMenu* toolsMenu = menu->GetMenu(menu->FindMenu(_("&Tools")));
    toolsMenu->Enable(IDM_DEBUGGER, false);
    wxMenu* options_menu = menu->GetMenu(menu->FindMenu(_("&Options")));
    {
      options_menu->AppendSeparator();

      const auto& config_instance = SConfig::GetInstance();

      auto* const boot_to_pause =
        options_menu->AppendCheckItem(IDM_BOOT_TO_PAUSE, _("Boot to Pause"),
          _("Start the game directly instead of booting to pause"));
      boot_to_pause->Check(config_instance.bBootToPause);

      auto* const automatic_start = options_menu->AppendCheckItem(
        IDM_AUTOMATIC_START, _("&Automatic Start"),
        _("Automatically load the Default ISO when Dolphin starts, or the last game you loaded,"
          " if you have not given it an elf file with the --elf command line. [This can be"
          " convenient if you are bug-testing with a certain game and want to rebuild"
          " and retry it several times, either with changes to Dolphin or if you are"
          " developing a homebrew game.]"));
      automatic_start->Check(config_instance.bAutomaticStart);

      options_menu->Append(IDM_FONT_PICKER, _("&Font..."));
    }
    wxMenu* view_menu = menu->GetMenu(menu->FindMenu(_("&View")));
    view_menu->AppendSeparator();
    {
      view_menu->AppendCheckItem(IDM_REGISTER_WINDOW, _("&Registers"));
      // i18n: This kind of "watch" is used for watching emulated memory.
      // It's not related to timekeeping devices.
      view_menu->AppendCheckItem(IDM_WATCH_WINDOW, _("&Watch"));
      view_menu->AppendCheckItem(IDM_BREAKPOINT_WINDOW, _("&Breakpoints"));
      view_menu->AppendCheckItem(IDM_MEMORY_WINDOW, _("&Memory"));
      view_menu->AppendCheckItem(IDM_JIT_WINDOW, _("&JIT"));
      view_menu->AppendCheckItem(IDM_SOUND_WINDOW, _("&Sound"));
      view_menu->AppendCheckItem(IDM_VIDEO_WINDOW, _("&Video"));
      view_menu->AppendSeparator();
    }
    {
      menu->Append(menu->CreateJITMenu(), _("&JIT"));
      menu->Append(menu->CreateDebugMenu(), _("&Debug"));
      menu->Append(menu->CreateSymbolsMenu(), _("&Symbols"));
      menu->Append(menu->CreateProfilerMenu(), _("&Profiler"));
    }
    menu->BindEvents();

    // Create toolbar
    //TODONOW RecreateToolbar();
    //if (!SConfig::GetInstance().m_InterfaceToolbar)
    //  DoToggleToolbar(false);
    // Commit
    m_mgr->Update();

    if (GetMenuBar()->IsChecked(IDM_AUTOMATIC_START))
    {
      BootGame("");
    }

    // Load perspective
    DoLoadPerspective();
    // Update controls
    UpdateGUI();
    // TODONOW   if (g_pCodeWindow)
    //  g_pCodeWindow->UpdateButtonStates();
  }
}

void CFrame::OnBruteForce(wxCommandEvent& event)
{
  if (event.GetId() == IDM_BRUTEFORCE_ALL)
  {
    SConfig::GetInstance().m_BruteforceScreenshotAll = !SConfig::GetInstance().m_BruteforceScreenshotAll;
    ARBruteForcer::ch_screenshot_all = SConfig::GetInstance().m_BruteforceScreenshotAll;
    SConfig::GetInstance().SaveSettings();

    return;
  }
  if (ARBruteForcer::ch_bruteforce)
    return;
  NOTICE_LOG(VR, "OnBruteForce");

  char result = event.GetId() - IDM_BRUTEFORCE0 + '0';
  bool running = Core::IsRunning();
  bool was_unpaused = running && Core::PauseAndLock(true);
  ARBruteForcer::ch_code += result;
  ARBruteForcer::ch_dont_save_settings = true;

  ARBruteForcer::ch_take_screenshot = 0;
  ARBruteForcer::ch_next_code = false;
  ARBruteForcer::ch_begin_search = false;
  ARBruteForcer::ch_cycles_without_snapshot = 0;
  ARBruteForcer::ch_last_search = false;
  ARBruteForcer::ch_begun = false;

  if (running)
  {
    NOTICE_LOG(VR, "running");
    std::string existing_map_file, writable_map_file, title_id_str;
    bool map_exists = CBoot::FindMapFile(&existing_map_file, &writable_map_file);
    if (!map_exists)
    {
      NOTICE_LOG(VR, "map doesn't exist, creating");
      PPCAnalyst::FindFunctions(0x80000000, 0x81800000, &g_symbolDB);
      SignatureDB db(SignatureDB::HandlerType::DSY);
      if (db.Load(File::GetSysDirectory() + TOTALDB))
        db.Apply(&g_symbolDB);
      //todo: if debugger active, call NotifyMapLoaded();
      g_symbolDB.SaveSymbolMap(writable_map_file);
    }
    ARBruteForcer::ParseMapFile(SConfig::GetInstance().GetGameID());
  }

  int position = ARBruteForcer::LoadLastPosition();
  NOTICE_LOG(VR, "position = %d", position);
  if (running && (position < 0))
  {
    NOTICE_LOG(VR, "Set render settings and save state");
    g_Config.iEFBScale = SCALE_1X;
    //g_Config.bFullscreen = false;
    g_Config.bFreeLook = true;
    g_Config.iMaxAnisotropy = 0; // 1x
    State::Save(1);
  }

  ARBruteForcer::ch_bruteforce = true;
  NOTICE_LOG(VR, "Starting");
  if (running)
    Core::PauseAndLock(false, was_unpaused);
  else
    BootGame("");
  NOTICE_LOG(VR, "Bruteforcer Started");
}

void CFrame::OnConnectWiimote(wxCommandEvent& event)
{
  const auto ios = IOS::HLE::GetIOS();
  if (!ios || SConfig::GetInstance().m_bt_passthrough_enabled)
    return;
  Core::RunAsCPUThread([&] {
    const auto bt = std::static_pointer_cast<IOS::HLE::Device::BluetoothEmu>(
        ios->GetDeviceByName("/dev/usb/oh1/57e/305"));
    const unsigned int wiimote_index = event.GetId() - IDM_CONNECT_WIIMOTE1;
    const bool is_connected = bt && bt->AccessWiiMote(wiimote_index | 0x100)->IsConnected();
    Wiimote::Connect(wiimote_index, !is_connected);
  });
}

// Toggle fullscreen. In Windows the fullscreen mode is accomplished by expanding the m_panel to
// cover the entire screen (when we render to the main window).
void CFrame::OnToggleFullscreen(wxCommandEvent& WXUNUSED(event))
{
  DoFullscreen(!RendererIsFullscreen());
}

void CFrame::OnToggleSkipIdle(wxCommandEvent& WXUNUSED(event))
{
  SConfig::GetInstance().bSkipIdle = !SConfig::GetInstance().bSkipIdle;
  SConfig::GetInstance().SaveSettings();
}

void CFrame::OnLoadStateFromFile(wxCommandEvent& WXUNUSED(event))
{
  wxString path =
      wxFileSelector(_("Select the state to load"), wxEmptyString, wxEmptyString, wxEmptyString,
                     _("All Save States (sav, s##)") +
                         wxString::Format("|*.sav;*.s??|%s", wxGetTranslation(wxALL_FILES)),
                     wxFD_OPEN | wxFD_PREVIEW | wxFD_FILE_MUST_EXIST, this);

  if (!path.IsEmpty())
    State::LoadAs(WxStrToStr(path));
}

void CFrame::OnSaveStateToFile(wxCommandEvent& WXUNUSED(event))
{
  wxString path =
      wxFileSelector(_("Select the state to save"), wxEmptyString, wxEmptyString, wxEmptyString,
                     _("All Save States (sav, s##)") +
                         wxString::Format("|*.sav;*.s??|%s", wxGetTranslation(wxALL_FILES)),
                     wxFD_SAVE, this);

  if (!path.IsEmpty())
    State::SaveAs(WxStrToStr(path));
}

void CFrame::OnLoadLastState(wxCommandEvent& event)
{
  if (Core::IsRunningAndStarted())
  {
    int id = event.GetId();
    int slot = id - IDM_LOAD_LAST_1 + 1;
    State::LoadLastSaved(slot);
  }
}

void CFrame::OnSaveFirstState(wxCommandEvent& WXUNUSED(event))
{
  if (Core::IsRunningAndStarted())
    State::SaveFirstSaved();
}

void CFrame::OnUndoLoadState(wxCommandEvent& WXUNUSED(event))
{
  if (Core::IsRunningAndStarted())
    State::UndoLoadState();
}

void CFrame::OnUndoSaveState(wxCommandEvent& WXUNUSED(event))
{
  if (Core::IsRunningAndStarted())
    State::UndoSaveState();
}

void CFrame::OnLoadState(wxCommandEvent& event)
{
  if (Core::IsRunningAndStarted())
  {
    int id = event.GetId();
    int slot = id - IDM_LOAD_SLOT_1 + 1;
    State::Load(slot);
  }
}

void CFrame::OnSaveState(wxCommandEvent& event)
{
  if (Core::IsRunningAndStarted())
  {
    int id = event.GetId();
    int slot = id - IDM_SAVE_SLOT_1 + 1;
    State::Save(slot);
  }
}

void CFrame::OnFrameSkip(wxCommandEvent& event)
{
  int amount = event.GetId() - IDM_FRAME_SKIP_0;

  Movie::SetFrameSkipping((unsigned int)amount);
  SConfig::GetInstance().m_FrameSkip = amount;
}

void CFrame::OnSelectSlot(wxCommandEvent& event)
{
  m_save_slot = event.GetId() - IDM_SELECT_SLOT_1 + 1;
  Core::DisplayMessage(StringFromFormat("Selected slot %d - %s", m_save_slot,
                                        State::GetInfoStringOfSlot(m_save_slot, false).c_str()),
                       2500);
}

void CFrame::OnLoadCurrentSlot(wxCommandEvent& event)
{
  if (Core::IsRunningAndStarted())
  {
    State::Load(m_save_slot);
  }
}

void CFrame::OnSaveCurrentSlot(wxCommandEvent& event)
{
  if (Core::IsRunningAndStarted())
  {
    State::Save(m_save_slot);
  }
}

// GUI
// ---------------------

// Update the enabled/disabled status
void CFrame::UpdateGUI()
{
  // Save status
  bool Initialized = Core::IsRunning();
  bool Running = Core::GetState() == Core::State::Running;
  bool Paused = Core::GetState() == Core::State::Paused;
  bool Stopping = Core::GetState() == Core::State::Stopping;

  GetToolBar()->Refresh(false);
  GetMenuBar()->Refresh(false);

  // File
  GetMenuBar()->FindItem(wxID_OPEN)->Enable(!Initialized);
  GetMenuBar()->FindItem(IDM_DRIVES)->Enable(!Initialized);
  GetMenuBar()->FindItem(wxID_REFRESH)->Enable(!Initialized);

  // Emulation
  GetMenuBar()->FindItem(IDM_STOP)->Enable(Running || Paused);
  GetMenuBar()->FindItem(IDM_RESET)->Enable(Running || Paused);
  GetMenuBar()->FindItem(IDM_RECORD)->Enable(!Movie::IsRecordingInput());
  GetMenuBar()->FindItem(IDM_PLAY_RECORD)->Enable(!Initialized);
  GetMenuBar()->FindItem(IDM_STOP_RECORD)->Enable(Movie::IsMovieActive());
  GetMenuBar()->FindItem(IDM_RECORD_EXPORT)->Enable(Movie::IsMovieActive());
  GetMenuBar()->FindItem(IDM_FRAMESTEP)->Enable(Running || Paused);
  GetMenuBar()->FindItem(IDM_SCREENSHOT)->Enable(Running || Paused);
  GetMenuBar()->FindItem(IDM_TOGGLE_FULLSCREEN)->Enable(Running || Paused);
  GetMenuBar()->FindItem(IDM_LOAD_STATE)->Enable(Initialized);
  GetMenuBar()->FindItem(IDM_SAVE_STATE)->Enable(Initialized);
  // Misc
  GetMenuBar()->FindItem(IDM_CHANGE_DISC)->Enable(Initialized);
  GetMenuBar()->FindItem(IDM_EJECT_DISC)->Enable(Initialized);
  GetMenuBar()
      ->FindItem(IDM_LOAD_GC_IPL_JAP)
      ->Enable(!Initialized && File::Exists(SConfig::GetInstance().GetBootROMPath(JAP_DIR)));
  GetMenuBar()
      ->FindItem(IDM_LOAD_GC_IPL_USA)
      ->Enable(!Initialized && File::Exists(SConfig::GetInstance().GetBootROMPath(USA_DIR)));
  GetMenuBar()
      ->FindItem(IDM_LOAD_GC_IPL_EUR)
      ->Enable(!Initialized && File::Exists(SConfig::GetInstance().GetBootROMPath(EUR_DIR)));

  // Tools
  GetMenuBar()->FindItem(IDM_CHEATS)->Enable(SConfig::GetInstance().bEnableCheats);

  const auto ios = IOS::HLE::GetIOS();
  const auto bt = ios ? std::static_pointer_cast<IOS::HLE::Device::BluetoothEmu>(
                            ios->GetDeviceByName("/dev/usb/oh1/57e/305")) :
                        nullptr;
  bool ShouldEnableWiimotes = Running && bt && !SConfig::GetInstance().m_bt_passthrough_enabled;
  GetMenuBar()->FindItem(IDM_CONNECT_WIIMOTE1)->Enable(ShouldEnableWiimotes);
  GetMenuBar()->FindItem(IDM_CONNECT_WIIMOTE2)->Enable(ShouldEnableWiimotes);
  GetMenuBar()->FindItem(IDM_CONNECT_WIIMOTE3)->Enable(ShouldEnableWiimotes);
  GetMenuBar()->FindItem(IDM_CONNECT_WIIMOTE4)->Enable(ShouldEnableWiimotes);
  GetMenuBar()->FindItem(IDM_CONNECT_BALANCEBOARD)->Enable(ShouldEnableWiimotes);
  if (ShouldEnableWiimotes)
  {
    Core::RunAsCPUThread([&] {
      GetMenuBar()->FindItem(IDM_CONNECT_WIIMOTE1)->Check(bt->AccessWiiMote(0x0100)->IsConnected());
      GetMenuBar()->FindItem(IDM_CONNECT_WIIMOTE2)->Check(bt->AccessWiiMote(0x0101)->IsConnected());
      GetMenuBar()->FindItem(IDM_CONNECT_WIIMOTE3)->Check(bt->AccessWiiMote(0x0102)->IsConnected());
      GetMenuBar()->FindItem(IDM_CONNECT_WIIMOTE4)->Check(bt->AccessWiiMote(0x0103)->IsConnected());
      GetMenuBar()
          ->FindItem(IDM_CONNECT_BALANCEBOARD)
          ->Check(bt->AccessWiiMote(0x0104)->IsConnected());
    });
  }

  GetMenuBar()->FindItem(IDM_RECORD_READ_ONLY)->Enable(Running || Paused);

  if (!Initialized && !m_is_game_loading)
  {
    if (m_game_list_ctrl->IsEnabled())
    {
      // Prepare to load Default ISO, enable play button
      if (!SConfig::GetInstance().m_strDefaultISO.empty())
      {
        GetToolBar()->EnableTool(IDM_PLAY, true);
        GetMenuBar()->FindItem(IDM_PLAY)->Enable();
        GetMenuBar()->FindItem(IDM_RECORD)->Enable();
        GetMenuBar()->FindItem(IDM_PLAY_RECORD)->Enable();
      }
      // Prepare to load last selected file, enable play button
      else if (!SConfig::GetInstance().m_LastFilename.empty() &&
               File::Exists(SConfig::GetInstance().m_LastFilename))
      {
        GetToolBar()->EnableTool(IDM_PLAY, true);
        GetMenuBar()->FindItem(IDM_PLAY)->Enable();
        GetMenuBar()->FindItem(IDM_RECORD)->Enable();
        GetMenuBar()->FindItem(IDM_PLAY_RECORD)->Enable();
      }
      else
      {
        // No game has been selected yet, disable play button
        GetToolBar()->EnableTool(IDM_PLAY, false);
        GetMenuBar()->FindItem(IDM_PLAY)->Enable(false);
        GetMenuBar()->FindItem(IDM_RECORD)->Enable(false);
        GetMenuBar()->FindItem(IDM_PLAY_RECORD)->Enable(false);
      }
    }

    // Game has not started, show game list
    if (!m_game_list_ctrl->IsShown())
    {
      m_game_list_ctrl->Enable();
      m_game_list_ctrl->Show();
    }
    // Game has been selected but not started, enable play button
    if (m_game_list_ctrl->GetSelectedISO() != nullptr && m_game_list_ctrl->IsEnabled())
    {
      GetToolBar()->EnableTool(IDM_PLAY, true);
      GetMenuBar()->FindItem(IDM_PLAY)->Enable();
      GetMenuBar()->FindItem(IDM_RECORD)->Enable();
      GetMenuBar()->FindItem(IDM_PLAY_RECORD)->Enable();
    }

    // Reset the stop playing/recording input menu item
    GetMenuBar()->FindItem(IDM_STOP_RECORD)->SetItemLabel(_("Stop Playing/Recording Input"));
  }
  else if (Initialized)
  {
    // Game has been loaded, enable the pause button
    GetToolBar()->EnableTool(IDM_PLAY, !Stopping);
    GetMenuBar()->FindItem(IDM_PLAY)->Enable(!Stopping);

    // Reset game loading flag
    m_is_game_loading = false;

    // Rename the stop playing/recording menu item depending on current movie state
    if (Movie::IsRecordingInput())
      GetMenuBar()->FindItem(IDM_STOP_RECORD)->SetItemLabel(_("Stop Recording Input"));
    else if (Movie::IsPlayingInput())
      GetMenuBar()->FindItem(IDM_STOP_RECORD)->SetItemLabel(_("Stop Playing Input"));
    else
      GetMenuBar()->FindItem(IDM_STOP_RECORD)->SetItemLabel(_("Stop Playing/Recording Input"));
  }

  GetToolBar()->Refresh(false);

  // Commit changes to manager
  m_mgr->Update();

  // Update non-modal windows
  if (m_cheats_window)
  {
    if (SConfig::GetInstance().bEnableCheats)
      m_cheats_window->UpdateGUI();
    else
      m_cheats_window->Hide();
  }
}

void CFrame::GameListRefresh()
{
  wxCommandEvent event{DOLPHIN_EVT_REFRESH_GAMELIST, GetId()};
  event.SetEventObject(this);
  wxPostEvent(m_game_list_ctrl, event);
}

void CFrame::GameListRescan(bool purge_cache)
{
  wxCommandEvent event{DOLPHIN_EVT_RESCAN_GAMELIST, GetId()};
  event.SetEventObject(this);
  event.SetInt(purge_cache ? 1 : 0);
  wxPostEvent(m_game_list_ctrl, event);
}

void CFrame::GameListChanged(wxCommandEvent& event)
{
  switch (event.GetId())
  {
  case IDM_LIST_WII:
    SConfig::GetInstance().m_ListWii = event.IsChecked();
    break;
  case IDM_LIST_GC:
    SConfig::GetInstance().m_ListGC = event.IsChecked();
    break;
  case IDM_LIST_WAD:
    SConfig::GetInstance().m_ListWad = event.IsChecked();
    break;
  case IDM_LIST_ELFDOL:
    SConfig::GetInstance().m_ListElfDol = event.IsChecked();
    break;
  case IDM_LIST_JAP:
    SConfig::GetInstance().m_ListJap = event.IsChecked();
    break;
  case IDM_LIST_PAL:
    SConfig::GetInstance().m_ListPal = event.IsChecked();
    break;
  case IDM_LIST_USA:
    SConfig::GetInstance().m_ListUsa = event.IsChecked();
    break;
  case IDM_LIST_AUSTRALIA:
    SConfig::GetInstance().m_ListAustralia = event.IsChecked();
    break;
  case IDM_LIST_FRANCE:
    SConfig::GetInstance().m_ListFrance = event.IsChecked();
    break;
  case IDM_LIST_GERMANY:
    SConfig::GetInstance().m_ListGermany = event.IsChecked();
    break;
  case IDM_LIST_ITALY:
    SConfig::GetInstance().m_ListItaly = event.IsChecked();
    break;
  case IDM_LIST_KOREA:
    SConfig::GetInstance().m_ListKorea = event.IsChecked();
    break;
  case IDM_LIST_NETHERLANDS:
    SConfig::GetInstance().m_ListNetherlands = event.IsChecked();
    break;
  case IDM_LIST_RUSSIA:
    SConfig::GetInstance().m_ListRussia = event.IsChecked();
    break;
  case IDM_LIST_SPAIN:
    SConfig::GetInstance().m_ListSpain = event.IsChecked();
    break;
  case IDM_LIST_TAIWAN:
    SConfig::GetInstance().m_ListTaiwan = event.IsChecked();
    break;
  case IDM_LIST_WORLD:
    SConfig::GetInstance().m_ListWorld = event.IsChecked();
    break;
  case IDM_LIST_UNKNOWN:
    SConfig::GetInstance().m_ListUnknown = event.IsChecked();
    break;
  case IDM_LIST_DRIVES:
    SConfig::GetInstance().m_ListDrives = event.IsChecked();
    break;
  case IDM_PURGE_GAME_LIST_CACHE:
    std::vector<std::string> filenames =
        Common::DoFileSearch({File::GetUserPath(D_CACHE_IDX)}, {".cache"});

    for (const std::string& filename : filenames)
    {
      File::Delete(filename);
    }
    // Do rescan after cache has been cleared
    GameListRescan(true);
    return;
  }

  GameListRefresh();
}

// Enable and disable the toolbar
void CFrame::OnToggleToolbar(wxCommandEvent& event)
{
  SConfig::GetInstance().m_InterfaceToolbar = event.IsChecked();
  DoToggleToolbar(event.IsChecked());
}
void CFrame::DoToggleToolbar(bool _show)
{
  GetToolBar()->Show(_show);
  m_mgr->Update();
}

// Enable and disable the status bar
void CFrame::OnToggleStatusbar(wxCommandEvent& event)
{
  SConfig::GetInstance().m_InterfaceStatusbar = event.IsChecked();

  GetStatusBar()->Show(event.IsChecked());

  SendSizeEvent();
}

void CFrame::OnChangeColumnsVisible(wxCommandEvent& event)
{
  switch (event.GetId())
  {
  case IDM_SHOW_SYSTEM:
    SConfig::GetInstance().m_showSystemColumn = !SConfig::GetInstance().m_showSystemColumn;
    break;
  case IDM_SHOW_BANNER:
    SConfig::GetInstance().m_showBannerColumn = !SConfig::GetInstance().m_showBannerColumn;
    break;
  case IDM_SHOW_TITLE:
    SConfig::GetInstance().m_showTitleColumn = !SConfig::GetInstance().m_showTitleColumn;
    break;
  case IDM_SHOW_MAKER:
    SConfig::GetInstance().m_showMakerColumn = !SConfig::GetInstance().m_showMakerColumn;
    break;
  case IDM_SHOW_FILENAME:
    SConfig::GetInstance().m_showFileNameColumn = !SConfig::GetInstance().m_showFileNameColumn;
    break;
  case IDM_SHOW_ID:
    SConfig::GetInstance().m_showIDColumn = !SConfig::GetInstance().m_showIDColumn;
    break;
  case IDM_SHOW_REGION:
    SConfig::GetInstance().m_showRegionColumn = !SConfig::GetInstance().m_showRegionColumn;
    break;
  case IDM_SHOW_SIZE:
    SConfig::GetInstance().m_showSizeColumn = !SConfig::GetInstance().m_showSizeColumn;
    break;
  case IDM_SHOW_STATE:
    SConfig::GetInstance().m_showStateColumn = !SConfig::GetInstance().m_showStateColumn;
    break;
  case IDM_SHOW_VR_STATE:
    SConfig::GetInstance().m_showVRStateColumn = !SConfig::GetInstance().m_showVRStateColumn;
    break;
  default:
    return;
  }
  GameListRefresh();
  SConfig::GetInstance().SaveSettings();
}
