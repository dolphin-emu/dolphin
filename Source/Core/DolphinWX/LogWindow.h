// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

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

	DECLARE_EVENT_TABLE()

	wxTextCtrl * CreateTextCtrl(wxPanel* parent, wxWindowID id, long Style);
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
