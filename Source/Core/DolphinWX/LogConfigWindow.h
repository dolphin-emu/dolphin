// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

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
	bool m_writeFile, m_writeWindow;
	bool enableAll;

	// Controls
	wxCheckBox *m_writeFileCB, *m_writeWindowCB;
	wxCheckListBox* m_checks;
	wxRadioBox *m_verbosity;

	void CreateGUIControls();
	void OnVerbosityChange(wxCommandEvent& event);
	void OnWriteFileChecked(wxCommandEvent& event);
	void OnWriteWindowChecked(wxCommandEvent& event);
	void OnToggleAll(wxCommandEvent& event);
	void ToggleLog(int _logType, bool enable);
	void OnLogCheck(wxCommandEvent& event);
};
