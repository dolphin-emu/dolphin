// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <cstdarg>
#include <cstdio>
#include <mutex>
#include <string>
#include <vector>
#include <wx/app.h>
#include <wx/bitmap.h>
#include <wx/chartype.h>
#include <wx/defs.h>
#include <wx/event.h>
#include <wx/filedlg.h>
#include <wx/filefn.h>
#include <wx/gdicmn.h>
#include <wx/menu.h>
#include <wx/menuitem.h>
#include <wx/msgdlg.h>
#include <wx/panel.h>
#include <wx/progdlg.h>
#include <wx/statusbr.h>
#include <wx/strconv.h>
#include <wx/string.h>
#include <wx/thread.h>
#include <wx/toolbar.h>
#include <wx/toplevel.h>
#include <wx/translation.h>
#include <wx/utils.h>
#include <wx/window.h>
#include <wx/aui/framemanager.h>

#ifdef __APPLE__
#include <AppKit/AppKit.h>
#endif

#include "Common/CDUtils.h"
#include "Common/CommonTypes.h"
#include "Common/FileSearch.h"
#include "Common/FileUtil.h"
#include "Common/NandPaths.h"

#include "Core/BootManager.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/CoreParameter.h"
#include "Core/Host.h"
#include "Core/Movie.h"
#include "Core/State.h"
#include "Core/HW/CPU.h"
#include "Core/HW/DVDInterface.h"
#include "Core/HW/GCPad.h"
#include "Core/HW/ProcessorInterface.h"
#include "Core/HW/SI_Device.h"
#include "Core/HW/Wiimote.h"
#include "Core/HW/WiiSaveCrypted.h"
#include "Core/IPC_HLE/WII_IPC_HLE_Device_usb.h"
#include "Core/IPC_HLE/WII_IPC_HLE_WiiMote.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/PowerPC/PPCSymbolDB.h"

#include "DiscIO/NANDContentLoader.h"

#include "DolphinWX/AboutDolphin.h"
#include "DolphinWX/ConfigMain.h"
#include "DolphinWX/FifoPlayerDlg.h"
#include "DolphinWX/Frame.h"
#include "DolphinWX/GameListCtrl.h"
#include "DolphinWX/Globals.h"
#include "DolphinWX/HotkeyDlg.h"
#include "DolphinWX/InputConfigDiag.h"
#include "DolphinWX/ISOFile.h"
#include "DolphinWX/LogWindow.h"
#include "DolphinWX/MemcardManager.h"
#include "DolphinWX/NetWindow.h"
#include "DolphinWX/TASInputDlg.h"
#include "DolphinWX/WiimoteConfigDiag.h"
#include "DolphinWX/WXInputBase.h"
#include "DolphinWX/WxUtils.h"
#include "DolphinWX/Cheats/CheatsWindow.h"
#include "DolphinWX/Debugger/BreakpointWindow.h"
#include "DolphinWX/Debugger/CodeWindow.h"
#include "DolphinWX/Debugger/WatchWindow.h"

#include "InputCommon/ControllerInterface/ControllerInterface.h"

#include "VideoBackends/Software/SWVideoConfig.h"

#include "VideoCommon/VideoBackendBase.h"
#include "VideoCommon/VideoConfig.h"

#ifdef _WIN32
#ifndef SM_XVIRTUALSCREEN
#define SM_XVIRTUALSCREEN 76
#endif
#ifndef SM_YVIRTUALSCREEN
#define SM_YVIRTUALSCREEN 77
#endif
#ifndef SM_CXVIRTUALSCREEN
#define SM_CXVIRTUALSCREEN 78
#endif
#ifndef SM_CYVIRTUALSCREEN
#define SM_CYVIRTUALSCREEN 79
#endif
#endif

// Resources
extern "C" {
#include "DolphinWX/resources/Dolphin.c" // NOLINT: Dolphin icon
};

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
wxMenuBar* CFrame::CreateMenu()
{
	wxMenuBar* menubar = new wxMenuBar();

	// file menu
	wxMenu* fileMenu = new wxMenu;
	fileMenu->Append(wxID_OPEN, GetMenuLabel(HK_OPEN));
	fileMenu->Append(IDM_CHANGEDISC, GetMenuLabel(HK_CHANGE_DISC));

	wxMenu *externalDrive = new wxMenu;
	fileMenu->Append(IDM_DRIVES, _("&Boot from DVD Drive..."), externalDrive);

	drives = cdio_get_devices();
	// Windows Limitation of 24 character drives
	for (unsigned int i = 0; i < drives.size() && i < 24; i++)
	{
		externalDrive->Append(IDM_DRIVE1 + i, StrToWxStr(drives[i]));
	}

	fileMenu->AppendSeparator();
	fileMenu->Append(wxID_REFRESH, GetMenuLabel(HK_REFRESH_LIST));
	fileMenu->AppendSeparator();
	fileMenu->Append(IDM_BROWSE, _("&Browse for ISOs..."));
	fileMenu->AppendSeparator();
	fileMenu->Append(wxID_EXIT, _("E&xit") + wxString("\tAlt+F4"));
	menubar->Append(fileMenu, _("&File"));

	// Emulation menu
	wxMenu* emulationMenu = new wxMenu;
	emulationMenu->Append(IDM_PLAY, GetMenuLabel(HK_PLAY_PAUSE));
	emulationMenu->Append(IDM_STOP, GetMenuLabel(HK_STOP));
	emulationMenu->Append(IDM_RESET, GetMenuLabel(HK_RESET));
	emulationMenu->AppendSeparator();
	emulationMenu->Append(IDM_TOGGLE_FULLSCREEN, GetMenuLabel(HK_FULLSCREEN));
	emulationMenu->Append(IDM_FRAMESTEP, GetMenuLabel(HK_FRAME_ADVANCE), wxEmptyString);

	wxMenu *skippingMenu = new wxMenu;
	emulationMenu->AppendSubMenu(skippingMenu, _("Frame S&kipping"));
	for (int i = 0; i < 10; i++)
		skippingMenu->Append(IDM_FRAMESKIP0 + i, wxString::Format("%i", i), wxEmptyString, wxITEM_RADIO);
	skippingMenu->Check(IDM_FRAMESKIP0 + SConfig::GetInstance().m_FrameSkip, true);
	Movie::SetFrameSkipping(SConfig::GetInstance().m_FrameSkip);

	emulationMenu->AppendSeparator();
	emulationMenu->Append(IDM_SCREENSHOT, GetMenuLabel(HK_SCREENSHOT));

	emulationMenu->AppendSeparator();
	wxMenu *saveMenu = new wxMenu;
	wxMenu *loadMenu = new wxMenu;
	wxMenu *slotSelectMenu = new wxMenu;
	emulationMenu->Append(IDM_LOADSTATE, _("&Load State"), loadMenu);
	emulationMenu->Append(IDM_SAVESTATE, _("Sa&ve State"), saveMenu);
	emulationMenu->Append(IDM_SELECTSLOT, _("Select State slot"), slotSelectMenu);

	saveMenu->Append(IDM_SAVESTATEFILE, GetMenuLabel(HK_SAVE_STATE_FILE));
	saveMenu->Append(IDM_SAVESELECTEDSLOT, GetMenuLabel(HK_SAVE_STATE_SLOT_SELECTED));
	saveMenu->Append(IDM_SAVEFIRSTSTATE, GetMenuLabel(HK_SAVE_FIRST_STATE));
	loadMenu->Append(IDM_UNDOSAVESTATE, GetMenuLabel(HK_UNDO_SAVE_STATE));
	saveMenu->AppendSeparator();

	loadMenu->Append(IDM_LOADSTATEFILE,  GetMenuLabel(HK_LOAD_STATE_FILE));
	loadMenu->Append(IDM_LOADSELECTEDSLOT, GetMenuLabel(HK_LOAD_STATE_SLOT_SELECTED));
	loadMenu->Append(IDM_UNDOLOADSTATE, GetMenuLabel(HK_UNDO_LOAD_STATE));
	loadMenu->AppendSeparator();

	for (unsigned int i = 1; i <= State::NUM_STATES; i++)
	{
		loadMenu->Append(IDM_LOADSLOT1 + i - 1, GetMenuLabel(HK_LOAD_STATE_SLOT_1 + i - 1));
		saveMenu->Append(IDM_SAVESLOT1 + i - 1, GetMenuLabel(HK_SAVE_STATE_SLOT_1 + i - 1));
		slotSelectMenu->Append(IDM_SELECTSLOT1 + i - 1, GetMenuLabel(HK_SELECT_STATE_SLOT_1 + i -1));
	}

	loadMenu->AppendSeparator();
	for (unsigned int i = 1; i <= State::NUM_STATES; i++)
		loadMenu->Append(IDM_LOADLAST1 + i - 1, GetMenuLabel(HK_LOAD_LAST_STATE_1 + i - 1));

	menubar->Append(emulationMenu, _("&Emulation"));

	// Movie menu
	wxMenu* movieMenu = new wxMenu;
	movieMenu->Append(IDM_RECORD, GetMenuLabel(HK_START_RECORDING));
	movieMenu->Append(IDM_PLAYRECORD, GetMenuLabel(HK_PLAY_RECORDING));
	movieMenu->Append(IDM_RECORDEXPORT, GetMenuLabel(HK_EXPORT_RECORDING));
	movieMenu->Append(IDM_RECORDREADONLY, GetMenuLabel(HK_READ_ONLY_MODE), wxEmptyString, wxITEM_CHECK);
	movieMenu->Append(IDM_TASINPUT, _("TAS Input"));
	movieMenu->AppendSeparator();
	movieMenu->AppendCheckItem(IDM_TOGGLE_PAUSEMOVIE, _("Pause at End of Movie"));
	movieMenu->Check(IDM_TOGGLE_PAUSEMOVIE, SConfig::GetInstance().m_PauseMovie);
	movieMenu->AppendCheckItem(IDM_SHOWLAG, _("Show Lag Counter"));
	movieMenu->Check(IDM_SHOWLAG, SConfig::GetInstance().m_ShowLag);
	movieMenu->AppendCheckItem(IDM_SHOWFRAMECOUNT, _("Show Frame Counter"));
	movieMenu->Check(IDM_SHOWFRAMECOUNT, SConfig::GetInstance().m_ShowFrameCount);
	movieMenu->Check(IDM_RECORDREADONLY, true);
	movieMenu->AppendCheckItem(IDM_SHOWINPUTDISPLAY, _("Show Input Display"));
	movieMenu->Check(IDM_SHOWINPUTDISPLAY, SConfig::GetInstance().m_ShowInputDisplay);
	movieMenu->AppendSeparator();
	movieMenu->AppendCheckItem(IDM_TOGGLE_DUMPFRAMES, _("Dump Frames"));
	movieMenu->Check(IDM_TOGGLE_DUMPFRAMES, SConfig::GetInstance().m_DumpFrames);
	movieMenu->AppendCheckItem(IDM_TOGGLE_DUMPAUDIO, _("Dump Audio"));
	movieMenu->Check(IDM_TOGGLE_DUMPAUDIO, SConfig::GetInstance().m_DumpAudio);
	menubar->Append(movieMenu, _("&Movie"));

	// Options menu
	wxMenu* pOptionsMenu = new wxMenu;
	pOptionsMenu->Append(wxID_PREFERENCES, _("Co&nfigure..."));
	pOptionsMenu->AppendSeparator();
	pOptionsMenu->Append(IDM_CONFIG_GFX_BACKEND, _("&Graphics Settings"));
	pOptionsMenu->Append(IDM_CONFIG_DSP_EMULATOR, _("&DSP Settings"));
	pOptionsMenu->Append(IDM_CONFIG_PAD_PLUGIN, _("GameCube &Pad Settings"));
	pOptionsMenu->Append(IDM_CONFIG_WIIMOTE_PLUGIN, _("&Wiimote Settings"));
	pOptionsMenu->Append(IDM_CONFIG_HOTKEYS, _("&Hotkey Settings"));
	if (g_pCodeWindow)
	{
		pOptionsMenu->AppendSeparator();
		g_pCodeWindow->CreateMenuOptions(pOptionsMenu);
	}
	menubar->Append(pOptionsMenu, _("&Options"));

	// Tools menu
	wxMenu* toolsMenu = new wxMenu;
	toolsMenu->Append(IDM_MEMCARD, _("&Memcard Manager (GC)"));
	toolsMenu->Append(IDM_IMPORTSAVE, _("Import Wii Save"));
	toolsMenu->Append(IDM_EXPORTALLSAVE, _("Export All Wii Saves"));
	toolsMenu->Append(IDM_CHEATS, _("&Cheats Manager"));

	toolsMenu->Append(IDM_NETPLAY, _("Start &NetPlay"));

	toolsMenu->Append(IDM_MENU_INSTALLWAD, _("Install WAD"));
	UpdateWiiMenuChoice(toolsMenu->Append(IDM_LOAD_WII_MENU, "Dummy string to keep wxw happy"));

	toolsMenu->Append(IDM_FIFOPLAYER, _("Fifo Player"));

	toolsMenu->AppendSeparator();
	wxMenu* wiimoteMenu = new wxMenu;
	toolsMenu->AppendSubMenu(wiimoteMenu, _("Connect Wiimotes"));
	wiimoteMenu->AppendCheckItem(IDM_CONNECT_WIIMOTE1, GetMenuLabel(HK_WIIMOTE1_CONNECT));
	wiimoteMenu->AppendCheckItem(IDM_CONNECT_WIIMOTE2, GetMenuLabel(HK_WIIMOTE2_CONNECT));
	wiimoteMenu->AppendCheckItem(IDM_CONNECT_WIIMOTE3, GetMenuLabel(HK_WIIMOTE3_CONNECT));
	wiimoteMenu->AppendCheckItem(IDM_CONNECT_WIIMOTE4, GetMenuLabel(HK_WIIMOTE4_CONNECT));
	wiimoteMenu->AppendSeparator();
	wiimoteMenu->AppendCheckItem(IDM_CONNECT_BALANCEBOARD, GetMenuLabel(HK_BALANCEBOARD_CONNECT));

	menubar->Append(toolsMenu, _("&Tools"));

	wxMenu* viewMenu = new wxMenu;
	viewMenu->AppendCheckItem(IDM_TOGGLE_TOOLBAR, _("Show &Toolbar"));
	viewMenu->Check(IDM_TOGGLE_TOOLBAR, SConfig::GetInstance().m_InterfaceToolbar);
	viewMenu->AppendCheckItem(IDM_TOGGLE_STATUSBAR, _("Show &Statusbar"));
	viewMenu->Check(IDM_TOGGLE_STATUSBAR, SConfig::GetInstance().m_InterfaceStatusbar);
	viewMenu->AppendSeparator();
	viewMenu->AppendCheckItem(IDM_LOGWINDOW, _("Show &Log"));
	viewMenu->AppendCheckItem(IDM_LOGCONFIGWINDOW, _("Show Log &Configuration"));
	viewMenu->AppendSeparator();

	if (g_pCodeWindow)
	{
		viewMenu->Check(IDM_LOGWINDOW, g_pCodeWindow->bShowOnStart[0]);

		const wxString MenuText[] = {
			wxTRANSLATE("&Registers"),
			wxTRANSLATE("&Watch"),
			wxTRANSLATE("&Breakpoints"),
			wxTRANSLATE("&Memory"),
			wxTRANSLATE("&JIT"),
			wxTRANSLATE("&Sound"),
			wxTRANSLATE("&Video")
		};

		for (int i = IDM_REGISTERWINDOW; i <= IDM_VIDEOWINDOW; i++)
		{
			viewMenu->AppendCheckItem(i, wxGetTranslation(MenuText[i - IDM_REGISTERWINDOW]));
			viewMenu->Check(i, g_pCodeWindow->bShowOnStart[i - IDM_LOGWINDOW]);
		}

		viewMenu->AppendSeparator();
	}
	else
	{
		viewMenu->Check(IDM_LOGWINDOW, SConfig::GetInstance().m_InterfaceLogWindow);
		viewMenu->Check(IDM_LOGCONFIGWINDOW, SConfig::GetInstance().m_InterfaceLogConfigWindow);
	}

	wxMenu *platformMenu = new wxMenu;
	viewMenu->AppendSubMenu(platformMenu, _("Show Platforms"));
	platformMenu->AppendCheckItem(IDM_LISTWII, _("Show Wii"));
	platformMenu->Check(IDM_LISTWII, SConfig::GetInstance().m_ListWii);
	platformMenu->AppendCheckItem(IDM_LISTGC, _("Show GameCube"));
	platformMenu->Check(IDM_LISTGC, SConfig::GetInstance().m_ListGC);
	platformMenu->AppendCheckItem(IDM_LISTWAD, _("Show Wad"));
	platformMenu->Check(IDM_LISTWAD, SConfig::GetInstance().m_ListWad);

	wxMenu *regionMenu = new wxMenu;
	viewMenu->AppendSubMenu(regionMenu, _("Show Regions"));
	regionMenu->AppendCheckItem(IDM_LISTJAP, _("Show JAP"));
	regionMenu->Check(IDM_LISTJAP, SConfig::GetInstance().m_ListJap);
	regionMenu->AppendCheckItem(IDM_LISTPAL, _("Show PAL"));
	regionMenu->Check(IDM_LISTPAL, SConfig::GetInstance().m_ListPal);
	regionMenu->AppendCheckItem(IDM_LISTUSA, _("Show USA"));
	regionMenu->Check(IDM_LISTUSA, SConfig::GetInstance().m_ListUsa);
	regionMenu->AppendSeparator();
	regionMenu->AppendCheckItem(IDM_LISTAUSTRALIA, _("Show Australia"));
	regionMenu->Check(IDM_LISTAUSTRALIA, SConfig::GetInstance().m_ListAustralia);
	regionMenu->AppendCheckItem(IDM_LISTFRANCE, _("Show France"));
	regionMenu->Check(IDM_LISTFRANCE, SConfig::GetInstance().m_ListFrance);
	regionMenu->AppendCheckItem(IDM_LISTGERMANY, _("Show Germany"));
	regionMenu->Check(IDM_LISTGERMANY, SConfig::GetInstance().m_ListGermany);
	regionMenu->AppendCheckItem(IDM_LISTINTERNATIONAL, _("Show International"));
	regionMenu->Check(IDM_LISTINTERNATIONAL, SConfig::GetInstance().m_ListInternational);
	regionMenu->AppendCheckItem(IDM_LISTITALY, _("Show Italy"));
	regionMenu->Check(IDM_LISTITALY, SConfig::GetInstance().m_ListItaly);
	regionMenu->AppendCheckItem(IDM_LISTKOREA, _("Show Korea"));
	regionMenu->Check(IDM_LISTKOREA, SConfig::GetInstance().m_ListKorea);
	regionMenu->AppendCheckItem(IDM_LISTNETHERLANDS, _("Show Netherlands"));
	regionMenu->Check(IDM_LISTNETHERLANDS, SConfig::GetInstance().m_ListNetherlands);
	regionMenu->AppendCheckItem(IDM_LISTRUSSIA, _("Show Russia"));
	regionMenu->Check(IDM_LISTRUSSIA, SConfig::GetInstance().m_ListRussia);
	regionMenu->AppendCheckItem(IDM_LISTSPAIN, _("Show Spain"));
	regionMenu->Check(IDM_LISTSPAIN, SConfig::GetInstance().m_ListSpain);
	regionMenu->AppendCheckItem(IDM_LISTTAIWAN, _("Show Taiwan"));
	regionMenu->Check(IDM_LISTTAIWAN, SConfig::GetInstance().m_ListTaiwan);
	regionMenu->AppendCheckItem(IDM_LIST_UNK, _("Show Unknown"));
	regionMenu->Check(IDM_LIST_UNK, SConfig::GetInstance().m_ListUnknown);

	viewMenu->AppendCheckItem(IDM_LISTDRIVES, _("Show Drives"));
	viewMenu->Check(IDM_LISTDRIVES, SConfig::GetInstance().m_ListDrives);
	viewMenu->Append(IDM_PURGECACHE, _("Purge Cache"));

	wxMenu *columnsMenu = new wxMenu;
	viewMenu->AppendSubMenu(columnsMenu, _("Select Columns"));
	columnsMenu->AppendCheckItem(IDM_SHOW_SYSTEM, _("Platform"));
	columnsMenu->Check(IDM_SHOW_SYSTEM, SConfig::GetInstance().m_showSystemColumn);
	columnsMenu->AppendCheckItem(IDM_SHOW_BANNER, _("Banner"));
	columnsMenu->Check(IDM_SHOW_BANNER, SConfig::GetInstance().m_showBannerColumn);
	columnsMenu->AppendCheckItem(IDM_SHOW_NOTES, _("Notes"));
	columnsMenu->Check(IDM_SHOW_NOTES, SConfig::GetInstance().m_showNotesColumn);
	columnsMenu->AppendCheckItem(IDM_SHOW_ID, _("Game ID"));
	columnsMenu->Check(IDM_SHOW_ID, SConfig::GetInstance().m_showIDColumn);
	columnsMenu->AppendCheckItem(IDM_SHOW_REGION, _("Region"));
	columnsMenu->Check(IDM_SHOW_REGION, SConfig::GetInstance().m_showRegionColumn);
	columnsMenu->AppendCheckItem(IDM_SHOW_SIZE, _("File Size"));
	columnsMenu->Check(IDM_SHOW_SIZE, SConfig::GetInstance().m_showSizeColumn);
	columnsMenu->AppendCheckItem(IDM_SHOW_STATE, _("State"));
	columnsMenu->Check(IDM_SHOW_STATE, SConfig::GetInstance().m_showStateColumn);



	menubar->Append(viewMenu, _("&View"));

	if (g_pCodeWindow)
	{
		g_pCodeWindow->CreateMenu(SConfig::GetInstance().m_LocalCoreStartupParameter, menubar);
	}

	// Help menu
	wxMenu* helpMenu = new wxMenu;
	// Re-enable when there's something useful to display */
	// helpMenu->Append(wxID_HELP, _("&Help"));
	helpMenu->Append(IDM_HELPWEBSITE, _("Dolphin &Web Site"));
	helpMenu->Append(IDM_HELPONLINEDOCS, _("Online &Documentation"));
	helpMenu->Append(IDM_HELPGITHUB, _("Dolphin at &GitHub"));
	helpMenu->AppendSeparator();
	helpMenu->Append(wxID_ABOUT, _("&About..."));
	menubar->Append(helpMenu, _("&Help"));

	return menubar;
}

wxString CFrame::GetMenuLabel(int Id)
{
	int hotkey = SConfig::GetInstance().\
		m_LocalCoreStartupParameter.iHotkey[Id];
	int hotkeymodifier = SConfig::GetInstance().\
		m_LocalCoreStartupParameter.iHotkeyModifier[Id];
	wxString Hotkey, Label, Modifier;

	switch (Id)
	{
		case HK_OPEN:
			Label = _("&Open...");
			break;
		case HK_CHANGE_DISC:
			Label = _("Change &Disc...");
			break;
		case HK_REFRESH_LIST:
			Label = _("&Refresh List");
			break;

		case HK_PLAY_PAUSE:
			if (Core::GetState() == Core::CORE_RUN)
				Label = _("&Pause");
			else
				Label = _("&Play");
			break;
		case HK_STOP:
			Label = _("&Stop");
			break;
		case HK_RESET:
			Label = _("&Reset");
			break;
		case HK_FRAME_ADVANCE:
			Label = _("&Frame Advance");
			break;

		case HK_START_RECORDING:
			Label = _("Start Re&cording Input");
			break;
		case HK_PLAY_RECORDING:
			Label = _("P&lay Input Recording...");
			break;
		case HK_EXPORT_RECORDING:
			Label = _("Export Recording...");
			break;
		case HK_READ_ONLY_MODE:
			Label = _("&Read-Only Mode");
			break;

		case HK_FULLSCREEN:
			Label = _("&Fullscreen");
			break;
		case HK_SCREENSHOT:
			Label = _("Take Screenshot");
			break;
		case HK_EXIT:
			Label = _("Exit");
			break;

		case HK_WIIMOTE1_CONNECT:
		case HK_WIIMOTE2_CONNECT:
		case HK_WIIMOTE3_CONNECT:
		case HK_WIIMOTE4_CONNECT:
			Label = wxString::Format(_("Connect Wiimote %i"),
					Id - HK_WIIMOTE1_CONNECT + 1);
			break;
		case HK_BALANCEBOARD_CONNECT:
			Label = _("Connect Balance Board");
			break;
		case HK_LOAD_STATE_SLOT_1:
		case HK_LOAD_STATE_SLOT_2:
		case HK_LOAD_STATE_SLOT_3:
		case HK_LOAD_STATE_SLOT_4:
		case HK_LOAD_STATE_SLOT_5:
		case HK_LOAD_STATE_SLOT_6:
		case HK_LOAD_STATE_SLOT_7:
		case HK_LOAD_STATE_SLOT_8:
		case HK_LOAD_STATE_SLOT_9:
		case HK_LOAD_STATE_SLOT_10:
			Label = wxString::Format(_("Slot %i"),
					Id - HK_LOAD_STATE_SLOT_1 + 1);
			break;

		case HK_SAVE_STATE_SLOT_1:
		case HK_SAVE_STATE_SLOT_2:
		case HK_SAVE_STATE_SLOT_3:
		case HK_SAVE_STATE_SLOT_4:
		case HK_SAVE_STATE_SLOT_5:
		case HK_SAVE_STATE_SLOT_6:
		case HK_SAVE_STATE_SLOT_7:
		case HK_SAVE_STATE_SLOT_8:
		case HK_SAVE_STATE_SLOT_9:
		case HK_SAVE_STATE_SLOT_10:
			Label = wxString::Format(_("Slot %i"),
					Id - HK_SAVE_STATE_SLOT_1 + 1);
			break;
		case HK_SAVE_STATE_FILE:
			Label = _("Save State...");
			break;

		case HK_LOAD_LAST_STATE_1:
		case HK_LOAD_LAST_STATE_2:
		case HK_LOAD_LAST_STATE_3:
		case HK_LOAD_LAST_STATE_4:
		case HK_LOAD_LAST_STATE_5:
		case HK_LOAD_LAST_STATE_6:
		case HK_LOAD_LAST_STATE_7:
		case HK_LOAD_LAST_STATE_8:
			Label = wxString::Format(_("Last %i"),
				Id - HK_LOAD_LAST_STATE_1 + 1);
			break;
		case HK_LOAD_STATE_FILE:
			Label = _("Load State...");
			break;

		case HK_SAVE_FIRST_STATE: Label = _("Save Oldest State"); break;
		case HK_UNDO_LOAD_STATE:  Label = _("Undo Load State");   break;
		case HK_UNDO_SAVE_STATE:  Label = _("Undo Save State");   break;

		case HK_SAVE_STATE_SLOT_SELECTED:
			Label = _("Save state to selected slot");
			break;

		case HK_LOAD_STATE_SLOT_SELECTED:
			Label = _("Load state from selected slot");
			break;

		case HK_SELECT_STATE_SLOT_1:
		case HK_SELECT_STATE_SLOT_2:
		case HK_SELECT_STATE_SLOT_3:
		case HK_SELECT_STATE_SLOT_4:
		case HK_SELECT_STATE_SLOT_5:
		case HK_SELECT_STATE_SLOT_6:
		case HK_SELECT_STATE_SLOT_7:
		case HK_SELECT_STATE_SLOT_8:
		case HK_SELECT_STATE_SLOT_9:
		case HK_SELECT_STATE_SLOT_10:
			Label = wxString::Format(_("Select Slot %i"), Id - HK_SELECT_STATE_SLOT_1 + 1);
			break;


		default:
			Label = wxString::Format(_("Undefined %i"), Id);
	}

	hotkeymodifier &= wxMOD_CONTROL | wxMOD_ALT | wxMOD_SHIFT;

	Modifier = WxUtils::WXKeymodToString(hotkeymodifier);
	Hotkey = WxUtils::WXKeyToString(hotkey);
	if (Modifier.Len() + Hotkey.Len() > 0)
		Label += '\t';

	return Label + Modifier + Hotkey;
}


// Create toolbar items
// ---------------------
void CFrame::PopulateToolbar(wxToolBar* ToolBar)
{
	int w = m_Bitmaps[Toolbar_FileOpen].GetWidth(),
		h = m_Bitmaps[Toolbar_FileOpen].GetHeight();
	ToolBar->SetToolBitmapSize(wxSize(w, h));


	WxUtils::AddToolbarButton(ToolBar, wxID_OPEN,                 _("Open"),     m_Bitmaps[Toolbar_FileOpen],   _("Open file..."));
	WxUtils::AddToolbarButton(ToolBar, wxID_REFRESH,              _("Refresh"),  m_Bitmaps[Toolbar_Refresh],    _("Refresh game list"));
	WxUtils::AddToolbarButton(ToolBar, IDM_BROWSE,                _("Browse"),   m_Bitmaps[Toolbar_Browse],     _("Browse for an ISO directory..."));
	ToolBar->AddSeparator();
	WxUtils::AddToolbarButton(ToolBar, IDM_PLAY,                  _("Play"),     m_Bitmaps[Toolbar_Play],       _("Play"));
	WxUtils::AddToolbarButton(ToolBar, IDM_STOP,                  _("Stop"),     m_Bitmaps[Toolbar_Stop],       _("Stop"));
	WxUtils::AddToolbarButton(ToolBar, IDM_TOGGLE_FULLSCREEN,     _("FullScr"),  m_Bitmaps[Toolbar_FullScreen], _("Toggle Fullscreen"));
	WxUtils::AddToolbarButton(ToolBar, IDM_SCREENSHOT,            _("ScrShot"),  m_Bitmaps[Toolbar_Screenshot], _("Take Screenshot"));
	ToolBar->AddSeparator();
	WxUtils::AddToolbarButton(ToolBar, wxID_PREFERENCES,          _("Config"),   m_Bitmaps[Toolbar_ConfigMain], _("Configure..."));
	WxUtils::AddToolbarButton(ToolBar, IDM_CONFIG_GFX_BACKEND,    _("Graphics"), m_Bitmaps[Toolbar_ConfigGFX],  _("Graphics settings"));
	WxUtils::AddToolbarButton(ToolBar, IDM_CONFIG_DSP_EMULATOR,   _("DSP"),      m_Bitmaps[Toolbar_ConfigDSP],  _("DSP settings"));
	WxUtils::AddToolbarButton(ToolBar, IDM_CONFIG_PAD_PLUGIN,     _("GCPad"),    m_Bitmaps[Toolbar_ConfigPAD],  _("GameCube Pad settings"));
	WxUtils::AddToolbarButton(ToolBar, IDM_CONFIG_WIIMOTE_PLUGIN, _("Wiimote"),  m_Bitmaps[Toolbar_Wiimote],    _("Wiimote settings"));
}


// Delete and recreate the toolbar
void CFrame::RecreateToolbar()
{
	static const long TOOLBAR_STYLE = wxTB_DEFAULT_STYLE | wxTB_TEXT;

	if (m_ToolBar != nullptr)
	{
		m_ToolBar->Destroy();
		m_ToolBar = nullptr;
	}

	m_ToolBar = CreateToolBar(TOOLBAR_STYLE, wxID_ANY);

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
	auto const dir = StrToWxStr(File::GetThemeDir(SConfig::GetInstance().m_LocalCoreStartupParameter.theme_name));

	m_Bitmaps[Toolbar_FileOpen  ].LoadFile(dir + "open.png",       wxBITMAP_TYPE_PNG);
	m_Bitmaps[Toolbar_Refresh   ].LoadFile(dir + "refresh.png",    wxBITMAP_TYPE_PNG);
	m_Bitmaps[Toolbar_Browse    ].LoadFile(dir + "browse.png",     wxBITMAP_TYPE_PNG);
	m_Bitmaps[Toolbar_Play      ].LoadFile(dir + "play.png",       wxBITMAP_TYPE_PNG);
	m_Bitmaps[Toolbar_Stop      ].LoadFile(dir + "stop.png",       wxBITMAP_TYPE_PNG);
	m_Bitmaps[Toolbar_Pause     ].LoadFile(dir + "pause.png",      wxBITMAP_TYPE_PNG);
	m_Bitmaps[Toolbar_ConfigMain].LoadFile(dir + "config.png",     wxBITMAP_TYPE_PNG);
	m_Bitmaps[Toolbar_ConfigGFX ].LoadFile(dir + "graphics.png",   wxBITMAP_TYPE_PNG);
	m_Bitmaps[Toolbar_ConfigDSP ].LoadFile(dir + "dsp.png",        wxBITMAP_TYPE_PNG);
	m_Bitmaps[Toolbar_ConfigPAD ].LoadFile(dir + "gcpad.png",      wxBITMAP_TYPE_PNG);
	m_Bitmaps[Toolbar_Wiimote   ].LoadFile(dir + "wiimote.png",    wxBITMAP_TYPE_PNG);
	m_Bitmaps[Toolbar_Screenshot].LoadFile(dir + "screenshot.png", wxBITMAP_TYPE_PNG);
	m_Bitmaps[Toolbar_FullScreen].LoadFile(dir + "fullscreen.png", wxBITMAP_TYPE_PNG);

	// Update in case the bitmap has been updated
	if (m_ToolBar != nullptr)
		RecreateToolbar();
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
	SCoreStartupParameter& StartUp = SConfig::GetInstance().m_LocalCoreStartupParameter;

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
		else if (!StartUp.m_strDefaultISO.empty() &&
		         File::Exists(StartUp.m_strDefaultISO))
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
			if (g_pCodeWindow->m_WatchWindow)
				g_pCodeWindow->m_WatchWindow->LoadAll();
			if (g_pCodeWindow->m_BreakpointWindow)
				g_pCodeWindow->m_BreakpointWindow->LoadAll();
		}
	}
}

// Open file to boot
void CFrame::OnOpen(wxCommandEvent& WXUNUSED (event))
{
	DoOpen(true);
}

void CFrame::DoOpen(bool Boot)
{
	std::string currentDir = File::GetCurrentDir();

	wxString path = wxFileSelector(
			_("Select the file to load"),
			wxEmptyString, wxEmptyString, wxEmptyString,
			_("All GC/Wii files (elf, dol, gcm, iso, wbfs, ciso, gcz, wad)") +
			wxString::Format("|*.elf;*.dol;*.gcm;*.iso;*.wbfs;*.ciso;*.gcz;*.wad;*.dff;*.tmd|%s",
				wxGetTranslation(wxALL_FILES)),
			wxFD_OPEN | wxFD_FILE_MUST_EXIST,
			this);

	if (path.IsEmpty())
		return;

	std::string currentDir2 = File::GetCurrentDir();

	if (currentDir != currentDir2)
	{
		PanicAlertT("Current directory changed from %s to %s after wxFileSelector!",
				currentDir.c_str(), currentDir2.c_str());
		File::SetCurrentDir(currentDir);
	}

	// Should we boot a new game or just change the disc?
	if (Boot && !path.IsEmpty())
	{
		BootGame(WxStrToStr(path));
	}
	else
	{
		DVDInterface::ChangeDisc(WxStrToStr(path));
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
		if (SConfig::GetInstance().m_SIDevice[i] != SIDEVICE_NONE && SConfig::GetInstance().m_SIDevice[i] != SIDEVICE_GC_GBA)
		{
			g_TASInputDlg[i]->CreateGCLayout();
			g_TASInputDlg[i]->Show(true);
			g_TASInputDlg[i]->SetTitle(wxString::Format(_("TAS Input - Controller %d"), i + 1));
		}

		if (g_wiimote_sources[i] == WIIMOTE_SRC_EMU && !(Core::IsRunning() && !SConfig::GetInstance().m_LocalCoreStartupParameter.bWii))
		{
			g_TASInputDlg[i+4]->CreateWiiLayout(i);
			g_TASInputDlg[i+4]->Show(true);
			g_TASInputDlg[i+4]->SetTitle(wxString::Format(_("TAS Input - Wiimote %d"), i + 1));
		}
	}
}

void CFrame::OnTogglePauseMovie(wxCommandEvent& WXUNUSED (event))
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

void CFrame::OnShowLag(wxCommandEvent& WXUNUSED (event))
{
	SConfig::GetInstance().m_ShowLag = !SConfig::GetInstance().m_ShowLag;
	SConfig::GetInstance().SaveSettings();
}

void CFrame::OnShowFrameCount(wxCommandEvent& WXUNUSED (event))
{
	SConfig::GetInstance().m_ShowFrameCount = !SConfig::GetInstance().m_ShowFrameCount;
	SConfig::GetInstance().SaveSettings();
}

void CFrame::OnShowInputDisplay(wxCommandEvent& WXUNUSED(event))
{
	SConfig::GetInstance().m_ShowInputDisplay = !SConfig::GetInstance().m_ShowInputDisplay;
	SConfig::GetInstance().SaveSettings();
}

void CFrame::OnFrameStep(wxCommandEvent& event)
{
	bool wasPaused = (Core::GetState() == Core::CORE_PAUSE);

	Movie::DoFrameStep();

	bool isPaused = (Core::GetState() == Core::CORE_PAUSE);
	if (isPaused && !wasPaused) // don't update on unpause, otherwise the status would be wrong when pausing next frame
		UpdateGUI();
}

void CFrame::OnChangeDisc(wxCommandEvent& WXUNUSED (event))
{
	DoOpen(false);
}

void CFrame::OnRecord(wxCommandEvent& WXUNUSED (event))
{
	if ((!Core::IsRunningAndStarted() && Core::IsRunning()) || Movie::IsRecordingInput() || Movie::IsPlayingInput())
		return;

	int controllers = 0;

	if (Movie::IsReadOnly())
	{
		// The user just chose to record a movie, so that should take precedence
		Movie::SetReadOnly(false);
		GetMenuBar()->FindItem(IDM_RECORDREADONLY)->Check(false);
	}

	for (int i = 0; i < 4; i++)
	{
		if (SConfig::GetInstance().m_SIDevice[i] == SIDEVICE_GC_CONTROLLER || SConfig::GetInstance().m_SIDevice[i] == SIDEVICE_GC_TARUKONGA)
			controllers |= (1 << i);

		if (g_wiimote_sources[i] != WIIMOTE_SRC_NONE)
			controllers |= (1 << (i + 4));
	}

	if (Movie::BeginRecordingInput(controllers))
		BootGame("");
}

void CFrame::OnPlayRecording(wxCommandEvent& WXUNUSED (event))
{
	wxString path = wxFileSelector(
			_("Select The Recording File"),
			wxEmptyString, wxEmptyString, wxEmptyString,
			_("Dolphin TAS Movies (*.dtm)") +
				wxString::Format("|*.dtm|%s", wxGetTranslation(wxALL_FILES)),
			wxFD_OPEN | wxFD_PREVIEW | wxFD_FILE_MUST_EXIST,
			this);

	if (path.IsEmpty())
		return;

	if (!Movie::IsReadOnly())
	{
		// let's make the read-only flag consistent at the start of a movie.
		Movie::SetReadOnly(true);
		GetMenuBar()->FindItem(IDM_RECORDREADONLY)->Check(true);
	}

	if (Movie::PlayInput(WxStrToStr(path)))
		BootGame("");
}

void CFrame::OnRecordExport(wxCommandEvent& WXUNUSED (event))
{
	DoRecordingSave();
}

void CFrame::OnPlay(wxCommandEvent& WXUNUSED (event))
{
	if (Core::IsRunning())
	{
		// Core is initialized and emulator is running
		if (UseDebugger)
		{
			if (CCPU::IsStepping())
				CCPU::EnableStepping(false);
			else
				CCPU::EnableStepping(true);  // Break

			wxThread::Sleep(20);
			g_pCodeWindow->JumpToAddress(PC);
			g_pCodeWindow->Update();
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
	if (Core::GetState() != Core::CORE_UNINITIALIZED &&
		!RendererIsFullscreen() && !m_RenderFrame->IsMaximized() && !m_RenderFrame->IsIconized())
	{
		SConfig::GetInstance().m_LocalCoreStartupParameter.iRenderWindowXPos = m_RenderFrame->GetPosition().x;
		SConfig::GetInstance().m_LocalCoreStartupParameter.iRenderWindowYPos = m_RenderFrame->GetPosition().y;
	}
	event.Skip();
}

void CFrame::OnRenderParentResize(wxSizeEvent& event)
{
	if (Core::GetState() != Core::CORE_UNINITIALIZED)
	{
		int width, height;
		if (!SConfig::GetInstance().m_LocalCoreStartupParameter.bRenderToMain &&
			!RendererIsFullscreen() && !m_RenderFrame->IsMaximized() && !m_RenderFrame->IsIconized())
		{
			m_RenderFrame->GetClientSize(&width, &height);
			SConfig::GetInstance().m_LocalCoreStartupParameter.iRenderWindowWidth = width;
			SConfig::GetInstance().m_LocalCoreStartupParameter.iRenderWindowHeight = height;
		}
		m_LogWindow->Refresh();
		m_LogWindow->Update();
	}
	event.Skip();
}

void CFrame::ToggleDisplayMode(bool bFullscreen)
{
#ifdef _WIN32
	if (bFullscreen && SConfig::GetInstance().m_LocalCoreStartupParameter.strFullscreenResolution != "Auto")
	{
		DEVMODE dmScreenSettings;
		memset(&dmScreenSettings,0,sizeof(dmScreenSettings));
		dmScreenSettings.dmSize = sizeof(dmScreenSettings);
		sscanf(SConfig::GetInstance().m_LocalCoreStartupParameter.strFullscreenResolution.c_str(),
				"%dx%d", &dmScreenSettings.dmPelsWidth, &dmScreenSettings.dmPelsHeight);
		dmScreenSettings.dmBitsPerPel = 32;
		dmScreenSettings.dmFields = DM_BITSPERPEL|DM_PELSWIDTH|DM_PELSHEIGHT;

		// Try To Set Selected Mode And Get Results.  NOTE: CDS_FULLSCREEN Gets Rid Of Start Bar.
		ChangeDisplaySettings(&dmScreenSettings, CDS_FULLSCREEN);
	}
	else
	{
		// Change to default resolution
		ChangeDisplaySettings(nullptr, CDS_FULLSCREEN);
	}
#elif defined(HAVE_XRANDR) && HAVE_XRANDR
	if (SConfig::GetInstance().m_LocalCoreStartupParameter.strFullscreenResolution != "Auto")
		m_XRRConfig->ToggleDisplayMode(bFullscreen);
#elif defined __APPLE__
	if (bFullscreen)
		CGDisplayHideCursor(CGMainDisplayID());
	else
		CGDisplayShowCursor(CGMainDisplayID());
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

	if (SConfig::GetInstance().m_LocalCoreStartupParameter.bRenderToMain)
	{
		// Game has been started, hide the game list
		m_GameListCtrl->Disable();
		m_GameListCtrl->Hide();

		m_RenderParent = m_Panel;
		m_RenderFrame = this;
		if (SConfig::GetInstance().m_LocalCoreStartupParameter.bKeepWindowOnTop)
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
		wxPoint position(SConfig::GetInstance().m_LocalCoreStartupParameter.iRenderWindowXPos,
				SConfig::GetInstance().m_LocalCoreStartupParameter.iRenderWindowYPos);
#ifdef __APPLE__
		// On OS X, the render window's title bar is not visible,
		// and the window therefore not easily moved, when the
		// position is 0,0. Weed out the 0's from existing configs.
		if (position == wxPoint(0, 0))
			position = wxDefaultPosition;
#endif

		wxSize size(SConfig::GetInstance().m_LocalCoreStartupParameter.iRenderWindowWidth,
				SConfig::GetInstance().m_LocalCoreStartupParameter.iRenderWindowHeight);
#ifdef _WIN32
		// Out of desktop check
		int leftPos = GetSystemMetrics(SM_XVIRTUALSCREEN);
		int topPos = GetSystemMetrics(SM_YVIRTUALSCREEN);
		int width =  GetSystemMetrics(SM_CXVIRTUALSCREEN);
		int height = GetSystemMetrics(SM_CYVIRTUALSCREEN);
		if ((leftPos + width) < (position.x + size.GetWidth()) || leftPos > position.x || (topPos + height) < (position.y + size.GetHeight()) || topPos > position.y)
			position.x = position.y = wxDefaultCoord;
#endif
		m_RenderFrame = new CRenderFrame((wxFrame*)this, wxID_ANY, _("Dolphin"), position);
		if (SConfig::GetInstance().m_LocalCoreStartupParameter.bKeepWindowOnTop)
			m_RenderFrame->SetWindowStyle(m_RenderFrame->GetWindowStyle() | wxSTAY_ON_TOP);
		else
			m_RenderFrame->SetWindowStyle(m_RenderFrame->GetWindowStyle() & ~wxSTAY_ON_TOP);

		m_RenderFrame->SetBackgroundColour(*wxBLACK);
		m_RenderFrame->SetClientSize(size.GetWidth(), size.GetHeight());
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
	NSView *view = (NSView *) m_RenderFrame->GetHandle();
	NSWindow *window = [view window];

	[window setCollectionBehavior:NSWindowCollectionBehaviorFullScreenPrimary];
#endif

	wxBeginBusyCursor();

	DoFullscreen(SConfig::GetInstance().m_LocalCoreStartupParameter.bFullscreen);

	if (!BootManager::BootCore(filename))
	{
		DoFullscreen(false);
		// Destroy the renderer frame when not rendering to main
		if (!SConfig::GetInstance().m_LocalCoreStartupParameter.bRenderToMain)
			m_RenderFrame->Destroy();
		m_RenderParent = nullptr;
		m_bGameLoading = false;
		UpdateGUI();
	}
	else
	{
#if defined(HAVE_X11) && HAVE_X11
	if (SConfig::GetInstance().m_LocalCoreStartupParameter.bDisableScreenSaver)
		X11Utils::InhibitScreensaver(X11Utils::XDisplayFromHandle(GetHandle()),
				X11Utils::XWindowFromHandle(GetHandle()), true);
#endif

		m_RenderParent->SetFocus();

		wxTheApp->Bind(wxEVT_KEY_DOWN,    &CFrame::OnKeyDown, this);
		wxTheApp->Bind(wxEVT_KEY_UP,      &CFrame::OnKeyUp,   this);
		wxTheApp->Bind(wxEVT_RIGHT_DOWN,  &CFrame::OnMouse,   this);
		wxTheApp->Bind(wxEVT_RIGHT_UP,    &CFrame::OnMouse,   this);
		wxTheApp->Bind(wxEVT_MIDDLE_DOWN, &CFrame::OnMouse,   this);
		wxTheApp->Bind(wxEVT_MIDDLE_UP,   &CFrame::OnMouse,   this);
		wxTheApp->Bind(wxEVT_MOTION,      &CFrame::OnMouse,   this);
		m_RenderParent->Bind(wxEVT_SIZE, &CFrame::OnRenderParentResize, this);
	}

	wxEndBusyCursor();
}

void CFrame::OnBootDrive(wxCommandEvent& event)
{
	BootGame(drives[event.GetId()-IDM_DRIVE1]);
}

// Refresh the file list and browse for a favorites directory
void CFrame::OnRefresh(wxCommandEvent& WXUNUSED (event))
{
	if (m_GameListCtrl)
	{
		m_GameListCtrl->Update();
	}
}


void CFrame::OnBrowse(wxCommandEvent& WXUNUSED (event))
{
	if (m_GameListCtrl)
	{
		m_GameListCtrl->BrowseForDirectory();
	}
}

// Create screenshot
void CFrame::OnScreenshot(wxCommandEvent& WXUNUSED (event))
{
	Core::SaveScreenShot();
}

// Pause the emulation
void CFrame::DoPause()
{
	if (Core::GetState() == Core::CORE_RUN)
	{
		Core::SetState(Core::CORE_PAUSE);
		if (SConfig::GetInstance().m_LocalCoreStartupParameter.bHideCursor)
			m_RenderParent->SetCursor(wxNullCursor);
		Core::UpdateTitle();
	}
	else
	{
		Core::SetState(Core::CORE_RUN);
		if (SConfig::GetInstance().m_LocalCoreStartupParameter.bHideCursor &&
				RendererHasFocus())
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
	if (Core::GetState() != Core::CORE_UNINITIALIZED ||
			m_RenderParent != nullptr)
	{
#if defined __WXGTK__
		wxMutexGuiLeave();
		std::lock_guard<std::recursive_mutex> lk(keystate_lock);
		wxMutexGuiEnter();
#endif
		// Ask for confirmation in case the user accidentally clicked Stop / Escape
		if (SConfig::GetInstance().m_LocalCoreStartupParameter.bConfirmStop)
		{
			// Exit fullscreen to ensure it does not cover the stop dialog.
			DoFullscreen(false);

			// Pause the state during confirmation and restore it afterwards
			Core::EState state = Core::GetState();

			// If exclusive fullscreen is not enabled then we can pause the emulation
			// before we've exited fullscreen. If not then we need to exit fullscreen first.
			if (!RendererIsFullscreen() || !g_Config.ExclusiveFullscreenEnabled() ||
				SConfig::GetInstance().m_LocalCoreStartupParameter.bRenderToMain)
			{
				Core::SetState(Core::CORE_PAUSE);
			}

			wxMessageDialog m_StopDlg(
				this,
				_("Do you want to stop the current emulation?"),
				_("Please confirm..."),
				wxYES_NO | wxSTAY_ON_TOP | wxICON_EXCLAMATION,
				wxDefaultPosition);

			int Ret = m_StopDlg.ShowModal();
			if (Ret != wxID_YES)
			{
				Core::SetState(state);
				m_confirmStop = false;
				return;
			}
		}

		if (UseDebugger && g_pCodeWindow)
		{
			if (g_pCodeWindow->m_WatchWindow)
			{
				g_pCodeWindow->m_WatchWindow->SaveAll();
				PowerPC::watches.Clear();
			}
			if (g_pCodeWindow->m_BreakpointWindow)
			{
				g_pCodeWindow->m_BreakpointWindow->SaveAll();
				PowerPC::breakpoints.Clear();
				PowerPC::memchecks.Clear();
				g_pCodeWindow->m_BreakpointWindow->NotifyUpdate();
			}
			g_symbolDB.Clear();
			Host_NotifyMapLoaded();
		}

		// TODO: Show the author/description dialog here
		if (Movie::IsRecordingInput())
			DoRecordingSave();
		if (Movie::IsMovieActive())
			Movie::EndPlayInput(false);
		NetPlay::StopGame();

		wxBeginBusyCursor();
		BootManager::Stop();
		UpdateGUI();
	}
}

void CFrame::OnStopped()
{
	wxEndBusyCursor();

	m_confirmStop = false;

#if defined(HAVE_X11) && HAVE_X11
	if (SConfig::GetInstance().m_LocalCoreStartupParameter.bDisableScreenSaver)
		X11Utils::InhibitScreensaver(X11Utils::XDisplayFromHandle(GetHandle()),
				X11Utils::XWindowFromHandle(GetHandle()), false);
#endif
	m_RenderFrame->SetTitle(StrToWxStr(scm_rev_str));

	// Destroy the renderer frame when not rendering to main
	m_RenderParent->Unbind(wxEVT_SIZE, &CFrame::OnRenderParentResize, this);

	// Keyboard
	wxTheApp->Unbind(wxEVT_KEY_DOWN,    &CFrame::OnKeyDown, this);
	wxTheApp->Unbind(wxEVT_KEY_UP,      &CFrame::OnKeyUp,   this);

	// Mouse
	wxTheApp->Unbind(wxEVT_RIGHT_DOWN,  &CFrame::OnMouse, this);
	wxTheApp->Unbind(wxEVT_RIGHT_UP,    &CFrame::OnMouse, this);
	wxTheApp->Unbind(wxEVT_MIDDLE_DOWN, &CFrame::OnMouse, this);
	wxTheApp->Unbind(wxEVT_MIDDLE_UP,   &CFrame::OnMouse, this);
	wxTheApp->Unbind(wxEVT_MOTION,      &CFrame::OnMouse, this);
	if (SConfig::GetInstance().m_LocalCoreStartupParameter.bHideCursor)
		m_RenderParent->SetCursor(wxNullCursor);
	DoFullscreen(false);
	if (!SConfig::GetInstance().m_LocalCoreStartupParameter.bRenderToMain)
	{
		m_RenderFrame->Destroy();
	}
	else
	{
#if defined(__APPLE__)
		// Disable the full screen button when not in a game.
		NSView *view = (NSView *)m_RenderFrame->GetHandle();
		NSWindow *window = [view window];

		[window setCollectionBehavior : NSWindowCollectionBehaviorDefault];
#endif

		// Make sure the window is not longer set to stay on top
		m_RenderFrame->SetWindowStyle(m_RenderFrame->GetWindowStyle() & ~wxSTAY_ON_TOP);
	}
	m_RenderParent = nullptr;

	// Clean framerate indications from the status bar.
	GetStatusBar()->SetStatusText(" ", 0);

	// Clear wiimote connection status from the status bar.
	GetStatusBar()->SetStatusText(" ", 1);

	// If batch mode was specified on the command-line or we were already closing, exit now.
	if (m_bBatchMode || m_bClosing)
		Close(true);

	// If using auto size with render to main, reset the application size.
	if (SConfig::GetInstance().m_LocalCoreStartupParameter.bRenderToMain &&
		SConfig::GetInstance().m_LocalCoreStartupParameter.bRenderWindowAutoSize)
		SetSize(SConfig::GetInstance().m_LocalCoreStartupParameter.iWidth,
		SConfig::GetInstance().m_LocalCoreStartupParameter.iHeight);

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

	wxString path = wxFileSelector(
			_("Select The Recording File"),
			wxEmptyString, wxEmptyString, wxEmptyString,
			_("Dolphin TAS Movies (*.dtm)") +
				wxString::Format("|*.dtm|%s", wxGetTranslation(wxALL_FILES)),
			wxFD_SAVE | wxFD_PREVIEW | wxFD_OVERWRITE_PROMPT,
			this);

	if (path.IsEmpty())
		return;

	Movie::SaveRecording(WxStrToStr(path));

	if (!paused)
		DoPause();
}

void CFrame::OnStop(wxCommandEvent& WXUNUSED (event))
{
	DoStop();
}

void CFrame::OnReset(wxCommandEvent& WXUNUSED (event))
{
	if (Movie::IsRecordingInput())
		Movie::g_bReset = true;
	ProcessorInterface::ResetButton_Tap();
}

void CFrame::OnConfigMain(wxCommandEvent& WXUNUSED (event))
{
	CConfigMain ConfigMain(this);
	if (ConfigMain.ShowModal() == wxID_OK)
		m_GameListCtrl->Update();
	UpdateGUI();
}

void CFrame::OnConfigGFX(wxCommandEvent& WXUNUSED (event))
{
	if (g_video_backend)
		g_video_backend->ShowConfig(this);
}

void CFrame::OnConfigDSP(wxCommandEvent& WXUNUSED (event))
{
	CConfigMain ConfigMain(this);
	ConfigMain.SetSelectedTab(CConfigMain::ID_AUDIOPAGE);
	if (ConfigMain.ShowModal() == wxID_OK)
		m_GameListCtrl->Update();
}

void CFrame::OnConfigPAD(wxCommandEvent& WXUNUSED (event))
{
	InputConfig* const pad_plugin = Pad::GetConfig();
	bool was_init = false;
	if (g_controller_interface.IsInit()) // check if game is running
	{
		was_init = true;
	}
	else
	{
#if defined(HAVE_X11) && HAVE_X11
		Window win = X11Utils::XWindowFromHandle(GetHandle());
		Pad::Initialize(reinterpret_cast<void*>(win));
#else
		Pad::Initialize(reinterpret_cast<void*>(GetHandle()));
#endif
	}
	InputConfigDialog m_ConfigFrame(this, *pad_plugin, _trans("Dolphin GCPad Configuration"));
	m_ConfigFrame.ShowModal();
	m_ConfigFrame.Destroy();
	if (!was_init) // if game isn't running
	{
		Pad::Shutdown();
	}
}

void CFrame::OnConfigWiimote(wxCommandEvent& WXUNUSED (event))
{
	InputConfig* const wiimote_plugin = Wiimote::GetConfig();
	bool was_init = false;
	if (g_controller_interface.IsInit()) // check if game is running
	{
		was_init = true;
	}
	else
	{
#if defined(HAVE_X11) && HAVE_X11
		Window win = X11Utils::XWindowFromHandle(GetHandle());
		Wiimote::Initialize(reinterpret_cast<void*>(win));
#else
		Wiimote::Initialize(reinterpret_cast<void*>(GetHandle()));
#endif
	}
	WiimoteConfigDiag m_ConfigFrame(this, *wiimote_plugin);
	m_ConfigFrame.ShowModal();
	m_ConfigFrame.Destroy();
	if (!was_init) // if game isn't running
	{
		Wiimote::Shutdown();
	}
}

void CFrame::OnConfigHotkey(wxCommandEvent& WXUNUSED (event))
{
	HotkeyConfigDialog *m_HotkeyDialog = new HotkeyConfigDialog(this);
	m_HotkeyDialog->ShowModal();
	m_HotkeyDialog->Destroy();
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
			frame.ShowModal();
		}
		break;
	case IDM_HELPWEBSITE:
		WxUtils::Launch("https://dolphin-emu.org/");
		break;
	case IDM_HELPONLINEDOCS:
		WxUtils::Launch("https://dolphin-emu.org/docs/guides/");
		break;
	case IDM_HELPGITHUB:
		WxUtils::Launch("https://github.com/dolphin-emu/dolphin/");
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

void CFrame::StatusBarMessage(const char * Text, ...)
{
	const int MAX_BYTES = 1024*10;
	char Str[MAX_BYTES];
	va_list ArgPtr;
	va_start(ArgPtr, Text);
	vsnprintf(Str, MAX_BYTES, Text, ArgPtr);
	va_end(ArgPtr);

	if (this->GetStatusBar()->IsEnabled())
	{
		this->GetStatusBar()->SetStatusText(StrToWxStr(Str),0);
	}
}


// Miscellaneous menus
// ---------------------
// NetPlay stuff
void CFrame::OnNetPlay(wxCommandEvent& WXUNUSED (event))
{
	if (!g_NetPlaySetupDiag)
	{
		if (NetPlayDiag::GetInstance() != nullptr)
			NetPlayDiag::GetInstance()->Raise();
		else
			g_NetPlaySetupDiag = new NetPlaySetupDiag(this, m_GameListCtrl);
	}
	else
	{
		g_NetPlaySetupDiag->Raise();
	}
}

void CFrame::OnMemcard(wxCommandEvent& WXUNUSED (event))
{
	CMemcardManager MemcardManager(this);
	MemcardManager.ShowModal();
}

void CFrame::OnExportAllSaves(wxCommandEvent& WXUNUSED (event))
{
	CWiiSaveCrypted::ExportAllSaves();
}

void CFrame::OnImportSave(wxCommandEvent& WXUNUSED (event))
{
	wxString path = wxFileSelector(_("Select the save file"),
			wxEmptyString, wxEmptyString, wxEmptyString,
			_("Wii save files (*.bin)|*.bin"),
			wxFD_OPEN | wxFD_PREVIEW | wxFD_FILE_MUST_EXIST,
			this);

	if (!path.IsEmpty())
	{
		CWiiSaveCrypted::ImportWiiSave(WxStrToStr(path));
	}
}

void CFrame::OnShow_CheatsWindow(wxCommandEvent& WXUNUSED (event))
{
	if (!g_CheatsWindow)
		g_CheatsWindow = new wxCheatsWindow(this);
	else
		g_CheatsWindow->Raise();
}

void CFrame::OnLoadWiiMenu(wxCommandEvent& WXUNUSED(event))
{
	BootGame(Common::GetTitleContentPath(TITLEID_SYSMENU));
}

void CFrame::OnInstallWAD(wxCommandEvent& event)
{
	std::string fileName;

	switch (event.GetId())
	{
	case IDM_LIST_INSTALLWAD:
	{
		const GameListItem *iso = m_GameListCtrl->GetSelectedISO();
		if (!iso)
			return;
		fileName = iso->GetFileName();
		break;
	}
	case IDM_MENU_INSTALLWAD:
	{
		wxString path = wxFileSelector(
			_("Select a Wii WAD file to install"),
			wxEmptyString, wxEmptyString, wxEmptyString,
			"Wii WAD file (*.wad)|*.wad",
			wxFD_OPEN | wxFD_PREVIEW | wxFD_FILE_MUST_EXIST,
			this);
		fileName = WxStrToStr(path);
		break;
	}
	default:
		return;
	}

	wxProgressDialog dialog(_("Installing WAD..."),
		_("Working..."),
		1000,
		this,
		wxPD_APP_MODAL |
		wxPD_ELAPSED_TIME | wxPD_ESTIMATED_TIME | wxPD_REMAINING_TIME |
		wxPD_SMOOTH
		);

	u64 titleID = DiscIO::CNANDContentManager::Access().Install_WiiWAD(fileName);
	if (titleID == TITLEID_SYSMENU)
	{
		UpdateWiiMenuChoice();
	}
}


void CFrame::UpdateWiiMenuChoice(wxMenuItem *WiiMenuItem)
{
	if (!WiiMenuItem)
	{
		WiiMenuItem = GetMenuBar()->FindItem(IDM_LOAD_WII_MENU);
	}

	const DiscIO::INANDContentLoader & SysMenu_Loader = DiscIO::CNANDContentManager::Access().GetNANDLoader(TITLEID_SYSMENU, true);
	if (SysMenu_Loader.IsValid())
	{
		int sysmenuVersion = SysMenu_Loader.GetTitleVersion();
		char sysmenuRegion = SysMenu_Loader.GetCountryChar();
		WiiMenuItem->Enable();
		WiiMenuItem->SetItemLabel(wxString::Format(_("Load Wii System Menu %d%c"), sysmenuVersion, sysmenuRegion));
	}
	else
	{
		WiiMenuItem->Enable(false);
		WiiMenuItem->SetItemLabel(_("Load Wii System Menu"));
	}
}

void CFrame::OnFifoPlayer(wxCommandEvent& WXUNUSED (event))
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
	if (Core::IsRunning() && SConfig::GetInstance().m_LocalCoreStartupParameter.bWii)
	{
		GetUsbPointer()->AccessWiiMote(wm_idx | 0x100)->Activate(connect);
		wxString msg(wxString::Format(_("Wiimote %i %s"), wm_idx + 1,
					connect ? _("Connected") : _("Disconnected")));
		Core::DisplayMessage(WxStrToStr(msg), 3000);
		Host_UpdateMainFrame();
	}
}

void CFrame::OnConnectWiimote(wxCommandEvent& event)
{
	ConnectWiimote(event.GetId() - IDM_CONNECT_WIIMOTE1, !GetUsbPointer()->AccessWiiMote((event.GetId() - IDM_CONNECT_WIIMOTE1) | 0x100)->IsConnected());
}

// Toggle fullscreen. In Windows the fullscreen mode is accomplished by expanding the m_Panel to cover
// the entire screen (when we render to the main window).
void CFrame::OnToggleFullscreen(wxCommandEvent& WXUNUSED (event))
{
	DoFullscreen(!RendererIsFullscreen());
}

void CFrame::OnToggleDualCore(wxCommandEvent& WXUNUSED (event))
{
	SConfig::GetInstance().m_LocalCoreStartupParameter.bCPUThread = !SConfig::GetInstance().m_LocalCoreStartupParameter.bCPUThread;
	SConfig::GetInstance().SaveSettings();
}

void CFrame::OnToggleSkipIdle(wxCommandEvent& WXUNUSED (event))
{
	SConfig::GetInstance().m_LocalCoreStartupParameter.bSkipIdle = !SConfig::GetInstance().m_LocalCoreStartupParameter.bSkipIdle;
	SConfig::GetInstance().SaveSettings();
}

void CFrame::OnLoadStateFromFile(wxCommandEvent& WXUNUSED (event))
{
	wxString path = wxFileSelector(
		_("Select the state to load"),
		wxEmptyString, wxEmptyString, wxEmptyString,
		_("All Save States (sav, s##)") +
			wxString::Format("|*.sav;*.s??|%s", wxGetTranslation(wxALL_FILES)),
		wxFD_OPEN | wxFD_PREVIEW | wxFD_FILE_MUST_EXIST,
		this);

	if (!path.IsEmpty())
		State::LoadAs(WxStrToStr(path));
}

void CFrame::OnSaveStateToFile(wxCommandEvent& WXUNUSED (event))
{
	wxString path = wxFileSelector(
		_("Select the state to save"),
		wxEmptyString, wxEmptyString, wxEmptyString,
		_("All Save States (sav, s##)") +
			wxString::Format("|*.sav;*.s??|%s", wxGetTranslation(wxALL_FILES)),
		wxFD_SAVE,
		this);

	if (!path.IsEmpty())
		State::SaveAs(WxStrToStr(path));
}

void CFrame::OnLoadLastState(wxCommandEvent& event)
{
	if (Core::IsRunningAndStarted())
	{
		int id = event.GetId();
		int slot = id - IDM_LOADLAST1 + 1;
		State::LoadLastSaved(slot);
	}
}

void CFrame::OnSaveFirstState(wxCommandEvent& WXUNUSED(event))
{
	if (Core::IsRunningAndStarted())
		State::SaveFirstSaved();
}

void CFrame::OnUndoLoadState(wxCommandEvent& WXUNUSED (event))
{
	if (Core::IsRunningAndStarted())
		State::UndoLoadState();
}

void CFrame::OnUndoSaveState(wxCommandEvent& WXUNUSED (event))
{
	if (Core::IsRunningAndStarted())
		State::UndoSaveState();
}


void CFrame::OnLoadState(wxCommandEvent& event)
{
	if (Core::IsRunningAndStarted())
	{
		int id = event.GetId();
		int slot = id - IDM_LOADSLOT1 + 1;
		State::Load(slot);
	}
}

void CFrame::OnSaveState(wxCommandEvent& event)
{
	if (Core::IsRunningAndStarted())
	{
		int id = event.GetId();
		int slot = id - IDM_SAVESLOT1 + 1;
		State::Save(slot);
	}
}

void CFrame::OnFrameSkip(wxCommandEvent& event)
{
	int amount = event.GetId() - IDM_FRAMESKIP0;

	Movie::SetFrameSkipping((unsigned int)amount);
	SConfig::GetInstance().m_FrameSkip = amount;
}

void CFrame::OnSelectSlot(wxCommandEvent& event)
{
	g_saveSlot = event.GetId() - IDM_SELECTSLOT1 + 1;
	Core::DisplayMessage(StringFromFormat("Selected slot %d", g_saveSlot), 1000);
}

void CFrame::OnLoadCurrentSlot(wxCommandEvent& event)
{
	if (Core::IsRunningAndStarted())
	{
		State::Load(g_saveSlot);
	}
}

void CFrame::OnSaveCurrentSlot(wxCommandEvent& event)
{
	if (Core::IsRunningAndStarted())
	{
		State::Save(g_saveSlot);
	}
}



// GUI
// ---------------------

// Update the enabled/disabled status
void CFrame::UpdateGUI()
{
	// Save status
	bool Initialized     = Core::IsRunning();
	bool Running         = Core::GetState() == Core::CORE_RUN;
	bool Paused          = Core::GetState() == Core::CORE_PAUSE;
	bool Stopping        = Core::GetState() == Core::CORE_STOPPING;
	bool RunningWii      = Initialized && SConfig::GetInstance().m_LocalCoreStartupParameter.bWii;
	bool RunningGamecube = Initialized && !SConfig::GetInstance().m_LocalCoreStartupParameter.bWii;

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
		// Don't allow wiimote config while in GameCube mode
		m_ToolBar->EnableTool(IDM_CONFIG_WIIMOTE_PLUGIN, !RunningGamecube);
	}

	// File
	GetMenuBar()->FindItem(wxID_OPEN)->Enable(!Initialized);
	GetMenuBar()->FindItem(IDM_DRIVES)->Enable(!Initialized);
	GetMenuBar()->FindItem(wxID_REFRESH)->Enable(!Initialized);
	GetMenuBar()->FindItem(IDM_BROWSE)->Enable(!Initialized);

	// Emulation
	GetMenuBar()->FindItem(IDM_STOP)->Enable(Running || Paused);
	GetMenuBar()->FindItem(IDM_RESET)->Enable(Running || Paused);
	GetMenuBar()->FindItem(IDM_RECORD)->Enable(!Movie::IsRecordingInput());
	GetMenuBar()->FindItem(IDM_PLAYRECORD)->Enable(!Initialized);
	GetMenuBar()->FindItem(IDM_RECORDEXPORT)->Enable(Movie::IsMovieActive());
	GetMenuBar()->FindItem(IDM_FRAMESTEP)->Enable(Running || Paused);
	GetMenuBar()->FindItem(IDM_SCREENSHOT)->Enable(Running || Paused);
	GetMenuBar()->FindItem(IDM_TOGGLE_FULLSCREEN)->Enable(Running || Paused);

	// Update Menu Accelerators
	for (unsigned int i = 0; i < NUM_HOTKEYS; i++)
	{
		if (GetCmdForHotkey(i) == -1)
			continue;
		if (GetMenuBar()->FindItem(GetCmdForHotkey(i)))
			GetMenuBar()->FindItem(GetCmdForHotkey(i))->SetItemLabel(GetMenuLabel(i));
	}

	GetMenuBar()->FindItem(IDM_LOADSTATE)->Enable(Initialized);
	GetMenuBar()->FindItem(IDM_SAVESTATE)->Enable(Initialized);
	// Misc
	GetMenuBar()->FindItem(IDM_CHANGEDISC)->Enable(Initialized);
	if (DiscIO::CNANDContentManager::Access().GetNANDLoader(TITLEID_SYSMENU).IsValid())
		GetMenuBar()->FindItem(IDM_LOAD_WII_MENU)->Enable(!Initialized);

	// Tools
	GetMenuBar()->FindItem(IDM_CHEATS)->Enable(SConfig::GetInstance().m_LocalCoreStartupParameter.bEnableCheats);

	GetMenuBar()->FindItem(IDM_CONNECT_WIIMOTE1)->Enable(RunningWii);
	GetMenuBar()->FindItem(IDM_CONNECT_WIIMOTE2)->Enable(RunningWii);
	GetMenuBar()->FindItem(IDM_CONNECT_WIIMOTE3)->Enable(RunningWii);
	GetMenuBar()->FindItem(IDM_CONNECT_WIIMOTE4)->Enable(RunningWii);
	GetMenuBar()->FindItem(IDM_CONNECT_BALANCEBOARD)->Enable(RunningWii);
	GetMenuBar()->FindItem(IDM_CONFIG_WIIMOTE_PLUGIN)->Enable(!RunningGamecube);
	if (RunningWii)
	{
		GetMenuBar()->FindItem(IDM_CONNECT_WIIMOTE1)->Check(GetUsbPointer()->
				AccessWiiMote(0x0100)->IsConnected());
		GetMenuBar()->FindItem(IDM_CONNECT_WIIMOTE2)->Check(GetUsbPointer()->
				AccessWiiMote(0x0101)->IsConnected());
		GetMenuBar()->FindItem(IDM_CONNECT_WIIMOTE3)->Check(GetUsbPointer()->
				AccessWiiMote(0x0102)->IsConnected());
		GetMenuBar()->FindItem(IDM_CONNECT_WIIMOTE4)->Check(GetUsbPointer()->
				AccessWiiMote(0x0103)->IsConnected());
		GetMenuBar()->FindItem(IDM_CONNECT_BALANCEBOARD)->Check(GetUsbPointer()->
				AccessWiiMote(0x0104)->IsConnected());
	}

	if (m_ToolBar)
	{
		// Get the tool that controls pausing/playing
		wxToolBarToolBase * PlayTool = m_ToolBar->FindById(IDM_PLAY);

		if (PlayTool)
		{
			if (Running)
			{
				PlayTool->SetLabel(_("Pause"));
				PlayTool->SetShortHelp(_("Pause"));
				m_ToolBar->SetToolNormalBitmap(IDM_PLAY, m_Bitmaps[Toolbar_Pause]);
			}
			else
			{
				PlayTool->SetLabel(_("Play"));
				PlayTool->SetShortHelp(_("Play"));
				m_ToolBar->SetToolNormalBitmap(IDM_PLAY, m_Bitmaps[Toolbar_Play]);
			}
		}
	}

	GetMenuBar()->FindItem(IDM_RECORDREADONLY)->Enable(Running || Paused);

	if (!Initialized && !m_bGameLoading)
	{
		if (m_GameListCtrl->IsEnabled())
		{
			// Prepare to load Default ISO, enable play button
			if (!SConfig::GetInstance().m_LocalCoreStartupParameter.m_strDefaultISO.empty())
			{
				if (m_ToolBar)
					m_ToolBar->EnableTool(IDM_PLAY, true);
				GetMenuBar()->FindItem(IDM_PLAY)->Enable(true);
				GetMenuBar()->FindItem(IDM_RECORD)->Enable(true);
				GetMenuBar()->FindItem(IDM_PLAYRECORD)->Enable(true);
			}
			// Prepare to load last selected file, enable play button
			else if (!SConfig::GetInstance().m_LastFilename.empty() &&
			         File::Exists(SConfig::GetInstance().m_LastFilename))
			{
				if (m_ToolBar)
					m_ToolBar->EnableTool(IDM_PLAY, true);
				GetMenuBar()->FindItem(IDM_PLAY)->Enable(true);
				GetMenuBar()->FindItem(IDM_RECORD)->Enable(true);
				GetMenuBar()->FindItem(IDM_PLAYRECORD)->Enable(true);
			}
			else
			{
				// No game has been selected yet, disable play button
				if (m_ToolBar)
					m_ToolBar->EnableTool(IDM_PLAY, false);
				GetMenuBar()->FindItem(IDM_PLAY)->Enable(false);
				GetMenuBar()->FindItem(IDM_RECORD)->Enable(false);
				GetMenuBar()->FindItem(IDM_PLAYRECORD)->Enable(false);
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
			GetMenuBar()->FindItem(IDM_PLAY)->Enable(true);
			GetMenuBar()->FindItem(IDM_RECORD)->Enable(true);
			GetMenuBar()->FindItem(IDM_PLAYRECORD)->Enable(true);
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
		if (SConfig::GetInstance().m_LocalCoreStartupParameter.bEnableCheats)
			g_CheatsWindow->UpdateGUI();
		else
			g_CheatsWindow->Close();
	}
}

void CFrame::UpdateGameList()
{
	m_GameListCtrl->Update();
}

void CFrame::GameListChanged(wxCommandEvent& event)
{
	switch (event.GetId())
	{
	case IDM_LISTWII:
		SConfig::GetInstance().m_ListWii = event.IsChecked();
		break;
	case IDM_LISTGC:
		SConfig::GetInstance().m_ListGC = event.IsChecked();
		break;
	case IDM_LISTWAD:
		SConfig::GetInstance().m_ListWad = event.IsChecked();
		break;
	case IDM_LISTJAP:
		SConfig::GetInstance().m_ListJap = event.IsChecked();
		break;
	case IDM_LISTPAL:
		SConfig::GetInstance().m_ListPal = event.IsChecked();
		break;
	case IDM_LISTUSA:
		SConfig::GetInstance().m_ListUsa = event.IsChecked();
		break;
	case IDM_LISTAUSTRALIA:
		SConfig::GetInstance().m_ListAustralia = event.IsChecked();
		break;
	case IDM_LISTFRANCE:
		SConfig::GetInstance().m_ListFrance = event.IsChecked();
		break;
	case IDM_LISTGERMANY:
		SConfig::GetInstance().m_ListGermany = event.IsChecked();
		break;
	case IDM_LISTITALY:
		SConfig::GetInstance().m_ListItaly = event.IsChecked();
		break;
	case IDM_LISTKOREA:
		SConfig::GetInstance().m_ListKorea = event.IsChecked();
		break;
	case IDM_LISTNETHERLANDS:
		SConfig::GetInstance().m_ListNetherlands = event.IsChecked();
		break;
	case IDM_LISTRUSSIA:
		SConfig::GetInstance().m_ListRussia = event.IsChecked();
		break;
	case IDM_LISTSPAIN:
		SConfig::GetInstance().m_ListSpain = event.IsChecked();
		break;
	case IDM_LISTTAIWAN:
		SConfig::GetInstance().m_ListTaiwan = event.IsChecked();
		break;
	case IDM_LIST_UNK:
		SConfig::GetInstance().m_ListUnknown = event.IsChecked();
		break;
	case IDM_LISTDRIVES:
		SConfig::GetInstance().m_ListDrives = event.IsChecked();
		break;
	case IDM_PURGECACHE:
		CFileSearch::XStringVector Directories;
		Directories.push_back(File::GetUserPath(D_CACHE_IDX));
		CFileSearch::XStringVector Extensions;
		Extensions.push_back("*.cache");

		CFileSearch FileSearch(Extensions, Directories);
		const CFileSearch::XStringVector& rFilenames = FileSearch.GetFileNames();

		for (auto& rFilename : rFilenames)
		{
			File::Delete(rFilename);
		}
		break;
	}

	// Update gamelist
	if (m_GameListCtrl)
	{
		m_GameListCtrl->Update();
	}
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
	if (SConfig::GetInstance().m_InterfaceStatusbar == true)
		GetStatusBar()->Show();
	else
		GetStatusBar()->Hide();

	this->SendSizeEvent();
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
	case IDM_SHOW_NOTES:
		SConfig::GetInstance().m_showNotesColumn = !SConfig::GetInstance().m_showNotesColumn;
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
	default: return;
	}
	m_GameListCtrl->Update();
	SConfig::GetInstance().SaveSettings();
}
