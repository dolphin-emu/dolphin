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

#include "Debugger.h"
#include "LogManager.h"

#include <wx/button.h>
#include <wx/textctrl.h>
#include <wx/listbox.h>
#include <wx/checklst.h>

#include "Core.h" // for Core::GetState()
#include "LogWindow.h"
#include "Console.h"
#include "IniFile.h"

// declare this now to be able to use it in Load()
CDebugger_LogSettings*	LogManager::m_LogSettings;

BEGIN_EVENT_TABLE(CLogWindow, wxDialog)
	EVT_BUTTON(IDM_SUBMITCMD, CLogWindow::OnSubmit)
	EVT_BUTTON(IDM_UPDATELOG, CLogWindow::OnUpdateLog)
	EVT_BUTTON(IDM_CLEARLOG,  CLogWindow::OnClear)
	EVT_BUTTON(IDM_ENABLEALL,  CLogWindow::OnEnableAll)
	EVT_CHECKLISTBOX(IDM_LOGCHECKS, CLogWindow::OnLogCheck)
	EVT_RADIOBOX(IDM_RADIO0, CLogWindow::OnRadioChange)
END_EVENT_TABLE()


CLogWindow::CLogWindow(wxWindow* parent)
	: wxDialog(parent, wxID_ANY, _T("Log/Console"), wxPoint(100, 700), wxSize(800, 270),
			wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
{
	wxBoxSizer* sizerTop = new wxBoxSizer(wxHORIZONTAL), // buttons
	* sizerUber = new wxBoxSizer(wxHORIZONTAL), // whole plane
	* sizerBig = new wxBoxSizer(wxVERTICAL), // RIGHT sizer
	* sizerBottom = new wxBoxSizer(wxHORIZONTAL), // submit row
	* sizerLeft = new wxBoxSizer(wxVERTICAL); // LEFT sizer

	// left checkboxes and radio boxes -----------------------------------
	m_checks = new wxCheckListBox(this, IDM_LOGCHECKS, wxDefaultPosition, wxSize(120, 280));

	int m_radioBoxNChoices[1];
	wxString m_radioBoxChoices0[] = { wxT("0"), wxT("1"), wxT("2"), wxT("3") };
	m_radioBoxNChoices[0] = sizeof( m_radioBoxChoices0 ) / sizeof( wxString );
	m_RadioBox[0] = new wxRadioBox( this, IDM_RADIO0, wxT("Verbosity"),
		wxDefaultPosition, wxDefaultSize, m_radioBoxNChoices[0], m_radioBoxChoices0, 1, wxRA_SPECIFY_ROWS);
	sizerLeft->Add(m_RadioBox[0], 0, wxGROW);
	sizerLeft->Add(m_checks, 1, wxGROW);

	// right windows -----------------------------------------------------
	m_log = new wxTextCtrl(this, IDM_LOG, _T(""), wxDefaultPosition, wxSize(600, 120), wxTE_MULTILINE | wxTE_READONLY | wxTE_DONTWRAP);
	m_cmdline = new wxTextCtrl(this, wxID_ANY, _T(""), wxDefaultPosition);
	wxButton* btn = new wxButton(this, IDM_SUBMITCMD, _T("Submit"));

	sizerTop->Add(new wxButton(this, IDM_UPDATELOG, _T("Update")));
	sizerTop->Add(new wxButton(this, IDM_CLEARLOG, _T("Clear")));
	sizerTop->Add(new wxButton(this, IDM_ENABLEALL, _T("Enable all")));

	sizerBottom->Add(m_cmdline, 8, wxGROW | wxRIGHT, 5);
	sizerBottom->Add(btn, 1, wxGROW, 0);

	sizerBig->Add(sizerTop, 0, wxGROW);
	sizerBig->Add(m_log, 1, wxGROW | wxSHRINK);
	sizerBig->Add(sizerBottom, 0, wxGROW);

	sizerUber->Add(sizerLeft, 0, wxGROW);
	sizerUber->Add(sizerBig, 1, wxGROW);

	SetSizer(sizerUber);
	SetAffirmativeId(IDM_SUBMITCMD);

	// declare this now to be able to use it in Load()
	LogManager::m_LogSettings = new CDebugger_LogSettings;

	//sizerTop->SetSizeHints(this);
	//sizerTop->Fit(this);
	UpdateChecks();
	m_cmdline->SetFocus();
	m_bCheckDirty = false;
}


void CLogWindow::Save(IniFile& _IniFile) const
{
	_IniFile.Set("LogWindow", "x", GetPosition().x);
	_IniFile.Set("LogWindow", "y", GetPosition().y);
	_IniFile.Set("LogWindow", "w", GetSize().GetWidth());
	_IniFile.Set("LogWindow", "h", GetSize().GetHeight());
}


void CLogWindow::Load(IniFile& _IniFile)
{
	int x,y,w,h;
	_IniFile.Get("LogWindow", "x", &x, GetPosition().x);
	_IniFile.Get("LogWindow", "y", &y, GetPosition().y);
	_IniFile.Get("LogWindow", "w", &w, GetSize().GetWidth());
	_IniFile.Get("LogWindow", "h", &h, GetSize().GetHeight());
	SetSize(x, y, w, h);

	// load verbosity setting
	int v;
	_IniFile.Get("LogWindow", "Verbosity", &v, m_RadioBox[0]->GetSelection());
	m_RadioBox[0]->SetSelection(v);
	LogManager::m_LogSettings->m_iVerbosity = v;
}

void CLogWindow::OnSubmit(wxCommandEvent& event)
{
	Console_Submit(m_cmdline->GetValue().To8BitData());
	m_cmdline->SetValue(_T(""));
	NotifyUpdate();
}


void CLogWindow::OnClear(wxCommandEvent& event)
{
	if (Core::GetState() != Core::CORE_UNINITIALIZED) // avoid crash
	{
		LogManager::Clear();
		LOG(MASTER_LOG, "(log cleared).");
		NotifyUpdate();
	}
}

void CLogWindow::OnEnableAll(wxCommandEvent& event)
{
	if (!LogManager::m_Log[0])
		return;
	static bool enable = true;
	int v = LogManager::m_LogSettings->m_iVerbosity;
	IniFile ini;
	ini.Load("Debugger.ini");
	for (int i = 0; i < LogTypes::NUMBER_OF_LOGS; i++)
	{
		m_checks->Check(i, enable);
		LogManager::m_Log[i + v*100]->m_bEnable = enable;
		LogManager::m_Log[i + v*100]->m_bShowInLog = enable;
		ini.Set("LogManager", LogManager::m_Log[i + v*100]->m_szShortName, enable);
	}
	ini.Save("Debugger.ini");
	enable = !enable;
}


// ----------------------------------------------------------------------------------------
// enable or disable logging groups
// ---------------
void CLogWindow::OnLogCheck(wxCommandEvent& event)
{
	if (!LogManager::m_bInitialized)
	{
		return;
	}

	IniFile ini;
	ini.Load("Debugger.ini");
	int v = LogManager::m_LogSettings->m_iVerbosity;

	for (int i = 0; i < LogTypes::NUMBER_OF_LOGS; i++)
	{
		// update groups to enabled or disabled
		bool Enabled = m_checks->IsChecked(i);
		LogManager::m_Log[i + 100*v]->m_bEnable = Enabled;
		LogManager::m_Log[i + 100*v]->m_bShowInLog = Enabled;

		ini.Set("LogManager", LogManager::m_Log[i + 100*v]->m_szShortName, Enabled);
	}

	ini.Save("Debugger.ini");

	m_bCheckDirty = true;
	if (Core::GetState() != Core::CORE_UNINITIALIZED) UpdateLog();
}

// ----------------------------------------------------------------------------------------
// Change verbosity level
// -------------
void CLogWindow::OnRadioChange(wxCommandEvent& event)
{
	// get selection
	int v = m_RadioBox[0]->GetSelection();

	// save it
	LogManager::m_LogSettings->m_iVerbosity = v;
	IniFile ini;
	ini.Load("Debugger.ini");
	ini.Set("LogWindow", "Verbosity", v);
	ini.Save("Debugger.ini");

	if (Core::GetState() != Core::CORE_UNINITIALIZED) 
	{
		for (int i = 0; i < LogTypes::NUMBER_OF_LOGS; i++)
		{
			// update groups to enabled or disabled
			bool Enabled;
			ini.Get("LogManager", LogManager::m_Log[i + 100*v]->m_szShortName, &Enabled, false);
			LogManager::m_Log[i + 100*v]->m_bEnable = Enabled;
			LogManager::m_Log[i + 100*v]->m_bShowInLog = Enabled;
			m_checks->Check(i, Enabled);
		}

		UpdateLog();
	}
}


// ----------------------------------------------------------------------------------------
// Update checked groups. 
// -------------
void CLogWindow::UpdateChecks()
{
	if (!LogManager::m_bInitialized)
	{
		return;
	}

	// This is only run once to append checkboxes to the wxCheckListBox.
	if (m_checks->GetCount() == 0)
	{
		// [F|RES] hide the window while we fill it... wxwidgets gets trouble if you don't do it
		// (at least the win version)
		m_checks->Show(false);

		for (int i = 0; i < LogTypes::NUMBER_OF_LOGS; i++)
		{
			m_checks->Append(wxString::FromAscii(LogManager::m_Log[i]->m_szName));
		}

		m_checks->Show(true);
	}

	// ----------------------------------------------------------------------------------------
	// Load the correct values
	// -------------
	int v = LogManager::m_LogSettings->m_iVerbosity;
	IniFile ini;
	ini.Load("Debugger.ini");

	for (int i = 0; i < LogTypes::NUMBER_OF_LOGS; i++)
	{
		bool Enabled = false;
		ini.Get("LogManager", LogManager::m_Log[i + v*100]->m_szShortName, &Enabled, false);

		m_checks->Check(i, Enabled);

		LogManager::m_Log[i]->m_bEnable = Enabled;
		LogManager::m_Log[i]->m_bShowInLog = Enabled;
	}

	m_bCheckDirty = true;
}


void CLogWindow::OnUpdateLog(wxCommandEvent& event)
{
	if (Core::GetState() != Core::CORE_UNINITIALIZED) NotifyUpdate();
}


void CLogWindow::NotifyUpdate()
{
	UpdateChecks();
	UpdateLog();
}


void CLogWindow::UpdateLog()
{
	static int last = -1;
	int v = LogManager::m_LogSettings->m_iVerbosity;
	int i = LogManager::m_nextMessages[v];

	if ((last == i) && !m_bCheckDirty)
	{
		return;
	}

	m_bCheckDirty = false;
	last = i;
	//bash together a log buffer really fast (no slow strcpy here, just memcpys)
	int count = 0;
	char* p = m_logBuffer;

	// go through all rows
	while (count < MAX_MESSAGES)
	{
		count++;
		const LogManager::SMessage& message = LogManager::m_Messages[v][i];

		if (message.m_bInUse)
		{
			int len = message.m_dwMsgLen;

			if (LogManager::m_activeLog == LogTypes::MASTER_LOG)
			{
				if (LogManager::m_Log[message.m_type]->m_bShowInLog)
				{
					memcpy(p, message.m_szMessage, len);
					p += len;
				}
			}
			else
			{
				if (message.m_type == LogManager::m_activeLog)
				{
					memcpy(p, message.m_szMessage, len);
					p += len;
				}
			}
		}

		i++;

		if (i >= MAX_MESSAGES)
		{
			i = 0;
		}
	}

	*p = 0; //end the string
	m_log->SetValue(wxString::FromAscii(m_logBuffer));
	m_log->SetInsertionPoint(p - m_logBuffer - 1);
}


