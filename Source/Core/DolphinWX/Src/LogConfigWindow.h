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

#ifndef _LOG_CONFIG_DIAG_H_
#define _LOG_CONFIG_DIAG_H_

#include <vector>
#include <wx/wx.h>

class CFrame;
class LogManager;
class CLogWindow;

class LogConfigWindow : public wxPanel
{
public:
	LogConfigWindow(wxWindow* parent, CLogWindow *log_window, wxWindowID id = wxID_ANY);
	~LogConfigWindow();

	void SaveSettings();
	void LoadSettings();

private:
	LogManager *m_LogManager;
	CLogWindow *m_LogWindow;
	bool m_writeFile, m_writeConsole, m_writeWindow, m_writeDebugger;
	bool enableAll;

	// Controls
	wxCheckBox *m_writeFileCB, *m_writeConsoleCB, *m_writeWindowCB, *m_writeDebuggerCB;
	wxCheckListBox* m_checks;
	wxRadioBox *m_verbosity;

	void CreateGUIControls();
	void OnVerbosityChange(wxCommandEvent& event);
	void OnWriteFileChecked(wxCommandEvent& event);
	void OnWriteConsoleChecked(wxCommandEvent& event);
	void OnWriteWindowChecked(wxCommandEvent& event);
	void OnWriteDebuggerChecked(wxCommandEvent& event);
	void OnToggleAll(wxCommandEvent& event);
	void ToggleLog(int _logType, bool enable);
	void OnLogCheck(wxCommandEvent& event);
};

#endif // _LOG_CONFIG_DIAG_H_
