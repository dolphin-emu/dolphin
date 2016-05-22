// Copyright 2011 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <vector>
#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/checklst.h>
#include <wx/panel.h>
#include <wx/radiobox.h>
#include <wx/sizer.h>
#include <wx/validate.h>

#include "Common/FileUtil.h"
#include "Common/IniFile.h"
#include "Common/Logging/ConsoleListener.h"
#include "Common/Logging/Log.h"
#include "Common/Logging/LogManager.h"
#include "DolphinWX/LogConfigWindow.h"
#include "DolphinWX/LogWindow.h"
#include "DolphinWX/WxUtils.h"

LogConfigWindow::LogConfigWindow(wxWindow* parent, wxWindowID id)
	: wxPanel(parent, id, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL, _("Log Configuration"))
	, enableAll(true)
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
			wxDefaultPosition, wxDefaultSize, wxLevelsUse, 0, wxRA_SPECIFY_ROWS);
	m_verbosity->Bind(wxEVT_RADIOBOX, &LogConfigWindow::OnVerbosityChange, this);

	// Options
	m_writeFileCB = new wxCheckBox(this, wxID_ANY, _("Write to File"));
	m_writeFileCB->Bind(wxEVT_CHECKBOX, &LogConfigWindow::OnWriteFileChecked, this);
	m_writeConsoleCB = new wxCheckBox(this, wxID_ANY, _("Write to Console"));
	m_writeConsoleCB->Bind(wxEVT_CHECKBOX, &LogConfigWindow::OnWriteConsoleChecked, this);
	m_writeWindowCB = new wxCheckBox(this, wxID_ANY, _("Write to Window"));
	m_writeWindowCB->Bind(wxEVT_CHECKBOX, &LogConfigWindow::OnWriteWindowChecked, this);

	wxButton *btn_toggle_all = new wxButton(this, wxID_ANY, _("Toggle All Log Types"),
			wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT);
	btn_toggle_all->Bind(wxEVT_BUTTON, &LogConfigWindow::OnToggleAll, this);
	m_checks = new wxCheckListBox(this, wxID_ANY);
	m_checks->Bind(wxEVT_CHECKLISTBOX, &LogConfigWindow::OnLogCheck, this);
	for (int i = 0; i < LogTypes::NUMBER_OF_LOGS; i++)
		m_checks->Append(StrToWxStr(m_LogManager->GetFullName((LogTypes::LOG_TYPE)i)));

	// Sizers
	wxStaticBoxSizer* sbOutputs = new wxStaticBoxSizer(wxVERTICAL, this, _("Logger Outputs"));
	sbOutputs->Add(m_writeFileCB, 0, wxDOWN, 1);
	sbOutputs->Add(m_writeConsoleCB, 0, wxDOWN, 1);
	sbOutputs->Add(m_writeWindowCB, 0);

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

	IniFile::Section* options = ini.GetOrCreateSection("Options");

	// Retrieve the verbosity value from the config ini file.
	int verbosity;
	options->Get("Verbosity", &verbosity, 0);

	// Ensure the verbosity level is valid.
	if (verbosity < 1)
		verbosity = 1;
	if (verbosity > MAX_LOGLEVEL)
		verbosity = MAX_LOGLEVEL;

	// Actually set the logging verbosity.
	m_verbosity->SetSelection(verbosity - 1);

	// Get the logger output settings from the config ini file.
	options->Get("WriteToFile", &m_writeFile, false);
	m_writeFileCB->SetValue(m_writeFile);
	options->Get("WriteToConsole", &m_writeConsole, true);
	m_writeConsoleCB->SetValue(m_writeConsole);
	options->Get("WriteToWindow", &m_writeWindow, true);
	m_writeWindowCB->SetValue(m_writeWindow);

	// Run through all of the log types and check each checkbox for each logging type
	// depending on its set value within the config ini.
	for (int i = 0; i < LogTypes::NUMBER_OF_LOGS; ++i)
	{
		bool log_enabled;
		ini.GetOrCreateSection("Logs")->Get(m_LogManager->GetShortName((LogTypes::LOG_TYPE)i), &log_enabled, false);

		if (log_enabled)
			enableAll = false;

		m_checks->Check(i, log_enabled);
	}
}

void LogConfigWindow::SaveSettings()
{
	IniFile ini;
	ini.Load(File::GetUserPath(F_LOGGERCONFIG_IDX));

	IniFile::Section* options = ini.GetOrCreateSection("Options");
	options->Set("Verbosity", m_verbosity->GetSelection() + 1);
	options->Set("WriteToFile", m_writeFile);
	options->Set("WriteToConsole", m_writeConsole);
	options->Set("WriteToWindow", m_writeWindow);

	// Save all enabled/disabled states of the log types to the config ini.
	for (int i = 0; i < LogTypes::NUMBER_OF_LOGS; ++i)
	{
		ini.GetOrCreateSection("Logs")->Set(m_LogManager->GetShortName((LogTypes::LOG_TYPE)i), m_checks->IsChecked(i));
	}

	ini.Save(File::GetUserPath(F_LOGGERCONFIG_IDX));
}

// If the verbosity changes while logging
void LogConfigWindow::OnVerbosityChange(wxCommandEvent& event)
{
	// Get the new verbosity
	int v = m_verbosity->GetSelection() + 1;

	// Set all log types to that verbosity level
	for (int i = 0; i < LogTypes::NUMBER_OF_LOGS; i++)
	{
		m_LogManager->SetLogLevel((LogTypes::LOG_TYPE)i, (LogTypes::LOG_LEVELS)v);
	}

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
				m_LogManager->AddListener((LogTypes::LOG_TYPE)i, LogListener::FILE_LISTENER);
			else
				m_LogManager->RemoveListener((LogTypes::LOG_TYPE)i, LogListener::FILE_LISTENER);
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
				m_LogManager->AddListener((LogTypes::LOG_TYPE)i, LogListener::CONSOLE_LISTENER);
			else
				m_LogManager->RemoveListener((LogTypes::LOG_TYPE)i, LogListener::CONSOLE_LISTENER);
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
				m_LogManager->AddListener((LogTypes::LOG_TYPE)i, LogListener::LOG_WINDOW_LISTENER);
			else
				m_LogManager->RemoveListener((LogTypes::LOG_TYPE)i, LogListener::LOG_WINDOW_LISTENER);
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
			m_LogManager->AddListener(logType, LogListener::LOG_WINDOW_LISTENER);
		if (m_writeFile)
			m_LogManager->AddListener(logType, LogListener::FILE_LISTENER);
		if (m_writeConsole)
			m_LogManager->AddListener(logType, LogListener::CONSOLE_LISTENER);
	}
	else
	{
		m_LogManager->RemoveListener(logType, LogListener::LOG_WINDOW_LISTENER);
		m_LogManager->RemoveListener(logType, LogListener::FILE_LISTENER);
		m_LogManager->RemoveListener(logType, LogListener::CONSOLE_LISTENER);
	}
}

void LogConfigWindow::OnLogCheck(wxCommandEvent& event)
{
	int i = event.GetInt();
	ToggleLog(i, m_checks->IsChecked(i));
}

