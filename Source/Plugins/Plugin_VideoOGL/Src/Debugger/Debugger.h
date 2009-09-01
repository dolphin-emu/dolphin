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

#ifndef _CDEBUGGER_H_
#define _CDEBUGGER_H_

#include <wx/wx.h>
#include <wx/notebook.h>

#include "../Globals.h"

class IniFile;

class GFXDebuggerOGL : public wxDialog
{
public:
	GFXDebuggerOGL(wxWindow *parent,
		wxWindowID id = 1,
		const wxString &title = wxT("Video"),
		const wxPoint& pos = wxDefaultPosition,
		const wxSize& size = wxDefaultSize,
		#ifdef _WIN32
		long style = wxNO_BORDER);
		#else
		long style = wxDEFAULT_FRAME_STYLE | wxCLIP_CHILDREN | wxNO_FULL_REPAINT_ON_RESIZE);
		#endif

	virtual ~GFXDebuggerOGL();

	void SaveSettings() const;
	void LoadSettings();

	bool bInfoLog;
	bool bPrimLog;
	bool bSaveTextures;
	bool bSaveTargets;
	bool bSaveShaders;

private:
	DECLARE_EVENT_TABLE();

	wxPanel *m_MainPanel;

	wxCheckBox *m_Check[6];

	// WARNING: Make sure these are not also elsewhere
	enum
	{
		ID_MAINPANEL = 2000,
		ID_SAVETOFILE,
		ID_INFOLOG,
		ID_PRIMLOG,
		ID_SAVETEXTURES,
		ID_SAVETARGETS,
		ID_SAVESHADERS,
		NUM_OPTIONS
	};

	void OnClose(wxCloseEvent& event);		
	void CreateGUIControls();

	void GeneralSettings(wxCommandEvent& event);
};

#endif // _CDEBUGGER_H_
