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

#include <vector>
#include <string>

#include "Common.h"
#include "CommonPaths.h"

#if defined HAVE_X11 && HAVE_X11
#include <X11/Xlib.h>
#endif

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
#include "Frame.h"

// ------------
//  Main window

IMPLEMENT_APP(DolphinApp)

BEGIN_EVENT_TABLE(DolphinApp, wxApp)
	EVT_TIMER(wxID_ANY, DolphinApp::AfterInit)
END_EVENT_TABLE()

#include <wx/stdpaths.h>
bool wxMsgAlert(const char*, const char*, bool, int);

CFrame* main_frame = NULL;

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
	// Declarations and definitions
	bool UseDebugger = false;
	bool BatchMode = false;
	bool UseLogger = false;
	bool selectVideoPlugin = false;
	bool selectAudioPlugin = false;
	bool selectWiimotePlugin = false;

	wxString videoPluginFilename;
	wxString audioPluginFilename;
	wxString wiimotePluginFilename;

#if wxUSE_CMDLINE_PARSER // Parse command lines
#if wxCHECK_VERSION(2, 9, 0)
	wxCmdLineEntryDesc cmdLineDesc[] =
	{
		{
			wxCMD_LINE_SWITCH, "h", "help", "Show this help message",
			wxCMD_LINE_VAL_NONE, wxCMD_LINE_OPTION_HELP
		},
		{
			wxCMD_LINE_SWITCH, "d", "debugger", "Opens the debugger"
		},
		{
			wxCMD_LINE_SWITCH, "l", "logger", "Opens the logger"
		},
		{
			wxCMD_LINE_OPTION, "e", "exec", "Loads the specified file (DOL, ELF, WAD, GCM, ISO)",
			wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL
		},
		{
			wxCMD_LINE_SWITCH, "b", "batch", "Exit Dolphin with emulator"
		},
		{
			wxCMD_LINE_OPTION, "V", "video_plugin","Specify a video plugin",
			wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL
		},
		{
			wxCMD_LINE_OPTION, "A", "audio_plugin","Specify an audio plugin",
			wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL
		},
		{
			wxCMD_LINE_OPTION, "W", "wiimote_plugin","Specify a wiimote plugin",
			wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL
		},
		{
			wxCMD_LINE_NONE
		}
	};
#else
	wxCmdLineEntryDesc cmdLineDesc[] =
	{
		{
			wxCMD_LINE_SWITCH, _("h"), _("help"), 
			wxT("Show this help message"),
			wxCMD_LINE_VAL_NONE, wxCMD_LINE_OPTION_HELP
		},
		{
			wxCMD_LINE_SWITCH, _("d"), _("debugger"), wxT("Opens the debugger")
		},
		{
			wxCMD_LINE_SWITCH, _("l"), _("logger"), wxT("Opens the logger")
		},
		{
			wxCMD_LINE_OPTION, _("e"), _("exec"), wxT("Loads the specified file (DOL, ELF, WAD, GCM, ISO)"),
			wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL
		},
		{
			wxCMD_LINE_SWITCH, _("b"), _("batch"), wxT("Exit Dolphin with emulator")
		},
		{
			wxCMD_LINE_OPTION, _("V"), _("video_plugin"), wxT("Specify a video plugin"),
			wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL
		},
		{
			wxCMD_LINE_OPTION, _("A"), _("audio_plugin"), wxT("Specify an audio plugin"),
			wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL
		},
		{
			wxCMD_LINE_OPTION, _("W"), _("wiimote_plugin"), wxT("Specify a wiimote plugin"),
			wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL
		},
		{
			wxCMD_LINE_NONE
		}
	};
#endif
	// Gets the command line parameters
	wxCmdLineParser parser(cmdLineDesc, argc, argv);

	if (parser.Parse() != 0)
	{
		return false;
	} 
#if wxCHECK_VERSION(2, 9, 0)
	UseDebugger = parser.Found("debugger");
	UseLogger = parser.Found("logger");
	LoadFile = parser.Found("exec", &FileToLoad);
	BatchMode = parser.Found("batch");
#else
	UseDebugger = parser.Found(wxT("debugger"));
	UseLogger = parser.Found(wxT("logger"));
	LoadFile = parser.Found(wxT("exec"), &FileToLoad);
	BatchMode = parser.Found(wxT("batch"));
#endif

#if wxCHECK_VERSION(2, 9, 0)
	selectVideoPlugin = parser.Found("video_plugin", &videoPluginFilename);
	selectAudioPlugin = parser.Found("audio_plugin", &audioPluginFilename);
	selectWiimotePlugin = parser.Found("wiimote_plugin", &wiimotePluginFilename);
#else
	selectVideoPlugin = parser.Found(wxT("video_plugin"), &videoPluginFilename);
	selectAudioPlugin = parser.Found(wxT("audio_plugin"), &audioPluginFilename);
	selectWiimotePlugin = parser.Found(wxT("wiimote_plugin"), &wiimotePluginFilename);
#endif
#endif // wxUSE_CMDLINE_PARSER

#if defined _DEBUG && defined _WIN32
	int tmpflag = _CrtSetDbgFlag(_CRTDBG_REPORT_FLAG);
	tmpflag |= _CRTDBG_DELAY_FREE_MEM_DF;
	_CrtSetDbgFlag(tmpflag);
#endif

	// Register message box handler
#ifndef _WIN32
	RegisterMsgAlertHandler(&wxMsgAlert);
#endif

	// "ExtendedTrace" looks freakin dangerous!!!
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

#if ! defined(__APPLE__) && ! defined(__linux__)
	// Keep the user config dir free unless user wants to save the working dir
	if (!File::Exists((std::string(File::GetUserPath(D_CONFIG_IDX)) + "portable").c_str()))
	{
		char tmp[1024];
		sprintf(tmp, "%s/.dolphin%swd", (const char*)wxStandardPaths::Get().GetUserConfigDir().mb_str(),
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
				FILE* portable = fopen((std::string(File::GetUserPath(D_CONFIG_IDX)) + "portable").c_str(), "w");
				if (!portable)
				{
					PanicAlert("Portable Setting could not be saved\n Are you running Dolphin from read only media or from a directory that dolphin is not located in?");
				}
				else
				{
					fclose(portable);
				}
			}
			else
			{
				char CWD[1024];
				sprintf(CWD, "%s", (const char*)wxGetCwd().mb_str());
				if (PanicYesNo("Set install location to:\n %s ?", CWD))
				{
					FILE* workingDirF = fopen(tmp, "w");
					if (!workingDirF)
						PanicAlert("Install directory could not be saved");
					else
					{
						fwrite(CWD, ((std::string)CWD).size()+1, 1, workingDirF);
						fwrite("", 1, 1, workingDirF); //seems to be needed on linux
						fclose(workingDirF);
					}
				}
				else
					PanicAlert("Relaunch Dolphin from the install directory and save from there");
			}
		}
		else
		{
			char *tmpChar;
			long len;
			fseek(workingDir, 0, SEEK_END);
			len = ftell(workingDir);
			fseek(workingDir, 0, SEEK_SET);
			tmpChar = new char[len];
			fread(tmpChar, len, 1, workingDir);
			fclose(workingDir);
			if (!wxSetWorkingDirectory(wxString::From8BitData(tmpChar)))
			{
				INFO_LOG(CONSOLE, "set working directory failed");
			}
			delete [] tmpChar;
		}
	}
#endif

#ifdef __APPLE__
	const char *AppSupportDir = File::GetUserPath(D_USER_IDX);

	if (!File::Exists(AppSupportDir))
	{
		// Fresh run: create Dolphin dir and copy contents of User within the bundle to App Support
		File::CopyDir(std::string(File::GetBundleDirectory() + DIR_SEP USERDATA_DIR DIR_SEP).c_str(), AppSupportDir);
	}
	else if (!File::IsDirectory(AppSupportDir))
		PanicAlert("~/Library/Application Support/Dolphin exists, but is not a directory");
#endif

#ifdef __linux__
	//create all necessary directories in user directory
	//TODO : detect the revision and upgrade where necessary
	File::CopyDir(SHARED_USER_DIR CONFIG_DIR DIR_SEP, File::GetUserPath(D_CONFIG_IDX));
	File::CopyDir(SHARED_USER_DIR GAMECONFIG_DIR DIR_SEP, File::GetUserPath(D_GAMECONFIG_IDX));
	File::CopyDir(SHARED_USER_DIR MAPS_DIR DIR_SEP, File::GetUserPath(D_MAPS_IDX));
	File::CopyDir(SHARED_USER_DIR SHADERS_DIR DIR_SEP, File::GetUserPath(D_SHADERS_IDX));
	File::CopyDir(SHARED_USER_DIR WII_USER_DIR DIR_SEP, File::GetUserPath(D_WIIUSER_IDX));

	if (!File::Exists(File::GetUserPath(D_GCUSER_IDX)))
		File::CreateFullPath(File::GetUserPath(D_GCUSER_IDX));
	if (!File::Exists(File::GetUserPath(D_CACHE_IDX)))
		File::CreateFullPath(File::GetUserPath(D_CACHE_IDX));
	if (!File::Exists(File::GetUserPath(D_DUMPDSP_IDX)))
		File::CreateFullPath(File::GetUserPath(D_DUMPDSP_IDX));
	if (!File::Exists(File::GetUserPath(D_DUMPTEXTURES_IDX)))
		File::CreateFullPath(File::GetUserPath(D_DUMPTEXTURES_IDX));
	if (!File::Exists(File::GetUserPath(D_HIRESTEXTURES_IDX)))
		File::CreateFullPath(File::GetUserPath(D_HIRESTEXTURES_IDX));
	if (!File::Exists(File::GetUserPath(D_SCREENSHOTS_IDX)))
		File::CreateFullPath(File::GetUserPath(D_SCREENSHOTS_IDX));
	if (!File::Exists(File::GetUserPath(D_STATESAVES_IDX)))
		File::CreateFullPath(File::GetUserPath(D_STATESAVES_IDX));
	if (!File::Exists(File::GetUserPath(D_MAILLOGS_IDX)))
		File::CreateFullPath(File::GetUserPath(D_MAILLOGS_IDX));
#endif

	LogManager::Init();
	SConfig::Init();
	CPluginManager::Init();

	if (selectVideoPlugin && videoPluginFilename != wxEmptyString)
		SConfig::GetInstance().m_LocalCoreStartupParameter.m_strVideoPlugin =
			std::string(videoPluginFilename.mb_str());

	if (selectAudioPlugin && audioPluginFilename != wxEmptyString)
		SConfig::GetInstance().m_LocalCoreStartupParameter.m_strDSPPlugin =
			std::string(audioPluginFilename.mb_str());

	if (selectWiimotePlugin && wiimotePluginFilename != wxEmptyString)
		SConfig::GetInstance().m_LocalCoreStartupParameter.m_strWiimotePlugin =
			std::string(wiimotePluginFilename.mb_str());

	// Enable the PNG image handler
	wxInitAllImageHandlers(); 

	SetEnableAlert(SConfig::GetInstance().m_LocalCoreStartupParameter.bUsePanicHandlers);

	int x = SConfig::GetInstance().m_LocalCoreStartupParameter.iPosX;
	int y = SConfig::GetInstance().m_LocalCoreStartupParameter.iPosY;
	int w = SConfig::GetInstance().m_LocalCoreStartupParameter.iWidth;
	int h = SConfig::GetInstance().m_LocalCoreStartupParameter.iHeight;

	// The following is not needed in linux.  Linux window managers do not allow windows to
	// be created off the desktop.
#ifdef _WIN32
	// Out of desktop check
	HWND hDesktop = GetDesktopWindow();
	RECT rc;
	GetWindowRect(hDesktop, &rc);
	if (rc.right < x + w || rc.bottom < y + h)
		x = y = -1;
#endif

	main_frame = new CFrame((wxFrame*)NULL, wxID_ANY,
				wxString::FromAscii(svn_rev_str),
				wxPoint(x, y), wxSize(w, h),
				UseDebugger, BatchMode, UseLogger);
	SetTopWindow(main_frame);

#if defined HAVE_X11 && HAVE_X11
	XInitThreads();
#endif 

	// Postpone final actions until event handler is running
	m_afterinit = new wxTimer(this, wxID_ANY);
	m_afterinit->Start(1, wxTIMER_ONE_SHOT);

	return true;
}

void DolphinApp::AfterInit(wxTimerEvent& WXUNUSED(event))
{
	delete m_afterinit;

	// Updating the game list makes use of wxProgressDialog which may
	// only be run after OnInit() when the event handler is running.
	main_frame->UpdateGameList();

	// Check the autoboot options:

	// First check if we have an exec command line.
	if (LoadFile && FileToLoad != wxEmptyString)
	{
		main_frame->BootGame(std::string(FileToLoad.mb_str()));
	}

	// If we have selected Automatic Start, start the default ISO,
	// or if no default ISO exists, start the last loaded ISO
	else if (main_frame->g_pCodeWindow)
	{
		if (main_frame->g_pCodeWindow->AutomaticStart())
		{
			if(!SConfig::GetInstance().m_LocalCoreStartupParameter.m_strDefaultGCM.empty()
					&& File::Exists(SConfig::GetInstance().m_LocalCoreStartupParameter.
						m_strDefaultGCM.c_str()))
			{
				main_frame->BootGame(SConfig::GetInstance().m_LocalCoreStartupParameter.
						m_strDefaultGCM);
			}
			else if(!SConfig::GetInstance().m_LastFilename.empty()
					&& File::Exists(SConfig::GetInstance().m_LastFilename.c_str()))
			{
				main_frame->BootGame(SConfig::GetInstance().m_LastFilename);
			}	
		}
	}
}

void DolphinApp::OnEndSession()
{
	SConfig::GetInstance().SaveSettings();
}

int DolphinApp::OnExit()
{
#ifdef _WIN32
	if (SConfig::GetInstance().m_WiiAutoUnpair)
	{
		if (CPluginManager::GetInstance().GetWiimote())
			CPluginManager::GetInstance().GetWiimote()->Wiimote_UnPairWiimotes();
	}
#endif
	CPluginManager::Shutdown();
	SConfig::Shutdown();
	LogManager::Shutdown();

	return wxApp::OnExit();
}


// ------------
// Talk to GUI


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

bool wxMsgAlert(const char* caption, const char* text, bool yes_no, int /*Style*/)
{
	return wxYES == wxMessageBox(wxString::FromAscii(text), 
				 wxString::FromAscii(caption),
				 (yes_no)?wxYES_NO:wxOK);
}

// Accessor for the main window class
CFrame* DolphinApp::GetCFrame()
{
	return main_frame;
}

void Host_Message(int Id)
{
	wxCommandEvent event(wxEVT_HOST_COMMAND, Id);
	main_frame->GetEventHandler()->AddPendingEvent(event);
}

// OK, this thread boundary is DANGEROUS on linux
// wxPostEvent / wxAddPendingEvent is the solution.
void Host_NotifyMapLoaded()
{
	wxCommandEvent event(wxEVT_HOST_COMMAND, IDM_NOTIFYMAPLOADED);
	main_frame->GetEventHandler()->AddPendingEvent(event);

	if (main_frame->g_pCodeWindow)
	{
		main_frame->g_pCodeWindow->GetEventHandler()->AddPendingEvent(event);
	}
}


void Host_UpdateLogDisplay()
{
	wxCommandEvent event(wxEVT_HOST_COMMAND, IDM_UPDATELOGDISPLAY);
	main_frame->GetEventHandler()->AddPendingEvent(event);

	if (main_frame->g_pCodeWindow)
	{
		main_frame->g_pCodeWindow->GetEventHandler()->AddPendingEvent(event);
	}
}


void Host_UpdateDisasmDialog()
{
	wxCommandEvent event(wxEVT_HOST_COMMAND, IDM_UPDATEDISASMDIALOG);
	main_frame->GetEventHandler()->AddPendingEvent(event);

	if (main_frame->g_pCodeWindow)
	{
		main_frame->g_pCodeWindow->GetEventHandler()->AddPendingEvent(event);
	}
}


void Host_ShowJitResults(unsigned int address)
{
	CJitWindow::ViewAddr(address);
}

void Host_UpdateMainFrame()
{
	wxCommandEvent event(wxEVT_HOST_COMMAND, IDM_UPDATEGUI);
	main_frame->GetEventHandler()->AddPendingEvent(event);

	if (main_frame->g_pCodeWindow)
	{
		main_frame->g_pCodeWindow->GetEventHandler()->AddPendingEvent(event);
	}
}

void Host_UpdateTitle(const char* title)
{
	wxCommandEvent event(wxEVT_HOST_COMMAND, IDM_UPDATETITLE);
	event.SetString(wxString::FromAscii(title));
	main_frame->GetEventHandler()->AddPendingEvent(event);
}

void Host_UpdateBreakPointView()
{
	wxCommandEvent event(wxEVT_HOST_COMMAND, IDM_UPDATEBREAKPOINTS);
	main_frame->GetEventHandler()->AddPendingEvent(event);

	if (main_frame->g_pCodeWindow)
	{
		main_frame->g_pCodeWindow->GetEventHandler()->AddPendingEvent(event);
	}
}

void Host_UpdateMemoryView()
{}

void Host_SetDebugMode(bool)
{}

void Host_RequestWindowSize(int& x, int& y, int& width, int& height)
{
	main_frame->OnSizeRequest(x, y, width, height);
}

void Host_SetWaitCursor(bool enable)
{
	if (enable)
		wxBeginBusyCursor();
	else
		wxEndBusyCursor();
}

void Host_UpdateStatusBar(const char* _pText, int Field)
{
	wxCommandEvent event(wxEVT_HOST_COMMAND, IDM_UPDATESTATUSBAR);
	// Set the event string
	event.SetString(wxString::FromAscii(_pText));
	// Update statusbar field
	event.SetInt(Field);
	// Post message
	// TODO : this has been said to cause hang (??) how is that even possible ? :d
	event.StopPropagation();
	main_frame->GetEventHandler()->AddPendingEvent(event);
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
	event.SetInt(1);

	main_frame->GetEventHandler()->AddPendingEvent(event);
}

bool Host_RendererHasFocus()
{
	return main_frame->RendererHasFocus();
}
