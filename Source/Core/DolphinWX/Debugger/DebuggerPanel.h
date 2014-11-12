// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <wx/defs.h>
#include <wx/event.h>
#include <wx/gdicmn.h>
#include <wx/panel.h>
#include <wx/string.h>
#include <wx/translation.h>
#include <wx/windowid.h>

#include "VideoCommon/Debugger.h"

class wxButton;
class wxChoice;
class wxTextCtrl;
class wxWindow;

class GFXDebuggerPanel : public wxPanel, public GFXDebuggerBase
{
public:
	GFXDebuggerPanel(wxWindow* parent,
		wxWindowID id = wxID_ANY,
		const wxPoint& pos = wxDefaultPosition,
		const wxSize& size = wxDefaultSize,
		long style = wxTAB_TRAVERSAL,
		const wxString &title = _("GFX Debugger"));

	virtual ~GFXDebuggerPanel();

	void SaveSettings() const;
	void LoadSettings();

	bool bInfoLog;
	bool bPrimLog;
	bool bSaveTextures;
	bool bSaveTargets;
	bool bSaveShaders;

	void OnPause() override;

	// Called from GFX thread once the GFXDebuggerPauseFlag spin lock has finished
	void OnContinue() override;

private:
	wxPanel*    m_MainPanel;

	wxButton*   m_pButtonPause;
	wxButton*   m_pButtonPauseAtNext;
	wxButton*   m_pButtonPauseAtNextFrame;
	wxButton*   m_pButtonCont;
	wxChoice*   m_pPauseAtList;
	wxButton*   m_pButtonDump;
	wxChoice*   m_pDumpList;
	wxButton*   m_pButtonUpdateScreen;
	wxButton*   m_pButtonClearScreen;
	wxButton*   m_pButtonClearTextureCache;
	wxButton*   m_pButtonClearVertexShaderCache;
	wxButton*   m_pButtonClearPixelShaderCache;
	wxTextCtrl* m_pCount;

	void OnClose(wxCloseEvent& event);
	void CreateGUIControls();

	void GeneralSettings(wxCommandEvent& event);

	// These set GFXDebuggerPauseFlag to true (either immediately or once the specified event has occurred)
	void OnPauseButton(wxCommandEvent& event);
	void OnPauseAtNextButton(wxCommandEvent& event);

	void OnPauseAtNextFrameButton(wxCommandEvent& event);
	void OnDumpButton(wxCommandEvent& event);

	// sets GFXDebuggerPauseFlag to false
	void OnContButton(wxCommandEvent& event);

	void OnUpdateScreenButton(wxCommandEvent& event);
	void OnClearScreenButton(wxCommandEvent& event);
	void OnClearTextureCacheButton(wxCommandEvent& event);
	void OnClearVertexShaderCacheButton(wxCommandEvent& event);
	void OnClearPixelShaderCacheButton(wxCommandEvent& event);
	void OnCountEnter(wxCommandEvent& event);
};
