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
#include "Main.h" // for wxGetApp
#include "LogManager.h"
#include "Frame.h"
#include "DebuggerUIUtil.h"

#include "IniFile.h" // Common
#include "Thread.h"
#include <queue>

enum
{
    IDM_LOG,
    IDM_CLEARLOG,
    IDM_LOGCHECKS,
    IDM_OPTIONS,
    IDM_TOGGLEALL,
	IDM_WRAPLINE,
    IDM_WRITEFILE,
    IDM_WRITECONSOLE,
    IDM_WRITEWINDOW,
	IDTM_UPDATELOG,
	IDM_VERBOSITY,
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
		const wxString& title = wxT("Log"),
		const wxPoint& pos = wxPoint(100, 700),
		const wxSize& size = wxSize(800, 270),
		long style = wxNO_BORDER);
	~CLogWindow();
	void NotifyUpdate();

	void SaveSettings();
	void LoadSettings();
	void Log(LogTypes::LOG_LEVELS, const char *text);

private:
	CFrame *Parent;
	wxFont DefaultFont, MonoSpaceFont;
	std::vector<wxFont> Font;	
	wxTimer *m_LogTimer;
	FileLogListener *m_fileLog;
	ConsoleListener *m_console;
	LogManager *m_LogManager;
	std::queue<std::pair<u8, wxString> > msgQueue;
	bool m_writeFile, m_writeConsole, m_writeWindow, m_LogAccess;

	// Controls
	wxBoxSizer *sUber, *sLeft, *sRight, *sRightBottom;
	wxTextCtrl *m_Log, *m_cmdline;
	wxChoice * m_FontChoice;
	wxCheckBox *m_writeFileCB, *m_writeConsoleCB, *m_writeWindowCB;
	wxCheckListBox* m_checks;
	wxRadioBox *m_verbosity;

	Common::CriticalSection m_LogSection;

	DECLARE_EVENT_TABLE()

	wxTextCtrl * CreateTextCtrl(wxPanel* parent, wxWindowID id = wxID_ANY, long Style = NULL);
	void CreateGUIControls();
	void PopulateRight(); void UnPopulateRight();
	void OnClose(wxCloseEvent& event);
	void OnSubmit(wxCommandEvent& event);
	void OnOptionsCheck(wxCommandEvent& event);
	void OnLogCheck(wxCommandEvent& event);
	void OnClear(wxCommandEvent& event);
	void OnToggleAll(wxCommandEvent& event);
	void OnLogTimer(wxTimerEvent& WXUNUSED(event));

	void ToggleLog(int _logType, bool enable);
	void UpdateChecks();
	void UpdateLog();

	// LogListener
	const char *getName() const { return "LogWindow"; }
};

#endif /*LOGWINDOW_H_*/
