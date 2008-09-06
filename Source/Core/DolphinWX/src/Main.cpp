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
#ifdef WIN32
	#include "svnrev.h"
#endif
#include "CPUDetect.h"
#include "Globals.h"
#include "Common.h"
#include "IniFile.h"
#include "Main.h"
#include "Frame.h"
#include "Config.h"
#include "CodeWindow.h"
#include "ExtendedTrace.h"

IMPLEMENT_APP(DolphinApp)

CFrame* main_frame = NULL;
CCodeWindow* g_pCodeWindow = NULL;

#ifdef WIN32
//Has no error handling.
//I think that if an error occurs here there's no way to handle it anyway.
LONG WINAPI MyUnhandledExceptionFilter(LPEXCEPTION_POINTERS e) {
	//EnterCriticalSection(&g_uefcs);

	FILE* file=NULL;
	fopen_s(&file, "exceptioninfo.txt", "a");
	fseek(file, 0, SEEK_END);
	etfprint(file, "\n");
	//etfprint(file, g_buildtime);
	//etfprint(file, "\n");
	//dumpCurrentDate(file);
	etfprintf(file, "Unhandled Exception\n  Code: 0x%08X\n",
		e->ExceptionRecord->ExceptionCode);
	STACKTRACE2(file, e->ContextRecord->Eip, e->ContextRecord->Esp, e->ContextRecord->Ebp);
	fclose(file);
	_flushall();

	//LeaveCriticalSection(&g_uefcs);
	return EXCEPTION_CONTINUE_SEARCH;
}
#endif

// The `main program' equivalent, creating the windows and returning the
// main frame
bool DolphinApp::OnInit()
{
	DetectCPU();
#ifndef _WIN32
//	RegisterPanicAlertHandler(&wxPanicAlert);
#endif

#ifdef _WIN32
	EXTENDEDTRACEINITIALIZE(".");
	SetUnhandledExceptionFilter(&MyUnhandledExceptionFilter);

	// TODO: if First Boot
	if (!cpu_info.bSSE2) 
	{
		MessageBox(0, _T("Hi,\n\nDolphin requires that your CPU has support for SSE2 extensions.\n"
			             "Unfortunately your CPU does not support them, so Dolphin will not run.\n\n"
						 "Sayonara!\n"), "Dolphin", MB_ICONINFORMATION);
		return false;
	}
#else
	if (!cpu_info.bSSE2)
	{
		printf("%s", "Hi,\n\nDolphin requires that your CPU has support for SSE2 extensions.\n"
               "Unfortunately your CPU does not support them, so Dolphin will not run.\n\n"
    		   "Sayonara!\n");
		exit(0);
	}
#endif

	bool UseDebugger = false;

#if wxUSE_CMDLINE_PARSER
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
		return false;
	} 

	UseDebugger = parser.Found(_T("debugger"));
#endif

	SConfig::GetInstance().LoadSettings();
	wxInitAllImageHandlers();
	// Create the main frame window
#ifdef _WIN32
	#ifdef _DEBUG
		const char *title = "Dolphin Debug SVN R " SVN_REV_STR;
	#else
		const char *title = "Dolphin SVN R " SVN_REV_STR;
	#endif
#else
	#ifdef _DEBUG
		const char *title = "Dolphin Debug SVN Linux";
	#else
		const char *title = "Dolphin SVN Linux";
	#endif
#endif
	main_frame = new CFrame((wxFrame*) NULL, wxID_ANY, wxString::FromAscii(title),
			wxPoint(100, 100), wxSize(800, 600));

	// create debugger
	if (UseDebugger)
	{
		g_pCodeWindow = new CCodeWindow(SConfig::GetInstance().m_LocalCoreStartupParameter, main_frame);
		g_pCodeWindow->Show(true);
	}

	SetTopWindow(main_frame);
	return true;
}


void DolphinApp::OnEndSession()
{
	SConfig::GetInstance().SaveSettings();
}


bool wxPanicAlert(const char* text, bool /*yes_no*/)
{
	wxMessageBox(wxString::FromAscii(text), _T("PANIC ALERT"));
	return(true);
}


void Host_BootingStarted()
{
	wxCommandEvent event(wxEVT_HOST_COMMAND, IDM_BOOTING_STARTED);
	wxPostEvent(main_frame, event);

	if (g_pCodeWindow)
	{
		wxPostEvent(g_pCodeWindow, event);
	}
}


void Host_BootingEnded()
{
	wxCommandEvent event(wxEVT_HOST_COMMAND, IDM_BOOTING_ENDED);
	wxPostEvent(main_frame, event);

	if (g_pCodeWindow)
	{
		wxPostEvent(g_pCodeWindow, event);
	}
}


// OK, this thread boundary is DANGEROUS on linux
// wxPostEvent / wxAddPendingEvent is the solution.
void Host_NotifyMapLoaded()
{
	wxCommandEvent event(wxEVT_HOST_COMMAND, IDM_NOTIFYMAPLOADED);
	wxPostEvent(main_frame, event);

	if (g_pCodeWindow)
	{
		wxPostEvent(g_pCodeWindow, event);
	}
}


void Host_UpdateLogDisplay()
{
	wxCommandEvent event(wxEVT_HOST_COMMAND, IDM_UPDATELOGDISPLAY);
	wxPostEvent(main_frame, event);

	if (g_pCodeWindow)
	{
		wxPostEvent(g_pCodeWindow, event);
	}
}


void Host_UpdateDisasmDialog()
{
	wxCommandEvent event(wxEVT_HOST_COMMAND, IDM_UPDATEDISASMDIALOG);
	wxPostEvent(main_frame, event);
	if (g_pCodeWindow)
	{
		wxPostEvent(g_pCodeWindow, event);
	}
}


void Host_UpdateMainFrame()
{
	wxCommandEvent event(wxEVT_HOST_COMMAND, IDM_UPDATEGUI);
	wxPostEvent(main_frame, event);

	if (g_pCodeWindow)
	{
		wxPostEvent(g_pCodeWindow, event);
	}
}

void Host_UpdateBreakPointView()
{
	wxCommandEvent event(wxEVT_HOST_COMMAND, IDM_UPDATEBREAKPOINTS);
	wxPostEvent(main_frame, event);

	if (g_pCodeWindow)
	{
		wxPostEvent(g_pCodeWindow, event);
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

void Host_UpdateStatusBar(const char* _pText)
{
	wxCommandEvent event(wxEVT_HOST_COMMAND, IDM_UPDATESTATUSBAR);
	event.SetString(wxString::FromAscii(_pText));

	wxPostEvent(main_frame, event);
}
