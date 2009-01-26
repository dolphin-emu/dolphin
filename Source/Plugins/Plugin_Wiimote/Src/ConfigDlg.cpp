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
#include "main.h"
#include "wiimote_real.h"
/////////////////////////////


/////////////////////////////////////////////////////////////////////////
// Definitions
// ------------

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

	EVT_CHECKBOX(ID_CONNECT_REAL, ConfigDialog::GeneralSettingsChanged)
	EVT_CHECKBOX(ID_USE_REAL, ConfigDialog::GeneralSettingsChanged)

	EVT_TIMER(IDTM_EXIT, ConfigDialog::FlashLights)
	
END_EVENT_TABLE()
/////////////////////////////


ConfigDialog::ConfigDialog(wxWindow *parent, wxWindowID id, const wxString &title,
						   const wxPoint &position, const wxSize& size, long style)
: wxDialog(parent, id, title, position, size, style)
{
	#if wxUSE_TIMER
		m_ExitTimer = new wxTimer(this, IDTM_EXIT);
		// Reset values
		ShutDown = false;
	#endif
	
	g_Config.Load();
	CreateGUIControls();
	UpdateGUI();
}

ConfigDialog::~ConfigDialog()
{
	g_FrameOpen = false;
	if (!g_EmulatorRunning) Shutdown();
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


	////////////////////////////////////////////
	// Put notebook and buttons in sMain
	// ----------------	
	wxBoxSizer* sButtons;
	sButtons = new wxBoxSizer(wxHORIZONTAL);
	//sButtons->Add(m_About, 0, wxALL, 5); // there is no about
	sButtons->AddStretchSpacer();
	sButtons->Add(m_Close, 0, wxALL, 5);

	wxBoxSizer* sMain;
	sMain = new wxBoxSizer(wxVERTICAL);
	sMain->Add(m_Notebook, 1, wxEXPAND|wxALL, 5);
	sMain->Add(sButtons, 0, wxEXPAND, 5);
	/////////////////////////////////


	////////////////////////////////////////////
	// Emulated Wiimote
	// ----------------
	// General
	wxStaticBoxSizer * sEmulatedBasic = new wxStaticBoxSizer(wxVERTICAL, m_PageEmu, wxT("Basic Settings"));
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
	wxBoxSizer * sEmulatedMain = new wxBoxSizer(wxVERTICAL);
	wxGridBagSizer * GbsBasic = new wxGridBagSizer(0, 0);
	GbsBasic->Add(m_SidewaysDPad, wxGBPosition(0, 0), wxGBSpan(1, 2), wxALL, 5);
	GbsBasic->Add(m_WideScreen, wxGBPosition(1, 0), wxGBSpan(1, 2), wxALL, 5);
	GbsBasic->Add(m_NunchuckConnected, wxGBPosition(2, 0), wxGBSpan(1, 2), wxALL, 5);
	GbsBasic->Add(m_ClassicControllerConnected, wxGBPosition(3, 0), wxGBSpan(1, 2), wxALL, 5);
	sEmulatedBasic->Add(GbsBasic);
	sEmulatedMain->Add(sEmulatedBasic, 0, wxEXPAND | (wxALL), 5);
	/////////////////////////////////


	////////////////////////////////////////////
	// Real Wiimote
	// ----------------
	// General
	wxStaticBoxSizer * sbRealBasic = new wxStaticBoxSizer(wxVERTICAL, m_PageReal, wxT("Basic Settings"));
	m_ConnectRealWiimote = new wxCheckBox(m_PageReal, ID_CONNECT_REAL, wxT("Connect real Wiimote"));
	m_UseRealWiimote = new wxCheckBox(m_PageReal, ID_USE_REAL, wxT("Use real Wiimote"));
	m_ConnectRealWiimote->SetToolTip(wxT("Connected to the real wiimote"));
	m_UseRealWiimote->SetToolTip(wxT("Use the real Wiimote in the game"));
	m_ConnectRealWiimote->SetValue(g_Config.bConnectRealWiimote);
	m_UseRealWiimote->SetValue(g_Config.bUseRealWiimote);

	// ----------------------------------------------------------------------
	// Set up sizers
	// ----------------
	wxBoxSizer * sRealMain = new wxBoxSizer(wxVERTICAL);
	sRealMain->Add(sbRealBasic, 0, wxEXPAND | (wxALL), 5);
	sbRealBasic->Add(m_ConnectRealWiimote, 0, wxEXPAND | (wxALL), 5);
	sbRealBasic->Add(m_UseRealWiimote, 0, wxEXPAND | (wxALL), 5);
	/////////////////////////////////


	////////////////////////////////////////////
	// Set sizers and layout
	// ----------------
	m_PageEmu->SetSizer(sEmulatedMain);
	m_PageReal->SetSizer(sRealMain);
	this->SetSizer(sMain);

	//sEmulatedMain->Layout();
	this->Layout();

	Fit();
	Center();
	/////////////////////////////////
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
/////////////////////////////////


/////////////////////////////////////////////////////////////////////////
/* Flash lights and rumble (for Connect and Disconnect) in its own thread like this
   to avoid a delay when the Connect checkbox is pressed (that would occur if we use
   Sleep() instead). */
// ------------
void ConfigDialog::StartTimer()
{
	TimerCounter = 0;

	// Start the constant timer
	int TimesPerSecond = 10;
	m_ExitTimer->Start( floor((double)(1000 / TimesPerSecond)) );

	// Run it immedeately for the first time
	DoFlashLights();
}

void ConfigDialog::DoFlashLights()
{
	TimerCounter++;

	if(TimerCounter == 1) wiiuse_rumble(WiiMoteReal::g_WiiMotesFromWiiUse[0], 1);

	if(TimerCounter == 1)
	{
		wiiuse_set_leds(WiiMoteReal::g_WiiMotesFromWiiUse[0], WIIMOTE_LED_1);
		wiiuse_set_leds(WiiMoteReal::g_WiiMotesFromWiiUse[0], WIIMOTE_LED_2);
		wiiuse_set_leds(WiiMoteReal::g_WiiMotesFromWiiUse[0], WIIMOTE_LED_3);
		wiiuse_set_leds(WiiMoteReal::g_WiiMotesFromWiiUse[0], WIIMOTE_LED_4);
	}

	// Make the rumble period equal on both Init and Shutdown
	if (TimerCounter == 1 && ShutDown) TimerCounter++;

	if (TimerCounter >= 3 || TimerCounter <= 5)
	{
		wiiuse_rumble(WiiMoteReal::g_WiiMotesFromWiiUse[0], 0);		
	}

	if(TimerCounter == 3)
	{
		if(ShutDown)
		{
			// Set led 4
			wiiuse_set_leds(WiiMoteReal::g_WiiMotesFromWiiUse[0], WIIMOTE_LED_NONE);
			wiiuse_set_leds(WiiMoteReal::g_WiiMotesFromWiiUse[0], WIIMOTE_LED_4);

			// Clean up wiiuse
			wiiuse_cleanup(WiiMoteReal::g_WiiMotesFromWiiUse, WiiMoteReal::g_NumberOfWiiMotes);

			ShutDown = false;
		}
		else
		{
			wiiuse_set_leds(WiiMoteReal::g_WiiMotesFromWiiUse[0], WIIMOTE_LED_NONE);
			wiiuse_set_leds(WiiMoteReal::g_WiiMotesFromWiiUse[0], WIIMOTE_LED_1);
		}

		// Stop timer
		m_ExitTimer->Stop();
	}

	Console::Print("TimerCounter == %i\n", TimerCounter);
}
/////////////////////////////////


// ===================================================
/* Do use real wiimote */
// ----------------
void ConfigDialog::DoConnectReal()
{
	g_Config.bConnectRealWiimote = m_ConnectRealWiimote->IsChecked();

	if(g_Config.bConnectRealWiimote)
	{
		if (!g_RealWiiMoteInitialized) WiiMoteReal::Initialize();
	}
	else
	{
		if (g_RealWiiMoteInitialized) WiiMoteReal::Shutdown();
	}
}


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
/* Change settings */
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


		//////////////////////////
		// Real Wiimote
		// -----------
	case ID_CONNECT_REAL:
		DoConnectReal();
		break;

	case ID_USE_REAL:
		g_Config.bUseRealWiimote = m_UseRealWiimote->IsChecked();
		break;
		/////////////////
	}
	g_Config.Save();
	UpdateGUI();
}



// =======================================================
// Update the enabled/disabled status
// -------------
void ConfigDialog::UpdateGUI()
{
	/* I have disabled this option during a running game because it's enough to be able to switch
	   between using and not using then. To also use the connect option during a running game would
	   mean that the wiimote must be sent the current reporting mode and the channel ID after it
	   has been initialized. If you know how to set that manually please feel free to make functions
	   for that so that this option can be enabled during gameplay. */
	m_ConnectRealWiimote->Enable(!g_EmulatorRunning);
	m_UseRealWiimote->Enable(g_RealWiiMotePresent && g_Config.bConnectRealWiimote);
	Console::Print("Present: %i, Connect: %i\n", g_RealWiiMotePresent, g_Config.bConnectRealWiimote);
}
