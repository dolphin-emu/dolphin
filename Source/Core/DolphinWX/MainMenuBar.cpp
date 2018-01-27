// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinWX/MainMenuBar.h"

#include <string>
#include <vector>

#include "Common/CDUtils.h"
#include "Core/ARBruteForcer.h"
#include "Core/CommonTitles.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/IOS/ES/ES.h"
#include "Core/IOS/ES/Formats.h"
#include "Core/Movie.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/State.h"
#include "DiscIO/Enums.h"
#include "DolphinWX/Frame.h"
#include "DolphinWX/Globals.h"
#include "DolphinWX/WxUtils.h"

wxDEFINE_EVENT(EVT_POPULATE_PERSPECTIVES_MENU, MainMenuBar::PopulatePerspectivesEvent);

// Utilized for menu items where their labels don't matter on initial construction.
// This is necessary, as wx, in its infinite glory, asserts if a menu item is appended to a menu
// with an empty label. Entries with this string are intended to have their correct label
// loaded through the refresh cycle.
#if defined(_MSC_VER) && _MSC_VER <= 1800
#define constexpr
#endif
constexpr const char dummy_string[] = "Dummy string";

MainMenuBar::MainMenuBar(MenuType type, long style) : wxMenuBar{style}, m_type{type}
{
  BindEvents();
  AddMenus();
  RefreshWiiSystemMenuLabel();
}

void MainMenuBar::Refresh(bool erase_background, const wxRect* rect)
{
  wxMenuBar::Refresh(erase_background, rect);

  RefreshMenuLabels();
}

void MainMenuBar::AddMenus()
{
  Append(CreateFileMenu(), _("&File"));
  Append(CreateEmulationMenu(), _("&Emulation"));
  Append(CreateMovieMenu(), _("&Movie"));
  Append(CreateOptionsMenu(), _("&Options"));
  Append(CreateToolsMenu(), _("&Tools"));
  Append(CreateViewMenu(), _("&View"));

  if (m_type == MenuType::Debug)
  {
    Append(CreateJITMenu(), _("&JIT"));
    Append(CreateDebugMenu(), _("&Debug"));
    Append(CreateSymbolsMenu(), _("&Symbols"));
    Append(CreateProfilerMenu(), _("&Profiler"));
  }

  Append(CreateHelpMenu(), _("&Help"));
}

void MainMenuBar::BindEvents()
{
  Bind(EVT_POPULATE_PERSPECTIVES_MENU, &MainMenuBar::OnPopulatePerspectivesMenu, this);
  Bind(DOLPHIN_EVT_UPDATE_LOAD_WII_MENU_ITEM, &MainMenuBar::OnUpdateWiiMenuTool, this);
}

wxMenu* MainMenuBar::CreateFileMenu() const
{
  auto* const external_drive_menu = new wxMenu;

  const std::vector<std::string> drives = cdio_get_devices();
  // Windows Limitation of 24 character drives
  for (size_t i = 0; i < drives.size() && i < 24; i++)
  {
    const int drive_id = static_cast<int>(IDM_DRIVE1 + i);
    external_drive_menu->Append(drive_id, StrToWxStr(drives[i]));
  }

  auto* const file_menu = new wxMenu;
  file_menu->Append(wxID_OPEN, _("&Open..."));
  file_menu->Append(IDM_CHANGE_DISC, _("Change &Disc..."));
  file_menu->Append(IDM_EJECT_DISC, _("Eject Disc"));
  file_menu->Append(IDM_DRIVES, _("&Boot from DVD Backup"), external_drive_menu);
  file_menu->AppendSeparator();
  file_menu->Append(wxID_REFRESH, _("&Refresh Game List"));
  file_menu->AppendSeparator();
  file_menu->Append(wxID_EXIT, _("E&xit") + "\tAlt+F4");

  return file_menu;
}

wxMenu* MainMenuBar::CreateEmulationMenu() const
{
  auto* const load_state_menu = new wxMenu;
  load_state_menu->Append(IDM_LOAD_STATE_FILE, _("Load State..."));
  load_state_menu->Append(IDM_LOAD_SELECTED_SLOT, _("Load State from Selected Slot"));
  load_state_menu->Append(IDM_UNDO_LOAD_STATE, _("Undo Load State"));
  load_state_menu->AppendSeparator();

  auto* const save_state_menu = new wxMenu;
  save_state_menu->Append(IDM_SAVE_STATE_FILE, _("Save State..."));
  save_state_menu->Append(IDM_SAVE_SELECTED_SLOT, _("Save State to Selected Slot"));
  save_state_menu->Append(IDM_SAVE_FIRST_STATE, _("Save Oldest State"));
  save_state_menu->Append(IDM_UNDO_SAVE_STATE, _("Undo Save State"));
  save_state_menu->AppendSeparator();

  auto* const slot_select_menu = new wxMenu;

  for (unsigned int i = 0; i < State::NUM_STATES; i++)
  {
    load_state_menu->Append(IDM_LOAD_SLOT_1 + i, dummy_string);
    save_state_menu->Append(IDM_SAVE_SLOT_1 + i, dummy_string);
    slot_select_menu->Append(IDM_SELECT_SLOT_1 + i, dummy_string);
  }

  load_state_menu->AppendSeparator();
  for (unsigned int i = 0; i < State::NUM_STATES; i++)
    load_state_menu->Append(IDM_LOAD_LAST_1 + i, wxString::Format(_("Last %i"), i + 1));

  auto* const emulation_menu = new wxMenu;
  emulation_menu->Append(IDM_PLAY, dummy_string);
  emulation_menu->Append(IDM_STOP, _("&Stop"));
  emulation_menu->Append(IDM_RESET, _("&Reset"));
  emulation_menu->AppendSeparator();
  emulation_menu->Append(IDM_TOGGLE_FULLSCREEN, _("Toggle &Fullscreen"));
  emulation_menu->Append(IDM_FRAMESTEP, _("&Frame Advance"));
  wxMenu* skippingMenu = new wxMenu;
  emulation_menu->AppendSubMenu(skippingMenu, _("Frame S&kipping"));
  for (int i = 0; i < 10; i++)
     skippingMenu->AppendRadioItem(IDM_FRAME_SKIP_0 + i, wxString::Format("%i", i));
  skippingMenu->Check(IDM_FRAME_SKIP_0 + SConfig::GetInstance().m_FrameSkip, true);
  Movie::SetFrameSkipping(SConfig::GetInstance().m_FrameSkip);

  emulation_menu->AppendSeparator();
  emulation_menu->Append(IDM_SCREENSHOT, _("Take Screenshot"));
  emulation_menu->AppendSeparator();
  emulation_menu->Append(IDM_LOAD_STATE, _("&Load State"), load_state_menu);
  emulation_menu->Append(IDM_SAVE_STATE, _("Sa&ve State"), save_state_menu);
  emulation_menu->Append(IDM_SELECT_SLOT, _("Select State Slot"), slot_select_menu);

  return emulation_menu;
}

wxMenu* MainMenuBar::CreateMovieMenu() const
{
  auto* const movie_menu = new wxMenu;
  const auto& config_instance = SConfig::GetInstance();

  movie_menu->Append(IDM_RECORD, _("Start Re&cording Input"));
  movie_menu->Append(IDM_PLAY_RECORD, _("P&lay Input Recording..."));
  movie_menu->Append(IDM_STOP_RECORD, _("Stop Playing/Recording Input"));
  movie_menu->Append(IDM_RECORD_EXPORT, _("Export Recording..."));
  movie_menu->AppendCheckItem(IDM_RECORD_READ_ONLY, _("&Read-Only Mode"));
  movie_menu->Append(IDM_TAS_INPUT, _("TAS Input"));
  movie_menu->AppendSeparator();
  movie_menu->AppendCheckItem(IDM_TOGGLE_PAUSE_MOVIE, _("Pause at End of Movie"));
  movie_menu->Check(IDM_TOGGLE_PAUSE_MOVIE, config_instance.m_PauseMovie);
  movie_menu->AppendCheckItem(IDM_SHOW_LAG, _("Show Lag Counter"));
  movie_menu->Check(IDM_SHOW_LAG, config_instance.m_ShowLag);
  movie_menu->AppendCheckItem(IDM_SHOW_FRAME_COUNT, _("Show Frame Counter"));
  movie_menu->Check(IDM_SHOW_FRAME_COUNT, config_instance.m_ShowFrameCount);
  movie_menu->Check(IDM_RECORD_READ_ONLY, true);
  movie_menu->AppendCheckItem(IDM_SHOW_INPUT_DISPLAY, _("Show Input Display"));
  movie_menu->Check(IDM_SHOW_INPUT_DISPLAY, config_instance.m_ShowInputDisplay);
  movie_menu->AppendCheckItem(IDM_SHOW_RTC_DISPLAY, _("Show System Clock"));
  movie_menu->Check(IDM_SHOW_RTC_DISPLAY, config_instance.m_ShowRTC);
  movie_menu->AppendSeparator();
  movie_menu->AppendCheckItem(IDM_TOGGLE_DUMP_FRAMES, _("Dump Frames"));
  movie_menu->Check(IDM_TOGGLE_DUMP_FRAMES, config_instance.m_DumpFrames);
  movie_menu->AppendCheckItem(IDM_TOGGLE_DUMP_AUDIO, _("Dump Audio"));
  movie_menu->Check(IDM_TOGGLE_DUMP_AUDIO, config_instance.m_DumpAudio);

  return movie_menu;
}

wxMenu* MainMenuBar::CreateOptionsMenu() const
{
  auto* const options_menu = new wxMenu;
  options_menu->Append(wxID_PREFERENCES, _("Co&nfiguration"));
  options_menu->AppendSeparator();
  options_menu->Append(IDM_CONFIG_GFX_BACKEND, _("&Graphics Settings"));
  options_menu->Append(IDM_CONFIG_AUDIO, _("&Audio Settings"));
  options_menu->Append(IDM_CONFIG_CONTROLLERS, _("&Controller Settings"));
  options_menu->Append(IDM_CONFIG_VR, _("&VR Settings"));
  options_menu->Append(IDM_CONFIG_HOTKEYS, _("&Hotkey Settings"));

  if (m_type == MenuType::Debug)
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

  return options_menu;
}

wxMenu* MainMenuBar::CreateToolsMenu() const
{
  auto* const wiimote_menu = new wxMenu;
  for (int i = 0; i < 4; i++)
  {
    wiimote_menu->AppendCheckItem(IDM_CONNECT_WIIMOTE1 + i,
                                  wxString::Format(_("Connect Wii Remote %i"), i + 1));
  }
  wiimote_menu->AppendSeparator();
  wiimote_menu->AppendCheckItem(IDM_CONNECT_BALANCEBOARD, _("Connect Balance Board"));

  auto* const tools_menu = new wxMenu;
  tools_menu->Append(IDM_MEMCARD, _("&Memory Card Manager (GC)"));
  tools_menu->Append(IDM_IMPORT_SAVE, _("Import Wii Save..."));
  tools_menu->Append(IDM_EXPORT_ALL_SAVE, _("Export All Wii Saves"));
  tools_menu->AppendSeparator();
  auto* const gc_bios_menu = new wxMenu;
  gc_bios_menu->Append(IDM_LOAD_GC_IPL_JAP, _("NTSC-J"),
                       _("Load NTSC-J GameCube Main Menu from the JAP folder."));
  gc_bios_menu->Append(IDM_LOAD_GC_IPL_USA, _("NTSC-U"),
                       _("Load NTSC-U GameCube Main Menu from the USA folder."));
  gc_bios_menu->Append(IDM_LOAD_GC_IPL_EUR, _("PAL"),
                       _("Load PAL GameCube Main Menu from the EUR folder."));
  tools_menu->AppendSubMenu(gc_bios_menu, _("Load GameCube Main Menu"),
                            _("Load a GameCube Main Menu located under Dolphin's GC folder."));
  tools_menu->AppendSeparator();
  tools_menu->Append(IDM_CHEATS, _("&Cheat Manager"));
  tools_menu->Append(IDM_NETPLAY, _("Start &NetPlay..."));
  tools_menu->Append(IDM_FIFOPLAYER, _("FIFO Player"));
  tools_menu->AppendSeparator();
  tools_menu->Append(IDM_MENU_INSTALL_WAD, _("Install WAD..."));
  tools_menu->Append(IDM_LOAD_WII_MENU, dummy_string);
  tools_menu->Append(IDM_IMPORT_NAND, _("Import BootMii NAND Backup..."));
  tools_menu->Append(IDM_CHECK_NAND, _("Check NAND..."));
  tools_menu->Append(IDM_EXTRACT_CERTIFICATES, _("Extract Certificates from NAND"));
  auto* const online_update_menu = new wxMenu;
  online_update_menu->Append(IDM_PERFORM_ONLINE_UPDATE_CURRENT, _("Current Region"));
  online_update_menu->AppendSeparator();
  online_update_menu->Append(IDM_PERFORM_ONLINE_UPDATE_EUR, _("Europe"));
  online_update_menu->Append(IDM_PERFORM_ONLINE_UPDATE_JPN, _("Japan"));
  online_update_menu->Append(IDM_PERFORM_ONLINE_UPDATE_KOR, _("Korea"));
  online_update_menu->Append(IDM_PERFORM_ONLINE_UPDATE_USA, _("United States"));
  tools_menu->AppendSubMenu(
      online_update_menu, _("Perform Online System Update"),
      _("Update the Wii system software to the latest version from Nintendo."));
  tools_menu->AppendCheckItem(IDM_DEBUGGER, _("Debugger"));
  tools_menu->Enable(IDM_DEBUGGER, m_type != MenuType::Debug);
  tools_menu->Check(IDM_DEBUGGER, m_type == MenuType::Debug);

  wxMenu* bruteforceMenu = new wxMenu;
  tools_menu->AppendSubMenu(bruteforceMenu, _("Bruteforce Functions"));

  bruteforceMenu->AppendCheckItem(IDM_BRUTEFORCE0, _("return 0"));
  bruteforceMenu->AppendCheckItem(IDM_BRUTEFORCE1, _("return 1"));
  bruteforceMenu->AppendSeparator();
  bruteforceMenu->AppendCheckItem(IDM_BRUTEFORCE_ALL, _("Screenshot All"));
  bruteforceMenu->Enable(IDM_BRUTEFORCE0, !ARBruteForcer::ch_bruteforce);
  bruteforceMenu->Enable(IDM_BRUTEFORCE1, !ARBruteForcer::ch_bruteforce);
  bruteforceMenu->Check(IDM_BRUTEFORCE_ALL, SConfig::GetInstance().m_BruteforceScreenshotAll);

  tools_menu->AppendSeparator();
  tools_menu->AppendSubMenu(wiimote_menu, _("Connect Wii Remotes"));

  return tools_menu;
}

wxMenu* MainMenuBar::CreateViewMenu() const
{
  const auto& config_instance = SConfig::GetInstance();

  auto* const platform_menu = new wxMenu;
  platform_menu->AppendCheckItem(IDM_LIST_WII, _("Show Wii"));
  platform_menu->Check(IDM_LIST_WII, config_instance.m_ListWii);
  platform_menu->AppendCheckItem(IDM_LIST_GC, _("Show GameCube"));
  platform_menu->Check(IDM_LIST_GC, config_instance.m_ListGC);
  platform_menu->AppendCheckItem(IDM_LIST_WAD, _("Show WAD"));
  platform_menu->Check(IDM_LIST_WAD, config_instance.m_ListWad);
  platform_menu->AppendCheckItem(IDM_LIST_ELFDOL, _("Show ELF/DOL"));
  platform_menu->Check(IDM_LIST_ELFDOL, config_instance.m_ListElfDol);

  auto* const region_menu = new wxMenu;
  region_menu->AppendCheckItem(IDM_LIST_JAP, _("Show JAP"));
  region_menu->Check(IDM_LIST_JAP, config_instance.m_ListJap);
  region_menu->AppendCheckItem(IDM_LIST_PAL, _("Show PAL"));
  region_menu->Check(IDM_LIST_PAL, config_instance.m_ListPal);
  region_menu->AppendCheckItem(IDM_LIST_USA, _("Show USA"));
  region_menu->Check(IDM_LIST_USA, config_instance.m_ListUsa);
  region_menu->AppendSeparator();
  region_menu->AppendCheckItem(IDM_LIST_AUSTRALIA, _("Show Australia"));
  region_menu->Check(IDM_LIST_AUSTRALIA, config_instance.m_ListAustralia);
  region_menu->AppendCheckItem(IDM_LIST_FRANCE, _("Show France"));
  region_menu->Check(IDM_LIST_FRANCE, config_instance.m_ListFrance);
  region_menu->AppendCheckItem(IDM_LIST_GERMANY, _("Show Germany"));
  region_menu->Check(IDM_LIST_GERMANY, config_instance.m_ListGermany);
  region_menu->AppendCheckItem(IDM_LIST_ITALY, _("Show Italy"));
  region_menu->Check(IDM_LIST_ITALY, config_instance.m_ListItaly);
  region_menu->AppendCheckItem(IDM_LIST_KOREA, _("Show Korea"));
  region_menu->Check(IDM_LIST_KOREA, config_instance.m_ListKorea);
  region_menu->AppendCheckItem(IDM_LIST_NETHERLANDS, _("Show Netherlands"));
  region_menu->Check(IDM_LIST_NETHERLANDS, config_instance.m_ListNetherlands);
  region_menu->AppendCheckItem(IDM_LIST_RUSSIA, _("Show Russia"));
  region_menu->Check(IDM_LIST_RUSSIA, config_instance.m_ListRussia);
  region_menu->AppendCheckItem(IDM_LIST_SPAIN, _("Show Spain"));
  region_menu->Check(IDM_LIST_SPAIN, config_instance.m_ListSpain);
  region_menu->AppendCheckItem(IDM_LIST_TAIWAN, _("Show Taiwan"));
  region_menu->Check(IDM_LIST_TAIWAN, config_instance.m_ListTaiwan);
  region_menu->AppendCheckItem(IDM_LIST_WORLD, _("Show World"));
  region_menu->Check(IDM_LIST_WORLD, config_instance.m_ListWorld);
  region_menu->AppendCheckItem(IDM_LIST_UNKNOWN, _("Show Unknown"));
  region_menu->Check(IDM_LIST_UNKNOWN, config_instance.m_ListUnknown);

  auto* const columns_menu = new wxMenu;
  columns_menu->AppendCheckItem(IDM_SHOW_SYSTEM, _("Platform"));
  columns_menu->Check(IDM_SHOW_SYSTEM, config_instance.m_showSystemColumn);
  columns_menu->AppendCheckItem(IDM_SHOW_BANNER, _("Banner"));
  columns_menu->Check(IDM_SHOW_BANNER, config_instance.m_showBannerColumn);
  columns_menu->AppendCheckItem(IDM_SHOW_TITLE, _("Title"));
  columns_menu->Check(IDM_SHOW_TITLE, config_instance.m_showTitleColumn);
  columns_menu->AppendCheckItem(IDM_SHOW_MAKER, _("Maker"));
  columns_menu->Check(IDM_SHOW_MAKER, config_instance.m_showMakerColumn);
  columns_menu->AppendCheckItem(IDM_SHOW_FILENAME, _("File Name"));
  columns_menu->Check(IDM_SHOW_FILENAME, config_instance.m_showFileNameColumn);
  columns_menu->AppendCheckItem(IDM_SHOW_ID, _("Game ID"));
  columns_menu->Check(IDM_SHOW_ID, config_instance.m_showIDColumn);
  columns_menu->AppendCheckItem(IDM_SHOW_REGION, _("Region"));
  columns_menu->Check(IDM_SHOW_REGION, config_instance.m_showRegionColumn);
  columns_menu->AppendCheckItem(IDM_SHOW_SIZE, _("File Size"));
  columns_menu->Check(IDM_SHOW_SIZE, config_instance.m_showSizeColumn);
  columns_menu->AppendCheckItem(IDM_SHOW_STATE, _("State"));
  columns_menu->Check(IDM_SHOW_STATE, config_instance.m_showStateColumn);

  auto* const view_menu = new wxMenu;
  view_menu->AppendCheckItem(IDM_TOGGLE_TOOLBAR, _("Show &Toolbar"));
  view_menu->Check(IDM_TOGGLE_TOOLBAR, config_instance.m_InterfaceToolbar);
  view_menu->AppendCheckItem(IDM_TOGGLE_STATUSBAR, _("Show &Status Bar"));
  view_menu->Check(IDM_TOGGLE_STATUSBAR, config_instance.m_InterfaceStatusbar);
  view_menu->AppendSeparator();
  view_menu->AppendCheckItem(IDM_LOG_WINDOW, _("Show &Log"));
  view_menu->AppendCheckItem(IDM_LOG_CONFIG_WINDOW, _("Show Log &Configuration"));
  view_menu->AppendSeparator();

  if (m_type == MenuType::Debug)
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
  else
  {
    view_menu->Check(IDM_LOG_WINDOW, config_instance.m_InterfaceLogWindow);
    view_menu->Check(IDM_LOG_CONFIG_WINDOW, config_instance.m_InterfaceLogConfigWindow);
  }

  view_menu->AppendSubMenu(platform_menu, _("Show Platforms"));
  view_menu->AppendSubMenu(region_menu, _("Show Regions"));

  view_menu->AppendCheckItem(IDM_LIST_DRIVES, _("Show Drives"));
  view_menu->Check(IDM_LIST_DRIVES, config_instance.m_ListDrives);

  view_menu->Append(IDM_PURGE_GAME_LIST_CACHE, _("Purge Game List Cache"));
  view_menu->AppendSubMenu(columns_menu, _("Select Columns"));

  return view_menu;
}

wxMenu* MainMenuBar::CreateJITMenu() const
{
  auto* const jit_menu = new wxMenu;
  const auto& config_instance = SConfig::GetInstance();

  auto* const interpreter = jit_menu->AppendCheckItem(
      IDM_INTERPRETER, _("&Interpreter Core"),
      _("This is necessary to get break points"
        " and stepping to work as explained in the Developer Documentation. But it can be very"
        " slow, perhaps slower than 1 fps."));
  interpreter->Check(config_instance.iCPUCore == PowerPC::CORE_INTERPRETER);

  jit_menu->AppendSeparator();
  jit_menu->AppendCheckItem(IDM_JIT_NO_BLOCK_LINKING, _("&JIT Block Linking Off"),
                            _("Provide safer execution by not linking the JIT blocks."));

  jit_menu->AppendCheckItem(
      IDM_JIT_NO_BLOCK_CACHE, _("&Disable JIT Cache"),
      _("Avoid any involuntary JIT cache clearing, this may prevent Zelda TP from "
        "crashing.\n[This option must be selected before a game is started.]"));

  jit_menu->Append(IDM_CLEAR_CODE_CACHE, _("&Clear JIT Cache"));
  jit_menu->AppendSeparator();
  jit_menu->Append(IDM_LOG_INSTRUCTIONS, _("&Log JIT Instruction Coverage"));
  jit_menu->Append(IDM_SEARCH_INSTRUCTION, _("&Search for an Instruction"));
  jit_menu->AppendSeparator();

  jit_menu->AppendCheckItem(
      IDM_JIT_OFF, _("&JIT Off (JIT Core)"),
      _("Turn off all JIT functions, but still use the JIT core from Jit.cpp"));

  jit_menu->AppendCheckItem(IDM_JIT_LS_OFF, _("&JIT LoadStore Off"));
  jit_menu->AppendCheckItem(IDM_JIT_LSLBZX_OFF, _("&JIT LoadStore lbzx Off"));
  jit_menu->AppendCheckItem(IDM_JIT_LSLXZ_OFF, _("&JIT LoadStore lXz Off"));
  jit_menu->AppendCheckItem(IDM_JIT_LSLWZ_OFF, _("&JIT LoadStore lwz Off"));
  jit_menu->AppendCheckItem(IDM_JIT_LSF_OFF, _("&JIT LoadStore Floating Off"));
  jit_menu->AppendCheckItem(IDM_JIT_LSP_OFF, _("&JIT LoadStore Paired Off"));
  jit_menu->AppendCheckItem(IDM_JIT_FP_OFF, _("&JIT FloatingPoint Off"));
  jit_menu->AppendCheckItem(IDM_JIT_I_OFF, _("&JIT Integer Off"));
  jit_menu->AppendCheckItem(IDM_JIT_P_OFF, _("&JIT Paired Off"));
  jit_menu->AppendCheckItem(IDM_JIT_SR_OFF, _("&JIT SystemRegisters Off"));

  return jit_menu;
}

wxMenu* MainMenuBar::CreateDebugMenu()
{
  m_saved_perspectives_menu = new wxMenu;

  auto* const add_pane_menu = new wxMenu;
  add_pane_menu->Append(IDM_PERSPECTIVES_ADD_PANE_TOP, _("Top"));
  add_pane_menu->Append(IDM_PERSPECTIVES_ADD_PANE_BOTTOM, _("Bottom"));
  add_pane_menu->Append(IDM_PERSPECTIVES_ADD_PANE_LEFT, _("Left"));
  add_pane_menu->Append(IDM_PERSPECTIVES_ADD_PANE_RIGHT, _("Right"));
  add_pane_menu->Append(IDM_PERSPECTIVES_ADD_PANE_CENTER, _("Center"));

  auto* const perspective_menu = new wxMenu;
  perspective_menu->Append(IDM_SAVE_PERSPECTIVE, _("Save Perspectives"),
                           _("Save currently-toggled perspectives"));
  perspective_menu->AppendCheckItem(IDM_EDIT_PERSPECTIVES, _("Edit Perspectives"),
                                    _("Toggle editing of perspectives"));
  perspective_menu->AppendSeparator();
  perspective_menu->Append(IDM_ADD_PERSPECTIVE, _("Create New Perspective"));
  perspective_menu->AppendSubMenu(m_saved_perspectives_menu, _("Saved Perspectives"));
  perspective_menu->AppendSeparator();
  perspective_menu->AppendSubMenu(add_pane_menu, _("Add New Pane To"));
  perspective_menu->AppendCheckItem(IDM_TAB_SPLIT, _("Tab Split"));
  perspective_menu->AppendCheckItem(IDM_NO_DOCKING, _("Disable Docking"),
                                    _("Disable docking of perspective panes to main window"));

  auto* const debug_menu = new wxMenu;
  debug_menu->Append(IDM_STEP, _("Step &Into"));
  debug_menu->Append(IDM_STEPOVER, _("Step &Over"));
  debug_menu->Append(IDM_STEPOUT, _("Step O&ut"));
  debug_menu->Append(IDM_TOGGLE_BREAKPOINT, _("Toggle &Breakpoint"));
  debug_menu->AppendSeparator();
  debug_menu->AppendSubMenu(perspective_menu, _("Perspectives"), _("Edit Perspectives"));

  return debug_menu;
}

wxMenu* MainMenuBar::CreateSymbolsMenu() const
{
  auto* const symbols_menu = new wxMenu;
  symbols_menu->Append(IDM_CLEAR_SYMBOLS, _("&Clear Symbols"),
                       _("Remove names from all functions and variables."));
  auto* const generate_symbols_menu = new wxMenu;
  generate_symbols_menu->Append(IDM_SCAN_FUNCTIONS, _("&Address"),
                                _("Use generic zz_ names for functions."));
  generate_symbols_menu->Append(
      IDM_SCAN_SIGNATURES, _("&Signature Database"),
      _("Recognise standard functions from Sys/totaldb.dsy, and use generic zz_ "
        "names for other functions."));
  generate_symbols_menu->Append(IDM_SCAN_RSO, _("&RSO Modules"),
                                _("Find functions based on RSO modules (experimental)..."));
  symbols_menu->AppendSubMenu(generate_symbols_menu, _("&Generate Symbols From"));
  symbols_menu->AppendSeparator();
  symbols_menu->Append(IDM_LOAD_MAP_FILE, _("&Load Symbol Map"),
                       _("Try to load this game's function names automatically - but doesn't check "
                         ".map files stored on the disc image yet."));
  symbols_menu->Append(IDM_SAVEMAPFILE, _("&Save Symbol Map"),
                       _("Save the function names for each address to a .map file in your user "
                         "settings map folder, named after the title ID."));
  symbols_menu->AppendSeparator();
  symbols_menu->Append(
      IDM_LOAD_MAP_FILE_AS, _("Load &Other Map File..."),
      _("Load any .map file containing the function names and addresses for this game."));
  symbols_menu->Append(
      IDM_LOAD_BAD_MAP_FILE, _("Load &Bad Map File..."),
      _("Try to load a .map file that might be from a slightly different version."));
  symbols_menu->Append(IDM_SAVE_MAP_FILE_AS, _("Save Symbol Map &As..."),
                       _("Save the function names and addresses for this game as a .map file. "
                         "If you want to open the .map file in IDA Pro, use the .idc script."));
  symbols_menu->AppendSeparator();
  symbols_menu->Append(
      IDM_SAVE_MAP_FILE_WITH_CODES, _("Save Code"),
      _("Save the entire disassembled code. This may take a several seconds "
        "and may require between 50 and 100 MB of hard drive space. It will only save code "
        "that is in the first 4 MB of memory. If you are debugging a game that loads .rel "
        "files with code to memory, you may want to increase the limit to perhaps 8 MB. "
        "That can be done in SymbolDB::SaveMap()."));

  symbols_menu->AppendSeparator();
  symbols_menu->Append(
      IDM_CREATE_SIGNATURE_FILE, _("&Create Signature File..."),
      _("Create a .dsy file that can be used to recognise these same functions in other games."));
  symbols_menu->Append(IDM_APPEND_SIGNATURE_FILE, _("Append to &Existing Signature File..."),
                       _("Add any named functions missing from a .dsy file, so that these "
                         "additional functions can also be recognized in other games."));
  symbols_menu->Append(IDM_COMBINE_SIGNATURE_FILES, _("Combine Two Signature Files..."),
                       _("Make a new .dsy file which can recognise more functions, by combining "
                         "two existing files. The first input file has priority."));
  symbols_menu->Append(
      IDM_USE_SIGNATURE_FILE, _("Apply Signat&ure File..."),
      _("Must use Generate Symbols first! Recognise names of any standard library functions "
        "used in multiple games, by loading them from a .dsy, .csv, or .mega file."));
  symbols_menu->AppendSeparator();
  symbols_menu->Append(IDM_PATCH_HLE_FUNCTIONS, _("&Patch HLE Functions"));
  symbols_menu->Append(IDM_RENAME_SYMBOLS, _("&Rename Symbols from File..."));

  return symbols_menu;
}

wxMenu* MainMenuBar::CreateProfilerMenu() const
{
  auto* const profiler_menu = new wxMenu;
  // i18n: "Profile" is used as a verb, not a noun.
  profiler_menu->AppendCheckItem(IDM_PROFILE_BLOCKS, _("&Profile Blocks"));
  profiler_menu->AppendSeparator();
  profiler_menu->Append(IDM_WRITE_PROFILE, _("&Write to profile.txt, Show"));

  return profiler_menu;
}

wxMenu* MainMenuBar::CreateHelpMenu() const
{
  auto* const help_menu = new wxMenu;
  help_menu->Append(IDM_HELP_WEBSITE, _("&Website"));
  help_menu->Append(IDM_HELP_ONLINE_DOCS, _("Online &Documentation"));
  help_menu->Append(IDM_HELP_GITHUB, _("&GitHub Repository"));
  help_menu->AppendSeparator();
  help_menu->Append(wxID_ABOUT, _("&About"));

  return help_menu;
}

void MainMenuBar::OnPopulatePerspectivesMenu(PopulatePerspectivesEvent& event)
{
  ClearSavedPerspectivesMenu();

  const auto& perspective_names = event.PerspectiveNames();
  if (perspective_names.empty())
    return;

  PopulateSavedPerspectivesMenu(perspective_names);
  CheckPerspectiveWithID(IDM_PERSPECTIVES_0 + event.ActivePerspective());
}

void MainMenuBar::RefreshMenuLabels() const
{
  RefreshPlayMenuLabel();
  RefreshSaveStateMenuLabels();
  RefreshWiiToolsLabels();
}

void MainMenuBar::RefreshPlayMenuLabel() const
{
  auto* const item = FindItem(IDM_PLAY);

  if (Core::GetState() == Core::State::Running)
    item->SetItemLabel(_("&Pause"));
  else
    item->SetItemLabel(_("&Play"));
}

void MainMenuBar::RefreshSaveStateMenuLabels() const
{
  for (unsigned int i = 0; i < State::NUM_STATES; i++)
  {
    const auto slot_number = i + 1;
    const auto slot_string = StrToWxStr(State::GetInfoStringOfSlot(i + 1));

    FindItem(IDM_LOAD_SLOT_1 + i)
        ->SetItemLabel(wxString::Format(_("Slot %u - %s"), slot_number, slot_string));

    FindItem(IDM_SAVE_SLOT_1 + i)
        ->SetItemLabel(wxString::Format(_("Slot %u - %s"), slot_number, slot_string));

    FindItem(IDM_SELECT_SLOT_1 + i)
        ->SetItemLabel(wxString::Format(_("Select Slot %u - %s"), slot_number, slot_string));
  }
}

void MainMenuBar::RefreshWiiToolsLabels() const
{
  // The Install WAD option should not be enabled while emulation is running, because
  // having unexpected title changes can confuse emulated software; and of course, this is
  // not possible on a real Wii and won't be if we have IOS LLE (or simply more accurate IOS HLE).
  //
  // For similar reasons, it should not be possible to export or import saves, because this can
  // result in the emulated software being confused, or even worse, exported saves having
  // inconsistent data.
  const bool enable_wii_tools = !Core::IsRunning() || !SConfig::GetInstance().bWii;
  for (const int index : {IDM_MENU_INSTALL_WAD, IDM_EXPORT_ALL_SAVE, IDM_IMPORT_SAVE,
                          IDM_IMPORT_NAND, IDM_CHECK_NAND, IDM_EXTRACT_CERTIFICATES})
  {
    FindItem(index)->Enable(enable_wii_tools);
  }
}

void MainMenuBar::EnableUpdateMenu(UpdateMenuMode mode) const
{
  FindItem(IDM_PERFORM_ONLINE_UPDATE_CURRENT)->Enable(mode == UpdateMenuMode::CurrentRegionOnly);
  for (const int idm : {IDM_PERFORM_ONLINE_UPDATE_EUR, IDM_PERFORM_ONLINE_UPDATE_JPN,
                        IDM_PERFORM_ONLINE_UPDATE_KOR, IDM_PERFORM_ONLINE_UPDATE_USA})
  {
    FindItem(idm)->Enable(mode == UpdateMenuMode::SpecificRegionsOnly);
  }
}

void MainMenuBar::RefreshWiiSystemMenuLabel() const
{
  auto* const item = FindItem(IDM_LOAD_WII_MENU);

  if (Core::IsRunning())
  {
    item->Enable(false);
    for (const int idm : {IDM_PERFORM_ONLINE_UPDATE_CURRENT, IDM_PERFORM_ONLINE_UPDATE_EUR,
                          IDM_PERFORM_ONLINE_UPDATE_JPN, IDM_PERFORM_ONLINE_UPDATE_KOR,
                          IDM_PERFORM_ONLINE_UPDATE_USA})
    {
      FindItem(idm)->Enable(false);
    }
    return;
  }

  IOS::HLE::Kernel ios;
  const IOS::ES::TMDReader sys_menu_tmd = ios.GetES()->FindInstalledTMD(Titles::SYSTEM_MENU);
  if (sys_menu_tmd.IsValid())
  {
    const u16 version_number = sys_menu_tmd.GetTitleVersion();
    const wxString version_string = StrToWxStr(DiscIO::GetSysMenuVersionString(version_number));
    item->Enable();
    item->SetItemLabel(wxString::Format(_("Load Wii System Menu %s"), version_string));
    EnableUpdateMenu(UpdateMenuMode::CurrentRegionOnly);
  }
  else
  {
    item->Enable(false);
    item->SetItemLabel(_("Load Wii System Menu"));
    EnableUpdateMenu(UpdateMenuMode::SpecificRegionsOnly);
  }
}

void MainMenuBar::OnUpdateWiiMenuTool(wxCommandEvent&)
{
  RefreshWiiSystemMenuLabel();
}

void MainMenuBar::ClearSavedPerspectivesMenu() const
{
  while (m_saved_perspectives_menu->GetMenuItemCount() != 0)
  {
    // Delete the first menu item in the list (while there are menu items)
    m_saved_perspectives_menu->Delete(m_saved_perspectives_menu->FindItemByPosition(0));
  }
}

void MainMenuBar::PopulateSavedPerspectivesMenu(
    const std::vector<std::string>& perspective_names) const
{
  for (size_t i = 0; i < perspective_names.size(); i++)
  {
    const int item_id = static_cast<int>(IDM_PERSPECTIVES_0 + i);
    m_saved_perspectives_menu->AppendCheckItem(item_id, StrToWxStr(perspective_names[i]));
  }
}

void MainMenuBar::CheckPerspectiveWithID(int perspective_id) const
{
  auto* const item = m_saved_perspectives_menu->FindItem(perspective_id);
  if (item == nullptr)
    return;

  item->Check();
}
