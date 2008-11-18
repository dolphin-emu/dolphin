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
#ifndef _ABOUTDOLPHIN_H_
#define _ABOUTDOLPHIN_H_

#include <wx/wx.h>
#include <wx/dialog.h>
#include <wx/button.h>
#include <wx/stattext.h>
#include <wx/statbox.h>
#include <wx/mstream.h>
#include <wx/statbmp.h>

#undef ABOUTDOLPHIN_STYLE
#define ABOUTDOLPHIN_STYLE wxCAPTION | wxSYSTEM_MENU | wxDIALOG_NO_PARENT | wxCLOSE_BOX

class AboutDolphin : public wxDialog
{
	public:
		AboutDolphin(wxWindow *parent, wxWindowID id = 1, const wxString &title = wxT("About Dolphin"), const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize, long style = ABOUTDOLPHIN_STYLE);
		virtual ~AboutDolphin();
		void CloseClick(wxCommandEvent& event);

	private:
		DECLARE_EVENT_TABLE();

		wxBoxSizer *sMain;
		wxBoxSizer *sButtons;
		wxBoxSizer *sMainHor;
		wxBoxSizer *sInfo;

		wxButton *m_Close;
		wxStaticText *Message;
		wxBitmap *DolphinLogo;
		wxStaticBitmap *sbDolphinLogo;

		enum
		{
			ID_CLOSE = 1000,
			ID_LOGO,
			ID_MESSAGE
		};

		void OnClose(wxCloseEvent& event);
		void CreateGUIControls();
};

#endif //_ABOUTDOLPHIN_H_
