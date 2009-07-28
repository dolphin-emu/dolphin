//////////////////////////////////////////////////////////////////////////////////////////
// Project description
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
// Name: nJoy 
// Description: A Dolphin Compatible Input Plugin
//
// Author: Falcon4ever (nJoy@falcon4ever.com)
// Site: www.multigesture.net
// Copyright (C) 2003 Dolphin Project.
//
//////////////////////////////////////////////////////////////////////////////////////////
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
//////////////////////////////////////////////////////////////////////////////////////////

#include "AboutBox.h"
#include "../nJoy.h"
#include "Images/njoy.xpm"

BEGIN_EVENT_TABLE(AboutBox,wxDialog)
	EVT_CLOSE(AboutBox::OnClose)
	EVT_BUTTON(ID_OK, AboutBox::OKClick)
END_EVENT_TABLE()

AboutBox::AboutBox(wxWindow *parent, wxWindowID id, const wxString &title, const wxPoint &position, const wxSize& size, long style)
: wxDialog(parent, id, title, position, size, style)
{
	CreateGUIControls();
}

AboutBox::~AboutBox()
{
	// empty
} 

void AboutBox::CreateGUIControls()
{
	SetTitle(wxT("About: nJoy Input Plugin"));
	SetIcon(wxNullIcon);
	//SetSize(8,8,200,399);
	SetClientSize(200,376);
	Center();

#ifndef _WIN32
	// Force a 8pt font so that it looks more or less "correct" regardless of the default font setting
	wxFont f(8,wxFONTFAMILY_DEFAULT,wxFONTSTYLE_NORMAL,wxFONTWEIGHT_NORMAL);
	SetFont(f);
#endif

	m_thankyoutext = new wxStaticText(this, ID_THANKYOU, THANKYOU, wxPoint(18,196), wxDefaultSize, 0, wxEmptyString);
	m_thankyoutext->Wrap(167);
	m_specialthanks = new wxStaticText(this, ID_SPECIALTHANKS, wxT("F|RES and ector."), wxPoint(18,153), wxDefaultSize, 0, wxEmptyString);
	m_pluginversion = new wxStaticText(this, ID_PLUGINVERSION, PLUGIN_VER_STR, wxPoint(18,80), wxDefaultSize, 0, wxEmptyString);
	m_OK = new wxButton(this, ID_OK, wxT("OK"), wxPoint(116,343), wxSize(75,25), 0, wxDefaultValidator, wxEmptyString);
	m_version = new wxStaticText(this, ID_STATUSV, wxT("PUBLIC RELEASE"), wxPoint(14,349), wxDefaultSize, 0, wxEmptyString);
	m_thankyougroup = new wxStaticBox(this, IDG_THANKYOU, wxT("Greetings to:"), wxPoint(10,180), wxSize(180,160));
	m_specialthanksgroup = new wxStaticBox(this, IDG_SPECIALTHANKS, wxT("Special thanks to:"), wxPoint(10,135), wxSize(180,39));
	m_pluginversiongroup = new wxStaticBox(this, IDG_PLUGINVERSION, wxT("Plugin info:"), wxPoint(10,61), wxSize(180,68));

	wxBitmap WxStaticBitmap1_BITMAP(AboutBox_WxStaticBitmap1_XPM);
	m_njoylogo = new wxStaticBitmap(this, ID_AWESOMEPICTURE, WxStaticBitmap1_BITMAP, wxPoint(-1,-2), wxSize(200,60));
}

void AboutBox::OnClose(wxCloseEvent& /*event*/)
{
	EndModal(0);
}

void AboutBox::OKClick(wxCommandEvent& event)
{
	if (event.GetId() == ID_OK)
	{
		Close(); 
	}
}
