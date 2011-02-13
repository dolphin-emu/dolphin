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
#include "HW/Wiimote.h"

#include "Globals.h" // Local
#include "Main.h"
#include "ConfigManager.h"
#include "Debugger/CodeWindow.h"
#include "Debugger/JitWindow.h"
#include "ExtendedTrace.h"
#include "BootManager.h"
#include "Frame.h"

#include "VideoBackendBase.h"

#include <wx/intl.h>

// ------------
//  Main window

IMPLEMENT_APP(DolphinApp)

BEGIN_EVENT_TABLE(DolphinApp, wxApp)
	EVT_TIMER(wxID_ANY, DolphinApp::AfterInit)
END_EVENT_TABLE()

#include <wx/stdpaths.h>
bool wxMsgAlert(const char*, const char*, bool, int);
std::string wxStringTranslator(const char *);

CFrame* main_frame = NULL;

#ifdef WIN32
//Has no error handling.
//I think that if an error occurs here there's no way to handle it anyway.
LONG WINAPI MyUnhandledExceptionFilter(LPEXCEPTION_POINTERS e) {
	//EnterCriticalSection(&g_uefcs);

	FILE* file = NULL;
	fopen_s(&file, "exceptioninfo.txt", "a");
	fseeko(file, 0, SEEK_END);
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
	InitLanguageSupport();

	// Declarations and definitions
	bool UseDebugger = false;
	bool UseLogger = false;
	bool selectVideoBackend = false;
	bool selectAudioEmulation = false;

	wxString videoBackendName;
	wxString audioEmulationName;

#if wxUSE_CMDLINE_PARSER // Parse command lines
	wxCmdLineEntryDesc cmdLineDesc[] =
	{
		{
			wxCMD_LINE_SWITCH, wxS("h"), wxS("help"),
			_("Show this help message"),
			wxCMD_LINE_VAL_NONE, wxCMD_LINE_OPTION_HELP
		},
		{
			wxCMD_LINE_SWITCH, wxS("d"), wxS("debugger"),
			_("Opens the debugger")
		},
		{
			wxCMD_LINE_SWITCH, wxS("l"), wxS("logger"),
			_("Opens the logger")
		},
		{
			wxCMD_LINE_OPTION, wxS("e"), wxS("exec"),
			_("Loads the specified file (DOL, ELF, WAD, GCM, ISO)"),
			wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL
		},
		{
			wxCMD_LINE_SWITCH, wxS("b"), wxS("batch"),
			_("Exit Dolphin with emulator")
		},
		{
			wxCMD_LINE_OPTION, wxS("V"), wxS("video_backend"),
			_("Specify a video backend"),
			wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL
		},
		{
			wxCMD_LINE_OPTION, wxS("A"), wxS("audio_emulation"),
			_("Specify low level (LLE) or high level (HLE) audio emulation"),
			wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL
		},
		{
			wxCMD_LINE_NONE
		}
	};

	// Gets the command line parameters
	wxCmdLineParser parser(cmdLineDesc, argc, argv);
	if (parser.Parse() != 0)
	{
		return false;
	} 

	UseDebugger = parser.Found(wxT("debugger"));
	UseLogger = parser.Found(wxT("logger"));
	LoadFile = parser.Found(wxT("exec"), &FileToLoad);
	BatchMode = parser.Found(wxT("batch"));
	selectVideoBackend = parser.Found(wxT("video_backend"),
		&videoBackendName);
	// TODO:  This currently has no effect.  Implement or delete.
	selectAudioEmulation = parser.Found(wxT("audio_emulation"),
		&audioEmulationName);
#endif // wxUSE_CMDLINE_PARSER

#if defined _DEBUG && defined _WIN32
	int tmpflag = _CrtSetDbgFlag(_CRTDBG_REPORT_FLAG);
	tmpflag |= _CRTDBG_DELAY_FREE_MEM_DF;
	_CrtSetDbgFlag(tmpflag);
#endif

	// Register message box and translation handlers
	RegisterMsgAlertHandler(&wxMsgAlert);
	RegisterStringTranslator(&wxStringTranslator);

	// "ExtendedTrace" looks freakin dangerous!!!
#ifdef _WIN32
	EXTENDEDTRACEINITIALIZE(".");
	SetUnhandledExceptionFilter(&MyUnhandledExceptionFilter);
#elif wxUSE_ON_FATAL_EXCEPTION
	wxHandleFatalExceptions(true);
#endif

	// TODO: if First Boot
	if (!cpu_info.bSSE2) 
	{
		PanicAlertT("Hi,\n\nDolphin requires that your CPU has support for SSE2 extensions.\n"
				"Unfortunately your CPU does not support them, so Dolphin will not run.\n\n"
				"Sayonara!\n");
		return false;
	}

#ifdef _WIN32
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
			if (PanicYesNoT("Dolphin has not been configured with an install location,\nKeep Dolphin portable?"))
			{
				FILE* portable = fopen((std::string(File::GetUserPath(D_CONFIG_IDX)) + "portable").c_str(), "w");
				if (!portable)
				{
					PanicAlertT("Portable Setting could not be saved\n Are you running Dolphin from read only media or from a directory that dolphin is not located in?");
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
				if (PanicYesNoT("Set install location to:\n %s ?", CWD))
				{
					FILE* workingDirF = fopen(tmp, "w");
					if (!workingDirF)
						PanicAlertT("Install directory could not be saved");
					else
					{
						fwrite(CWD, ((std::string)CWD).size()+1, 1, workingDirF);
						fwrite("", 1, 1, workingDirF); //seems to be needed on linux
						fclose(workingDirF);
					}
				}
				else
					PanicAlertT("Relaunch Dolphin from the install directory and save from there");
			}
		}
		else
		{
			char *tmpChar;
			size_t len = (size_t)File::GetSize(workingDir);
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
#else
	//create all necessary directories in user directory
	//TODO : detect the revision and upgrade where necessary
	File::CopyDir(std::string(SHARED_USER_DIR CONFIG_DIR DIR_SEP).c_str(),
		File::GetUserPath(D_CONFIG_IDX));
	File::CopyDir(std::string(SHARED_USER_DIR GAMECONFIG_DIR DIR_SEP).c_str(),
		File::GetUserPath(D_GAMECONFIG_IDX));
	File::CopyDir(std::string(SHARED_USER_DIR MAPS_DIR DIR_SEP).c_str(),
		File::GetUserPath(D_MAPS_IDX));
	File::CopyDir(std::string(SHARED_USER_DIR SHADERS_DIR DIR_SEP).c_str(),
		File::GetUserPath(D_SHADERS_IDX));
	File::CopyDir(std::string(SHARED_USER_DIR WII_USER_DIR DIR_SEP).c_str(),
		File::GetUserPath(D_WIIUSER_IDX));
	File::CopyDir(std::string(SHARED_USER_DIR OPENCL_DIR DIR_SEP).c_str(),
		File::GetUserPath(D_OPENCL_IDX));

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
	VideoBackend::PopulateList();
	WiimoteReal::LoadSettings();

	if (selectVideoBackend && videoBackendName != wxEmptyString)
		SConfig::GetInstance().m_LocalCoreStartupParameter.m_strVideoBackend =
			std::string(videoBackendName.mb_str());

	VideoBackend::ActivateBackend(SConfig::GetInstance().m_LocalCoreStartupParameter.m_strVideoBackend);

	// Enable the PNG image handler for screenshots
	wxImage::AddHandler(new wxPNGHandler);

	SetEnableAlert(SConfig::GetInstance().m_LocalCoreStartupParameter.bUsePanicHandlers);

	int x = SConfig::GetInstance().m_LocalCoreStartupParameter.iPosX;
	int y = SConfig::GetInstance().m_LocalCoreStartupParameter.iPosY;
	int w = SConfig::GetInstance().m_LocalCoreStartupParameter.iWidth;
	int h = SConfig::GetInstance().m_LocalCoreStartupParameter.iHeight;

	// The following is not needed with X11, where window managers
	// do not allow windows to be created off the desktop.
#ifdef _WIN32
	// Out of desktop check
	HWND hDesktop = GetDesktopWindow();
	RECT rc;
	GetWindowRect(hDesktop, &rc);
	if (rc.right < x + w || rc.bottom < y + h)
		x = y = wxDefaultCoord;
#elif defined __APPLE__
	if (y < 1)
		y = wxDefaultCoord;
#endif

	main_frame = new CFrame((wxFrame*)NULL, wxID_ANY,
				wxString::FromAscii(svn_rev_str),
				wxPoint(x, y), wxSize(w, h),
				UseDebugger, BatchMode, UseLogger);
	SetTopWindow(main_frame);
	main_frame->SetMinSize(wxSize(400, 300));

#if defined HAVE_X11 && HAVE_X11
	XInitThreads();
#endif 

	// Postpone final actions until event handler is running.
	// Updating the game list makes use of wxProgressDialog which may
	// only be run after OnInit() when the event handler is running.
	m_afterinit = new wxTimer(this, wxID_ANY);
	m_afterinit->Start(1, wxTIMER_ONE_SHOT);

	return true;
}

void DolphinApp::MacOpenFile(const wxString &fileName)
{
	FileToLoad = fileName;
	LoadFile = true;

	if (m_afterinit == NULL)
		main_frame->BootGame(std::string(FileToLoad.mb_str()));
}

void DolphinApp::AfterInit(wxTimerEvent& WXUNUSED(event))
{
	delete m_afterinit;
	m_afterinit = NULL;

	if (!BatchMode)
		main_frame->UpdateGameList();

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

void DolphinApp::InitLanguageSupport()
{
	unsigned int language = 0;

	IniFile ini;
	ini.Load(File::GetUserPath(F_DOLPHINCONFIG_IDX));
	ini.Get("Interface", "Language", &language, wxLANGUAGE_DEFAULT);

	// Load language if possible, fall back to system default otherwise
	if(wxLocale::IsAvailable(language))
	{
		m_locale = new wxLocale(language);

#ifdef _WIN32
		m_locale->AddCatalogLookupPathPrefix(wxT("Languages"));
#endif

		m_locale->AddCatalog(wxT("dolphin-emu"));

		if(!m_locale->IsOk())
		{
			PanicAlertT("Error loading selected language. Falling back to system default.");
			delete m_locale;
			m_locale = new wxLocale(wxLANGUAGE_DEFAULT);
		}
	}
	else
	{
		PanicAlertT("The selected language is not supported by your system. Falling back to system default.");
		m_locale = new wxLocale(wxLANGUAGE_DEFAULT);
	}
}

int DolphinApp::OnExit()
{
	WiimoteReal::Shutdown();
#ifdef _WIN32
	if (SConfig::GetInstance().m_WiiAutoUnpair)
		WiimoteReal::UnPair();
#endif
	VideoBackend::ClearList();
	SConfig::Shutdown();
	LogManager::Shutdown();

	delete m_locale;

	return wxApp::OnExit();
}

void DolphinApp::OnFatalException()
{
	WiimoteReal::Shutdown();
}


// ------------
// Talk to GUI

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
#ifdef __WXGTK__
	if (wxIsMainThread())
#endif
		return wxYES == wxMessageBox(wxString::FromUTF8(text), 
				wxString::FromUTF8(caption),
				(yes_no) ? wxYES_NO : wxOK, main_frame);
#ifdef __WXGTK__
	else
	{
		wxCommandEvent event(wxEVT_HOST_COMMAND, IDM_PANIC);
		event.SetString(wxString::FromUTF8(text));
		event.SetInt(yes_no);
		main_frame->GetEventHandler()->AddPendingEvent(event);
		main_frame->panic_event.Wait();
		return main_frame->bPanicResult;
	}
#endif
}

std::string wxStringTranslator(const char *text)
{
	return (const char *)wxString(wxGetTranslation(wxString::From8BitData(text))).ToUTF8();
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

#ifdef _WIN32
extern "C" HINSTANCE wxGetInstance();
void* Host_GetInstance()
{
	return (void*)wxGetInstance();
}
#else
void* Host_GetInstance()
{
	return NULL;
}
#endif

void* Host_GetRenderHandle()
{
    return main_frame->GetRenderHandle();
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

bool Host_GetKeyState(int keycode)
{
#ifdef _WIN32
	return GetAsyncKeyState(keycode);
#elif defined __WXGTK__
	wxCommandEvent event(wxEVT_HOST_COMMAND, IDM_KEYSTATE);
	event.SetInt(keycode);
	main_frame->GetEventHandler()->AddPendingEvent(event);
	main_frame->keystate_event.Wait();
	return main_frame->bKeyStateResult;
#else
	return wxGetKeyState(wxKeyCode(keycode));
#endif
}

void Host_GetRenderWindowSize(int& x, int& y, int& width, int& height)
{
	main_frame->GetRenderWindowSize(x, y, width, height);
}

void Host_RequestRenderWindowSize(int width, int height)
{
	wxCommandEvent event(wxEVT_HOST_COMMAND, IDM_WINDOWSIZEREQUEST);
	event.SetClientData(new std::pair<int, int>(width, height));
	main_frame->GetEventHandler()->AddPendingEvent(event);
}

void Host_SetStartupDebuggingParameters()
{
	SCoreStartupParameter& StartUp = SConfig::GetInstance().m_LocalCoreStartupParameter;
	if (main_frame->g_pCodeWindow)
	{    
		StartUp.bBootToPause = main_frame->g_pCodeWindow->BootToPause();
		StartUp.bAutomaticStart = main_frame->g_pCodeWindow->AutomaticStart();
		StartUp.bJITNoBlockCache = main_frame->g_pCodeWindow->JITNoBlockCache();
		StartUp.bJITBlockLinking = main_frame->g_pCodeWindow->JITBlockLinking();
	}
	else
	{
		StartUp.bBootToPause = false;
	}
	StartUp.bEnableDebugging = main_frame->g_pCodeWindow ? true : false; // RUNNING_DEBUG
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
	case 0: event.SetString(_("Not connected")); break;
	case 1: event.SetString(_("Connecting...")); break;
	case 2: event.SetString(_("Wiimote Connected")); break;
	}
	// Update field 1 or 2
	event.SetInt(1);

	main_frame->GetEventHandler()->AddPendingEvent(event);
}

bool Host_RendererHasFocus()
{
	return main_frame->RendererHasFocus();
}

void Host_ConnectWiimote(int wm_idx, bool connect)
{
	CFrame::ConnectWiimote(wm_idx, connect);
}
