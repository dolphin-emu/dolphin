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

#ifndef __LUAWINDOW_H__
#define __LUAWINDOW_H__

#include <wx/wx.h>
#include <wx/sizer.h>
#include <wx/filepicker.h>
#include <wx/statbmp.h>
#include <wx/imaglist.h>
#include <wx/treectrl.h>
#include <wx/gbsizer.h>
#include <wx/notebook.h>
#include <wx/mimetype.h>
#include <wx/colour.h>
#include <wx/listbox.h>
#include <string>

#include "Filesystem.h"
#include "IniFile.h"

class wxLuaWindow : public wxFrame
{
	public:

		wxLuaWindow(wxFrame* parent, const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize);

		void PrintMessage(const char *text);

		void OnStop();

		virtual ~wxLuaWindow();

	protected:
		static int luaCount;
		int luaID;
		bool bScriptRunning;
		wxString currentScript;

		// Event Table
		DECLARE_EVENT_TABLE();

		// --- GUI Controls ---

		wxPanel *m_Tab_Log;

		wxButton *m_Button_Close, *m_Button_LoadScript, *m_Button_Run, 
			     *m_Button_Stop;

		wxTextCtrl *m_TextCtrl_Log;

		// GUI IDs
		enum
		{
			ID_TAB_LOG,
			ID_BUTTON_CLOSE,
			ID_BUTTON_LOAD,
			ID_BUTTON_RUN,
			ID_BUTTON_STOP,
			ID_TEXTCTRL_LOG
		};

		void InitGUIControls();


		// --- Wx Events Handlers ---
		// $ Window
		void OnEvent_Window_Resize(wxSizeEvent& event);
		void OnEvent_Window_Close(wxCloseEvent& event);

		// $ Buttons
		void OnEvent_ButtonClose_Press(wxCommandEvent& event);
		void OnEvent_ScriptLoad_Press(wxCommandEvent& event);
		void OnEvent_ScriptRun_Press(wxCommandEvent& event);
		void OnEvent_ScriptStop_Press(wxCommandEvent& event);


};

#endif

