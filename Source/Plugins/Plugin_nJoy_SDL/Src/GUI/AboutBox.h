
// Project description
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
// Name: nJoy 
// Description: A Dolphin Compatible Input Plugin
//
// Author: Falcon4ever (nJoy@falcon4ever.com)
// Site: www.multigesture.net
// Copyright (C) 2003 Dolphin Project.
//

//
// Licensetype: GNU General Public License (GPL)
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.
//
// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/
//
// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/
//


#ifndef __ABOUTBOX_h__
#define __ABOUTBOX_h__

#ifndef WX_PRECOMP
	#include <wx/wx.h>
	#include <wx/dialog.h>
#else
	#include <wx/wxprec.h>
#endif

#include <wx/button.h>
#include <wx/stattext.h>
#include <wx/statbox.h>
#include <wx/statbmp.h>


class AboutBox : public wxDialog
{
	private:
		DECLARE_EVENT_TABLE();
		
	public:
		AboutBox(wxWindow *parent, wxWindowID id = 1, const wxString &title = wxT("About: nJoy Input Plugin"),
			const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize, long style = wxDEFAULT_DIALOG_STYLE);
		virtual ~AboutBox();
		void OKClick(wxCommandEvent& event);

	private:
		wxStaticText *m_thankyoutext;
		wxStaticText *m_specialthanks;
		wxStaticText *m_pluginversion;
		wxButton *m_OK;
		wxStaticText *m_version;
		wxStaticBox *m_thankyougroup;
		wxStaticBox *m_specialthanksgroup;
		wxStaticBox *m_pluginversiongroup;
		wxStaticBitmap *m_njoylogo;
		
	private:
		enum
		{
			ID_THANKYOU = 1009,
			ID_SPECIALTHANKS = 1008,
			ID_PLUGINVERSION = 1007,
			ID_OK = 1006,
			ID_STATUSV = 1005,
			IDG_THANKYOU = 1004,
			IDG_SPECIALTHANKS = 1003,
			IDG_PLUGINVERSION = 1002,
			ID_AWESOMEPICTURE = 1001,

			ID_DUMMY_VALUE_ //don't remove this value unless you have other enum values
		};
	
	private:
		void OnClose(wxCloseEvent& event);
		void CreateGUIControls();
};

#endif
