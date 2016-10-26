// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <array>
#include <cstdarg>
#include <cstdio>
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
#include <wx/thread.h>
#include <wx/toolbar.h>
#include <wx/toplevel.h>

#include "Common/CDUtils.h"
#include "Common/CommonTypes.h"
#include "Common/FileSearch.h"
#include "Common/FileUtil.h"
#include "Common/NandPaths.h"
#include "Common/StringUtil.h"

#include "Core/BootManager.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/HW/CPU.h"
#include "Core/HW/DVDInterface.h"
#include "Core/HW/GCKeyboard.h"
#include "Core/HW/GCPad.h"
#include "Core/HW/ProcessorInterface.h"
#include "Core/HW/SI_Device.h"
#include "Core/HW/WiiSaveCrypted.h"
#include "Core/HW/Wiimote.h"
#include "Core/Host.h"
#include "Core/HotkeyManager.h"
#include "Core/IPC_HLE/WII_IPC_HLE.h"
#include "Core/IPC_HLE/WII_IPC_HLE_Device_stm.h"
#include "Core/IPC_HLE/WII_IPC_HLE_Device_usb_bt_emu.h"
#include "Core/IPC_HLE/WII_IPC_HLE_WiiMote.h"
#include "Core/Movie.h"
#include "Core/PowerPC/PPCSymbolDB.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/State.h"

#include "DiscIO/NANDContentLoader.h"

#include "DolphinWX/AboutDolphin.h"
#include "DolphinWX/Cheats/CheatsWindow.h"
#include "DolphinWX/Config/ConfigMain.h"
#include "DolphinWX/ControllerConfigDiag.h"
#include "DolphinWX/Debugger/BreakpointWindow.h"
#include "DolphinWX/Debugger/CodeWindow.h"
#include "DolphinWX/Debugger/WatchWindow.h"
#include "DolphinWX/FifoPlayerDlg.h"
#include "DolphinWX/Frame.h"
#include "DolphinWX/GameListCtrl.h"
#include "DolphinWX/Globals.h"
#include "DolphinWX/ISOFile.h"
#include "DolphinWX/InputConfigDiag.h"
#include "DolphinWX/LogWindow.h"
#include "DolphinWX/MainMenuBar.h"
#include "DolphinWX/MemcardManager.h"
#include "DolphinWX/NetPlay/NetPlaySetupFrame.h"
#include "DolphinWX/NetPlay/NetWindow.h"
#include "DolphinWX/TASInputDlg.h"
#include "DolphinWX/WXInputBase.h"
#include "DolphinWX/WxUtils.h"

#include "InputCommon/ControllerInterface/ControllerInterface.h"

#include "VideoCommon/RenderBase.h"
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

const wxSize& CFrame::GetToolbarBitmapSize() const
{
  return m_toolbar_bitmap_size;
}

// Create menu items
// ---------------------
wxMenuBar* CFrame::CreateMenuBar() const
{
  const auto menu_type =
      UseDebugger ? MainMenuBar::MenuType::Debug : MainMenuBar::MenuType::Regular;

  return new MainMenuBar{menu_type};
}

void CFrame::BindMenuBarEvents()
{
  // File menu
  Bind(wxEVT_MENU, &CFrame::OnOpen, this, wxID_OPEN);
  Bind(wxEVT_MENU, &CFrame::OnChangeDisc, this, IDM_CHANGE_DISC);
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
  Bind(wxEVT_MENU, &CFrame::OnConfigHotkey, this, IDM_CONFIG_HOTKEYS);

  // Tools menu
  Bind(wxEVT_MENU, &CFrame::OnMemcard, this, IDM_MEMCARD);
  Bind(wxEVT_MENU, &CFrame::OnImportSave, this, IDM_IMPORT_SAVE);
  Bind(wxEVT_MENU, &CFrame::OnExportAllSaves, this, IDM_EXPORT_ALL_SAVE);
  Bind(wxEVT_MENU, &CFrame::OnShowCheatsWindow, this, IDM_CHEATS);
  Bind(wxEVT_MENU, &CFrame::OnNetPlay, this, IDM_NETPLAY);
  Bind(wxEVT_MENU, &CFrame::OnInstallWAD, this, IDM_MENU_INSTALL_WAD);
  Bind(wxEVT_MENU, &CFrame::OnLoadWiiMenu, this, IDM_LOAD_WII_MENU);
  Bind(wxEVT_MENU, &CFrame::OnFifoPlayer, this, IDM_FIFOPLAYER);
  Bind(wxEVT_MENU, &CFrame::OnConnectWiimote, this, IDM_CONNECT_WIIMOTE1, IDM_CONNECT_BALANCEBOARD);

  // View menu
  Bind(wxEVT_MENU, &CFrame::OnToggleToolbar, this, IDM_TOGGLE_TOOLBAR);
  Bind(wxEVT_MENU, &CFrame::OnToggleStatusbar, this, IDM_TOGGLE_STATUSBAR);
  Bind(wxEVT_MENU, &CFrame::OnToggleWindow, this, IDM_LOG_WINDOW, IDM_VIDEO_WINDOW);
  Bind(wxEVT_MENU, &CFrame::GameListChanged, this, IDM_LIST_WAD, IDM_LIST_DRIVES);
  Bind(wxEVT_MENU, &CFrame::GameListChanged, this, IDM_PURGE_GAME_LIST_CACHE);
  Bind(wxEVT_MENU, &CFrame::OnChangeColumnsVisible, this, IDM_SHOW_SYSTEM, IDM_SHOW_STATE);

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

  // Help menu
  Bind(wxEVT_MENU, &CFrame::OnHelp, this, IDM_HELP_WEBSITE);
  Bind(wxEVT_MENU, &CFrame::OnHelp, this, IDM_HELP_ONLINE_DOCS);
  Bind(wxEVT_MENU, &CFrame::OnHelp, this, IDM_HELP_GITHUB);
  Bind(wxEVT_MENU, &CFrame::OnHelp, this, wxID_ABOUT);
}

// Create toolbar items
// ---------------------
void CFrame::PopulateToolbar(wxToolBar* ToolBar)
{
  WxUtils::AddToolbarButton(ToolBar, wxID_OPEN, _("Open"), m_Bitmaps[Toolbar_FileOpen],
                            _("Open file..."));
  WxUtils::AddToolbarButton(ToolBar, wxID_REFRESH, _("Refresh"), m_Bitmaps[Toolbar_Refresh],
                            _("Refresh game list"));
  ToolBar->AddSeparator();
  WxUtils::AddToolbarButton(ToolBar, IDM_PLAY, _("Play"), m_Bitmaps[Toolbar_Play], _("Play"));
  WxUtils::AddToolbarButton(ToolBar, IDM_STOP, _("Stop"), m_Bitmaps[Toolbar_Stop], _("Stop"));
  WxUtils::AddToolbarButton(ToolBar, IDM_TOGGLE_FULLSCREEN, _("FullScr"),
                            m_Bitmaps[Toolbar_FullScreen], _("Toggle fullscreen"));
  WxUtils::AddToolbarButton(ToolBar, IDM_SCREENSHOT, _("ScrShot"), m_Bitmaps[Toolbar_Screenshot],
                            _("Take screenshot"));
  ToolBar->AddSeparator();
  WxUtils::AddToolbarButton(ToolBar, wxID_PREFERENCES, _("Config"), m_Bitmaps[Toolbar_ConfigMain],
                            _("Configure..."));
  WxUtils::AddToolbarButton(ToolBar, IDM_CONFIG_GFX_BACKEND, _("Graphics"),
                            m_Bitmaps[Toolbar_ConfigGFX], _("Graphics settings"));
  WxUtils::AddToolbarButton(ToolBar, IDM_CONFIG_CONTROLLERS, _("Controllers"),
                            m_Bitmaps[Toolbar_Controller], _("Controller settings"));
}

// Delete and recreate the toolbar
void CFrame::RecreateToolbar()
{
  static constexpr long TOOLBAR_STYLE = wxTB_DEFAULT_STYLE | wxTB_TEXT | wxTB_FLAT;

  if (m_ToolBar != nullptr)
  {
    m_ToolBar->Destroy();
    m_ToolBar = nullptr;
  }

  m_ToolBar = CreateToolBar(TOOLBAR_STYLE, wxID_ANY);
  m_ToolBar->SetToolBitmapSize(m_toolbar_bitmap_size);

  if (g_pCodeWindow)
  {
    g_pCodeWindow->PopulateToolbar(m_ToolBar);
    m_ToolBar->AddSeparator();
  }

  PopulateToolbar(m_ToolBar);
  // after adding the buttons to the toolbar, must call Realize() to reflect
  // the changes
  m_ToolBar->Realize();

  UpdateGUI();
}

void CFrame::InitBitmaps()
{
  static constexpr std::array<const char* const, EToolbar_Max> s_image_names{
      {"open", "refresh", "play", "stop", "pause", "screenshot", "fullscreen", "config", "graphics",
       "classic"}};
  for (std::size_t i = 0; i < s_image_names.size(); ++i)
    m_Bitmaps[i] = WxUtils::LoadScaledThemeBitmap(s_image_names[i], this, m_toolbar_bitmap_size);

  // Update in case the bitmap has been updated
  if (m_ToolBar != nullptr)
    RecreateToolbar();
}

void CFrame::OpenGeneralConfiguration(int tab)
{
  CConfigMain config_main(this);
  if (tab > -1)
    config_main.SetSelectedTab(tab);

  HotkeyManagerEmu::Enable(false);
  if (config_main.ShowModal() == wxID_OK)
    UpdateGameList();
  HotkeyManagerEmu::Enable(true);

  UpdateGUI();
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

  if (Core::GetState() != Core::CORE_UNINITIALIZED)
    return;

  // Start filename if non empty.
  // Start the selected ISO, or try one of the saved paths.
  // If all that fails, ask to add a dir and don't boot
  if (bootfile.empty())
  {
    if (m_GameListCtrl->GetSelectedISO() != nullptr)
    {
      if (m_GameListCtrl->GetSelectedISO()->IsValid())
        bootfile = m_GameListCtrl->GetSelectedISO()->GetFileName();
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
        m_GameListCtrl->BrowseForDirectory();
        return;
      }
    }
  }
  if (!bootfile.empty())
  {
    StartGame(bootfile);
    if (UseDebugger && g_pCodeWindow)
    {
      if (g_pCodeWindow->HasPanel<CWatchWindow>())
        g_pCodeWindow->GetPanel<CWatchWindow>()->LoadAll();
      if (g_pCodeWindow->HasPanel<CBreakPointWindow>())
        g_pCodeWindow->GetPanel<CBreakPointWindow>()->LoadAll();
    }
  }
}

// Open file to boot
void CFrame::OnOpen(wxCommandEvent& WXUNUSED(event))
{
  if (Core::GetState() == Core::CORE_UNINITIALIZED)
    DoOpen(true);
}

void CFrame::DoOpen(bool Boot)
{
  std::string currentDir = File::GetCurrentDir();

  wxString path = wxFileSelector(
      _("Select the file to load"), wxEmptyString, wxEmptyString, wxEmptyString,
      _("All GC/Wii files (elf, dol, gcm, iso, wbfs, ciso, gcz, wad)") +
          wxString::Format("|*.elf;*.dol;*.gcm;*.iso;*.wbfs;*.ciso;*.gcz;*.wad;*.dff;*.tmd|%s",
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
    DVDInterface::ChangeDiscAsHost(WxStrToStr(path));
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
    if (SConfig::GetInstance().m_SIDevice[i] != SIDEVICE_NONE &&
        SConfig::GetInstance().m_SIDevice[i] != SIDEVICE_GC_GBA)
    {
      g_TASInputDlg[i]->CreateGCLayout();
      g_TASInputDlg[i]->Show();
      g_TASInputDlg[i]->SetTitle(wxString::Format(_("TAS Input - Controller %d"), i + 1));
    }

    if (g_wiimote_sources[i] == WIIMOTE_SRC_EMU &&
        !(Core::IsRunning() && !SConfig::GetInstance().bWii))
    {
      g_TASInputDlg[i + 4]->CreateWiiLayout(i);
      g_TASInputDlg[i + 4]->Show();
      g_TASInputDlg[i + 4]->SetTitle(wxString::Format(_("TAS Input - Wii Remote %d"), i + 1));
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
  bool wasPaused = (Core::GetState() == Core::CORE_PAUSE);

  Movie::DoFrameStep();

  bool isPaused = (Core::GetState() == Core::CORE_PAUSE);
  if (isPaused && !wasPaused)  // don't update on unpause, otherwise the status would be wrong when
                               // pausing next frame
    UpdateGUI();
}

void CFrame::OnChangeDisc(wxCommandEvent& WXUNUSED(event))
{
  DoOpen(false);
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
    if (SIDevice_IsGCController(SConfig::GetInstance().m_SIDevice[i]))
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
      wxFileSelector(_("Select The Recording File"), wxEmptyString, wxEmptyString, wxEmptyString,
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

void CFrame::OnRecordExport(wxCommandEvent& WXUNUSED(event))
{
  DoRecordingSave();
}

void CFrame::OnPlay(wxCommandEvent& WXUNUSED(event))
{
  if (Core::IsRunning())
  {
    // Core is initialized and emulator is running
    if (UseDebugger)
    {
      CPU::EnableStepping(!CPU::IsStepping());

      wxThread::Sleep(20);
      g_pCodeWindow->JumpToAddress(PC);
      g_pCodeWindow->Repopulate();
      // Update toolbar with Play/Pause status
      UpdateGUI();
    }
    else
    {
      DoPause();
    }
  }
  else
  {
    // Core is uninitialized, start the game
    BootGame("");
  }
}

void CFrame::OnRenderParentClose(wxCloseEvent& event)
{
  // Before closing the window we need to shut down the emulation core.
  // We'll try to close this window again once that is done.
  if (Core::GetState() != Core::CORE_UNINITIALIZED)
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
  if (Core::GetState() != Core::CORE_UNINITIALIZED && !RendererIsFullscreen() &&
      !m_RenderFrame->IsMaximized() && !m_RenderFrame->IsIconized())
  {
    SConfig::GetInstance().iRenderWindowXPos = m_RenderFrame->GetPosition().x;
    SConfig::GetInstance().iRenderWindowYPos = m_RenderFrame->GetPosition().y;
  }
  event.Skip();
}

void CFrame::OnRenderParentResize(wxSizeEvent& event)
{
  if (Core::GetState() != Core::CORE_UNINITIALIZED)
  {
    int width, height;
    if (!SConfig::GetInstance().bRenderToMain && !RendererIsFullscreen() &&
        !m_RenderFrame->IsMaximized() && !m_RenderFrame->IsIconized())
    {
      m_RenderFrame->GetClientSize(&width, &height);
      SConfig::GetInstance().iRenderWindowWidth = width;
      SConfig::GetInstance().iRenderWindowHeight = height;
    }
    m_LogWindow->Refresh();
    m_LogWindow->Update();

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
  else
  {
    // Change to default resolution
    ChangeDisplaySettings(nullptr, CDS_FULLSCREEN);
  }
#elif defined(HAVE_XRANDR) && HAVE_XRANDR
  if (SConfig::GetInstance().strFullscreenResolution != "Auto")
    m_XRRConfig->ToggleDisplayMode(bFullscreen);
#endif
}

// Prepare the GUI to start the game.
void CFrame::StartGame(const std::string& filename)
{
  if (m_bGameLoading)
    return;
  m_bGameLoading = true;

  if (m_ToolBar)
    m_ToolBar->EnableTool(IDM_PLAY, false);
  GetMenuBar()->FindItem(IDM_PLAY)->Enable(false);

  if (SConfig::GetInstance().bRenderToMain)
  {
    // Game has been started, hide the game list
    m_GameListCtrl->Disable();
    m_GameListCtrl->Hide();

    m_RenderParent = m_Panel;
    m_RenderFrame = this;
    if (SConfig::GetInstance().bKeepWindowOnTop)
      m_RenderFrame->SetWindowStyle(m_RenderFrame->GetWindowStyle() | wxSTAY_ON_TOP);
    else
      m_RenderFrame->SetWindowStyle(m_RenderFrame->GetWindowStyle() & ~wxSTAY_ON_TOP);

    // No, I really don't want TAB_TRAVERSAL being set behind my back,
    // thanks.  (Note that calling DisableSelfFocus would prevent this flag
    // from being set for new children, but wouldn't reset the existing
    // flag.)
    m_RenderParent->SetWindowStyle(m_RenderParent->GetWindowStyle() & ~wxTAB_TRAVERSAL);
  }
  else
  {
    wxRect window_geometry(
        SConfig::GetInstance().iRenderWindowXPos, SConfig::GetInstance().iRenderWindowYPos,
        SConfig::GetInstance().iRenderWindowWidth, SConfig::GetInstance().iRenderWindowHeight);
    // Set window size in framebuffer pixels since the 3D rendering will be operating at
    // that level.
    wxSize default_size{wxSize(640, 480) * (1.0 / GetContentScaleFactor())};
    m_RenderFrame = new CRenderFrame(this, wxID_ANY, _("Dolphin"), wxDefaultPosition, default_size);

    // Convert ClientSize coordinates to frame sizes.
    wxSize decoration_fudge = m_RenderFrame->GetSize() - m_RenderFrame->GetClientSize();
    default_size += decoration_fudge;
    if (!window_geometry.IsEmpty())
      window_geometry.SetSize(window_geometry.GetSize() + decoration_fudge);

    WxUtils::SetWindowSizeAndFitToScreen(m_RenderFrame, window_geometry.GetPosition(),
                                         window_geometry.GetSize(), default_size);

    if (SConfig::GetInstance().bKeepWindowOnTop)
      m_RenderFrame->SetWindowStyle(m_RenderFrame->GetWindowStyle() | wxSTAY_ON_TOP);
    else
      m_RenderFrame->SetWindowStyle(m_RenderFrame->GetWindowStyle() & ~wxSTAY_ON_TOP);

    m_RenderFrame->SetBackgroundColour(*wxBLACK);
    m_RenderFrame->Bind(wxEVT_CLOSE_WINDOW, &CFrame::OnRenderParentClose, this);
    m_RenderFrame->Bind(wxEVT_ACTIVATE, &CFrame::OnActive, this);
    m_RenderFrame->Bind(wxEVT_MOVE, &CFrame::OnRenderParentMove, this);
#ifdef _WIN32
    // The renderer should use a top-level window for exclusive fullscreen support.
    m_RenderParent = m_RenderFrame;
#else
    // To capture key events on Linux and Mac OS X the frame needs at least one child.
    m_RenderParent = new wxPanel(m_RenderFrame, IDM_MPANEL, wxDefaultPosition, wxDefaultSize, 0);
#endif

    m_RenderFrame->Show();
  }

#if defined(__APPLE__)
  m_RenderFrame->EnableFullScreenView(true);
#endif

  wxBusyCursor hourglass;

  DoFullscreen(SConfig::GetInstance().bFullscreen);

  if (!BootManager::BootCore(filename))
  {
    DoFullscreen(false);
    // Destroy the renderer frame when not rendering to main
    if (!SConfig::GetInstance().bRenderToMain)
      m_RenderFrame->Destroy();
    m_RenderFrame = nullptr;
    m_RenderParent = nullptr;
    m_bGameLoading = false;
    UpdateGUI();
  }
  else
  {
#if defined(HAVE_X11) && HAVE_X11
    if (SConfig::GetInstance().bDisableScreenSaver)
      X11Utils::InhibitScreensaver(X11Utils::XDisplayFromHandle(GetHandle()),
                                   X11Utils::XWindowFromHandle(GetHandle()), true);
#endif

#ifdef _WIN32
    // Prevents Windows from sleeping, turning off the display, or idling
    EXECUTION_STATE shouldScreenSave =
        SConfig::GetInstance().bDisableScreenSaver ? ES_DISPLAY_REQUIRED : 0;
    SetThreadExecutionState(ES_CONTINUOUS | shouldScreenSave | ES_SYSTEM_REQUIRED);
#endif

    m_RenderParent->SetFocus();

    wxTheApp->Bind(wxEVT_KEY_DOWN, &CFrame::OnKeyDown, this);
    wxTheApp->Bind(wxEVT_RIGHT_DOWN, &CFrame::OnMouse, this);
    wxTheApp->Bind(wxEVT_RIGHT_UP, &CFrame::OnMouse, this);
    wxTheApp->Bind(wxEVT_MIDDLE_DOWN, &CFrame::OnMouse, this);
    wxTheApp->Bind(wxEVT_MIDDLE_UP, &CFrame::OnMouse, this);
    wxTheApp->Bind(wxEVT_MOTION, &CFrame::OnMouse, this);
    wxTheApp->Bind(wxEVT_SET_FOCUS, &CFrame::OnFocusChange, this);
    wxTheApp->Bind(wxEVT_KILL_FOCUS, &CFrame::OnFocusChange, this);
    m_RenderParent->Bind(wxEVT_SIZE, &CFrame::OnRenderParentResize, this);
  }
}

void CFrame::OnBootDrive(wxCommandEvent& event)
{
  BootGame(drives[event.GetId() - IDM_DRIVE1]);
}

// Refresh the file list and browse for a favorites directory
void CFrame::OnRefresh(wxCommandEvent& WXUNUSED(event))
{
  UpdateGameList();
}

// Create screenshot
void CFrame::OnScreenshot(wxCommandEvent& WXUNUSED(event))
{
  Core::SaveScreenShot();
}

// Pause the emulation
void CFrame::DoPause()
{
  if (Core::GetState() == Core::CORE_RUN)
  {
    Core::SetState(Core::CORE_PAUSE);
    if (SConfig::GetInstance().bHideCursor)
      m_RenderParent->SetCursor(wxNullCursor);
    Core::UpdateTitle();
  }
  else
  {
    Core::SetState(Core::CORE_RUN);
    if (SConfig::GetInstance().bHideCursor && RendererHasFocus())
      m_RenderParent->SetCursor(wxCURSOR_BLANK);
  }
  UpdateGUI();
}

// Stop the emulation
void CFrame::DoStop()
{
  if (!Core::IsRunningAndStarted())
    return;
  if (m_confirmStop)
    return;

  // don't let this function run again until it finishes, or is aborted.
  m_confirmStop = true;

  m_bGameLoading = false;
  if (Core::GetState() != Core::CORE_UNINITIALIZED || m_RenderParent != nullptr)
  {
#if defined __WXGTK__
    wxMutexGuiLeave();
    std::lock_guard<std::recursive_mutex> lk(keystate_lock);
    wxMutexGuiEnter();
#endif
    // Ask for confirmation in case the user accidentally clicked Stop / Escape
    if (SConfig::GetInstance().bConfirmStop)
    {
      // Exit fullscreen to ensure it does not cover the stop dialog.
      DoFullscreen(false);

      // Pause the state during confirmation and restore it afterwards
      Core::EState state = Core::GetState();

      // Do not pause if netplay is running as CPU thread might be blocked
      // waiting on inputs
      bool should_pause = !NetPlayDialog::GetNetPlayClient();

      // If exclusive fullscreen is not enabled then we can pause the emulation
      // before we've exited fullscreen. If not then we need to exit fullscreen first.
      should_pause =
          should_pause && (!RendererIsFullscreen() || !g_Config.ExclusiveFullscreenEnabled() ||
                           SConfig::GetInstance().bRenderToMain);

      if (should_pause)
      {
        Core::SetState(Core::CORE_PAUSE);
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

        m_confirmStop = false;
        return;
      }
    }

    const auto& stm = WII_IPC_HLE_Interface::GetDeviceByName("/dev/stm/eventhook");
    if (!m_tried_graceful_shutdown && stm &&
        std::static_pointer_cast<CWII_IPC_HLE_Device_stm_eventhook>(stm)->HasHookInstalled())
    {
      Core::DisplayMessage("Shutting down", 30000);
      // Unpause because gracefully shutting down needs the game to actually request a shutdown
      if (Core::GetState() == Core::CORE_PAUSE)
        DoPause();
      ProcessorInterface::PowerButton_Tap();
      m_confirmStop = false;
      m_tried_graceful_shutdown = true;
      return;
    }

    if (UseDebugger && g_pCodeWindow)
    {
      if (g_pCodeWindow->HasPanel<CWatchWindow>())
        g_pCodeWindow->GetPanel<CWatchWindow>()->SaveAll();
      PowerPC::watches.Clear();
      if (g_pCodeWindow->HasPanel<CBreakPointWindow>())
        g_pCodeWindow->GetPanel<CBreakPointWindow>()->SaveAll();
      PowerPC::breakpoints.Clear();
      PowerPC::memchecks.Clear();
      if (g_pCodeWindow->HasPanel<CBreakPointWindow>())
        g_pCodeWindow->GetPanel<CBreakPointWindow>()->NotifyUpdate();
      g_symbolDB.Clear();
      Host_NotifyMapLoaded();
    }

    // TODO: Show the author/description dialog here
    if (Movie::IsRecordingInput())
      DoRecordingSave();
    if (Movie::IsMovieActive())
      Movie::EndPlayInput(false);

    if (NetPlayDialog::GetNetPlayClient())
      NetPlayDialog::GetNetPlayClient()->Stop();

    Core::Stop();
    UpdateGUI();
  }
}

void CFrame::OnStopped()
{
  m_confirmStop = false;
  m_tried_graceful_shutdown = false;

#if defined(HAVE_X11) && HAVE_X11
  if (SConfig::GetInstance().bDisableScreenSaver)
    X11Utils::InhibitScreensaver(X11Utils::XDisplayFromHandle(GetHandle()),
                                 X11Utils::XWindowFromHandle(GetHandle()), false);
#endif

#ifdef _WIN32
  // Allow windows to resume normal idling behavior
  SetThreadExecutionState(ES_CONTINUOUS);
#endif

  m_RenderFrame->SetTitle(StrToWxStr(scm_rev_str));

  // Destroy the renderer frame when not rendering to main
  m_RenderParent->Unbind(wxEVT_SIZE, &CFrame::OnRenderParentResize, this);

  // Mouse
  wxTheApp->Unbind(wxEVT_RIGHT_DOWN, &CFrame::OnMouse, this);
  wxTheApp->Unbind(wxEVT_RIGHT_UP, &CFrame::OnMouse, this);
  wxTheApp->Unbind(wxEVT_MIDDLE_DOWN, &CFrame::OnMouse, this);
  wxTheApp->Unbind(wxEVT_MIDDLE_UP, &CFrame::OnMouse, this);
  wxTheApp->Unbind(wxEVT_MOTION, &CFrame::OnMouse, this);
  if (SConfig::GetInstance().bHideCursor)
    m_RenderParent->SetCursor(wxNullCursor);
  DoFullscreen(false);
  if (!SConfig::GetInstance().bRenderToMain)
  {
    m_RenderFrame->Destroy();
  }
  else
  {
#if defined(__APPLE__)
    // Disable the full screen button when not in a game.
    m_RenderFrame->EnableFullScreenView(false);
#endif

    // Make sure the window is not longer set to stay on top
    m_RenderFrame->SetWindowStyle(m_RenderFrame->GetWindowStyle() & ~wxSTAY_ON_TOP);
  }
  m_RenderParent = nullptr;

  // Clean framerate indications from the status bar.
  GetStatusBar()->SetStatusText(" ", 0);

  // Clear Wii Remote connection status from the status bar.
  GetStatusBar()->SetStatusText(" ", 1);

  // If batch mode was specified on the command-line or we were already closing, exit now.
  if (m_bBatchMode || m_bClosing)
    Close(true);

  // If using auto size with render to main, reset the application size.
  if (SConfig::GetInstance().bRenderToMain && SConfig::GetInstance().bRenderWindowAutoSize)
    SetSize(SConfig::GetInstance().iWidth, SConfig::GetInstance().iHeight);

  m_GameListCtrl->Enable();
  m_GameListCtrl->Show();
  m_GameListCtrl->SetFocus();
  UpdateGUI();
}

void CFrame::DoRecordingSave()
{
  bool paused = (Core::GetState() == Core::CORE_PAUSE);

  if (!paused)
    DoPause();

  wxString path =
      wxFileSelector(_("Select The Recording File"), wxEmptyString, wxEmptyString, wxEmptyString,
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

void CFrame::OnConfigHotkey(wxCommandEvent& WXUNUSED(event))
{
  InputConfig* const hotkey_plugin = HotkeyManagerEmu::GetConfig();

  // check if game is running
  bool game_running = false;
  if (Core::GetState() == Core::CORE_RUN)
  {
    Core::SetState(Core::CORE_PAUSE);
    game_running = true;
  }

  HotkeyManagerEmu::Enable(false);

  InputConfigDialog m_ConfigFrame(this, *hotkey_plugin, _("Dolphin Hotkeys"));
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
    Core::SetState(Core::CORE_RUN);
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
    WxUtils::Launch("https://dolphin-emu.org/");
    break;
  case IDM_HELP_ONLINE_DOCS:
    WxUtils::Launch("https://dolphin-emu.org/docs/guides/");
    break;
  case IDM_HELP_GITHUB:
    WxUtils::Launch("https://github.com/dolphin-emu/dolphin");
    break;
  }
}

void CFrame::ClearStatusBar()
{
  if (this->GetStatusBar()->IsEnabled())
  {
    this->GetStatusBar()->SetStatusText("", 0);
  }
}

void CFrame::StatusBarMessage(const char* Text, ...)
{
  const int MAX_BYTES = 1024 * 10;
  char Str[MAX_BYTES];
  va_list ArgPtr;
  va_start(ArgPtr, Text);
  vsnprintf(Str, MAX_BYTES, Text, ArgPtr);
  va_end(ArgPtr);

  if (this->GetStatusBar()->IsEnabled())
  {
    this->GetStatusBar()->SetStatusText(StrToWxStr(Str), 0);
  }
}

// Miscellaneous menus
// ---------------------
// NetPlay stuff
void CFrame::OnNetPlay(wxCommandEvent& WXUNUSED(event))
{
  if (!g_NetPlaySetupDiag)
  {
    if (NetPlayDialog::GetInstance() != nullptr)
      NetPlayDialog::GetInstance()->Raise();
    else
      g_NetPlaySetupDiag = new NetPlaySetupFrame(this, m_GameListCtrl);
  }
  else
  {
    g_NetPlaySetupDiag->Raise();
  }
}

void CFrame::OnMemcard(wxCommandEvent& WXUNUSED(event))
{
  CMemcardManager MemcardManager(this);
  HotkeyManagerEmu::Enable(false);
  MemcardManager.ShowModal();
  HotkeyManagerEmu::Enable(true);
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
  if (!g_CheatsWindow)
    g_CheatsWindow = new wxCheatsWindow(this);
  else
    g_CheatsWindow->Raise();
}

void CFrame::OnLoadWiiMenu(wxCommandEvent& WXUNUSED(event))
{
  BootGame(Common::GetTitleContentPath(TITLEID_SYSMENU, Common::FROM_CONFIGURED_ROOT));
}

void CFrame::OnInstallWAD(wxCommandEvent& event)
{
  std::string fileName;

  switch (event.GetId())
  {
  case IDM_LIST_INSTALL_WAD:
  {
    const GameListItem* iso = m_GameListCtrl->GetSelectedISO();
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

  u64 titleID = DiscIO::CNANDContentManager::Access().Install_WiiWAD(fileName);
  if (titleID == TITLEID_SYSMENU)
  {
    UpdateWiiMenuChoice();
  }
}

void CFrame::UpdateWiiMenuChoice(wxMenuItem* WiiMenuItem)
{
  if (!WiiMenuItem)
  {
    WiiMenuItem = GetMenuBar()->FindItem(IDM_LOAD_WII_MENU);
  }

  const DiscIO::CNANDContentLoader& SysMenu_Loader =
      DiscIO::CNANDContentManager::Access().GetNANDLoader(TITLEID_SYSMENU,
                                                          Common::FROM_CONFIGURED_ROOT);
  if (SysMenu_Loader.IsValid())
  {
    int sysmenuVersion = SysMenu_Loader.GetTitleVersion();
    char sysmenuRegion = SysMenu_Loader.GetCountryChar();
    WiiMenuItem->Enable();
    WiiMenuItem->SetItemLabel(
        wxString::Format(_("Load Wii System Menu %d%c"), sysmenuVersion, sysmenuRegion));
  }
  else
  {
    WiiMenuItem->Enable(false);
    WiiMenuItem->SetItemLabel(_("Load Wii System Menu"));
  }
}

void CFrame::OnFifoPlayer(wxCommandEvent& WXUNUSED(event))
{
  if (m_FifoPlayerDlg)
  {
    m_FifoPlayerDlg->Show();
    m_FifoPlayerDlg->SetFocus();
  }
  else
  {
    m_FifoPlayerDlg = new FifoPlayerDlg(this);
  }
}

void CFrame::ConnectWiimote(int wm_idx, bool connect)
{
  if (Core::IsRunning() && SConfig::GetInstance().bWii &&
      !SConfig::GetInstance().m_bt_passthrough_enabled)
  {
    bool was_unpaused = Core::PauseAndLock(true);
    GetUsbPointer()->AccessWiiMote(wm_idx | 0x100)->Activate(connect);
    const char* message = connect ? "Wii Remote %i connected" : "Wii Remote %i disconnected";
    Core::DisplayMessage(StringFromFormat(message, wm_idx + 1), 3000);
    Host_UpdateMainFrame();
    Core::PauseAndLock(false, was_unpaused);
  }
}

void CFrame::OnConnectWiimote(wxCommandEvent& event)
{
  if (SConfig::GetInstance().m_bt_passthrough_enabled)
    return;
  bool was_unpaused = Core::PauseAndLock(true);
  ConnectWiimote(event.GetId() - IDM_CONNECT_WIIMOTE1,
                 !GetUsbPointer()
                      ->AccessWiiMote((event.GetId() - IDM_CONNECT_WIIMOTE1) | 0x100)
                      ->IsConnected());
  Core::PauseAndLock(false, was_unpaused);
}

// Toggle fullscreen. In Windows the fullscreen mode is accomplished by expanding the m_Panel to
// cover
// the entire screen (when we render to the main window).
void CFrame::OnToggleFullscreen(wxCommandEvent& WXUNUSED(event))
{
  DoFullscreen(!RendererIsFullscreen());
}

void CFrame::OnToggleDualCore(wxCommandEvent& WXUNUSED(event))
{
  SConfig::GetInstance().bCPUThread = !SConfig::GetInstance().bCPUThread;
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

void CFrame::OnSelectSlot(wxCommandEvent& event)
{
  m_saveSlot = event.GetId() - IDM_SELECT_SLOT_1 + 1;
  Core::DisplayMessage(StringFromFormat("Selected slot %d - %s", m_saveSlot,
                                        State::GetInfoStringOfSlot(m_saveSlot, false).c_str()),
                       2500);
}

void CFrame::OnLoadCurrentSlot(wxCommandEvent& event)
{
  if (Core::IsRunningAndStarted())
  {
    State::Load(m_saveSlot);
  }
}

void CFrame::OnSaveCurrentSlot(wxCommandEvent& event)
{
  if (Core::IsRunningAndStarted())
  {
    State::Save(m_saveSlot);
  }
}

// GUI
// ---------------------

// Update the enabled/disabled status
void CFrame::UpdateGUI()
{
  // Save status
  bool Initialized = Core::IsRunning();
  bool Running = Core::GetState() == Core::CORE_RUN;
  bool Paused = Core::GetState() == Core::CORE_PAUSE;
  bool Stopping = Core::GetState() == Core::CORE_STOPPING;

  // Make sure that we have a toolbar
  if (m_ToolBar)
  {
    // Enable/disable the Config and Stop buttons
    m_ToolBar->EnableTool(wxID_OPEN, !Initialized);
    // Don't allow refresh when we don't show the list
    m_ToolBar->EnableTool(wxID_REFRESH, !Initialized);
    m_ToolBar->EnableTool(IDM_STOP, Running || Paused);
    m_ToolBar->EnableTool(IDM_TOGGLE_FULLSCREEN, Running || Paused);
    m_ToolBar->EnableTool(IDM_SCREENSHOT, Running || Paused);
  }

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
  GetMenuBar()->FindItem(IDM_RECORD_EXPORT)->Enable(Movie::IsMovieActive());
  GetMenuBar()->FindItem(IDM_FRAMESTEP)->Enable(Running || Paused);
  GetMenuBar()->FindItem(IDM_SCREENSHOT)->Enable(Running || Paused);
  GetMenuBar()->FindItem(IDM_TOGGLE_FULLSCREEN)->Enable(Running || Paused);
  GetMenuBar()->FindItem(IDM_LOAD_STATE)->Enable(Initialized);
  GetMenuBar()->FindItem(IDM_SAVE_STATE)->Enable(Initialized);
  // Misc
  GetMenuBar()->FindItem(IDM_CHANGE_DISC)->Enable(Initialized);
  if (DiscIO::CNANDContentManager::Access()
          .GetNANDLoader(TITLEID_SYSMENU, Common::FROM_CONFIGURED_ROOT)
          .IsValid())
    GetMenuBar()->FindItem(IDM_LOAD_WII_MENU)->Enable(!Initialized);

  // Tools
  GetMenuBar()->FindItem(IDM_CHEATS)->Enable(SConfig::GetInstance().bEnableCheats);

  bool ShouldEnableWiimotes = Initialized && SConfig::GetInstance().bWii &&
                              !SConfig::GetInstance().m_bt_passthrough_enabled;
  GetMenuBar()->FindItem(IDM_CONNECT_WIIMOTE1)->Enable(ShouldEnableWiimotes);
  GetMenuBar()->FindItem(IDM_CONNECT_WIIMOTE2)->Enable(ShouldEnableWiimotes);
  GetMenuBar()->FindItem(IDM_CONNECT_WIIMOTE3)->Enable(ShouldEnableWiimotes);
  GetMenuBar()->FindItem(IDM_CONNECT_WIIMOTE4)->Enable(ShouldEnableWiimotes);
  GetMenuBar()->FindItem(IDM_CONNECT_BALANCEBOARD)->Enable(ShouldEnableWiimotes);
  if (ShouldEnableWiimotes)
  {
    bool was_unpaused = Core::PauseAndLock(true);
    GetMenuBar()
        ->FindItem(IDM_CONNECT_WIIMOTE1)
        ->Check(GetUsbPointer()->AccessWiiMote(0x0100)->IsConnected());
    GetMenuBar()
        ->FindItem(IDM_CONNECT_WIIMOTE2)
        ->Check(GetUsbPointer()->AccessWiiMote(0x0101)->IsConnected());
    GetMenuBar()
        ->FindItem(IDM_CONNECT_WIIMOTE3)
        ->Check(GetUsbPointer()->AccessWiiMote(0x0102)->IsConnected());
    GetMenuBar()
        ->FindItem(IDM_CONNECT_WIIMOTE4)
        ->Check(GetUsbPointer()->AccessWiiMote(0x0103)->IsConnected());
    GetMenuBar()
        ->FindItem(IDM_CONNECT_BALANCEBOARD)
        ->Check(GetUsbPointer()->AccessWiiMote(0x0104)->IsConnected());
    Core::PauseAndLock(false, was_unpaused);
  }

  if (m_ToolBar)
  {
    // Get the tool that controls pausing/playing
    wxToolBarToolBase* PlayTool = m_ToolBar->FindById(IDM_PLAY);

    if (PlayTool)
    {
      int position = m_ToolBar->GetToolPos(IDM_PLAY);

      if (Running)
      {
        m_ToolBar->DeleteTool(IDM_PLAY);
        m_ToolBar->InsertTool(position, IDM_PLAY, _("Pause"), m_Bitmaps[Toolbar_Pause],
                              WxUtils::CreateDisabledButtonBitmap(m_Bitmaps[Toolbar_Pause]),
                              wxITEM_NORMAL, _("Pause"));
      }
      else
      {
        m_ToolBar->DeleteTool(IDM_PLAY);
        m_ToolBar->InsertTool(position, IDM_PLAY, _("Play"), m_Bitmaps[Toolbar_Play],
                              WxUtils::CreateDisabledButtonBitmap(m_Bitmaps[Toolbar_Play]),
                              wxITEM_NORMAL, _("Play"));
      }
      m_ToolBar->Realize();
    }
  }

  GetMenuBar()->FindItem(IDM_RECORD_READ_ONLY)->Enable(Running || Paused);

  if (!Initialized && !m_bGameLoading)
  {
    if (m_GameListCtrl->IsEnabled())
    {
      // Prepare to load Default ISO, enable play button
      if (!SConfig::GetInstance().m_strDefaultISO.empty())
      {
        if (m_ToolBar)
          m_ToolBar->EnableTool(IDM_PLAY, true);
        GetMenuBar()->FindItem(IDM_PLAY)->Enable();
        GetMenuBar()->FindItem(IDM_RECORD)->Enable();
        GetMenuBar()->FindItem(IDM_PLAY_RECORD)->Enable();
      }
      // Prepare to load last selected file, enable play button
      else if (!SConfig::GetInstance().m_LastFilename.empty() &&
               File::Exists(SConfig::GetInstance().m_LastFilename))
      {
        if (m_ToolBar)
          m_ToolBar->EnableTool(IDM_PLAY, true);
        GetMenuBar()->FindItem(IDM_PLAY)->Enable();
        GetMenuBar()->FindItem(IDM_RECORD)->Enable();
        GetMenuBar()->FindItem(IDM_PLAY_RECORD)->Enable();
      }
      else
      {
        // No game has been selected yet, disable play button
        if (m_ToolBar)
          m_ToolBar->EnableTool(IDM_PLAY, false);
        GetMenuBar()->FindItem(IDM_PLAY)->Enable(false);
        GetMenuBar()->FindItem(IDM_RECORD)->Enable(false);
        GetMenuBar()->FindItem(IDM_PLAY_RECORD)->Enable(false);
      }
    }

    // Game has not started, show game list
    if (!m_GameListCtrl->IsShown())
    {
      m_GameListCtrl->Enable();
      m_GameListCtrl->Show();
    }
    // Game has been selected but not started, enable play button
    if (m_GameListCtrl->GetSelectedISO() != nullptr && m_GameListCtrl->IsEnabled())
    {
      if (m_ToolBar)
        m_ToolBar->EnableTool(IDM_PLAY, true);
      GetMenuBar()->FindItem(IDM_PLAY)->Enable();
      GetMenuBar()->FindItem(IDM_RECORD)->Enable();
      GetMenuBar()->FindItem(IDM_PLAY_RECORD)->Enable();
    }
  }
  else if (Initialized)
  {
    // Game has been loaded, enable the pause button
    if (m_ToolBar)
      m_ToolBar->EnableTool(IDM_PLAY, !Stopping);
    GetMenuBar()->FindItem(IDM_PLAY)->Enable(!Stopping);

    // Reset game loading flag
    m_bGameLoading = false;
  }

  // Refresh toolbar
  if (m_ToolBar)
  {
    m_ToolBar->Refresh();
  }

  // Commit changes to manager
  m_Mgr->Update();

  // Update non-modal windows
  if (g_CheatsWindow)
  {
    if (SConfig::GetInstance().bEnableCheats)
      g_CheatsWindow->UpdateGUI();
    else
      g_CheatsWindow->Close();
  }
}

void CFrame::UpdateGameList()
{
  if (m_GameListCtrl)
    m_GameListCtrl->ReloadList();
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
    std::vector<std::string> rFilenames =
        DoFileSearch({".cache"}, {File::GetUserPath(D_CACHE_IDX)});

    for (const std::string& rFilename : rFilenames)
    {
      File::Delete(rFilename);
    }
    break;
  }

  UpdateGameList();
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
  m_Mgr->Update();
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
  default:
    return;
  }
  UpdateGameList();
  SConfig::GetInstance().SaveSettings();
}
