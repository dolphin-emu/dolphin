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


/////////////////////////////////////////////////////////////////////////
// Include
// ------------
//#include "Common.h" // for u16
#include "ConfigDlg.h"
#include "Config.h"
#include "EmuSubroutines.h" // for WmRequestStatus
/////////////////////////////

/////////////////////////////////////////////////////////////////////////
// Defines
// ------------
#ifndef _WIN32
#define Sleep(x) usleep(x*1000)
#endif
/////////////////////////

/////////////////////////////////////////////////////////////////////////
// Event table
// ------------
BEGIN_EVENT_TABLE(ConfigDialog,wxDialog)
	EVT_CLOSE(ConfigDialog::OnClose)
	EVT_BUTTON(ID_CLOSE, ConfigDialog::CloseClick)
	EVT_BUTTON(ID_ABOUTOGL, ConfigDialog::AboutClick)
	EVT_CHECKBOX(ID_SIDEWAYSDPAD, ConfigDialog::GeneralSettingsChanged)
	EVT_CHECKBOX(ID_WIDESCREEN, ConfigDialog::GeneralSettingsChanged)
	EVT_CHECKBOX(ID_NUNCHUCKCONNECTED, ConfigDialog::GeneralSettingsChanged)	
	EVT_CHECKBOX(ID_CLASSICCONTROLLERCONNECTED, ConfigDialog::GeneralSettingsChanged)
END_EVENT_TABLE()
/////////////////////////////


ConfigDialog::ConfigDialog(wxWindow *parent, wxWindowID id, const wxString &title, const wxPoint &position, const wxSize& size, long style)
: wxDialog(parent, id, title, position, size, style)
{
	g_Config.Load();
	CreateGUIControls();
}

ConfigDialog::~ConfigDialog()
{
}


/////////////////////////////////////////////////////////////////////////
// Create GUI
// ------------
void ConfigDialog::CreateGUIControls()
{
	// Notebook
	m_Notebook = new wxNotebook(this, ID_NOTEBOOK, wxDefaultPosition, wxDefaultSize);
	m_PageEmu = new wxPanel(m_Notebook, ID_PAGEEMU, wxDefaultPosition, wxDefaultSize);
	m_PageReal = new wxPanel(m_Notebook, ID_PAGEREAL, wxDefaultPosition, wxDefaultSize);
	m_Notebook->AddPage(m_PageEmu, wxT("Emulated Wiimote"));
	m_Notebook->AddPage(m_PageReal, wxT("Real Wiimote"));

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
	sbBasic = new wxStaticBoxSizer(wxVERTICAL, m_PageEmu, wxT("Basic Settings"));
	m_SidewaysDPad = new wxCheckBox(m_PageEmu, ID_SIDEWAYSDPAD, wxT("Sideways D-Pad"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	m_SidewaysDPad->SetValue(g_Config.bSidewaysDPad);
	m_WideScreen = new wxCheckBox(m_PageEmu, ID_WIDESCREEN, wxT("WideScreen Mode (for correct aiming)"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	m_WideScreen->SetValue(g_Config.bWideScreen);
	m_NunchuckConnected = new wxCheckBox(m_PageEmu, ID_NUNCHUCKCONNECTED, wxT("Nunchuck connected"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	m_NunchuckConnected->SetValue(g_Config.bNunchuckConnected);
	m_ClassicControllerConnected = new wxCheckBox(m_PageEmu, ID_CLASSICCONTROLLERCONNECTED, wxT("Classic Controller connected"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	m_ClassicControllerConnected->SetValue(g_Config.bClassicControllerConnected);


	// ----------------------------------------------------------------------
	// Set up sGeneral and sBasic
	// Usage: The wxGBPosition() must have a column and row
	// ----------------
	sGeneral = new wxBoxSizer(wxVERTICAL);
	sBasic = new wxGridBagSizer(0, 0);
	sBasic->Add(m_SidewaysDPad, wxGBPosition(0, 0), wxGBSpan(1, 2), wxALL, 5);
	sBasic->Add(m_WideScreen, wxGBPosition(1, 0), wxGBSpan(1, 2), wxALL, 5);
	sBasic->Add(m_NunchuckConnected, wxGBPosition(2, 0), wxGBSpan(1, 2), wxALL, 5);
	sBasic->Add(m_ClassicControllerConnected, wxGBPosition(3, 0), wxGBSpan(1, 2), wxALL, 5);
	sbBasic->Add(sBasic);
	sGeneral->Add(sbBasic, 0, wxEXPAND|wxALL, 5);

	m_PageEmu->SetSizer(sGeneral);
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
//////////////////////////


// ===================================================
/* Generate connect/disconnect status event */
// ----------------
void ConfigDialog::DoExtensionConnectedDisconnected()
{
	u8 DataFrame[8]; // make a blank report for it
	wm_request_status *rs = (wm_request_status*)DataFrame;

	// Check if a game is running, in that case change the status
	if(WiiMoteEmu::g_ReportingChannel > 0)
		WiiMoteEmu::WmRequestStatus(WiiMoteEmu::g_ReportingChannel, rs);
}


// ===================================================
/* Change general Emulated Wii Remote settings */
// ----------------
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

	case ID_NUNCHUCKCONNECTED:
		// Don't allow two extensions at the same time
		if(m_ClassicControllerConnected->IsChecked())
		{
			m_ClassicControllerConnected->SetValue(false);
			g_Config.bClassicControllerConnected = false;
			// Disconnect the extension so that the game recognize the change
			DoExtensionConnectedDisconnected();
			/* It doesn't seem to be needed but shouldn't it at least take 25 ms to
			   reconnect an extension after we disconnected another? */
			Sleep(25);
		}

		// Update status
		g_Config.bNunchuckConnected = m_NunchuckConnected->IsChecked();

		// Generate connect/disconnect status event
		memcpy(WiiMoteEmu::g_RegExt + 0x20, WiiMoteEmu::nunchuck_calibration,
			sizeof(WiiMoteEmu::nunchuck_calibration));
		memcpy(WiiMoteEmu::g_RegExt + 0xfa, WiiMoteEmu::nunchuck_id, sizeof(WiiMoteEmu::nunchuck_id));
		DoExtensionConnectedDisconnected();
		break;

	case ID_CLASSICCONTROLLERCONNECTED:
		// Don't allow two extensions at the same time
		if(m_NunchuckConnected->IsChecked())
		{
			m_NunchuckConnected->SetValue(false);
			g_Config.bNunchuckConnected = false;
			// Disconnect the extension so that the game recognize the change
			DoExtensionConnectedDisconnected();
		}

		g_Config.bClassicControllerConnected = m_ClassicControllerConnected->IsChecked();

		// Generate connect/disconnect status event
		memcpy(WiiMoteEmu::g_RegExt + 0x20, WiiMoteEmu::classic_calibration,
			sizeof(WiiMoteEmu::classic_calibration));
		memcpy(WiiMoteEmu::g_RegExt + 0xfa, WiiMoteEmu::classic_id, sizeof(WiiMoteEmu::classic_id));
		DoExtensionConnectedDisconnected();
		break;
	}
}
