// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>
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
	void OnInitCmdLine(wxCmdLineParser& parser) override;
	bool OnCmdLineParsed(wxCmdLineParser& parser) override;
	void OnFatalException() override;
	bool Initialize(int& c, wxChar** v) override;

#ifdef __APPLE__
	void MacOpenFile(const wxString &fileName) override;
#endif

	void OnEndSession(wxCloseEvent& event);
	void InitLanguageSupport();
	void AfterInit();
	void OnIdle(wxIdleEvent&);

	bool m_batch_mode = false;
	bool m_confirm_stop = false;
	bool m_load_file = false;
	bool m_play_movie = false;
	bool m_use_debugger = false;
	bool m_use_logger = false;
	bool m_select_video_backend = false;
	bool m_select_audio_emulation = false;
	wxString m_confirm_setting;
	wxString m_video_backend_name;
	wxString m_audio_emulation_name;
	wxString m_user_path;
	wxString m_file_to_load;
	wxString m_movie_file;
	std::unique_ptr<wxLocale> m_locale;
};

DECLARE_APP(DolphinApp);
