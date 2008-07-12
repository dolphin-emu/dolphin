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

#include "wx/button.h"
#include "wx/textctrl.h"
#include "wx/listbox.h"
#include "wx/checklst.h"

#include "LogWindow.h"
#include "Console.h"
#include "IniFile.h"

BEGIN_EVENT_TABLE(CLogWindow, wxDialog)
EVT_BUTTON(IDM_SUBMITCMD, CLogWindow::OnSubmit)
EVT_BUTTON(IDM_UPDATELOG, CLogWindow::OnUpdateLog)
EVT_BUTTON(IDM_CLEARLOG,  CLogWindow::OnClear)
EVT_CHECKLISTBOX(IDM_LOGCHECKS, CLogWindow::OnLogCheck)
END_EVENT_TABLE()


CLogWindow::CLogWindow(wxWindow* parent)
	: wxDialog(parent, wxID_ANY, _T("Log/Console"), wxPoint(100, 700), wxSize(800, 270),
			wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
{
	wxBoxSizer* sizerTop = new wxBoxSizer(wxHORIZONTAL),
	* sizerUber = new wxBoxSizer(wxHORIZONTAL),
	* sizerBig = new wxBoxSizer(wxVERTICAL),
	* sizerBottom = new wxBoxSizer(wxHORIZONTAL);

	m_log = new wxTextCtrl(this, IDM_LOG, _T(""), wxDefaultPosition, wxSize(600, 120), wxTE_MULTILINE | wxTE_READONLY | wxTE_DONTWRAP);
	m_cmdline = new wxTextCtrl(this, wxID_ANY, _T(""), wxDefaultPosition);
	wxButton* btn = new wxButton(this, IDM_SUBMITCMD, _T("Submit"));

	sizerTop->Add(new wxButton(this, IDM_UPDATELOG, _T("Update")));
	sizerTop->Add(new wxButton(this, IDM_CLEARLOG, _T("Clear")));
	m_checks = new wxCheckListBox(this, IDM_LOGCHECKS, wxDefaultPosition, wxSize(120, 280));
	sizerBottom->Add(m_cmdline, 8, wxGROW | wxRIGHT, 5);
	sizerBottom->Add(btn, 1, wxGROW, 0);

	sizerBig->Add(sizerTop, 0, wxGROW);
	sizerBig->Add(m_log, 1, wxGROW | wxSHRINK);
	sizerBig->Add(sizerBottom, 0, wxGROW);

	sizerUber->Add(m_checks, 0, wxGROW);
	sizerUber->Add(sizerBig, 1, wxGROW);

	SetSizer(sizerUber);
	SetAffirmativeId(IDM_SUBMITCMD);

	//sizerTop->SetSizeHints(this);
	//sizerTop->Fit(this);
	UpdateChecks();
	m_cmdline->SetFocus();
	m_bCheckDirty = false;
}


void CLogWindow::OnSubmit(wxCommandEvent& event)
{
	Console_Submit(m_cmdline->GetValue().c_str());
	m_cmdline->SetValue(_T(""));
	NotifyUpdate();
}


void CLogWindow::OnClear(wxCommandEvent& event)
{
	LogManager::Clear();
	LOG(MASTER_LOG, "(log cleared).");
	NotifyUpdate();
}


void CLogWindow::OnLogCheck(wxCommandEvent& event)
{
	if (!LogManager::m_bInitialized)
	{
		return;
	}

	IniFile ini;
	ini.Load("Dolphin.ini");

	for (int i = 0; i < LogTypes::NUMBER_OF_LOGS; i++)
	{
		bool Enabled = m_checks->IsChecked(i);
		LogManager::m_Log[i]->m_bEnable = Enabled;
		LogManager::m_Log[i]->m_bShowInLog = Enabled;

		ini.Set("LogManager", LogManager::m_Log[i]->m_szShortName, Enabled);
	}

	ini.Save("Dolphin.ini");

	m_bCheckDirty = true;
	UpdateLog();
}


void CLogWindow::UpdateChecks()
{
	if (!LogManager::m_bInitialized)
	{
		return;
	}

	if (m_checks->GetCount() == 0)
	{
		// [F|RES] hide the window while we fill it... wxwidgets gets trouble if you don't do it (at least the win version)
		m_checks->Show(false);

		for (int i = 0; i < LogTypes::NUMBER_OF_LOGS; i++)
		{
			m_checks->Append(LogManager::m_Log[i]->m_szName);
		}

		m_checks->Show(true);
	}

	IniFile ini;
	ini.Load("Dolphin.ini");

	for (int i = 0; i < LogTypes::NUMBER_OF_LOGS; i++)
	{
		bool Enabled = false;
		ini.Get("LogManager", LogManager::m_Log[i]->m_szShortName, &Enabled, false);

		m_checks->Check(i, Enabled);

		LogManager::m_Log[i]->m_bEnable = Enabled;
		LogManager::m_Log[i]->m_bShowInLog = Enabled;
	}

	m_bCheckDirty = true;
	UpdateLog();
}


void CLogWindow::OnUpdateLog(wxCommandEvent& event)
{
	NotifyUpdate();
}


void CLogWindow::NotifyUpdate()
{
	UpdateChecks();
	UpdateLog();
}


void CLogWindow::UpdateLog()
{
	static int last = -1;
	int i = LogManager::m_nextMessages;

	if ((last == i) && !m_bCheckDirty)
	{
		return;
	}

	m_bCheckDirty = false;
	last = i;
	//bash together a log buffer really fast (no slow strcpy here, just memcpys)
	int count = 0;
	char* p = m_logBuffer;

	while (count < MAX_MESSAGES)
	{
		count++;
		const LogManager::SMessage& message = LogManager::m_Messages[i];

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
	m_log->SetValue(m_logBuffer);
	m_log->SetInsertionPoint(p - m_logBuffer - 1);
}


