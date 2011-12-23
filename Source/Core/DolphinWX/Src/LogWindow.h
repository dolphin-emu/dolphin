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

#ifndef LOGWINDOW_H_
#define LOGWINDOW_H_

#include "LogManager.h"
#include "Frame.h"
#include "Thread.h"
#include <queue>

enum
{
	IDM_LOG,
	IDM_CLEARLOG,
	IDM_TOGGLEALL,
	IDM_WRAPLINE,
	IDTM_UPDATELOG,
	IDM_FONT,
	IDM_SUBMITCMD
};

class wxTextCtrl;
class wxCheckListBox;
class wxString;
class CFrame;

// Uses multiple inheritance - only sane because LogListener is a pure virtual interface.
class CLogWindow : public wxPanel, LogListener
{
public:
	CLogWindow(CFrame *parent,
		wxWindowID id = wxID_ANY,
		const wxPoint& pos = wxDefaultPosition,
		const wxSize& size = wxDefaultSize,
		long style = wxTAB_TRAVERSAL,
		const wxString& name = _("Log")
		);
	~CLogWindow();

	void SaveSettings();
	void Log(LogTypes::LOG_LEVELS, const char *text);

	int x, y, winpos;

private:
	CFrame *Parent;
	wxFont DefaultFont, MonoSpaceFont;
	std::vector<wxFont> LogFont;
	wxTimer *m_LogTimer;
	bool m_ignoreLogTimer;
	LogManager *m_LogManager;
	std::queue<std::pair<u8, wxString> > msgQueue;
	bool m_writeFile, m_writeConsole, m_writeWindow, m_writeDebugger, m_LogAccess;

	// Controls
	wxBoxSizer *sBottom;
	wxTextCtrl *m_Log, *m_cmdline;
	wxChoice *m_FontChoice;
	wxCheckBox *m_WrapLine;

	std::mutex m_LogSection;

	wxCSConv m_SJISConv;

	DECLARE_EVENT_TABLE()

	wxTextCtrl * CreateTextCtrl(wxPanel* parent, wxWindowID id = wxID_ANY, long Style = NULL);
	void CreateGUIControls();
	void PopulateBottom();
	void UnPopulateBottom();
	void OnClose(wxCloseEvent& event);
	void OnSubmit(wxCommandEvent& event);
	void OnFontChange(wxCommandEvent& event);
	void OnWrapLineCheck(wxCommandEvent& event);
	void OnClear(wxCommandEvent& event);
	void OnLogTimer(wxTimerEvent& WXUNUSED(event));
	void UpdateLog();

	// LogListener
	const char *getName() const { return "LogWindow"; }
};

#endif /*LOGWINDOW_H_*/
