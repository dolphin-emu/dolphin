// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

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
