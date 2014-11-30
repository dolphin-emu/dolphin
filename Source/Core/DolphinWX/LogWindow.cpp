// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <cstddef>
#include <mutex>
#include <queue>
#include <utility>
#include <vector>
#include <wx/anybutton.h>
#include <wx/button.h>
#include <wx/chartype.h>
#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/colour.h>
#include <wx/defs.h>
#include <wx/event.h>
#include <wx/font.h>
#include <wx/gdicmn.h>
#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/string.h>
#include <wx/textctrl.h>
#include <wx/timer.h>
#include <wx/translation.h>
#include <wx/validate.h>
#include <wx/window.h>
#include <wx/windowid.h>
#include <wx/aui/framemanager.h>

#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/IniFile.h"
#include "Common/Logging/ConsoleListener.h"
#include "Common/Logging/LogManager.h"
#include "DolphinWX/Frame.h"
#include "DolphinWX/LogWindow.h"
#include "DolphinWX/WxUtils.h"
#include "DolphinWX/Debugger/DebuggerUIUtil.h"

// Milliseconds between msgQueue flushes to wxTextCtrl
#define UPDATETIME 200

CLogWindow::CLogWindow(CFrame *parent, wxWindowID id, const wxPoint& pos,
		const wxSize& size, long style, const wxString& name)
	: wxPanel(parent, id, pos, size, style, name)
	, x(0), y(0), winpos(0)
	, Parent(parent), m_ignoreLogTimer(false), m_LogAccess(true)
	, m_Log(nullptr), m_cmdline(nullptr), m_FontChoice(nullptr)
{
	Bind(wxEVT_CLOSE_WINDOW, &CLogWindow::OnClose, this);
	Bind(wxEVT_TIMER, &CLogWindow::OnLogTimer, this);

	m_LogManager = LogManager::GetInstance();

	CreateGUIControls();

	m_LogTimer = new wxTimer(this);
	m_LogTimer->Start(UPDATETIME);
}

void CLogWindow::CreateGUIControls()
{
	IniFile ini;
	ini.Load(File::GetUserPath(F_LOGGERCONFIG_IDX));

	IniFile::Section* options    = ini.GetOrCreateSection("Options");
	IniFile::Section* log_window = ini.GetOrCreateSection("LogWindow");
	log_window->Get("x", &x, Parent->GetSize().GetX() / 2);
	log_window->Get("y", &y, Parent->GetSize().GetY());
	log_window->Get("pos", &winpos, wxAUI_DOCK_RIGHT);

	// Set up log listeners
	int verbosity;
	options->Get("Verbosity", &verbosity, 0);

	// Ensure the verbosity level is valid
	if (verbosity < 1)
		verbosity = 1;
	if (verbosity > MAX_LOGLEVEL)
		verbosity = MAX_LOGLEVEL;

	// Get the logger output settings from the config ini file.
	options->Get("WriteToFile", &m_writeFile, false);
	options->Get("WriteToWindow", &m_writeWindow, true);
#ifdef _MSC_VER
	if (IsDebuggerPresent())
	{
		options->Get("WriteToDebugger", &m_writeDebugger, true);
	}
	else
#endif
	{
		m_writeDebugger = false;
	}

	IniFile::Section* logs = ini.GetOrCreateSection("Logs");
	for (int i = 0; i < LogTypes::NUMBER_OF_LOGS; ++i)
	{
		bool enable;
		logs->Get(m_LogManager->GetShortName((LogTypes::LOG_TYPE)i), &enable, false);

		if (m_writeWindow && enable)
			m_LogManager->AddListener((LogTypes::LOG_TYPE)i, this);
		else
			m_LogManager->RemoveListener((LogTypes::LOG_TYPE)i, this);

		if (m_writeFile && enable)
			m_LogManager->AddListener((LogTypes::LOG_TYPE)i, m_LogManager->GetFileListener());
		else
			m_LogManager->RemoveListener((LogTypes::LOG_TYPE)i, m_LogManager->GetFileListener());

		if (m_writeDebugger && enable)
			m_LogManager->AddListener((LogTypes::LOG_TYPE)i, m_LogManager->GetDebuggerListener());
		else
			m_LogManager->RemoveListener((LogTypes::LOG_TYPE)i, m_LogManager->GetDebuggerListener());

		m_LogManager->SetLogLevel((LogTypes::LOG_TYPE)i, (LogTypes::LOG_LEVELS)(verbosity));
	}

	// Font
	m_FontChoice = new wxChoice(this, wxID_ANY);
	m_FontChoice->Bind(wxEVT_CHOICE, &CLogWindow::OnFontChange, this);
	m_FontChoice->Append(_("Default font"));
	m_FontChoice->Append(_("Monospaced font"));
	m_FontChoice->Append(_("Selected font"));

	DefaultFont = GetFont();
	MonoSpaceFont.SetNativeFontInfoUserDesc("lucida console windows-1252");
	LogFont.push_back(DefaultFont);
	LogFont.push_back(MonoSpaceFont);
	LogFont.push_back(DebuggerFont);

	int font;
	options->Get("Font", &font, 0);
	m_FontChoice->SetSelection(font);

	// Word wrap
	bool wrap_lines;
	options->Get("WrapLines", &wrap_lines, false);
	m_WrapLine = new wxCheckBox(this, wxID_ANY, _("Word Wrap"));
	m_WrapLine->Bind(wxEVT_CHECKBOX, &CLogWindow::OnWrapLineCheck, this);
	m_WrapLine->SetValue(wrap_lines);

	// Log viewer
	m_Log = CreateTextCtrl(this, wxID_ANY, wxTE_RICH | wxTE_MULTILINE | wxTE_READONLY |
			(wrap_lines ? wxTE_WORDWRAP : wxTE_DONTWRAP));

	// submit row
	m_cmdline = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize,
			wxTE_PROCESS_ENTER | wxTE_PROCESS_TAB);

	// Clear log button
	m_clear_log_btn = new wxButton(this, wxID_ANY, _("Clear"), wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT);
	m_clear_log_btn->Bind(wxEVT_BUTTON, &CLogWindow::OnClear, this);

	// Sizers
	wxBoxSizer* sTop = new wxBoxSizer(wxHORIZONTAL);
	sTop->Add(m_clear_log_btn);
	sTop->Add(m_FontChoice, 0, wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, 3);
	sTop->Add(m_WrapLine, 0, wxALIGN_CENTER_VERTICAL);

	sBottom = new wxBoxSizer(wxVERTICAL);
	PopulateBottom();

	wxBoxSizer* sMain = new wxBoxSizer(wxVERTICAL);
	sMain->Add(sTop, 0, wxEXPAND);
	sMain->Add(sBottom, 1, wxEXPAND);
	SetSizer(sMain);

	m_cmdline->SetFocus();
}

CLogWindow::~CLogWindow()
{
	for (int i = 0; i < LogTypes::NUMBER_OF_LOGS; ++i)
	{
		m_LogManager->RemoveListener((LogTypes::LOG_TYPE)i, this);
	}
	m_LogTimer->Stop();
	delete m_LogTimer;
}

void CLogWindow::OnClose(wxCloseEvent& event)
{
	SaveSettings();
	event.Skip();
}

void CLogWindow::SaveSettings()
{
	IniFile ini;
	ini.Load(File::GetUserPath(F_LOGGERCONFIG_IDX));

	if (!Parent->g_pCodeWindow)
	{
		IniFile::Section* log_window = ini.GetOrCreateSection("LogWindow");
		log_window->Set("x", x);
		log_window->Set("y", y);
		log_window->Set("pos", winpos);
	}

	IniFile::Section* options = ini.GetOrCreateSection("Options");
	options->Set("Font", m_FontChoice->GetSelection());
	options->Set("WrapLines", m_WrapLine->IsChecked());

	ini.Save(File::GetUserPath(F_LOGGERCONFIG_IDX));
}

void CLogWindow::OnClear(wxCommandEvent& WXUNUSED (event))
{
	m_Log->Clear();

	{
	std::lock_guard<std::mutex> lk(m_LogSection);
	while (!msgQueue.empty())
		msgQueue.pop();
	}

	m_LogManager->GetConsoleListener()->ClearScreen();
}

void CLogWindow::UnPopulateBottom()
{
	sBottom->Detach(m_Log);
	sBottom->Detach(m_cmdline);
}

void CLogWindow::PopulateBottom()
{
	sBottom->Add(m_Log, 1, wxEXPAND | wxSHRINK);
	sBottom->Add(m_cmdline, 0, wxEXPAND);
	Layout();
}

wxTextCtrl* CLogWindow::CreateTextCtrl(wxPanel* parent, wxWindowID id, long Style)
{
	wxTextCtrl* TC = new wxTextCtrl(parent, id, wxEmptyString, wxDefaultPosition, wxDefaultSize, Style);
#ifdef __APPLE__
	TC->SetBackgroundColour(*wxLIGHT_GREY);
#else
	TC->SetBackgroundColour(*wxBLACK);
#endif
	if (m_FontChoice && m_FontChoice->GetSelection() < (int)LogFont.size() && m_FontChoice->GetSelection() >= 0)
		TC->SetDefaultStyle(wxTextAttr(wxNullColour, wxNullColour, LogFont[m_FontChoice->GetSelection()]));

	return TC;
}

void CLogWindow::OnFontChange(wxCommandEvent& event)
{
	// Update selected font
	LogFont[LogFont.size()-1] = DebuggerFont;
	m_Log->SetStyle(0, m_Log->GetLastPosition(),
			wxTextAttr(wxNullColour, wxNullColour, LogFont[event.GetSelection()]));
	m_Log->SetDefaultStyle(wxTextAttr(wxNullColour, wxNullColour, LogFont[event.GetSelection()]));

	SaveSettings();
}

void CLogWindow::OnWrapLineCheck(wxCommandEvent& event)
{
#ifdef __WXGTK__
	// Clear the old word wrap state and set the new
	m_Log->SetWindowStyleFlag(m_Log->GetWindowStyleFlag() ^ (wxTE_WORDWRAP | wxTE_DONTWRAP));
#else
	wxString Text;
	// Unfortunately wrapping styles can only be changed dynamically with wxGTK
	// Notice: To retain the colors when changing word wrapping we need to
	//         loop through every letter with GetStyle and then reapply them letter by letter
	// Prevent m_Log access while it's being destroyed
	m_LogAccess = false;
	UnPopulateBottom();
	Text = m_Log->GetValue();
	m_Log->Destroy();
	if (event.IsChecked())
		m_Log = CreateTextCtrl(this, wxID_ANY, wxTE_RICH | wxTE_MULTILINE | wxTE_READONLY | wxTE_WORDWRAP);
	else
		m_Log = CreateTextCtrl(this, wxID_ANY, wxTE_RICH | wxTE_MULTILINE | wxTE_READONLY | wxTE_DONTWRAP);
	m_Log->SetDefaultStyle(wxTextAttr(*wxWHITE));
	m_Log->AppendText(Text);
	PopulateBottom();
	m_LogAccess = true;
#endif
	SaveSettings();
}

void CLogWindow::OnLogTimer(wxTimerEvent& WXUNUSED(event))
{
	if (!m_LogAccess || m_ignoreLogTimer)
		return;

	UpdateLog();

	// Scroll to the last line
	if (!msgQueue.empty())
	{
		m_Log->ScrollLines(1);
		m_Log->ShowPosition( m_Log->GetLastPosition() );
	}
}

void CLogWindow::UpdateLog()
{
	if (!m_LogAccess || !m_Log)
		return;

	// m_LogTimer->Stop();
	// instead of stopping the timer, let's simply ignore its calls during UpdateLog,
	// because repeatedly stopping and starting a timer churns memory (and potentially leaks it).
	m_ignoreLogTimer = true;

	std::lock_guard<std::mutex> lk(m_LogSection);
	while (!msgQueue.empty())
	{
		switch (msgQueue.front().first)
		{
			case LogTypes::LOG_LEVELS::LERROR:
				m_Log->SetDefaultStyle(wxTextAttr(*wxRED));
				break;

			case LogTypes::LOG_LEVELS::LWARNING:
				m_Log->SetDefaultStyle(wxTextAttr(*wxYELLOW));
				break;

			case LogTypes::LOG_LEVELS::LNOTICE:
				m_Log->SetDefaultStyle(wxTextAttr(*wxGREEN));
				break;

			case LogTypes::LOG_LEVELS::LINFO:
				m_Log->SetDefaultStyle(wxTextAttr(*wxCYAN));
				break;

			case LogTypes::LOG_LEVELS::LDEBUG:
				m_Log->SetDefaultStyle(wxTextAttr(*wxLIGHT_GREY));
				break;

			default:
				m_Log->SetDefaultStyle(wxTextAttr(*wxWHITE));
				break;
		}

		if (msgQueue.front().second.size())
		{
			int i = m_Log->GetLastPosition();
			m_Log->AppendText(msgQueue.front().second);
			// White timestamp
			m_Log->SetStyle(i, i + 9, wxTextAttr(*wxWHITE));
		}

		msgQueue.pop();
	}

	m_ignoreLogTimer = false;
}

void CLogWindow::Log(LogTypes::LOG_LEVELS level, const char *text)
{
	std::lock_guard<std::mutex> lk(m_LogSection);

	if (msgQueue.size() >= 100)
		msgQueue.pop();

	msgQueue.push(std::make_pair(u8(level), StrToWxStr(text)));
}
