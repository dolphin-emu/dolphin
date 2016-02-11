// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <utility>
#include <wx/app.h>
#include <wx/buffer.h>
#include <wx/cmdline.h>
#include <wx/image.h>
#include <wx/imagpng.h>
#include <wx/intl.h>
#include <wx/language.h>
#include <wx/msgdlg.h>
#include <wx/thread.h>
#include <wx/timer.h>
#include <wx/utils.h>
#include <wx/window.h>

#include "Common/CommonPaths.h"
#include "Common/CommonTypes.h"
#include "Common/CPUDetect.h"
#include "Common/FileUtil.h"
#include "Common/OnionConfig.h"
#include "Common/Thread.h"
#include "Common/Logging/LogManager.h"

#include "Core/ConfigManager.h"
#include "Core/Core.h"
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

#include "UICommon/CommandLineParse.h"
#include "UICommon/UICommon.h"

#include "VideoCommon/VideoBackendBase.h"

#if defined HAVE_X11 && HAVE_X11
#include <X11/Xlib.h>
#endif

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

#ifdef __APPLE__
#import <AppKit/AppKit.h>
#endif

class wxFrame;

// ------------
//  Main window

IMPLEMENT_APP(DolphinApp)

bool wxMsgAlert(const char*, const char*, bool, int);
std::string wxStringTranslator(const char *);

CFrame* main_frame = nullptr;

bool DolphinApp::Initialize(int& c, wxChar **v)
{
#if defined HAVE_X11 && HAVE_X11
	XInitThreads();
#endif
	return wxApp::Initialize(c, v);
}

// The 'main program' equivalent that creates the main window and return the main frame

bool DolphinApp::OnInit()
{
	if (!wxApp::OnInit())
		return false;

	Bind(wxEVT_QUERY_END_SESSION, &DolphinApp::OnEndSession, this);
	Bind(wxEVT_END_SESSION, &DolphinApp::OnEndSession, this);

	// Register message box and translation handlers
	RegisterMsgAlertHandler(&wxMsgAlert);
	RegisterStringTranslator(&wxStringTranslator);

#if wxUSE_ON_FATAL_EXCEPTION
	wxHandleFatalExceptions(true);
#endif

	ParseCommandLine();

	UICommon::SetUserDirectory(m_user_path.ToStdString());
	UICommon::CreateDirectories();

	UICommon::Init();
	InitLanguageSupport(); // The language setting is loaded from the user directory

	if (m_select_video_backend && !m_video_backend_name.empty())
		SConfig::GetInstance().m_strVideoBackend = WxStrToStr(m_video_backend_name);

	if (m_select_audio_emulation)
		SConfig::GetInstance().bDSPHLE = (m_audio_emulation_name.Upper() == "HLE");

	VideoBackendBase::ActivateBackend(SConfig::GetInstance().m_strVideoBackend);

	// Enable the PNG image handler for screenshots
	wxImage::AddHandler(new wxPNGHandler);

	int x = SConfig::GetInstance().iPosX;
	int y = SConfig::GetInstance().iPosY;
	int w = SConfig::GetInstance().iWidth;
	int h = SConfig::GetInstance().iHeight;

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

	main_frame = new CFrame(nullptr, wxID_ANY,
	                        StrToWxStr(scm_rev_str),
	                        wxPoint(x, y), wxSize(w, h),
	                        m_use_debugger, m_batch_mode, m_use_logger);

	SetTopWindow(main_frame);
	main_frame->SetMinSize(wxSize(400, 300));

	AfterInit();

	return true;
}

void DolphinApp::ParseCommandLine()
{
	std::unique_ptr<optparse::OptionParser> parser = CommandLineParse::CreateParser(true);
	optparse::Values& options = CommandLineParse::ParseArguments(parser.get(), argc, argv);
	std::vector<std::string> args = parser->args();

	if (options.is_set("exec"))
	{
		m_load_file = true;
		m_file_to_load = (const char*)options.get("exec");
	}
	else if (args.size())
	{
		m_load_file = true;
		m_file_to_load = args.front();
		args.erase(args.begin());
	}

	m_use_debugger = options.is_set("debugger");
	m_use_logger = options.is_set("logger");
	m_batch_mode = options.is_set("batch");

	m_confirm_stop = options.is_set("confirm");
	m_confirm_setting = options.get("confirm");

	m_select_video_backend = options.is_set("video_backend");
	m_video_backend_name = (const char*)options.get("video_backend");

	m_select_audio_emulation = options.is_set("audio_emulation");
	m_audio_emulation_name = (const char*)options.get("audio_emulation");

	m_play_movie = options.is_set("movie");
	m_movie_file = (const char*)options.get("movie");

	m_user_path = (const char*)options.get("user");
}

#ifdef __APPLE__
void DolphinApp::MacOpenFile(const wxString& fileName)
{
	m_file_to_load = fileName;
	m_load_file = true;
	main_frame->BootGame(WxStrToStr(m_file_to_load));
}
#endif

void DolphinApp::AfterInit()
{
	if (!m_batch_mode)
		main_frame->UpdateGameList();

	if (m_confirm_stop)
		SConfig::GetInstance().bConfirmStop = m_confirm_setting;

	if (m_play_movie && !m_movie_file.empty())
	{
		if (Movie::PlayInput(WxStrToStr(m_movie_file)))
		{
			if (m_load_file && !m_file_to_load.empty())
			{
				main_frame->BootGame(WxStrToStr(m_file_to_load));
			}
			else
			{
				main_frame->BootGame("");
			}
		}
	}
	// First check if we have an exec command line.
	else if (m_load_file && !m_file_to_load.empty())
	{
		main_frame->BootGame(WxStrToStr(m_file_to_load));
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

	OnionConfig::OnionPetal* interface = OnionConfig::GetOrCreatePetal(OnionConfig::OnionSystem::SYSTEM_MAIN, "Interface");
	interface->Get("Language", &language, wxLANGUAGE_DEFAULT);

	// Load language if possible, fall back to system default otherwise
	if (wxLocale::IsAvailable(language))
	{
		m_locale.reset(new wxLocale(language));

		// Specify where dolphins *.gmo files are located on each operating system
#ifdef _WIN32
		m_locale->AddCatalogLookupPathPrefix(StrToWxStr(File::GetExeDirectory() + DIR_SEP "Languages"));
#elif defined(__LINUX__)
		m_locale->AddCatalogLookupPathPrefix(StrToWxStr(DATA_DIR "../locale"));
#elif defined(__APPLE__)
		m_locale->AddCatalogLookupPathPrefix(StrToWxStr(File::GetBundleDirectory() + "Contents/Resources"));
#endif

		m_locale->AddCatalog("dolphin-emu");

		if (!m_locale->IsOk())
		{
			wxMessageBox(_("Error loading selected language. Falling back to system default."), _("Error"));
			m_locale.reset(new wxLocale(wxLANGUAGE_DEFAULT));
		}
	}
	else
	{
		wxMessageBox(_("The selected language is not supported by your system. Falling back to system default."), _("Error"));
		m_locale.reset(new wxLocale(wxLANGUAGE_DEFAULT));
	}
}

void DolphinApp::OnEndSession(wxCloseEvent& event)
{
	// Close if we've received wxEVT_END_SESSION (ignore wxEVT_QUERY_END_SESSION)
	if (!event.CanVeto())
	{
		main_frame->Close(true);
	}
}

int DolphinApp::OnExit()
{
	Core::Shutdown();
	UICommon::Shutdown();

	return wxApp::OnExit();
}

void DolphinApp::OnFatalException()
{
	WiimoteReal::Shutdown();
}

// ------------
// Talk to GUI

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

// OK, this thread boundary is DANGEROUS on Linux
// wxPostEvent / wxAddPendingEvent is the solution.
void Host_NotifyMapLoaded()
{
	wxCommandEvent event(wxEVT_HOST_COMMAND, IDM_NOTIFY_MAP_LOADED);
	main_frame->GetEventHandler()->AddPendingEvent(event);

	if (main_frame->g_pCodeWindow)
	{
		main_frame->g_pCodeWindow->GetEventHandler()->AddPendingEvent(event);
	}
}

void Host_UpdateDisasmDialog()
{
	wxCommandEvent event(wxEVT_HOST_COMMAND, IDM_UPDATE_DISASM_DIALOG);
	main_frame->GetEventHandler()->AddPendingEvent(event);

	if (main_frame->g_pCodeWindow)
	{
		main_frame->g_pCodeWindow->GetEventHandler()->AddPendingEvent(event);
	}
}

void Host_UpdateMainFrame()
{
	wxCommandEvent event(wxEVT_HOST_COMMAND, IDM_UPDATE_GUI);
	main_frame->GetEventHandler()->AddPendingEvent(event);

	if (main_frame->g_pCodeWindow)
	{
		main_frame->g_pCodeWindow->GetEventHandler()->AddPendingEvent(event);
	}
}

void Host_UpdateTitle(const std::string& title)
{
	wxCommandEvent event(wxEVT_HOST_COMMAND, IDM_UPDATE_TITLE);
	event.SetString(StrToWxStr(title));
	main_frame->GetEventHandler()->AddPendingEvent(event);
}

void Host_RequestRenderWindowSize(int width, int height)
{
	wxCommandEvent event(wxEVT_HOST_COMMAND, IDM_WINDOW_SIZE_REQUEST);
	event.SetClientData(new std::pair<int, int>(width, height));
	main_frame->GetEventHandler()->AddPendingEvent(event);
}

void Host_RequestFullscreen(bool enable_fullscreen)
{
	wxCommandEvent event(wxEVT_HOST_COMMAND, IDM_FULLSCREEN_REQUEST);
	event.SetInt(enable_fullscreen ? 1 : 0);
	main_frame->GetEventHandler()->AddPendingEvent(event);
}

void Host_SetStartupDebuggingParameters()
{
	SConfig& StartUp = SConfig::GetInstance();
	if (main_frame->g_pCodeWindow)
	{
		StartUp.bBootToPause = main_frame->g_pCodeWindow->BootToPause();
		StartUp.bAutomaticStart = main_frame->g_pCodeWindow->AutomaticStart();
		StartUp.bJITNoBlockCache = main_frame->g_pCodeWindow->JITNoBlockCache();
		StartUp.bJITNoBlockLinking = main_frame->g_pCodeWindow->JITNoBlockLinking();
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

	wxCommandEvent event(wxEVT_HOST_COMMAND, IDM_UPDATE_STATUS_BAR);

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

bool Host_RendererIsFullscreen()
{
	return main_frame->RendererIsFullscreen();
}

void Host_ConnectWiimote(int wm_idx, bool connect)
{
	if (connect)
	{
		wxCommandEvent event(wxEVT_HOST_COMMAND, IDM_FORCE_CONNECT_WIIMOTE1 + wm_idx);
		main_frame->GetEventHandler()->AddPendingEvent(event);
	}
	else
	{
		wxCommandEvent event(wxEVT_HOST_COMMAND, IDM_FORCE_DISCONNECT_WIIMOTE1 + wm_idx);
		main_frame->GetEventHandler()->AddPendingEvent(event);
	}
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
