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


#include "ConfigDlg.h"
#include "Config.h"


BEGIN_EVENT_TABLE(ConfigDialog,wxDialog)
	EVT_CLOSE(ConfigDialog::OnClose)
	EVT_BUTTON(ID_CLOSE, ConfigDialog::CloseClick)
	EVT_BUTTON(ID_ABOUTOGL, ConfigDialog::AboutClick)
	EVT_CHECKBOX(ID_SIDEWAYSDPAD, ConfigDialog::GeneralSettingsChanged)
	EVT_CHECKBOX(ID_WIDESCREEN, ConfigDialog::GeneralSettingsChanged)
END_EVENT_TABLE()

ConfigDialog::ConfigDialog(wxWindow *parent, wxWindowID id, const wxString &title, const wxPoint &position, const wxSize& size, long style)
: wxDialog(parent, id, title, position, size, style)
{
	g_Config.Load();
	CreateGUIControls();
}

ConfigDialog::~ConfigDialog()
{
}

void ConfigDialog::CreateGUIControls()
{
	// Notebook
	m_Notebook = new wxNotebook(this, ID_NOTEBOOK, wxDefaultPosition, wxDefaultSize);
	m_PageGeneral = new wxPanel(m_Notebook, ID_PAGEGENERAL, wxDefaultPosition, wxDefaultSize);
	m_Notebook->AddPage(m_PageGeneral, wxT("General"));

	// Buttons
	//m_About = new wxButton(this, ID_ABOUTOGL, wxT("About"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	m_Close = new wxButton(this, ID_CLOSE, wxT("Close"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);

	// Put notebook and buttons in sizers
	wxBoxSizer* sButtons;
	sButtons = new wxBoxSizer(wxHORIZONTAL);
	//sButtons->Add(m_About, 0, wxALL, 5); // there is no about
	sButtons->AddStretchSpacer();
	sButtons->Add(m_Close, 0, wxALL, 5);

	wxBoxSizer* sMain;
	sMain = new wxBoxSizer(wxVERTICAL);
	sMain->Add(m_Notebook, 1, wxEXPAND|wxALL, 5);
	sMain->Add(sButtons, 0, wxEXPAND, 5);


	// General
	sbBasic = new wxStaticBoxSizer(wxVERTICAL, m_PageGeneral, wxT("Basic Settings"));
	m_SidewaysDPad = new wxCheckBox(m_PageGeneral, ID_SIDEWAYSDPAD, wxT("Sideways D-Pad"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	m_SidewaysDPad->SetValue(g_Config.bSidewaysDPad);
	m_WideScreen = new wxCheckBox(m_PageGeneral, ID_WIDESCREEN, wxT("WideScreen Mode (for correct aiming)"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	m_WideScreen->SetValue(g_Config.bWideScreen);


	// ----------------------------------------------------------------------
	// Set up sGeneral and sBasic
	// Usage: The wxGBPosition() must have a column and row
	// ----------------
	sGeneral = new wxBoxSizer(wxVERTICAL);
	sBasic = new wxGridBagSizer(0, 0);
	sBasic->Add(m_SidewaysDPad, wxGBPosition(0, 0), wxGBSpan(1, 2), wxALL, 5);
	sBasic->Add(m_WideScreen, wxGBPosition(1, 0), wxGBSpan(1, 2), wxALL, 5);
	sbBasic->Add(sBasic);
	sGeneral->Add(sbBasic, 0, wxEXPAND|wxALL, 5);

	m_PageGeneral->SetSizer(sGeneral);
	sGeneral->Layout();

	this->SetSizer(sMain);
	this->Layout();
	// ----------------


	Fit();
	Center();
}

void ConfigDialog::OnClose(wxCloseEvent& WXUNUSED (event))
{
	g_Config.Save();
	EndModal(0);
}

void ConfigDialog::CloseClick(wxCommandEvent& WXUNUSED (event))
{
	Close();
}

void ConfigDialog::AboutClick(wxCommandEvent& WXUNUSED (event))
{

}

void ConfigDialog::GeneralSettingsChanged(wxCommandEvent& event)
{
	switch (event.GetId())
	{
	case ID_SIDEWAYSDPAD:
		g_Config.bSidewaysDPad = m_SidewaysDPad->IsChecked();
		break;
	case ID_WIDESCREEN:
		g_Config.bWideScreen = m_WideScreen->IsChecked();
		break;
	}
}