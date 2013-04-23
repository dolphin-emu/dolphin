// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#ifndef __MAIN_H_
#define __MAIN_H_

#include "Frame.h"

// Define a new application
class CFrame;
class DolphinApp : public wxApp
{
public:
	CFrame* GetCFrame();

private:
	bool OnInit();
	int OnExit();
	void OnFatalException();
	bool Initialize(int& c, wxChar **v);
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
};

DECLARE_APP(DolphinApp);

#endif
