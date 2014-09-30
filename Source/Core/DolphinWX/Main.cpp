// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <utility>
#include <wx/app.h>
#include <wx/buffer.h>
#include <wx/chartype.h>
#include <wx/cmdline.h>
#include <wx/defs.h>
#include <wx/event.h>
#include <wx/gdicmn.h>
#include <wx/image.h>
#include <wx/imagpng.h>
#include <wx/intl.h>
#include <wx/language.h>
#include <wx/msgdlg.h>
#include <wx/setup.h>
#include <wx/string.h>
#include <wx/thread.h>
#include <wx/timer.h>
#include <wx/translation.h>
#include <wx/utils.h>
#include <wx/window.h>

#include "Common/CommonPaths.h"
#include "Common/CommonTypes.h"
#include "Common/CPUDetect.h"
#include "Common/FileUtil.h"
#include "Common/IniFile.h"
#include "Common/Thread.h"
#include "Common/Logging/LogManager.h"

#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/CoreParameter.h"
#include "Core/Host.h"
#include "Core/Movie.h"
#include "Core/HW/Wiimote.h"

#include "DolphinWX/Frame.h"
#include "DolphinWX/Globals.h"
#include "DolphinWX/Main.h"
#include "DolphinWX/SoftwareVideoConfigDialog.h"
#include "DolphinWX/VideoConfigDiag.h"
#include "DolphinWX/WxUtils.h"
#include "DolphinWX/Debugger/CodeWindow.h"
#include "DolphinWX/Debugger/JitWindow.h"

#include "VideoCommon/VideoBackendBase.h"

#if defined HAVE_X11 && HAVE_X11
#include <X11/Xlib.h>
#endif

#ifdef _WIN32
#include <shellapi.h>
#include "Common/ExtendedTrace.h"

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

#ifdef __APPLE__
#import <AppKit/AppKit.h>
#endif

class wxFrame;

// Nvidia drivers >= v302 will check if the application exports a global
// variable named NvOptimusEnablement to know if it should run the app in high
// performance graphics mode or using the IGP.
#ifdef WIN32
extern "C" {
	__declspec(dllexport) DWORD NvOptimusEnablement = 1;
}
#endif

// ------------
//  Main window

IMPLEMENT_APP(DolphinApp)

BEGIN_EVENT_TABLE(DolphinApp, wxApp)
	EVT_TIMER(wxID_ANY, DolphinApp::AfterInit)
	EVT_QUERY_END_SESSION(DolphinApp::OnEndSession)
	EVT_END_SESSION(DolphinApp::OnEndSession)
END_EVENT_TABLE()

bool wxMsgAlert(const char*, const char*, bool, int);
std::string wxStringTranslator(const char *);

CFrame* main_frame = nullptr;

#ifdef WIN32
//Has no error handling.
//I think that if an error occurs here there's no way to handle it anyway.
LONG WINAPI MyUnhandledExceptionFilter(LPEXCEPTION_POINTERS e)
{
	//EnterCriticalSection(&g_uefcs);

	File::IOFile file("exceptioninfo.txt", "a");
	file.Seek(0, SEEK_END);
	etfprint(file.GetHandle(), "\n");
	//etfprint(file, g_buildtime);
	//etfprint(file, "\n");
	//dumpCurrentDate(file);
	etfprintf(file.GetHandle(), "Unhandled Exception\n  Code: 0x%08X\n",
		e->ExceptionRecord->ExceptionCode);

	STACKTRACE2(file.GetHandle(), e->ContextRecord->Rip, e->ContextRecord->Rsp, e->ContextRecord->Rbp);

	file.Close();
	_flushall();

	//LeaveCriticalSection(&g_uefcs);
	return EXCEPTION_CONTINUE_SEARCH;
}
#endif

bool DolphinApp::Initialize(int& c, wxChar **v)
{
#if defined HAVE_X11 && HAVE_X11
	XInitThreads();
#endif
	return wxApp::Initialize(c, v);
}

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
	wxString userPath;

#if wxUSE_CMDLINE_PARSER // Parse command lines
	wxCmdLineEntryDesc cmdLineDesc[] =
	{
		{
			wxCMD_LINE_SWITCH, "h", "help",
			"Show this help message",
			wxCMD_LINE_VAL_NONE, wxCMD_LINE_OPTION_HELP
		},
		{
			wxCMD_LINE_SWITCH, "d", "debugger",
			"Opens the debugger",
			wxCMD_LINE_VAL_NONE, wxCMD_LINE_PARAM_OPTIONAL
		},
		{
			wxCMD_LINE_SWITCH, "l", "logger",
			"Opens the logger",
			wxCMD_LINE_VAL_NONE, wxCMD_LINE_PARAM_OPTIONAL
		},
		{
			wxCMD_LINE_OPTION, "e", "exec",
			"Loads the specified file (DOL,ELF,GCM,ISO,WAD)",
			wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL
		},
		{
			wxCMD_LINE_SWITCH, "b", "batch",
			"Exit Dolphin with emulator",
			wxCMD_LINE_VAL_NONE, wxCMD_LINE_PARAM_OPTIONAL
		},
		{
			wxCMD_LINE_OPTION, "V", "video_backend",
			"Specify a video backend",
			wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL
		},
		{
			wxCMD_LINE_OPTION, "A", "audio_emulation",
			"Low level (LLE) or high level (HLE) audio",
			wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL
		},
		{
			wxCMD_LINE_OPTION, "m", "movie",
			"Play a movie file",
			wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL
		},
		{
			wxCMD_LINE_OPTION, "U", "user",
			"User folder path",
			wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL
		},
		{
			wxCMD_LINE_NONE, nullptr, nullptr, nullptr, wxCMD_LINE_VAL_NONE, 0
		}
	};

	// Gets the command line parameters
	wxCmdLineParser parser(cmdLineDesc, argc, argv);
	if (parser.Parse() != 0)
	{
		return false;
	}

	UseDebugger = parser.Found("debugger");
	UseLogger = parser.Found("logger");
	LoadFile = parser.Found("exec", &FileToLoad);
	BatchMode = parser.Found("batch");
	selectVideoBackend = parser.Found("video_backend", &videoBackendName);
	selectAudioEmulation = parser.Found("audio_emulation", &audioEmulationName);
	playMovie = parser.Found("movie", &movieFile);

	if (parser.Found("user", &userPath))
	{
		File::CreateFullPath(WxStrToStr(userPath) + DIR_SEP);
		File::GetUserPath(D_USER_IDX, userPath.ToStdString() + DIR_SEP);
	}
#endif // wxUSE_CMDLINE_PARSER

	// Register message box and translation handlers
	RegisterMsgAlertHandler(&wxMsgAlert);
	RegisterStringTranslator(&wxStringTranslator);

	// "ExtendedTrace" looks freakin' dangerous!!!
#ifdef _WIN32
	EXTENDEDTRACEINITIALIZE(".");
	SetUnhandledExceptionFilter(&MyUnhandledExceptionFilter);
#elif wxUSE_ON_FATAL_EXCEPTION
	wxHandleFatalExceptions(true);
#endif

#ifdef __APPLE__
	if (floor(NSAppKitVersionNumber) < NSAppKitVersionNumber10_7)
	{
		PanicAlertT("Hi,\n\nDolphin requires Mac OS X 10.7 or greater.\n"
				"Unfortunately you're running an old version of OS X.\n"
				"The last Dolphin version to support OS X 10.6 is Dolphin 3.5\n"
				"Please upgrade to 10.7 or greater to use the newest Dolphin version.\n\n"
				"Sayonara!\n");
		return false;
	}
#endif

	// Copy initial Wii NAND data from Sys to User.
	File::CopyDir(File::GetSysDirectory() + WII_USER_DIR DIR_SEP,
	              File::GetUserPath(D_WIIUSER_IDX));

	File::CreateFullPath(File::GetUserPath(D_USER_IDX));
	File::CreateFullPath(File::GetUserPath(D_CACHE_IDX));
	File::CreateFullPath(File::GetUserPath(D_CONFIG_IDX));
	File::CreateFullPath(File::GetUserPath(D_DUMPDSP_IDX));
	File::CreateFullPath(File::GetUserPath(D_DUMPTEXTURES_IDX));
	File::CreateFullPath(File::GetUserPath(D_GAMESETTINGS_IDX));
	File::CreateFullPath(File::GetUserPath(D_GCUSER_IDX));
	File::CreateFullPath(File::GetUserPath(D_GCUSER_IDX) + USA_DIR DIR_SEP);
	File::CreateFullPath(File::GetUserPath(D_GCUSER_IDX) + EUR_DIR DIR_SEP);
	File::CreateFullPath(File::GetUserPath(D_GCUSER_IDX) + JAP_DIR DIR_SEP);
	File::CreateFullPath(File::GetUserPath(D_HIRESTEXTURES_IDX));
	File::CreateFullPath(File::GetUserPath(D_MAILLOGS_IDX));
	File::CreateFullPath(File::GetUserPath(D_MAPS_IDX));
	File::CreateFullPath(File::GetUserPath(D_SCREENSHOTS_IDX));
	File::CreateFullPath(File::GetUserPath(D_SHADERS_IDX));
	File::CreateFullPath(File::GetUserPath(D_STATESAVES_IDX));
	File::CreateFullPath(File::GetUserPath(D_THEMES_IDX));

	LogManager::Init();
	SConfig::Init();
	VideoBackend::PopulateList();
	WiimoteReal::LoadSettings();

	if (selectVideoBackend && videoBackendName != wxEmptyString)
		SConfig::GetInstance().m_LocalCoreStartupParameter.m_strVideoBackend =
			WxStrToStr(videoBackendName);

	if (selectAudioEmulation)
	{
		if (audioEmulationName == "HLE")
			SConfig::GetInstance().m_LocalCoreStartupParameter.bDSPHLE = true;
		else if (audioEmulationName == "LLE")
			SConfig::GetInstance().m_LocalCoreStartupParameter.bDSPHLE = false;
	}

	VideoBackend::ActivateBackend(SConfig::GetInstance().m_LocalCoreStartupParameter.m_strVideoBackend);

	// Enable the PNG image handler for screenshots
	wxImage::AddHandler(new wxPNGHandler);

	SetEnableAlert(SConfig::GetInstance().m_LocalCoreStartupParameter.bUsePanicHandlers);

	int x = SConfig::GetInstance().m_LocalCoreStartupParameter.iPosX;
	int y = SConfig::GetInstance().m_LocalCoreStartupParameter.iPosY;
	int w = SConfig::GetInstance().m_LocalCoreStartupParameter.iWidth;
	int h = SConfig::GetInstance().m_LocalCoreStartupParameter.iHeight;

#ifdef _WIN32
	if (File::Exists("www.dolphin-emulator.com.txt"))
	{
		File::Delete("www.dolphin-emulator.com.txt");
		MessageBox(nullptr,
				   L"This version of Dolphin was downloaded from a website stealing money from developers of the emulator. Please "
				   L"download Dolphin from the official website instead: http://dolphin-emu.org/",
				   L"Unofficial version detected", MB_OK | MB_ICONWARNING);
		ShellExecute(nullptr, L"open", L"http://dolphin-emu.org/?ref=badver", nullptr, nullptr, SW_SHOWDEFAULT);
		exit(0);
	}
#endif

	// The following is not needed with X11, where window managers
	// do not allow windows to be created off the desktop.
#ifdef _WIN32
	// Out of desktop check
	int leftPos = GetSystemMetrics(SM_XVIRTUALSCREEN);
	int topPos = GetSystemMetrics(SM_YVIRTUALSCREEN);
	int width =  GetSystemMetrics(SM_CXVIRTUALSCREEN);
	int height = GetSystemMetrics(SM_CYVIRTUALSCREEN);
	if ((leftPos + width) < (x + w) || leftPos > x || (topPos + height) < (y + h) || topPos > y)
		x = y = wxDefaultCoord;
#elif defined __APPLE__
	if (y < 1)
		y = wxDefaultCoord;
#endif

	main_frame = new CFrame((wxFrame*)nullptr, wxID_ANY,
				StrToWxStr(scm_rev_str),
				wxPoint(x, y), wxSize(w, h),
				UseDebugger, BatchMode, UseLogger);
	SetTopWindow(main_frame);
	main_frame->SetMinSize(wxSize(400, 300));

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

	if (m_afterinit == nullptr)
		main_frame->BootGame(WxStrToStr(FileToLoad));
}

void DolphinApp::AfterInit(wxTimerEvent& WXUNUSED(event))
{
	delete m_afterinit;
	m_afterinit = nullptr;

	if (!BatchMode)
		main_frame->UpdateGameList();

	if (playMovie && movieFile != wxEmptyString)
	{
		if (Movie::PlayInput(WxStrToStr(movieFile)))
		{
			if (LoadFile && FileToLoad != wxEmptyString)
			{
				main_frame->BootGame(WxStrToStr(FileToLoad));
			}
			else
			{
				main_frame->BootGame(std::string(""));
			}
		}
	}

	// First check if we have an exec command line.
	else if (LoadFile && FileToLoad != wxEmptyString)
	{
		main_frame->BootGame(WxStrToStr(FileToLoad));
	}
	// If we have selected Automatic Start, start the default ISO,
	// or if no default ISO exists, start the last loaded ISO
	else if (main_frame->g_pCodeWindow)
	{
		if (main_frame->g_pCodeWindow->AutomaticStart())
		{
			main_frame->BootGame("");
		}
	}
}

void DolphinApp::InitLanguageSupport()
{
	unsigned int language = 0;

	IniFile ini;
	ini.Load(File::GetUserPath(F_DOLPHINCONFIG_IDX));
	ini.GetOrCreateSection("Interface")->Get("Language", &language, wxLANGUAGE_DEFAULT);

	// Load language if possible, fall back to system default otherwise
	if (wxLocale::IsAvailable(language))
	{
		m_locale = new wxLocale(language);

#ifdef _WIN32
		m_locale->AddCatalogLookupPathPrefix(StrToWxStr(File::GetExeDirectory() + DIR_SEP "Languages"));
#endif

		m_locale->AddCatalog("dolphin-emu");

		if (!m_locale->IsOk())
		{
			wxMessageBox(_("Error loading selected language. Falling back to system default."), _("Error"));
			delete m_locale;
			m_locale = new wxLocale(wxLANGUAGE_DEFAULT);
		}
	}
	else
	{
		wxMessageBox(_("The selected language is not supported by your system. Falling back to system default."), _("Error"));
		m_locale = new wxLocale(wxLANGUAGE_DEFAULT);
	}
}

void DolphinApp::OnEndSession(wxCloseEvent& event)
{
	// Close if we've recieved wxEVT_END_SESSION (ignore wxEVT_QUERY_END_SESSION)
	if (!event.CanVeto())
	{
		main_frame->Close(true);
	}
}

int DolphinApp::OnExit()
{
	Core::Shutdown();
	WiimoteReal::Shutdown();
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
	//wxMessageBox(StrToWxStr(msg));
	PanicAlert("%s", msg);
}

bool wxMsgAlert(const char* caption, const char* text, bool yes_no, int /*Style*/)
{
#ifdef __WXGTK__
	if (wxIsMainThread())
#endif
		return wxYES == wxMessageBox(StrToWxStr(text), StrToWxStr(caption),
				(yes_no) ? wxYES_NO : wxOK, wxWindow::FindFocus());
#ifdef __WXGTK__
	else
	{
		wxCommandEvent event(wxEVT_HOST_COMMAND, IDM_PANIC);
		event.SetString(StrToWxStr(caption) + ":" + StrToWxStr(text));
		event.SetInt(yes_no);
		main_frame->GetEventHandler()->AddPendingEvent(event);
		main_frame->panic_event.Wait();
		return main_frame->bPanicResult;
	}
#endif
}

std::string wxStringTranslator(const char *text)
{
	return WxStrToStr(wxGetTranslation(wxString::FromUTF8(text)));
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

void Host_UpdateDisasmDialog()
{
	wxCommandEvent event(wxEVT_HOST_COMMAND, IDM_UPDATEDISASMDIALOG);
	main_frame->GetEventHandler()->AddPendingEvent(event);

	if (main_frame->g_pCodeWindow)
	{
		main_frame->g_pCodeWindow->GetEventHandler()->AddPendingEvent(event);
	}
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

void Host_UpdateTitle(const std::string& title)
{
	wxCommandEvent event(wxEVT_HOST_COMMAND, IDM_UPDATETITLE);
	event.SetString(StrToWxStr(title));
	main_frame->GetEventHandler()->AddPendingEvent(event);
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

void Host_RequestFullscreen(bool enable_fullscreen)
{
	wxCommandEvent event(wxEVT_HOST_COMMAND, IDM_FULLSCREENREQUEST);
	event.SetInt(enable_fullscreen ? 1 : 0);
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

void Host_SetWiiMoteConnectionState(int _State)
{
	static int currentState = -1;
	if (_State == currentState)
		return;
	currentState = _State;

	wxCommandEvent event(wxEVT_HOST_COMMAND, IDM_UPDATESTATUSBAR);

	switch (_State)
	{
	case 0: event.SetString(_("Not connected")); break;
	case 1: event.SetString(_("Connecting...")); break;
	case 2: event.SetString(_("Wiimote Connected")); break;
	}
	// Update field 1 or 2
	event.SetInt(1);

	NOTICE_LOG(WIIMOTE, "%s", static_cast<const char*>(event.GetString().c_str()));

	main_frame->GetEventHandler()->AddPendingEvent(event);
}

bool Host_UIHasFocus()
{
	return main_frame->UIHasFocus();
}

bool Host_RendererHasFocus()
{
	return main_frame->RendererHasFocus();
}

void Host_ConnectWiimote(int wm_idx, bool connect)
{
	CFrame::ConnectWiimote(wm_idx, connect);
}

void Host_ShowVideoConfig(void* parent, const std::string& backend_name,
                          const std::string& config_name)
{
	if (backend_name == "Direct3D" || backend_name == "OpenGL")
	{
		VideoConfigDiag diag((wxWindow*)parent, backend_name, config_name);
		diag.ShowModal();
	}
	else if (backend_name == "Software Renderer")
	{
		SoftwareVideoConfigDialog diag((wxWindow*)parent, backend_name, config_name);
		diag.ShowModal();
	}
}
