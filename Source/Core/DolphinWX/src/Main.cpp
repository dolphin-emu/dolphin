// Copyright (C) 2003-2008 Dolphin Project.

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

#include <vector>
#include <string>

#include "CPUDetect.h"
#include "Globals.h"
#include "Common.h"
#include "IniFile.h"
#include "Main.h"
#include "Frame.h"
#include "Config.h"
#include "CodeWindow.h"

IMPLEMENT_APP(DolphinApp)

CFrame * main_frame = NULL;
CCodeWindow* code_frame = NULL;

// The `main program' equivalent, creating the windows and returning the
// main frame
bool DolphinApp::OnInit()
{
	DetectCPU();
#ifndef _WIN32
//	RegisterPanicAlertHandler(&wxPanicAlert);
#endif

#if 0
#ifdef _WIN32
	// TODO: if First Boot
#ifdef _M_IX86
	if (cpu_info.CPU64Bit && !cpu_info.OS64Bit)
	{
		MessageBox(0, _T("This is the 32-bit version of Dolphin. This computer is running a 64-bit OS.\n"
				 "Thus, this computer is capable of running Dolphin 64-bit, which is considerably "
				 "faster than Dolphin 32-bit. So, why are you running Dolphin 32-bit? :-)"));
	}

	// missing check
#endif
#endif
#endif

	bool UseDebugger = false;

#if wxUSE_CMDLINE_PARSER
	//
	//  What this does is get all the command line arguments
	//  and treat each one as a file to put to the initial playlist
	//
	wxCmdLineEntryDesc cmdLineDesc[] =
	{
		{wxCMD_LINE_SWITCH, _T("h"), _T("help"), _T("Show this help message"), wxCMD_LINE_VAL_NONE, wxCMD_LINE_OPTION_HELP},
		{wxCMD_LINE_SWITCH, _T("d"), _T("debugger"), _T("Opens the debugger")},
		{wxCMD_LINE_NONE}
	};

	//gets the passed media files from command line
	wxCmdLineParser parser(cmdLineDesc, argc, argv);

	// get filenames from the command line
	if (parser.Parse() != 0)
	{
		return(false);
	}

	UseDebugger = parser.Found(_T("debugger"));
#endif

	SConfig::GetInstance().LoadSettings();
	wxInitAllImageHandlers();
	// Create the main frame window
	main_frame = new CFrame((wxFrame*) NULL, wxID_ANY,
			_T("Dolphin"),
			wxPoint(100, 100), wxSize(800, 600));

	// create debugger
	if (UseDebugger)
	{
		code_frame = new CCodeWindow(SConfig::GetInstance().m_LocalCoreStartupParameter, main_frame);
		code_frame->Show(true);
	}

	SetTopWindow(main_frame);

	return(true);
}


void DolphinApp::OnEndSession()
{
	SConfig::GetInstance().SaveSettings();
}


bool wxPanicAlert(const char* text, bool /*yes_no*/)
{
	wxMessageBox(text, _T("PANIC ALERT"));
	return(true);
}


void Host_BootingStarted()
{
	wxCommandEvent event(wxEVT_HOST_COMMAND, IDM_BOOTING_STARTED);
	wxPostEvent(main_frame, event);

	if (code_frame)
	{
		wxPostEvent(code_frame, event);
	}
}


void Host_BootingEnded()
{
	wxCommandEvent event(wxEVT_HOST_COMMAND, IDM_BOOTING_ENDED);
	wxPostEvent(main_frame, event);

	if (code_frame)
	{
		wxPostEvent(code_frame, event);
	}
}


// OK, this thread boundary is DANGEROUS on linux
// wxPostEvent / wxAddPendingEvent is the solution.
void Host_NotifyMapLoaded()
{
	wxCommandEvent event(wxEVT_HOST_COMMAND, IDM_NOTIFYMAPLOADED);
	wxPostEvent(main_frame, event);

	if (code_frame)
	{
		wxPostEvent(code_frame, event);
	}
}


void Host_UpdateLogDisplay()
{
	wxCommandEvent event(wxEVT_HOST_COMMAND, IDM_UPDATELOGDISPLAY);
	wxPostEvent(main_frame, event);

	if (code_frame)
	{
		wxPostEvent(code_frame, event);
	}
}


void Host_UpdateDisasmDialog()
{
	wxCommandEvent event(wxEVT_HOST_COMMAND, IDM_UPDATEDISASMDIALOG);
	wxPostEvent(main_frame, event);
	if (code_frame)
	{
		wxPostEvent(code_frame, event);
	}
}


void Host_UpdateMainFrame()
{
	wxCommandEvent event(wxEVT_HOST_COMMAND, IDM_UPDATEGUI);
	wxPostEvent(main_frame, event);

	if (code_frame)
	{
		wxPostEvent(code_frame, event);
	}
}


void Host_UpdateMemoryView()
{}


void Host_SetDebugMode(bool)
{}


void Host_SetWaitCursor(bool enable)
{
#ifdef _WIN32
	if (enable)
	{
		SetCursor(LoadCursor(NULL, IDC_WAIT));
	}
	else
	{
		SetCursor(LoadCursor(NULL, IDC_ARROW));
	}
#endif

}


void Host_CreateDisplay()
{}


void Host_CloseDisplay()
{}

void Host_UpdateStatusBar(const char* _pText)
{
	wxCommandEvent event(wxEVT_HOST_COMMAND, IDM_UPDATESTATUSBAR);
	event.SetString(_pText);

	wxPostEvent(main_frame, event);
}
