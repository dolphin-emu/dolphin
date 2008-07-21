// Copyright (C) 2003-2008 Dolphin Project.

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

#ifndef __REGISTERWINDOW_h__
#define __REGISTERWINDOW_h__

class CRegisterView;

#undef RegisterWindow_STYLE
#define RegisterWindow_STYLE wxCAPTION | wxSYSTEM_MENU | wxCLOSE_BOX

class CRegisterWindow
	: public wxDialog
{
	private:	

		DECLARE_EVENT_TABLE();

	public:

		CRegisterWindow(wxWindow* parent, wxWindowID id = 1, const wxString& title = wxT("Registers"), const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize, long style = RegisterWindow_STYLE);
		virtual ~CRegisterWindow();

		void NotifyUpdate();


	private:

		enum
		{
			ID_GPR = 1002
		};

		CRegisterView* m_GPRListView;

		void OnClose(wxCloseEvent& event);
		void CreateGUIControls();
};

#endif
