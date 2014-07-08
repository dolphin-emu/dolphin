// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <wx/app.h>
#include <wx/chartype.h>
#include <wx/defs.h>
#include <wx/event.h>
#include <wx/string.h>

class CFrame;
class wxLocale;
class wxTimer;
class wxTimerEvent;

extern CFrame* main_frame;

// Define a new application
class DolphinApp : public wxApp
{
public:
	CFrame* GetCFrame();

private:
	bool OnInit() override;
	int OnExit() override;
	void OnFatalException() override;
	bool Initialize(int& c, wxChar **v) override;
	void InitLanguageSupport();
	void MacOpenFile(const wxString &fileName);

	DECLARE_EVENT_TABLE()

	wxTimer *m_afterinit;
	bool BatchMode;
	bool LoadFile;
	bool playMovie;
	wxString FileToLoad;
	wxString movieFile;
	wxLocale *m_locale;

	void AfterInit(wxTimerEvent& WXUNUSED(event));
	void OnEndSession(wxCloseEvent& event);
};

DECLARE_APP(DolphinApp);
