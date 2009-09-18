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

#ifndef _DX_DEBUGGER_H_
#define _DX_DEBUGGER_H_

#include <wx/wx.h>
#include <wx/notebook.h>

#include "../Globals.h"

class IniFile;

class GFXDebuggerDX9 : public wxDialog
{
public:
	GFXDebuggerDX9(wxWindow *parent,
		wxWindowID id = 1,
		const wxString &title = wxT("Video"),
		const wxPoint& pos = wxDefaultPosition,
		const wxSize& size = wxDefaultSize,
		#ifdef _WIN32
		long style = wxNO_BORDER);
		#else
		long style = wxDEFAULT_FRAME_STYLE | wxCLIP_CHILDREN | wxNO_FULL_REPAINT_ON_RESIZE);
		#endif

	virtual ~GFXDebuggerDX9();

	void SaveSettings() const;
	void LoadSettings();

	bool bInfoLog;
	bool bPrimLog;
	bool bSaveTextures;
	bool bSaveTargets;
	bool bSaveShaders;

	void EnableButtons(bool enable);

private:
	DECLARE_EVENT_TABLE();

	wxPanel *m_MainPanel;

	wxCheckBox	*m_Check[6];
	wxButton	*m_pButtonPause;
	wxButton	*m_pButtonPauseAtNext;
	wxButton	*m_pButtonPauseAtNextFrame;
	wxButton	*m_pButtonGo;
	wxChoice	*m_pPauseAtList;
	wxButton	*m_pButtonDump;
	wxChoice	*m_pDumpList;
	wxButton	*m_pButtonUpdateScreen;
	wxButton	*m_pButtonClearScreen;
	wxButton	*m_pButtonClearTextureCache;
	wxButton	*m_pButtonClearVertexShaderCache;
	wxButton	*m_pButtonClearPixelShaderCache;
	wxTextCtrl	*m_pCount;


	// WARNING: Make sure these are not also elsewhere
	enum
	{
		ID_MAINPANEL = 3900,
		ID_SAVETOFILE,
		ID_INFOLOG,
		ID_PRIMLOG,
		ID_SAVETEXTURES,
		ID_SAVETARGETS,
		ID_SAVESHADERS,
		NUM_OPTIONS,
		ID_GO,
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
	void OnGoButton(wxCommandEvent& event);
	void OnUpdateScreenButton(wxCommandEvent& event);
	void OnClearScreenButton(wxCommandEvent& event);
	void OnClearTextureCacheButton(wxCommandEvent& event);
	void OnClearVertexShaderCacheButton(wxCommandEvent& event);
	void OnClearPixelShaderCacheButton(wxCommandEvent& event);
	void OnCountEnter(wxCommandEvent& event);

};

enum PauseEvent {
	NEXT_FRAME,
	NEXT_FLUSH,

	NEXT_PIXEL_SHADER_CHANGE,
	NEXT_VERTEX_SHADER_CHANGE,
	NEXT_TEXTURE_CHANGE,
	NEXT_NEW_TEXTURE,

	NEXT_XFB_CMD,
	NEXT_EFB_CMD,

	NEXT_MATRIX_CMD,
	NEXT_VERTEX_CMD,
	NEXT_TEXTURE_CMD,
	NEXT_LIGHT_CMD,
	NEXT_FOG_CMD,

	NEXT_SET_TLUT,

	NEXT_FIFO,
	NEXT_DLIST,
	NEXT_UCODE,

	NEXT_ERROR,

	NOT_PAUSE,
};

extern volatile bool DX9DebuggerPauseFlag;
extern volatile PauseEvent DX9DebuggerToPauseAtNext;
extern volatile int DX9DebuggerEventToPauseCount;
void ContinueDX9Debugger();
void DX9DebuggerCheckAndPause(bool update);
void DX9DebuggerToPause(bool update);

#undef ENABLE_DX_DEBUGGER
#if defined(_DEBUG) || defined(DEBUGFAST)
#define ENABLE_DX_DEBUGGER
#endif

#ifdef ENABLE_DX_DEBUGGER

#define DEBUGGER_PAUSE_AT(event,update) {if ((DX9DebuggerToPauseAtNext == event && --DX9DebuggerEventToPauseCount<=0) || DX9DebuggerPauseFlag) DX9DebuggerToPause(update);}
#define DEBUGGER_PAUSE_LOG_AT(event,update,dumpfunc) {if ((DX9DebuggerToPauseAtNext == event && --DX9DebuggerEventToPauseCount<=0) || DX9DebuggerPauseFlag) {{dumpfunc};DX9DebuggerToPause(update);}}

#else
// Not to use debugger in release build
#define DEBUGGER_PAUSE_AT(event,update)
#define DEBUGGER_PAUSE_LOG_AT(event,update,dumpfunc)

#endif ENABLE_DX_DEBUGGER


#endif // _DX_DEBUGGER_H_
