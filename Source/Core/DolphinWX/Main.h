// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <wx/app.h>

class CFrame;
class wxLocale;

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
#ifdef __APPLE__
	void MacOpenFile(const wxString &fileName) override;
#endif

	bool BatchMode;
	bool LoadFile;
	bool playMovie;
	wxString FileToLoad;
	wxString movieFile;
	wxLocale *m_locale;

	void AfterInit();
	void OnEndSession(wxCloseEvent& event);
};

DECLARE_APP(DolphinApp);
