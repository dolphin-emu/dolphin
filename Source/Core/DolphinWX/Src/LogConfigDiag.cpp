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

#include "LogConfigDiag.h"
#include "LogManager.h"
#include "ConsoleListener.h"
#include "LogWindow.h"
#include "FileUtil.h"

#define _connect_macro_(b, f, c, s) \
	(b)->Connect(wxID_ANY, (c), wxCommandEventHandler(f), (wxObject*)0, (wxEvtHandler*)s)

LogConfigDiag::LogConfigDiag(wxWindow* parent, CLogWindow *log_window)
	: wxDialog(parent, wxID_ANY, _("Logger Configuration"), wxDefaultPosition, wxDefaultSize)
	, m_LogWindow(log_window), enableAll(true)
{
	Connect(wxID_ANY, wxEVT_CLOSE_WINDOW, wxCloseEventHandler(LogConfigDiag::OnClose), (wxObject*)0, this);
	m_LogManager = LogManager::GetInstance();
	CreateGUIControls();
	LoadSettings();
}

void LogConfigDiag::CreateGUIControls()
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
	_connect_macro_(m_verbosity, LogConfigDiag::OnVerbosityChange, wxEVT_COMMAND_RADIOBOX_SELECTED, this);

	// Options
	m_writeFileCB = new wxCheckBox(this, wxID_ANY, _("Write to File"));
	_connect_macro_(m_writeFileCB, LogConfigDiag::OnWriteFileChecked, wxEVT_COMMAND_CHECKBOX_CLICKED, this);
	m_writeConsoleCB = new wxCheckBox(this, wxID_ANY, _("Write to Console"));
	_connect_macro_(m_writeConsoleCB, LogConfigDiag::OnWriteConsoleChecked, wxEVT_COMMAND_CHECKBOX_CLICKED, this);
	m_writeWindowCB = new wxCheckBox(this, wxID_ANY, _("Write to Window"));
	_connect_macro_(m_writeWindowCB, LogConfigDiag::OnWriteWindowChecked, wxEVT_COMMAND_CHECKBOX_CLICKED, this);

	wxButton *btn_toggle_all = new wxButton(this, wxID_ANY, _("Toggle All Log Types"),
			wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT);
	_connect_macro_(btn_toggle_all, LogConfigDiag::OnToggleAll, wxEVT_COMMAND_BUTTON_CLICKED, this);
	m_checks = new wxCheckListBox(this, wxID_ANY);
	_connect_macro_(m_checks, LogConfigDiag::OnLogCheck, wxEVT_COMMAND_CHECKLISTBOX_TOGGLED, this);
	for (int i = 0; i < LogTypes::NUMBER_OF_LOGS; i++)
		m_checks->Append(wxString::FromAscii(m_LogManager->getFullName((LogTypes::LOG_TYPE)i)));

	// Sizers
	wxStaticBoxSizer* sbOptions = new wxStaticBoxSizer(wxVERTICAL, this, _("Logger Outputs"));
	sbOptions->Add(m_writeFileCB, 0, wxDOWN, 1);
	sbOptions->Add(m_writeConsoleCB, 0, wxDOWN, 1);
	sbOptions->Add(m_writeWindowCB, 0);

	wxStaticBoxSizer* sbLogTypes = new wxStaticBoxSizer(wxVERTICAL, this, _("Log Types"));
	sbLogTypes->Add(m_checks, 1, wxEXPAND);

	wxBoxSizer* sButtons = new wxBoxSizer(wxHORIZONTAL);
	sButtons->AddStretchSpacer();
	wxButton *btn_close = new wxButton(this, wxID_CLOSE);
	_connect_macro_(btn_close, LogConfigDiag::OnClickClose, wxEVT_COMMAND_BUTTON_CLICKED, this);
	sButtons->Add(btn_close, 0, wxALL, 5);	

	wxBoxSizer *sLeft = new wxBoxSizer(wxVERTICAL);
	sLeft->Add(m_verbosity, 0, wxEXPAND | wxLEFT | wxRIGHT, 5);
	sLeft->Add(sbOptions, 0, wxEXPAND | wxLEFT | wxRIGHT, 5);
	sLeft->AddStretchSpacer();
	sLeft->Add(btn_toggle_all, 0, wxEXPAND | wxLEFT | wxRIGHT, 5);

	wxBoxSizer *sTop = new wxBoxSizer(wxHORIZONTAL);
	sTop->Add(sLeft, 0, wxEXPAND);
	sTop->Add(sbLogTypes, 0, wxEXPAND | wxRIGHT, 5);

	wxBoxSizer *sMain = new wxBoxSizer(wxVERTICAL);
	sMain->Add(sTop, 0, wxEXPAND);
	sMain->Add(sButtons, 0, wxEXPAND);

	SetSizer(sMain);
	Layout();
	Fit();
}

void LogConfigDiag::LoadSettings()
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
	for (int i = 0; i < LogTypes::NUMBER_OF_LOGS; ++i)
	{
		bool log_enabled;
		ini.Get("Logs", m_LogManager->getShortName((LogTypes::LOG_TYPE)i), &log_enabled, true);
		if (log_enabled) enableAll=false;
		m_checks->Check(i, log_enabled);
	}
}

void LogConfigDiag::SaveSettings()
{
	IniFile ini;
	ini.Load(File::GetUserPath(F_LOGGERCONFIG_IDX));

	ini.Set("Options", "Verbosity", m_verbosity->GetSelection() + 1);
	ini.Set("Options", "WriteToFile", m_writeFile);
	ini.Set("Options", "WriteToConsole", m_writeConsole);
	ini.Set("Options", "WriteToWindow", m_writeWindow);
	for (int i = 0; i < LogTypes::NUMBER_OF_LOGS; ++i)
		ini.Set("Logs", m_LogManager->getShortName((LogTypes::LOG_TYPE)i), m_checks->IsChecked(i));
	ini.Save(File::GetUserPath(F_LOGGERCONFIG_IDX));
}

void LogConfigDiag::OnVerbosityChange(wxCommandEvent& event)
{
	int v = m_verbosity->GetSelection() + 1;
	for (int i = 0; i < LogTypes::NUMBER_OF_LOGS; i++)
		m_LogManager->setLogLevel((LogTypes::LOG_TYPE)i, (LogTypes::LOG_LEVELS)v);
	event.Skip();
}

void LogConfigDiag::OnWriteFileChecked(wxCommandEvent& event)
{
	for (int i = 0; i < LogTypes::NUMBER_OF_LOGS; ++i)
	{
		m_writeFile = event.IsChecked();
		if (m_checks->IsChecked(i))
		{
			if (m_writeFile)
				m_LogManager->addListener((LogTypes::LOG_TYPE)i, m_LogManager->getFileListener());
			else
				m_LogManager->removeListener((LogTypes::LOG_TYPE)i, m_LogManager->getFileListener());
		}
	}
}

void LogConfigDiag::OnWriteConsoleChecked(wxCommandEvent& event)
{
	for (int i = 0; i < LogTypes::NUMBER_OF_LOGS; ++i)
	{
		m_writeConsole = event.IsChecked();
		if (m_checks->IsChecked(i))
		{
			if (m_writeConsole)
				m_LogManager->addListener((LogTypes::LOG_TYPE)i, m_LogManager->getConsoleListener());
			else
				m_LogManager->removeListener((LogTypes::LOG_TYPE)i, m_LogManager->getConsoleListener());
		}
	}
}

void LogConfigDiag::OnWriteWindowChecked(wxCommandEvent& event)
{
	for (int i = 0; i < LogTypes::NUMBER_OF_LOGS; ++i)
	{
		m_writeWindow = event.IsChecked();
		if (m_checks->IsChecked(i))
		{
			if (m_writeWindow)
				m_LogManager->addListener((LogTypes::LOG_TYPE)i, (LogListener *)m_LogWindow);
			else
				m_LogManager->removeListener((LogTypes::LOG_TYPE)i, (LogListener *)m_LogWindow);
		}
	}
}

void LogConfigDiag::OnToggleAll(wxCommandEvent& WXUNUSED(event))
{
	for (int i = 0; i < LogTypes::NUMBER_OF_LOGS; ++i)
		ToggleLog(i, enableAll);

	enableAll = !enableAll;
}

void LogConfigDiag::ToggleLog(int _logType, bool enable)
{
	LogTypes::LOG_TYPE logType = (LogTypes::LOG_TYPE)_logType;

	m_checks->Check(_logType, enable);

	m_LogManager->setEnable(logType, enable);

	if (enable)
	{
		if (m_writeWindow)
			m_LogManager->addListener(logType, (LogListener *)m_LogWindow);
		if (m_writeFile)
			m_LogManager->addListener(logType, m_LogManager->getFileListener());
		if (m_writeConsole)
			m_LogManager->addListener(logType, m_LogManager->getConsoleListener());
	}
	else
	{
		m_LogManager->removeListener(logType, (LogListener *)m_LogWindow);
		m_LogManager->removeListener(logType, m_LogManager->getFileListener());
		m_LogManager->removeListener(logType, m_LogManager->getConsoleListener());
	}
}

void LogConfigDiag::OnLogCheck(wxCommandEvent& event)
{
	int i = event.GetInt();
	ToggleLog(i, m_checks->IsChecked(i));
}

void LogConfigDiag::OnClickClose(wxCommandEvent& WXUNUSED(event))
{
	Close();
}

void LogConfigDiag::OnClose(wxCloseEvent& event)
{
	SaveSettings();
	event.Skip();
}
