// Copyright (C) 2003 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/



// This file holds global data for DolphinWx and DebuggerWx



#ifndef _GLOBALS_H
#define _GLOBALS_H

#include "Common.h"

// Constant Colors
const unsigned long COLOR_GRAY = 0xDCDCDC;

enum
{
	Toolbar_DebugGo,
	Toolbar_DebugPause,
	Toolbar_Step,
	Toolbar_StepOver,
	Toolbar_Skip,
	Toolbar_GotoPC,
	Toolbar_SetPC,
	ToolbarDebugBitmapMax
};

enum
{
	IDM_LOADSTATE = 200, // File menu
	IDM_SAVESTATE,
	IDM_LOADLASTSTATE,
	IDM_UNDOLOADSTATE,
	IDM_UNDOSAVESTATE,
	IDM_LOADSTATEFILE,
	IDM_SAVESTATEFILE,
	IDM_SAVESLOT1,
	IDM_SAVESLOT2,
	IDM_SAVESLOT3,
	IDM_SAVESLOT4,
	IDM_SAVESLOT5,
	IDM_SAVESLOT6,
	IDM_SAVESLOT7,
	IDM_SAVESLOT8,
	IDM_LOADSLOT1,
	IDM_LOADSLOT2,
	IDM_LOADSLOT3,
	IDM_LOADSLOT4,
	IDM_LOADSLOT5,
	IDM_LOADSLOT6,
	IDM_LOADSLOT7,
	IDM_LOADSLOT8,
	IDM_FRAMESKIP0,
	IDM_FRAMESKIP1,
	IDM_FRAMESKIP2,
	IDM_FRAMESKIP3,
	IDM_FRAMESKIP4,
	IDM_FRAMESKIP5,
	IDM_FRAMESKIP6,
	IDM_FRAMESKIP7,
	IDM_FRAMESKIP8,
	IDM_FRAMESKIP9,
	IDM_PLAY,
	IDM_STOP,
	IDM_RESET,
	IDM_RECORD,
	IDM_PLAYRECORD,
	IDM_FRAMESTEP,
	IDM_SCREENSHOT,
	IDM_BROWSE,
	IDM_DRIVE1,
	IDM_DRIVE24 = IDM_DRIVE1 + 23,//248,

	IDM_MEMCARD, // Misc menu
	IDM_CHEATS,
	IDM_NETPLAY,
	IDM_RESTART,
	IDM_CHANGEDISC,
	IDM_PROPERTIES,
	IDM_LOAD_WII_MENU,
	IDM_LUA,
	IDM_CONNECT_WIIMOTE1,
	IDM_CONNECT_WIIMOTE2,
	IDM_CONNECT_WIIMOTE3,
	IDM_CONNECT_WIIMOTE4,

	IDM_LISTWAD,
	IDM_LISTWII,
	IDM_LISTGC,
	IDM_LISTJAP,
	IDM_LISTPAL,
	IDM_LISTUSA,
	IDM_LISTDRIVES,
	IDM_PURGECACHE,

	IDM_HELPABOUT, // Help menu
	IDM_HELPWEBSITE,
	IDM_HELPGOOGLECODE,

	IDM_CONFIG_MAIN,
	IDM_CONFIG_GFX_PLUGIN,
	IDM_CONFIG_DSP_PLUGIN,
	IDM_CONFIG_PAD_PLUGIN,
	IDM_CONFIG_WIIMOTE_PLUGIN,
	IDM_TOGGLE_FULLSCREEN,

	// --------------------------------------------------------------
	// Debugger Menu Entries
	// --------------------
	// CPU Mode
	IDM_INTERPRETER,
	//IDM_DUALCORE, // not used
	IDM_AUTOMATICSTART, IDM_BOOTTOPAUSE,
	IDM_JITUNLIMITED, IDM_JITBLOCKLINKING,  // JIT
	IDM_JITOFF,
	IDM_JITLSOFF, IDM_JITLSLXZOFF, IDM_JITLSLWZOFF, IDM_JITLSLBZXOFF,
	IDM_JITLSPOFF, IDM_JITLSFOFF,
	IDM_JITIOFF,
	IDM_JITFPOFF,
	IDM_JITPOFF,
	IDM_JITSROFF,
	IDM_FONTPICKER,

	// Views	
	IDM_LOGWINDOW,
	IDM_CONSOLEWINDOW,
	IDM_CODEWINDOW,
	IDM_REGISTERWINDOW,
	IDM_BREAKPOINTWINDOW,
	IDM_MEMORYWINDOW,
	IDM_JITWINDOW,
	IDM_SOUNDWINDOW,
	IDM_VIDEOWINDOW,

	// Symbols
	IDM_CLEARSYMBOLS,
	IDM_CLEANSYMBOLS, // not used
	IDM_SCANFUNCTIONS,
	IDM_LOADMAPFILE,
	IDM_SAVEMAPFILE, IDM_SAVEMAPFILEWITHCODES,
	IDM_CREATESIGNATUREFILE,
    IDM_RENAME_SYMBOLS,
	IDM_USESIGNATUREFILE,
	//IDM_USESYMBOLFILE, // not used
	IDM_PATCHHLEFUNCTIONS,

	// JIT
	IDM_CLEARCODECACHE,
	IDM_LOGINSTRUCTIONS,
	IDM_SEARCHINSTRUCTION,

	// Profiler
	IDM_PROFILEBLOCKS,
	IDM_WRITEPROFILE,
	// --------------------------------------------------------------

	// --------------------------------------------------------------
	// Debugger Toolbar
	// --------------------
	ID_TOOLBAR_DEBUG,
	IDM_DEBUG_GO,
	IDM_STEP,
	IDM_STEPOVER,
	IDM_SKIP,
	IDM_SETPC,
	IDM_GOTOPC,
	IDM_ADDRBOX,

	IDM_FLOAT_LOGWINDOW,
	IDM_FLOAT_CONSOLEWINDOW,
	IDM_FLOAT_CODEWINDOW,
	IDM_FLOAT_REGISTERWINDOW,
	IDM_FLOAT_BREAKPOINTWINDOW,
	IDM_FLOAT_MEMORYWINDOW,
	IDM_FLOAT_JITWINDOW,
	IDM_FLOAT_SOUNDWINDOW,
	IDM_FLOAT_VIDEOWINDOW,

	ID_TOOLBAR_AUI,
	IDM_SAVE_PERSPECTIVE,
	IDM_ADD_PERSPECTIVE,
	IDM_PERSPECTIVES_ADD_PANE,
	IDM_EDIT_PERSPECTIVES,
	IDM_TAB_SPLIT,
	IDM_NO_DOCKING,
	IDM_PERSPECTIVES_0,
	IDM_PERSPECTIVES_100 = IDM_PERSPECTIVES_0 + 100,
	// --------------------------------------------------------------

	IDM_LOGWINDOW_PARENT, // Window IDs
	IDM_CONSOLEWINDOW_PARENT,
	IDM_CODEWINDOW_PARENT,
	IDM_REGISTERWINDOW_PARENT,
	IDM_BREAKPOINTWINDOW_PARENT,
	IDM_MEMORYWINDOW_PARENT,
	IDM_JITWINDOW_PARENT,
	IDM_SOUNDWINDOW_PARENT,
	IDM_VIDEOWINDOW_PARENT,

	IDM_TOGGLE_DUALCORE, // Other
	IDM_TOGGLE_SKIPIDLE,
	IDM_TOGGLE_TOOLBAR,
	IDM_TOGGLE_STATUSBAR,
	IDM_NOTIFYMAPLOADED,
	IDM_OPENCONTAININGFOLDER,
	IDM_OPENSAVEFOLDER,
	IDM_SETDEFAULTGCM,
	IDM_DELETEGCM,
	IDM_COMPRESSGCM,
	IDM_MULTICOMPRESSGCM,
	IDM_MULTIDECOMPRESSGCM,
	IDM_INSTALLWAD,
	IDM_UPDATELOGDISPLAY,
	IDM_UPDATEDISASMDIALOG,
	IDM_UPDATEGUI,
	IDM_UPDATESTATUSBAR,
	IDM_UPDATEBREAKPOINTS,
	IDM_HOST_MESSAGE,

	IDM_MPANEL, ID_STATUSBAR,

	ID_TOOLBAR = 500,
	LIST_CTRL = 1000
};

#define wxUSE_XPM_IN_MSW 1
#define USE_XPM_BITMAPS 1

// For compilers that support precompilation, includes <wx/wx.h>.
//#include <wx/wxprec.h>

//#ifndef WX_PRECOMP
#if defined(HAVE_WX) && HAVE_WX
	#include <wx/wx.h>
	//#endif

	#include <wx/toolbar.h>
	#include <wx/log.h>
	#include <wx/image.h>
	#include <wx/aboutdlg.h>
	#include <wx/filedlg.h>
	#include <wx/spinctrl.h>
	#include <wx/srchctrl.h>
	#include <wx/listctrl.h>
	#include <wx/progdlg.h>
	#include <wx/imagpng.h>
	#include <wx/button.h>
	#include <wx/stattext.h>
	#include <wx/choice.h>
	#include <wx/cmdline.h>
	#include <wx/busyinfo.h>

	// Define this to use XPMs everywhere (by default, BMPs are used under Win)
	// BMPs use less space, but aren't compiled into the executable on other platforms
	#if USE_XPM_BITMAPS && defined (__WXMSW__) && !wxUSE_XPM_IN_MSW
	#error You need to enable XPM support to use XPM bitmaps with toolbar!
	#endif // USE_XPM_BITMAPS

	// custom message macro
	#define EVT_HOST_COMMAND(id, fn) \
		DECLARE_EVENT_TABLE_ENTRY(\
			wxEVT_HOST_COMMAND, id, wxID_ANY, \
			(wxObjectEventFunction)(wxEventFunction) wxStaticCastEvent(wxCommandEventFunction, &fn), \
			(wxObject*) NULL \
			),

	extern const wxEventType wxEVT_HOST_COMMAND;

#endif // HAVE_WX
#endif // _GLOBALS_H

