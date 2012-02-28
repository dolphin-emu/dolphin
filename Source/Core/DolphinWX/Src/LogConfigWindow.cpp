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

#include "LogConfigWindow.h"
#include "LogManager.h"
#include "ConsoleListener.h"
#include "LogWindow.h"
#include "FileUtil.h"

#define _connect_macro_(b, f, c, s) \
	(b)->Connect(wxID_ANY, (c), wxCommandEventHandler(f), (wxObject*)0, (wxEvtHandler*)s)

LogConfigWindow::LogConfigWindow(wxWindow* parent, CLogWindow *log_window, wxWindowID id)
	: wxPanel(parent, id, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL, _("Log Configuration"))
	, m_LogWindow(log_window), enableAll(true)
{
	SetMinSize(wxSize(100, 100));
	m_LogManager = LogManager::GetInstance();
	CreateGUIControls();
	LoadSettings();
}

LogConfigWindow::~LogConfigWindow()
{
	SaveSettings();
}

void LogConfigWindow::CreateGUIControls()
{
	// Verbosity
	wxArrayString wxLevels, wxLevelsUse;
	wxLevels.Add(_("Notice"));
	wxLevels.Add(_("Error"));
	wxLevels.Add(_("Warning"));
	wxLevels.Add(_("Info"));
	wxLevels.Add(_("Debug"));
	for (int i = 0; i < MAX_LOGLEVEL; ++i)
		wxLevelsUse.Add(wxLevels[i]);
	m_verbosity = new wxRadioBox(this, wxID_ANY, _("Verbosity"),
			wxDefaultPosition, wxDefaultSize, wxLevelsUse, 0,
			wxRA_SPECIFY_ROWS, wxDefaultValidator);
	_connect_macro_(m_verbosity, LogConfigWindow::OnVerbosityChange, wxEVT_COMMAND_RADIOBOX_SELECTED, this);

	// Options
	m_writeFileCB = new wxCheckBox(this, wxID_ANY, _("Write to File"));
	_connect_macro_(m_writeFileCB, LogConfigWindow::OnWriteFileChecked, wxEVT_COMMAND_CHECKBOX_CLICKED, this);
	m_writeConsoleCB = new wxCheckBox(this, wxID_ANY, _("Write to Console"));
	_connect_macro_(m_writeConsoleCB, LogConfigWindow::OnWriteConsoleChecked, wxEVT_COMMAND_CHECKBOX_CLICKED, this);
	m_writeWindowCB = new wxCheckBox(this, wxID_ANY, _("Write to Window"));
	_connect_macro_(m_writeWindowCB, LogConfigWindow::OnWriteWindowChecked, wxEVT_COMMAND_CHECKBOX_CLICKED, this);
	m_writeDebuggerCB = NULL;
#ifdef _MSC_VER
	if (IsDebuggerPresent())
	{
		m_writeDebuggerCB = new wxCheckBox(this, wxID_ANY, _("Write to Debugger"));
		_connect_macro_(m_writeDebuggerCB, LogConfigWindow::OnWriteDebuggerChecked, wxEVT_COMMAND_CHECKBOX_CLICKED, this);
	}
#endif

	wxButton *btn_toggle_all = new wxButton(this, wxID_ANY, _("Toggle All Log Types"),
			wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT);
	_connect_macro_(btn_toggle_all, LogConfigWindow::OnToggleAll, wxEVT_COMMAND_BUTTON_CLICKED, this);
	m_checks = new wxCheckListBox(this, wxID_ANY);
	_connect_macro_(m_checks, LogConfigWindow::OnLogCheck, wxEVT_COMMAND_CHECKLISTBOX_TOGGLED, this);
	for (int i = 0; i < LogTypes::NUMBER_OF_LOGS; i++)
		m_checks->Append(wxString::FromAscii(m_LogManager->GetFullName((LogTypes::LOG_TYPE)i)));

	// Sizers
	wxStaticBoxSizer* sbOutputs = new wxStaticBoxSizer(wxVERTICAL, this, _("Logger Outputs"));
	sbOutputs->Add(m_writeFileCB, 0, wxDOWN, 1);
	sbOutputs->Add(m_writeConsoleCB, 0, wxDOWN, 1);
#ifdef _MSC_VER
	if (m_writeDebuggerCB)
	{
		sbOutputs->Add(m_writeWindowCB, 0, wxDOWN, 1);
		sbOutputs->Add(m_writeDebuggerCB, 0);
	}
	else
#endif
	{
		sbOutputs->Add(m_writeWindowCB, 0);
	}

	wxStaticBoxSizer* sbLogTypes = new wxStaticBoxSizer(wxVERTICAL, this, _("Log Types"));
	sbLogTypes->Add(m_checks, 1, wxEXPAND);

	wxBoxSizer *sMain = new wxBoxSizer(wxVERTICAL);
	sMain->Add(m_verbosity, 0, wxEXPAND | wxLEFT | wxRIGHT, 5);
	sMain->Add(sbOutputs, 0, wxEXPAND | wxLEFT | wxRIGHT, 5);
	sMain->Add(btn_toggle_all, 0, wxEXPAND | wxLEFT | wxRIGHT, 5);
	sMain->Add(sbLogTypes, 1, wxEXPAND | wxLEFT | wxRIGHT, 5);

	SetSizer(sMain);
	Layout();
}

void LogConfigWindow::LoadSettings()
{
	IniFile ini;
	ini.Load(File::GetUserPath(F_LOGGERCONFIG_IDX));

	int verbosity;
	ini.Get("Options", "Verbosity", &verbosity, 0);
	if (verbosity < 1) verbosity = 1;
	if (verbosity > MAX_LOGLEVEL) verbosity = MAX_LOGLEVEL;
	m_verbosity->SetSelection(verbosity - 1);

	ini.Get("Options", "WriteToFile", &m_writeFile, false);
	m_writeFileCB->SetValue(m_writeFile);
	ini.Get("Options", "WriteToConsole", &m_writeConsole, true);
	m_writeConsoleCB->SetValue(m_writeConsole);
	ini.Get("Options", "WriteToWindow", &m_writeWindow, true);
	m_writeWindowCB->SetValue(m_writeWindow);
#ifdef _MSC_VER
	if (IsDebuggerPresent())
	{
		ini.Get("Options", "WriteToDebugger", &m_writeDebugger, true);
		m_writeDebuggerCB->SetValue(m_writeDebugger);
	}
	else
#endif
	{
		m_writeDebugger = false;
	}
	for (int i = 0; i < LogTypes::NUMBER_OF_LOGS; ++i)
	{
		bool log_enabled;
		ini.Get("Logs", m_LogManager->GetShortName((LogTypes::LOG_TYPE)i), &log_enabled, true);
		if (log_enabled) enableAll = false;
		m_checks->Check(i, log_enabled);
	}
}

void LogConfigWindow::SaveSettings()
{
	IniFile ini;
	ini.Load(File::GetUserPath(F_LOGGERCONFIG_IDX));

	ini.Set("Options", "Verbosity", m_verbosity->GetSelection() + 1);
	ini.Set("Options", "WriteToFile", m_writeFile);
	ini.Set("Options", "WriteToConsole", m_writeConsole);
	ini.Set("Options", "WriteToWindow", m_writeWindow);
#ifdef _MSC_VER
	if (IsDebuggerPresent())
		ini.Set("Options", "WriteToDebugger", m_writeDebugger);
#endif
	for (int i = 0; i < LogTypes::NUMBER_OF_LOGS; ++i)
		ini.Set("Logs", m_LogManager->GetShortName((LogTypes::LOG_TYPE)i), m_checks->IsChecked(i));
	ini.Save(File::GetUserPath(F_LOGGERCONFIG_IDX));
}

void LogConfigWindow::OnVerbosityChange(wxCommandEvent& event)
{
	int v = m_verbosity->GetSelection() + 1;
	for (int i = 0; i < LogTypes::NUMBER_OF_LOGS; i++)
		m_LogManager->SetLogLevel((LogTypes::LOG_TYPE)i, (LogTypes::LOG_LEVELS)v);
	event.Skip();
}

void LogConfigWindow::OnWriteFileChecked(wxCommandEvent& event)
{
	for (int i = 0; i < LogTypes::NUMBER_OF_LOGS; ++i)
	{
		m_writeFile = event.IsChecked();
		if (m_checks->IsChecked(i))
		{
			if (m_writeFile)
				m_LogManager->AddListener((LogTypes::LOG_TYPE)i, m_LogManager->GetFileListener());
			else
				m_LogManager->RemoveListener((LogTypes::LOG_TYPE)i, m_LogManager->GetFileListener());
		}
	}
}

void LogConfigWindow::OnWriteConsoleChecked(wxCommandEvent& event)
{
	for (int i = 0; i < LogTypes::NUMBER_OF_LOGS; ++i)
	{
		m_writeConsole = event.IsChecked();
		if (m_checks->IsChecked(i))
		{
			if (m_writeConsole)
				m_LogManager->AddListener((LogTypes::LOG_TYPE)i, m_LogManager->GetConsoleListener());
			else
				m_LogManager->RemoveListener((LogTypes::LOG_TYPE)i, m_LogManager->GetConsoleListener());
		}
	}
}

void LogConfigWindow::OnWriteWindowChecked(wxCommandEvent& event)
{
	for (int i = 0; i < LogTypes::NUMBER_OF_LOGS; ++i)
	{
		m_writeWindow = event.IsChecked();
		if (m_checks->IsChecked(i))
		{
			if (m_writeWindow)
				m_LogManager->AddListener((LogTypes::LOG_TYPE)i, (LogListener *)m_LogWindow);
			else
				m_LogManager->RemoveListener((LogTypes::LOG_TYPE)i, (LogListener *)m_LogWindow);
		}
	}
}

void LogConfigWindow::OnWriteDebuggerChecked(wxCommandEvent& event)
{
	for (int i = 0; i < LogTypes::NUMBER_OF_LOGS; ++i)
	{
		m_writeDebugger = event.IsChecked();
		if (m_checks->IsChecked(i))
		{
			if (m_writeDebugger)
				m_LogManager->AddListener((LogTypes::LOG_TYPE)i, (LogListener *)m_LogManager->GetDebuggerListener());
			else
				m_LogManager->RemoveListener((LogTypes::LOG_TYPE)i, (LogListener *)m_LogManager->GetDebuggerListener());
		}
	}
}

void LogConfigWindow::OnToggleAll(wxCommandEvent& WXUNUSED(event))
{
	for (int i = 0; i < LogTypes::NUMBER_OF_LOGS; ++i)
		ToggleLog(i, enableAll);

	enableAll = !enableAll;
}

void LogConfigWindow::ToggleLog(int _logType, bool enable)
{
	LogTypes::LOG_TYPE logType = (LogTypes::LOG_TYPE)_logType;

	m_checks->Check(_logType, enable);

	m_LogManager->SetEnable(logType, enable);

	if (enable)
	{
		if (m_writeWindow)
			m_LogManager->AddListener(logType, (LogListener *)m_LogWindow);
		if (m_writeFile)
			m_LogManager->AddListener(logType, m_LogManager->GetFileListener());
		if (m_writeConsole)
			m_LogManager->AddListener(logType, m_LogManager->GetConsoleListener());
		if (m_writeDebugger)
			m_LogManager->AddListener(logType, m_LogManager->GetDebuggerListener());
	}
	else
	{
		m_LogManager->RemoveListener(logType, (LogListener *)m_LogWindow);
		m_LogManager->RemoveListener(logType, m_LogManager->GetFileListener());
		m_LogManager->RemoveListener(logType, m_LogManager->GetConsoleListener());
		m_LogManager->RemoveListener(logType, m_LogManager->GetDebuggerListener());
	}
}

void LogConfigWindow::OnLogCheck(wxCommandEvent& event)
{
	int i = event.GetInt();
	ToggleLog(i, m_checks->IsChecked(i));
}
