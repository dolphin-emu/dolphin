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


//////////////////////////////////////////////////////////////////////////////////////////
// Includes
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯
#include <vector>
#include <string>
#include "svnrev.h"

#ifdef WIN32
#include <crtdbg.h>
#endif

#ifdef __APPLE__
#include <sys/param.h>
#endif

#include "Globals.h" // Core
#include "Host.h"

#include "Common.h" // Common
#include "CPUDetect.h"
#include "IniFile.h"
#include "FileUtil.h"

#include "Main.h" // Local
#include "Frame.h"
#include "ConfigManager.h"
#include "CodeWindow.h"
#include "LogWindow.h"
#include "ExtendedTrace.h"
#include "BootManager.h"
////////////////////////////////////


//////////////////////////////////////////////////////////////////////////////////////////
// Declarations and definitions
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯
IMPLEMENT_APP(DolphinApp)

#if defined(HAVE_WX) && HAVE_WX
	bool wxMsgAlert(const char*, const char*, bool, int);
#endif

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
///////////////////////////////////


/////////////////////////////////////////////////////////////
/* The `main program' equivalent that creates the mai nwindow and return the main frame */
// ¯¯¯¯¯¯¯¯¯
bool DolphinApp::OnInit()
{
	// Declarations and definitions
	bool UseDebugger = false;
	bool UseLogger = false;
	bool LoadElf = false; wxString ElfFile;

	// Detect CPU info and write it to the cpu_info struct
	cpu_info.Detect();

	#if defined _DEBUG && defined _WIN32
		int tmpflag = _CrtSetDbgFlag(_CRTDBG_REPORT_FLAG);
		tmpflag |= _CRTDBG_DELAY_FREE_MEM_DF;
		_CrtSetDbgFlag(tmpflag);
	#endif

	// Register message box handler
#if ! defined(_WIN32) && defined(HAVE_WX) && HAVE_WX
		RegisterMsgAlertHandler(&wxMsgAlert);
#endif


	// ------------------------------------------
	// Show CPU message
	// ---------------
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
	// ---------------


	// ------------------------------------------
	// Parse command lines
	// ---------------
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
				wxCMD_LINE_SWITCH, _T("l"), _T("logger"), _T("Opens The Logger")
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
			// check to see if ~/Library/Application Support/Dolphin exists; if not, create it
			char AppSupportDir[MAXPATHLEN];
			snprintf(AppSupportDir, sizeof(AppSupportDir), "%s/Library/Application Support", getenv("HOME"));
			if (!File::Exists(AppSupportDir) || !File::IsDirectory(AppSupportDir)) 
				PanicAlert("Could not open ~/Library/Application Support");

			strlcat(AppSupportDir, "/Dolphin", sizeof(AppSupportDir));
			
			if (!File::Exists(AppSupportDir))
				File::CreateDir(AppSupportDir);
			
			if (!File::IsDirectory(AppSupportDir))
				PanicAlert("~/Library/Application Support/Dolphin exists, but is not a directory");
			
			chdir(AppSupportDir);

			if (!File::Exists("User")) File::CreateDir("User");
			if (!File::Exists("User/GC")) File::CreateDir("User/GC");
			
			// HACK: Get rid of bogus osx param
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
		UseLogger = parser.Found(_T("logger"));
		LoadElf = parser.Found(_T("elf"), &ElfFile);

		if( LoadElf && ElfFile == wxEmptyString )
			PanicAlert("You did not specify a file name");

		// ============
	#endif

	// Load CONFIG_FILE settings
	SConfig::GetInstance().LoadSettings();

	// Enable the PNG image handler
	wxInitAllImageHandlers(); 

	// Create the window title
	#ifdef _DEBUG
			const char *title = "Dolphin Debug SVN R " SVN_REV_STR;
	#else
			const char *title = "Dolphin SVN R " SVN_REV_STR;
	#endif

	// ---------------------------------------------------------------------------------------
	// If we are debugging let use save the main window position and size
	// TODO: Save position and size on exit
	// ------------
	IniFile ini;
	ini.Load(DEBUGGER_CONFIG_FILE);

	int x, y, w, h;

	ini.Get("MainWindow", "x", &x, 100);
	ini.Get("MainWindow", "y", &y, 100);
	ini.Get("MainWindow", "w", &w, 800);
	ini.Get("MainWindow", "h", &h, 600);
	// -------------------
	if(UseDebugger)
	{
		main_frame = new CFrame((wxFrame*) NULL, wxID_ANY, wxString::FromAscii(title),
				wxPoint(x, y), wxSize(w, h));
	}
	else
	{
		main_frame = new CFrame((wxFrame*) NULL, wxID_ANY, wxString::FromAscii(title),
				wxPoint(100, 100), wxSize(w, h));
	}
	// ------------------


	// Create debugging window
	if (UseDebugger)
	{
		g_pCodeWindow = new CCodeWindow(SConfig::GetInstance().m_LocalCoreStartupParameter, main_frame);
		g_pCodeWindow->Show(true);
	}
	if(!UseDebugger && UseLogger)
	{
		#ifdef LOGGING
			// We aren't using debugger, just logger
			// Should be fine for a local copy
			CLogWindow* m_LogWindow = new CLogWindow(main_frame);
			m_LogWindow->Show(true);
		#endif
	}


	// ---------------------------------------------------
	// Check the autoboot options. 
	// ---------------------
	// First check if we have a elf command line. Todo: Should we place this under #if wxUSE_CMDLINE_PARSER?
	if (LoadElf && ElfFile != wxEmptyString)
	{
		BootManager::BootCore(std::string(ElfFile.ToAscii()));
	}
	/* If we have selected Automatic Start, start the default ISO, or if no default
	   ISO exists, start the last loaded ISO */
	else if (UseDebugger && g_pCodeWindow->AutomaticStart())
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
	// ---------------------

	// Set main parent window
	SetTopWindow(main_frame);
	return true;
}


void DolphinApp::OnEndSession()
{
	SConfig::GetInstance().SaveSettings();
}
///////////////////////////////////////  Main window created

#if defined HAVE_WX && HAVE_WX
bool wxMsgAlert(const char* caption, const char* text, bool yes_no, int /*Style*/)
{
    return wxYES == wxMessageBox(wxString::FromAscii(text), 
				 wxString::FromAscii(caption),
				 (yes_no)?wxYES_NO:wxOK);
}
#endif
//////////////////////////////////


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


/////////////////////////////////////////////////////////////
/* Update Wiimote status bar */
// ¯¯¯¯¯¯¯¯¯
void Host_UpdateLeds(int led_bits) {
	// Convert it to a simpler byte format
	main_frame->g_Leds[0] = led_bits >> 0;
	main_frame->g_Leds[1] = led_bits >> 1;
	main_frame->g_Leds[2] = led_bits >> 2;
	main_frame->g_Leds[3] = led_bits >> 3;

	main_frame->UpdateLeds();
}

void Host_UpdateSpeakerStatus(int index, int speaker_bits) {
	main_frame->g_Speakers[index] = speaker_bits;
	main_frame->UpdateSpeakers();
}

void Host_UpdateStatus()
{
	// Debugging
	//std::string Tmp = ArrayToString(main_frame->g_Speakers, sizeof(main_frame->g_Speakers));
	//std::string Tmp2 = ArrayToString(main_frame->g_Speakers_, sizeof(main_frame->g_Speakers_));
	//LOGV(CONSOLE, 0, "Tmp: %s", Tmp.c_str());
	//LOGV(CONSOLE, 0, "Tmp2: %s", Tmp2.c_str());
	// Don't do a lot of CreateBitmap() if we don't need to
	if(memcmp(main_frame->g_Speakers_, main_frame->g_Speakers,
		sizeof(main_frame->g_Speakers)))
	{
		main_frame->g_Speakers[2] = 0;
		main_frame->UpdateSpeakers();
		memcpy(main_frame->g_Speakers_, main_frame->g_Speakers, sizeof(main_frame->g_Speakers));
	}
}
///////////////////////////


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
