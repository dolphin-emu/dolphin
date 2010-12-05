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

#ifndef _GFX_DEBUGGER_PANEL_H_
#define _GFX_DEBUGGER_PANEL_H_

#include <wx/wx.h>
#include <wx/notebook.h>
#include "Debugger.h"

class GFXDebuggerPanel : public wxPanel, public GFXDebuggerBase
{
public:
	GFXDebuggerPanel(wxWindow *parent,
		wxWindowID id = wxID_ANY,
		const wxPoint& pos = wxDefaultPosition,
		const wxSize& size = wxDefaultSize,
		long style = wxTAB_TRAVERSAL,
		const wxString &title = wxT("GFX Debugger"));

	virtual ~GFXDebuggerPanel();

	void SaveSettings() const;
	void LoadSettings();

	bool bInfoLog;
	bool bPrimLog;
	bool bSaveTextures;
	bool bSaveTargets;
	bool bSaveShaders;

	void OnPause();
	void OnContinue();

private:
	DECLARE_EVENT_TABLE();

	wxPanel *m_MainPanel;

	wxButton	*m_pButtonPause;
	wxButton	*m_pButtonPauseAtNext;
	wxButton	*m_pButtonPauseAtNextFrame;
	wxButton	*m_pButtonCont;
	wxChoice	*m_pPauseAtList;
	wxButton	*m_pButtonDump;
	wxChoice	*m_pDumpList;
	wxButton	*m_pButtonUpdateScreen;
	wxButton	*m_pButtonClearScreen;
	wxButton	*m_pButtonClearTextureCache;
	wxButton	*m_pButtonClearVertexShaderCache;
	wxButton	*m_pButtonClearPixelShaderCache;
	wxTextCtrl	*m_pCount;


	// TODO: Prefix with GFX_
	enum
	{
		ID_MAINPANEL = 3900,
		ID_CONT,
		ID_PAUSE,
		ID_PAUSE_AT_NEXT,
		ID_PAUSE_AT_NEXT_FRAME,
		ID_PAUSE_AT_LIST,
		ID_DUMP,
		ID_DUMP_LIST,
		ID_UPDATE_SCREEN,
		ID_CLEAR_SCREEN,
		ID_CLEAR_TEXTURE_CACHE,
		ID_CLEAR_VERTEX_SHADER_CACHE,
		ID_CLEAR_PIXEL_SHADER_CACHE,
		ID_COUNT
	};

	void OnClose(wxCloseEvent& event);		
	void CreateGUIControls();

	void GeneralSettings(wxCommandEvent& event);
	void OnPauseButton(wxCommandEvent& event);
	void OnPauseAtNextButton(wxCommandEvent& event);
	void OnPauseAtNextFrameButton(wxCommandEvent& event);
	void OnDumpButton(wxCommandEvent& event);
	void OnContButton(wxCommandEvent& event);
	void OnUpdateScreenButton(wxCommandEvent& event);
	void OnClearScreenButton(wxCommandEvent& event);
	void OnClearTextureCacheButton(wxCommandEvent& event);
	void OnClearVertexShaderCacheButton(wxCommandEvent& event);
	void OnClearPixelShaderCacheButton(wxCommandEvent& event);
	void OnCountEnter(wxCommandEvent& event);
};

#endif // _GFX_DEBUGGER_PANEL_H_
