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

#ifdef __APPLE__
#include <sys/param.h>
#endif

#include "Common.h" // Common
#include "CPUDetect.h"
#include "IniFile.h"
#include "FileUtil.h"
#include "Setup.h"

#include "Host.h" // Core
#include "PluginManager.h"

#include "Globals.h" // Local
#include "Main.h"
#include "ConfigManager.h"
#include "CodeWindow.h"
#include "LogWindow.h"
#include "JitWindow.h"
#include "ExtendedTrace.h"
#include "BootManager.h"


IMPLEMENT_APP(DolphinApp)

#if defined(HAVE_WX) && HAVE_WX
	#include <wx/stdpaths.h>
	bool wxMsgAlert(const char*, const char*, bool, int);
#endif

CFrame* main_frame = NULL;
CCodeWindow* g_pCodeWindow = NULL;
LogManager *logManager = NULL;

#ifdef WIN32
//Has no error handling.
//I think that if an error occurs here there's no way to handle it anyway.
LONG WINAPI MyUnhandledExceptionFilter(LPEXCEPTION_POINTERS e) {
	//EnterCriticalSection(&g_uefcs);

	FILE* file = NULL;
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

// The `main program' equivalent that creates the main window and return the main frame 

bool DolphinApp::OnInit()
{

	NOTICE_LOG(BOOT, "Starting application");
	// Declarations and definitions
	bool UseDebugger = false;
	bool UseLogger = false;
	bool LoadElf = false;
	wxString ElfFile;

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


	// Show CPU message
	#ifdef _WIN32
		EXTENDEDTRACEINITIALIZE(".");
		SetUnhandledExceptionFilter(&MyUnhandledExceptionFilter);
	#endif
		
		// TODO: if First Boot
		if (!cpu_info.bSSE2) 
		{
			PanicAlert("Hi,\n\nDolphin requires that your CPU has support for SSE2 extensions.\n"
							 "Unfortunately your CPU does not support them, so Dolphin will not run.\n\n"
							 "Sayonara!\n");
			return false;
		}
#ifndef __APPLE__
	// Keep the user config dir free unless user wants to save the working dir
	FILE* noCheckForInstallDir = fopen(FULL_CONFIG_DIR "portable", "r");
	if (!noCheckForInstallDir)
	{
		char tmp[1024];
		sprintf(tmp, "%s/.dolphin%swd", (const char*)wxStandardPaths::Get().GetUserConfigDir().mb_str(wxConvUTF8),
#ifdef _M_IX86
			"x32");
#else
			"x64");
#endif
		FILE* workingDir = fopen(tmp, "r");
		if (!workingDir)
		{
			if (PanicYesNo("Dolphin has not been configured with an install location,\nKeep Dolphin portable?"))
			{
				FILE* portable = fopen(FULL_CONFIG_DIR "portable", "w");
				if (!portable)
				{
					PanicAlert("Portable Setting could not be saved\n Are you running Dolphin from read only media or from a directory that dolphin is not located in?");
				}
			}
			else
			{
				char CWD[1024];
				sprintf(CWD, "%s", (const char*)wxGetCwd().mb_str(wxConvUTF8));
				if (PanicYesNo("Set install location to:\n %s ?", CWD))
				{
					FILE* workingDirF = fopen(tmp, "w");
					if (!workingDirF) PanicAlert("Install directory could not be saved");
					else
					{
						fwrite(CWD, ((std::string)CWD).size()+1, 1, workingDirF);
						fwrite("", 1, 1, workingDirF); //seems to be needed on linux
						fclose(workingDirF);
					}
				}
				else PanicAlert("Relaunch Dolphin from the install directory and save from there");
			}
		}
		else
		{
			char *tmpChar;
			long len;
			fseek(workingDir,0,SEEK_END);
			len=ftell(workingDir);
			fseek(workingDir,0,SEEK_SET);
			tmpChar = new char[len];
			fread(tmpChar, len, 1, workingDir);
			fclose(workingDir);
			if (!wxSetWorkingDirectory(wxString::FromAscii(tmpChar)))
			{
				INFO_LOG(CONSOLE, "set working directory failed");
			}
			delete [] tmpChar;
		}
	}
#endif

	// Parse command lines
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

			//create all necessary dir in user directory
			if (!File::Exists(FULL_GC_USER_DIR)) File::CreateFullPath(FULL_GC_USER_DIR);
			if (!File::Exists(FULL_WII_SYSCONF_DIR)) File::CreateFullPath(FULL_WII_SYSCONF_DIR);
			if (!File::Exists(FULL_CONFIG_DIR)) File::CreateDir(FULL_CONFIG_DIR);
			if (!File::Exists(FULL_CACHE_DIR)) File::CreateDir(FULL_CACHE_DIR);
			if (!File::Exists(FULL_DSP_DUMP_DIR)) File::CreateFullPath(FULL_DSP_DUMP_DIR);
			if (!File::Exists(FULL_DUMP_TEXTURES_DIR)) File::CreateFullPath(FULL_DUMP_TEXTURES_DIR);
			if (!File::Exists(FULL_HIRES_TEXTURES_DIR)) File::CreateFullPath(FULL_HIRES_TEXTURES_DIR);
			if (!File::Exists(FULL_MAIL_LOGS_DIR)) File::CreateFullPath(FULL_MAIL_LOGS_DIR);
			if (!File::Exists(FULL_SCREENSHOTS_DIR)) File::CreateFullPath(FULL_SCREENSHOTS_DIR);
			if (!File::Exists(FULL_STATESAVES_DIR)) File::CreateFullPath(FULL_STATESAVES_DIR);

			//copy user wii shared2 SYSCONF if not exist
			if (!File::Exists(WII_SYSCONF_FILE)) File::Copy((File::GetBundleDirectory() + DIR_SEP + "Contents" + DIR_SEP + WII_SYSCONF_FILE).c_str(),WII_SYSCONF_FILE);
			//TODO : if not exist copy game config dir in user dir and detect the revision to upgrade if necessary
			//TODO : if not exist copy maps dir in user dir and detect revision to upgrade if necessary
			
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

		SetEnableAlert(SConfig::GetInstance().m_LocalCoreStartupParameter.bUsePanicHandlers);
		
	// Create the window title
	#ifdef _DEBUG
		const char *title = "Dolphin Debug SVN R " SVN_REV_STR;
	#else
		const char *title = "Dolphin SVN R " SVN_REV_STR;
	#endif

	// If we are debugging let use save the main window position and size
	IniFile ini;
	ini.Load(DEBUGGER_CONFIG_FILE);

	int x, y, w, h;

	ini.Get("MainWindow", "x", &x, 100);
	ini.Get("MainWindow", "y", &y, 100);
	ini.Get("MainWindow", "w", &w, 800);
	ini.Get("MainWindow", "h", &h, 600);

	if (UseDebugger)
	{
		main_frame = new CFrame(UseLogger, (wxFrame*) NULL, wxID_ANY, wxString::FromAscii(title),
				wxPoint(x, y), wxSize(w, h));
	}
	else
	{
		main_frame = new CFrame(UseLogger, (wxFrame*) NULL, wxID_ANY, wxString::FromAscii(title),
				wxPoint(100, 100), wxSize(800, 600));
	}

	// Create debugging window
	if (UseDebugger)
	{
		g_pCodeWindow = new CCodeWindow(SConfig::GetInstance().m_LocalCoreStartupParameter, main_frame);
		g_pCodeWindow->Show(true);
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

// Accessor for the main window class
CFrame* DolphinApp::GetCFrame()
{
	return main_frame;
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


void Host_ShowJitResults(unsigned int address)
{
	CJitWindow::ViewAddr(address);
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
void Host_UpdateLeds(int led_bits)
{
	// Convert it to a simpler byte format
	main_frame->g_Leds[0] = led_bits >> 0;
	main_frame->g_Leds[1] = led_bits >> 1;
	main_frame->g_Leds[2] = led_bits >> 2;
	main_frame->g_Leds[3] = led_bits >> 3;

	main_frame->UpdateLeds();
}

void Host_UpdateSpeakerStatus(int index, int speaker_bits)
{
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
		// Turn off the activity light again
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

void Host_UpdateStatusBar(const char* _pText, int Field)
{
	wxCommandEvent event(wxEVT_HOST_COMMAND, IDM_UPDATESTATUSBAR);
	// Set the event string
	event.SetString(wxString::FromAscii(_pText));
	// Update statusbar field
	event.SetInt(Field);
	// Post message
	// DISABLED - this has been found to cause random hangs.
	// wxPostEvent(main_frame, event);
}

// g_VideoInitialize.pSysMessage() goes here
void Host_SysMessage(const char *fmt, ...) 
{
	va_list list;
	char msg[512];

	va_start(list, fmt);
	vsprintf(msg, fmt, list);
	va_end(list);

	if (msg[strlen(msg)-1] == '\n') msg[strlen(msg)-1] = 0;
	//wxMessageBox(wxString::FromAscii(msg));
	PanicAlert("%s", msg);
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
	// Update field 1 or 2
	#ifdef RERECORDING
		event.SetInt(2);
	#else
		event.SetInt(1);
	#endif

	wxPostEvent(main_frame, event);
}
