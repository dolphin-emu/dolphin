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
#include "svnrev.h"

#ifdef WIN32
#include <crtdbg.h>
#endif

#include "Globals.h" // Core
#include "Host.h"

#include "Common.h" // Common
#include "CPUDetect.h"
#include "IniFile.h"
#include "FileUtil.h"

#include "Main.h" // Local
#include "Frame.h"
#include "Config.h"
#include "CodeWindow.h"
#include "ExtendedTrace.h"
#include "BootManager.h"

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
#ifndef _M_X64
	STACKTRACE2(file, e->ContextRecord->Eip, e->ContextRecord->Esp, e->ContextRecord->Ebp);
#else
	STACKTRACE2(file, e->ContextRecord->Rip, e->ContextRecord->Rsp, e->ContextRecord->Rbp);
#endif
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

#ifdef _DEBUG
	int tmpflag = _CrtSetDbgFlag(_CRTDBG_REPORT_FLAG);
	tmpflag |= _CRTDBG_DELAY_FREE_MEM_DF;
	_CrtSetDbgFlag(tmpflag);
#endif

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

	// ============
	// Check for debugger
	bool UseDebugger = false;
	bool LoadElf = false; wxString ElfFile;

#if wxUSE_CMDLINE_PARSER
	wxCmdLineEntryDesc cmdLineDesc[] =
	{
		{
			wxCMD_LINE_SWITCH, _T("h"), _T("help"), _T("Show this help message"),
			wxCMD_LINE_VAL_NONE, wxCMD_LINE_OPTION_HELP
		},
		{
			wxCMD_LINE_SWITCH, _T("d"), _T("debugger"), _T("Opens the debugger")
		},
		{
			wxCMD_LINE_OPTION, _T("e"), _T("elf"), _T("Loads an elf file"),
			wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL
		},
		{
			wxCMD_LINE_NONE
		}
	};

#if defined(__APPLE__) 
        // HACK: Get rid of bogous osx param
        if (argc > 1 && wxString(argv[argc - 1]).StartsWith(_("-psn_"))) {
            delete argv[argc-1];
            argv[argc-1] = NULL;
            argc--;
        }        
#endif

	// Gets the passed media files from command line
	wxCmdLineParser parser(cmdLineDesc, argc, argv);

	// Get filenames from the command line
	if (parser.Parse() != 0)
	{
		return false;
	} 

	UseDebugger = parser.Found(_T("debugger"));
	LoadElf = parser.Found(_T("elf"), &ElfFile);

	if( LoadElf && ElfFile == wxEmptyString )
		PanicAlert("You did not specify a file name");

	// ============
#endif

	SConfig::GetInstance().LoadSettings();
	wxInitAllImageHandlers();
	// Create the main frame window

#ifdef _DEBUG
        const char *title = "Dolphin Debug SVN R " SVN_REV_STR;
#else
        const char *title = "Dolphin SVN R " SVN_REV_STR;
#endif

	// ---------------------------------------------------------------------------------------
	// If we are debugging let use save the main window position and size
	// TODO: Save position and size on exit
	// ---------
	IniFile ini;
	ini.Load(DEBUGGER_CONFIG_FILE);

	int x, y, w, h;

	ini.Get("MainWindow", "x", &x, 100);
	ini.Get("MainWindow", "y", &y, 100);
	ini.Get("MainWindow", "w", &w, 600);
	ini.Get("MainWindow", "h", &h, 800);
	// ---------
	if(UseDebugger)
	{
		main_frame = new CFrame((wxFrame*) NULL, wxID_ANY, wxString::FromAscii(title),
				wxPoint(x, y), wxSize(h, w));
	}
	else
	{
		main_frame = new CFrame((wxFrame*) NULL, wxID_ANY, wxString::FromAscii(title),
				wxPoint(100, 100), wxSize(800, 600));
	}
	// ---------

	// create debugger
	if (UseDebugger)
	{
		g_pCodeWindow = new CCodeWindow(SConfig::GetInstance().m_LocalCoreStartupParameter, main_frame);
		g_pCodeWindow->Show(true);

		/* If we have selected Automatic Start, start the default ISO, or if no default
		   ISO exists, start the last loaded ISO */
		if (g_pCodeWindow->AutomaticStart())
		{
			if(!SConfig::GetInstance().m_LocalCoreStartupParameter.m_strDefaultGCM.empty()
				&& File::Exists(SConfig::GetInstance().m_LocalCoreStartupParameter.
					m_strDefaultGCM.c_str()))
			{
				BootManager::BootCore(SConfig::GetInstance().m_LocalCoreStartupParameter.
					m_strDefaultGCM);
			}
			else if(!SConfig::GetInstance().m_LastFilename.empty()
				&& File::Exists(SConfig::GetInstance().m_LastFilename.c_str()))
			{
				BootManager::BootCore(SConfig::GetInstance().m_LastFilename);
			}
			
		}
	}

	if (LoadElf && ElfFile != wxEmptyString)
		BootManager::BootCore(std::string(ElfFile.ToAscii()));

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
	event.SetInt(0);

	wxPostEvent(main_frame, event);
}

void Host_SysMessage(const char *fmt, ...) 
{
	va_list list;
	char msg[512];

	va_start(list, fmt);
	vsprintf(msg, fmt, list);
	va_end(list);

	if (msg[strlen(msg)-1] == '\n') msg[strlen(msg)-1] = 0;
	wxMessageBox(wxString::FromAscii(msg));
}

void Host_SetWiiMoteConnectionState(int _State)
{
	static int currentState = -1;
	if (_State == currentState)
		return;
	currentState = _State;

	wxCommandEvent event(wxEVT_HOST_COMMAND, IDM_UPDATESTATUSBAR);

	switch(_State)
	{
	case 0: event.SetString(wxString::FromAscii("Not connected")); break;
	case 1: event.SetString(wxString::FromAscii("Connecting...")); break;
	case 2: event.SetString(wxString::FromAscii("Wiimote Connected")); break;
	}

	event.SetInt(1);

	wxPostEvent(main_frame, event);
}
