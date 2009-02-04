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


//////////////////////////////////////////////////////////////////////////////////////////
// Includes
// ¯¯¯¯¯¯¯¯¯¯¯¯¯
//#include "Common.h" // for u16
#include "CommonTypes.h" // for u16
#include "IniFile.h"
#include "Timer.h"

#include "wiimote_real.h" // Local
#include "wiimote_hid.h"
#include "main.h"
#include "ConfigDlg.h"
#include "Config.h"
#include "EmuMain.h" // for LoadRecordedMovements()
#include "EmuSubroutines.h" // for WmRequestStatus
//////////////////////////////////////


//////////////////////////////////////////////////////////////////////////////////////////
// Event table
// ¯¯¯¯¯¯¯¯¯¯¯¯¯
BEGIN_EVENT_TABLE(ConfigDialog,wxDialog)
	EVT_CLOSE(ConfigDialog::OnClose)
	EVT_BUTTON(ID_CLOSE, ConfigDialog::CloseClick)
	EVT_BUTTON(ID_APPLY, ConfigDialog::CloseClick)
	EVT_BUTTON(ID_ABOUTOGL, ConfigDialog::AboutClick)

	EVT_CHECKBOX(ID_SIDEWAYSDPAD, ConfigDialog::GeneralSettingsChanged)
	EVT_CHECKBOX(ID_WIDESCREEN, ConfigDialog::GeneralSettingsChanged)
	EVT_CHECKBOX(ID_NUNCHUCKCONNECTED, ConfigDialog::GeneralSettingsChanged)	
	EVT_CHECKBOX(ID_CLASSICCONTROLLERCONNECTED, ConfigDialog::GeneralSettingsChanged)

	EVT_CHECKBOX(ID_CONNECT_REAL, ConfigDialog::GeneralSettingsChanged)
	EVT_CHECKBOX(ID_USE_REAL, ConfigDialog::GeneralSettingsChanged)
	EVT_CHECKBOX(ID_UPDATE_REAL, ConfigDialog::GeneralSettingsChanged)
	EVT_CHOICE(ID_NEUTRAL_CHOICE, ConfigDialog::GeneralSettingsChanged)	

	EVT_CHOICE(IDC_RECORD + 1, ConfigDialog::GeneralSettingsChanged)
	EVT_CHOICE(IDC_RECORD + 2, ConfigDialog::GeneralSettingsChanged)
	EVT_CHOICE(IDC_RECORD + 3, ConfigDialog::GeneralSettingsChanged)
	EVT_CHOICE(IDC_RECORD + 4, ConfigDialog::GeneralSettingsChanged)
	EVT_CHOICE(IDC_RECORD + 5, ConfigDialog::GeneralSettingsChanged)
	EVT_CHOICE(IDC_RECORD + 6, ConfigDialog::GeneralSettingsChanged)
	EVT_CHOICE(IDC_RECORD + 7, ConfigDialog::GeneralSettingsChanged)
	EVT_CHOICE(IDC_RECORD + 8, ConfigDialog::GeneralSettingsChanged)
	EVT_CHOICE(IDC_RECORD + 9, ConfigDialog::GeneralSettingsChanged)
	EVT_CHOICE(IDC_RECORD + 10, ConfigDialog::GeneralSettingsChanged)
	EVT_CHOICE(IDC_RECORD + 11, ConfigDialog::GeneralSettingsChanged)
	EVT_CHOICE(IDC_RECORD + 12, ConfigDialog::GeneralSettingsChanged)
	EVT_CHOICE(IDC_RECORD + 13, ConfigDialog::GeneralSettingsChanged)
	EVT_CHOICE(IDC_RECORD + 14, ConfigDialog::GeneralSettingsChanged)
	EVT_CHOICE(IDC_RECORD + 15, ConfigDialog::GeneralSettingsChanged)

	EVT_BUTTON(IDB_RECORD + 1, ConfigDialog::RecordMovement)
	EVT_BUTTON(IDB_RECORD + 2, ConfigDialog::RecordMovement)
	EVT_BUTTON(IDB_RECORD + 3, ConfigDialog::RecordMovement)
	EVT_BUTTON(IDB_RECORD + 4, ConfigDialog::RecordMovement)
	EVT_BUTTON(IDB_RECORD + 5, ConfigDialog::RecordMovement)
	EVT_BUTTON(IDB_RECORD + 6, ConfigDialog::RecordMovement)
	EVT_BUTTON(IDB_RECORD + 7, ConfigDialog::RecordMovement)
	EVT_BUTTON(IDB_RECORD + 8, ConfigDialog::RecordMovement)
	EVT_BUTTON(IDB_RECORD + 9, ConfigDialog::RecordMovement)
	EVT_BUTTON(IDB_RECORD + 10, ConfigDialog::RecordMovement)
	EVT_BUTTON(IDB_RECORD + 11, ConfigDialog::RecordMovement)
	EVT_BUTTON(IDB_RECORD + 12, ConfigDialog::RecordMovement)
	EVT_BUTTON(IDB_RECORD + 13, ConfigDialog::RecordMovement)
	EVT_BUTTON(IDB_RECORD + 14, ConfigDialog::RecordMovement)
	EVT_BUTTON(IDB_RECORD + 15, ConfigDialog::RecordMovement)

	EVT_TIMER(IDTM_UPDATE, ConfigDialog::Update)
	EVT_TIMER(IDTM_SHUTDOWN, ConfigDialog::ShutDown)
END_EVENT_TABLE()

//////////////////////////////////////////////////////////////////////////////////////////
// Class
// ¯¯¯¯¯¯¯¯¯¯¯¯¯
ConfigDialog::ConfigDialog(wxWindow *parent, wxWindowID id, const wxString &title,
						   const wxPoint &position, const wxSize& size, long style)
: wxDialog(parent, id, title, position, size, style)
{
	#if wxUSE_TIMER
		m_TimeoutTimer = new wxTimer(this, IDTM_UPDATE);
		m_ShutDownTimer = new wxTimer(this, IDTM_SHUTDOWN);
		m_TimeoutATimer = new wxTimer(this, IDTM_UPDATEA);
		// Reset values
		m_bWaitForRecording = false;
		m_bRecording = false;
	#endif

	ControlsCreated = false;
	Page = 0;
	m_vRecording.resize(RECORDING_ROWS + 1);

	g_Config.Load();
	CreateGUIControls();
	LoadFile();
	UpdateGUI();

	wxTheApp->Connect(wxID_ANY, wxEVT_KEY_DOWN, // Keyboard
		wxKeyEventHandler(ConfigDialog::OnKeyDown),
		(wxObject*)0, this);
}

ConfigDialog::~ConfigDialog()
{	
}

void ConfigDialog::OnKeyDown(wxKeyEvent& event)
{
	event.Skip();

	// Escape a recording event
	if (event.GetKeyCode() == WXK_ESCAPE)
	{
		m_bWaitForRecording = false;
		m_bRecording = false;
		UpdateGUI();
	}
}

void ConfigDialog::OnClose(wxCloseEvent& WXUNUSED (event))
{
	g_FrameOpen = false;
	SaveFile();
	g_Config.Save();	
	//SuccessAlert("Saved\n");
	if (!g_EmulatorRunning) Shutdown();
	EndModal(0);
}

/* Timeout the shutdown. In Windows at least the g_pReadThread execution will hang at any attempt to
   call a frame function after the main thread has entered WaitForSingleObject() or any other loop.
   We must therefore shut down the thread from here and wait for that before we can call ShutDown(). */
void ConfigDialog::ShutDown(wxTimerEvent& WXUNUSED(event))
{
	// Close() is a wxWidgets function that will trigger EVT_CLOSE() and then call this->Destroy().
	if(!WiiMoteReal::g_ThreadGoing)
	{
		m_ShutDownTimer->Stop();
		Close();
	}
}

void ConfigDialog::CloseClick(wxCommandEvent& event)
{
	switch(event.GetId())
	{
	case ID_CLOSE:
		if(!g_EmulatorRunning)
		{
			WiiMoteReal::g_Shutdown = true;
			m_ShutDownTimer->Start(10);
		}
		else
		{
			Close();
		}
		break;
	case ID_APPLY:
		SaveFile();
		WiiMoteEmu::LoadRecordedMovements();
		break;
	}
}

void ConfigDialog::AboutClick(wxCommandEvent& WXUNUSED (event))
{
}



//////////////////////////////////////////////////////////////////////////////////////////
// Includes
// ¯¯¯¯¯¯¯¯¯¯¯¯¯
//#include "Common.h" // for u16
#include "CommonTypes.h" // for u16
#include "IniFile.h"
#include "Timer.h"

#include "wiimote_real.h" // Local
#include "wiimote_hid.h"
#include "main.h"
#include "ConfigDlg.h"
#include "Config.h"
#include "EmuMain.h" // for LoadRecordedMovements()
#include "EmuSubroutines.h" // for WmRequestStatus
//////////////////////////////////////



//////////////////////////////////////////////////////////////////////////////////////////
// Variables
// ----------------
// Trigger Type
enum
{
	CTL_TRIGGER_SDL = 0, // 
	CTL_TRIGGER_XINPUT // The XBox 360 pad
};
// Trigger type
static const char* TriggerType[] =
{
	"SDL", // -0x8000 to 0x7fff
	"XInput", // 0x00 to 0xff
};
//////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////
// Bitmap box and dot
// ----------------
wxBitmap ConfigDialog::CreateBitmap()
{
	BoxW = 70, BoxH = 70;
	wxBitmap bitmap(BoxW, BoxH);
	wxMemoryDC dc;
	dc.SelectObject(bitmap);

	// Set outline and fill colors
	//wxBrush LightBlueBrush(_T("#0383f0"));
	//wxPen LightBluePen(_T("#80c5fd"));
	//wxPen LightGrayPen(_T("#909090"));
	wxPen LightBluePen(_T("#7f9db9")); // Windows XP color	
	dc.SetPen(LightBluePen);
	dc.SetBrush(*wxWHITE_BRUSH);

	dc.Clear();
	dc.DrawRectangle(0, 0, BoxW, BoxH);
	dc.SelectObject(wxNullBitmap);
	return bitmap;
}
wxBitmap ConfigDialog::CreateBitmapDot()
{
	int w = 2, h = 2;
	wxBitmap bitmap(w, h);
	wxMemoryDC dc;
	dc.SelectObject(bitmap);

	// Set outline and fill colors
	//wxBrush RedBrush(_T("#0383f0"));	
	//wxPen RedPen(_T("#80c5fd"));
	//wxPen LightGrayPen(_T("#909090"));
	dc.SetPen(*wxRED_PEN);
	dc.SetBrush(*wxRED_BRUSH);

	dc.Clear();
	dc.DrawRectangle(0, 0, w, h);
	dc.SelectObject(wxNullBitmap);
	return bitmap;
}
//////////////////////////////////////


void ConfigDialog::CreateGUIControls()
{

	////////////////////////////////////////////////////////////////////////////////
	// Notebook
	// ----------------	
	m_Notebook = new wxNotebook(this, ID_NOTEBOOK, wxDefaultPosition, wxDefaultSize);

	// Controller pages
	m_Controller[0] = new wxPanel(m_Notebook, ID_CONTROLLERPAGE1, wxDefaultPosition, wxDefaultSize);
	m_Controller[1] = new wxPanel(m_Notebook, ID_CONTROLLERPAGE2, wxDefaultPosition, wxDefaultSize);
	m_Controller[2] = new wxPanel(m_Notebook, ID_CONTROLLERPAGE3, wxDefaultPosition, wxDefaultSize);
	m_Controller[3] = new wxPanel(m_Notebook, ID_CONTROLLERPAGE4, wxDefaultPosition, wxDefaultSize);
	m_PageRecording = new wxPanel(m_Notebook, ID_PAGE_RECORDING, wxDefaultPosition, wxDefaultSize);

	m_Notebook->AddPage(m_Controller[0], wxT("Controller 1"));
	m_Notebook->AddPage(m_Controller[1], wxT("Controller 2"));
	m_Notebook->AddPage(m_Controller[2], wxT("Controller 3"));
	m_Notebook->AddPage(m_Controller[3], wxT("Controller 4"));
	m_Notebook->AddPage(m_PageRecording, wxT("Recording"));
	///////////////////////////////


	////////////////////////////////////////////////////////////////////////////////
	// Text lists
	// ----------------	

	// Search for devices and add them to the device list
	wxArrayString StrJoyname; // The string array
	int NumGoodPads = 0;
	if(NumGoodPads > 0)
	{
		//for(int x = 0; x < joyinfo.size(); x++)
		//	arrayStringFor_Joyname.Add(wxString::FromAscii(joyinfo[x].Name.c_str()));
	}
	else
	{
		StrJoyname.Add(wxString::FromAscii("No Joystick detected"));
	}

	// The tilt list
	wxArrayString StrTilt;
	StrTilt.Add(wxString::FromAscii("<Off>"));
	StrTilt.Add(wxString::FromAscii("Analog stick"));
	StrTilt.Add(wxString::FromAscii("Triggers"));
	StrTilt.Add(wxString::FromAscii("Keyboard"));
	wxArrayString StrTiltRange;
	for (int i = 3; i < 15; i++) StrTiltRange.Add(wxString::Format(wxT("%i"), i*5));


	// The Trigger type list
	wxArrayString wxAS_TriggerType;
	wxAS_TriggerType.Add(wxString::FromAscii(TriggerType[CTL_TRIGGER_SDL]));
	wxAS_TriggerType.Add(wxString::FromAscii(TriggerType[CTL_TRIGGER_XINPUT]));
	///////////////////////////////////////


	/* Populate all four pages. Page 2, 3 and 4 are currently disabled since we can't use more than one
	   Wiimote at the moment */
	for (int i = 0; i < 4; i++)
	{
		////////////////////////////////////////////////////
		// General and basic Settings
		// ----------------

		// Basic Settings
		m_SidewaysDPad[i] = new wxCheckBox(m_Controller[i], ID_SIDEWAYSDPAD, wxT("Sideways D-Pad"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
		m_WideScreen[i] = new wxCheckBox(m_Controller[i], ID_WIDESCREEN, wxT("WideScreen Mode (for correct aiming)"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
		m_ConnectRealWiimote[i] = new wxCheckBox(m_Controller[i], ID_CONNECT_REAL, wxT("Connect Real Wiimote"));
		m_UseRealWiimote[i] = new wxCheckBox(m_Controller[i], ID_USE_REAL, wxT("Use Real Wiimote"));

		// Extensions
		m_NunchuckConnected[i] = new wxCheckBox(m_Controller[i], ID_NUNCHUCKCONNECTED, wxT("Nunchuck Connected"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
		m_ClassicControllerConnected[i] = new wxCheckBox(m_Controller[i], ID_CLASSICCONTROLLERCONNECTED, wxT("Classic Controller Connected"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);

		// Default values
		m_NunchuckConnected[i]->SetValue(g_Config.bNunchuckConnected);
		m_ClassicControllerConnected[i]->SetValue(g_Config.bClassicControllerConnected);

		// Sizers
		m_SizeBasic[i] = new wxStaticBoxSizer(wxVERTICAL, m_Controller[i], wxT("Basic Settings"));
		m_SizeExtensions[i] = new wxStaticBoxSizer(wxVERTICAL, m_Controller[i], wxT("Extension"));

		m_SizePadding[i] = new wxBoxSizer(wxVERTICAL); m_SizeBasic[i]->Add(m_SizePadding[i], 0, (wxALL), 5);
		m_SizePadding[i]->Add(m_ConnectRealWiimote[i], 0, (wxUP), 2);
		m_SizePadding[i]->Add(m_UseRealWiimote[i], 0, (wxUP), 2);
		m_SizePadding[i]->Add(m_SidewaysDPad[i], 0, (wxUP), 2);
		m_SizePadding[i]->Add(m_WideScreen[i], 0, (wxUP), 2);

		m_SizeExtensionsPadding[i] = new wxBoxSizer(wxVERTICAL); m_SizeExtensions[i]->Add(m_SizeExtensionsPadding[i], 0, (wxALL), 5);
		m_SizeExtensionsPadding[i]->Add(m_NunchuckConnected[i], 0, (wxUP), 2);
		m_SizeExtensionsPadding[i]->Add(m_ClassicControllerConnected[i], 0, (wxUP), 2);		

		m_SizeBasicGeneral[i] = new wxBoxSizer(wxHORIZONTAL);
		m_SizeBasicGeneral[i]->Add(m_SizeBasic[i], 0, (wxUP), 0);
		m_SizeBasicGeneral[i]->Add(m_SizeExtensions[i], 0, (wxLEFT), 5);

		// Default values
		m_SidewaysDPad[i]->SetValue(g_Config.bSidewaysDPad);
		m_WideScreen[i]->SetValue(g_Config.bWideScreen);	
		m_ConnectRealWiimote[i]->SetValue(g_Config.bConnectRealWiimote);
		m_UseRealWiimote[i]->SetValue(g_Config.bUseRealWiimote);
	
		// Tooltips
		m_ConnectRealWiimote[i]->SetToolTip(wxT("Connected to the real wiimote. This can not be changed during gameplay."));
		m_UseRealWiimote[i]->SetToolTip(wxT(
			"Use the real Wiimote in the game. This can be changed during gameplay. This can not be selected"
			" when a recording is to be done. No status in this window will be updated when this is checked."));
		///////////////////////////


		////////////////////////////////////////////////////////////////////////
		// Gamepad input
		// ----------------

		// --------------------------------------------------------------------
		// Controller
		// -----------------------------
		/**/
		// Controls
		m_Joyname[i] = new wxComboBox(m_Controller[i], IDC_JOYNAME, StrJoyname[0], wxDefaultPosition, wxSize(476, 21), StrJoyname, wxCB_READONLY);
		m_Joyattach[i] = new wxCheckBox(m_Controller[i], IDC_JOYATTACH, wxT("Controller attached"), wxDefaultPosition, wxSize(109, 25));

		m_gJoyname[i] = new wxStaticBoxSizer (wxHORIZONTAL, m_Controller[i], wxT("Controller:"));
		m_gJoyname[i]->Add(m_Joyname[i], 0, (wxLEFT | wxRIGHT), 5);
		m_gJoyname[i]->Add(m_Joyattach[i], 0, (wxRIGHT | wxLEFT | wxBOTTOM), 1);

		// Tooltips
		m_Joyattach[i]->SetToolTip(wxString::Format(wxT("Decide if Controller %i shall be detected by the game."), 1));
		m_Joyname[i]->SetToolTip(wxT("Save your settings and configure another joypad"));
		

		// --------------------------------------------------------------------
		// Analog sticks
		// -----------------------------
		/**/
		m_pInStatus[i] = new wxPanel(m_Controller[i], wxID_ANY, wxDefaultPosition, wxDefaultSize);
		m_bmpSquare[i] = new wxStaticBitmap(m_pInStatus[i], wxID_ANY, CreateBitmap(), wxDefaultPosition, wxDefaultSize);
		m_bmpDot[i] = new wxStaticBitmap(m_pInStatus[i], wxID_ANY, CreateBitmapDot(), wxPoint(BoxW / 2, BoxH / 2), wxDefaultSize);

		m_pRightStatus[i] = new wxPanel(m_Controller[i], wxID_ANY, wxDefaultPosition, wxDefaultSize);
		m_bmpSquareRight[i] = new wxStaticBitmap(m_pRightStatus[i], wxID_ANY, CreateBitmap(), wxDefaultPosition, wxDefaultSize);
		m_bmpDotRight[i] = new wxStaticBitmap(m_pRightStatus[i], wxID_ANY, CreateBitmapDot(), wxPoint(BoxW / 2, BoxH / 2), wxDefaultSize);
		
		m_gAnalogLeft[i] = new wxStaticBoxSizer (wxHORIZONTAL, m_Controller[i], wxT("Analog 1"));
		m_gAnalogLeft[i]->Add(m_pInStatus[i], 0, (wxLEFT | wxRIGHT), 5);

		m_gAnalogRight[i] = new wxStaticBoxSizer (wxHORIZONTAL, m_Controller[i], wxT("Analog 2"));
		m_gAnalogRight[i]->Add(m_pRightStatus[i], 0, (wxLEFT | wxRIGHT), 5);

		// --------------------------------------------------------------------
		// Tilt Wiimote
		// -----------------------------
		/**/
		// Controls
		m_TiltCombo[i] = new wxComboBox(m_Controller[i], ID_TILT_COMBO, StrTilt[0], wxDefaultPosition, wxDefaultSize, StrTilt, wxCB_READONLY);
		m_TiltComboRange[i] = new wxComboBox(m_Controller[i], wxID_ANY, StrTiltRange[0], wxDefaultPosition, wxDefaultSize, StrTiltRange, wxCB_READONLY);
		m_TiltText[i] = new wxStaticText(m_Controller[i], wxID_ANY, wxT("Range"));

		m_TiltHoriz[i] = new wxBoxSizer(wxHORIZONTAL);
		m_TiltHoriz[i]->Add(m_TiltText[i], 0, (wxLEFT | wxTOP), 4);
		m_TiltHoriz[i]->Add(m_TiltComboRange[i], 0, (wxLEFT | wxRIGHT), 5);
		

		m_gTilt[i] = new wxStaticBoxSizer (wxVERTICAL, m_Controller[i], wxT("Tilt Wiimote"));
		m_gTilt[i]->Add(m_TiltCombo[i], 0, (wxLEFT | wxRIGHT | wxDOWN), 5);
		m_gTilt[i]->Add(m_TiltHoriz[i], 0, (wxLEFT | wxRIGHT), 5);		

		// Tooltips
		m_TiltCombo[i]->SetToolTip(wxT("Control tilting by an analog gamepad stick, an analog trigger or the keyboard."));		

		// Sizers
		m_HorizControllers[i] = new wxBoxSizer(wxHORIZONTAL);
		m_HorizControllers[i]->Add(m_gAnalogLeft[i]);
		m_HorizControllers[i]->Add(m_gAnalogRight[i], 0, (wxLEFT), 5);	
		m_HorizControllers[i]->Add(m_gTilt[i], 0, (wxLEFT), 5);
		///////////////////////////


		////////////////////////////////////////////////////////////////
		// Set up sizers and layout
		// Usage: The wxGBPosition() must have a column and row
		// ----------------
		m_sMain[i] = new wxBoxSizer(wxVERTICAL);
		m_sMain[i]->Add(m_SizeBasicGeneral[i], 0, wxEXPAND | (wxALL), 5);
		m_sMain[i]->Add(m_gJoyname[i], 0, wxEXPAND | (wxLEFT | wxRIGHT | wxDOWN), 5);
		m_sMain[i]->Add(m_HorizControllers[i], 0, wxEXPAND | (wxLEFT | wxRIGHT | wxDOWN), 5);

		m_Controller[i]->SetSizer(m_sMain[i]); // Set the main sizer
		/////////////////////////////////
	}

	////////////////////////////////////////////
	// Movement recording
	// ----------------
	CreateGUIControlsRecording();
	/////////////////////////////////


	////////////////////////////////////////////////////////////////////////////////
	// Buttons
	//m_About = new wxButton(this, ID_ABOUTOGL, wxT("About"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	m_Apply = new wxButton(this, ID_APPLY, wxT("Apply"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	m_Close = new wxButton(this, ID_CLOSE, wxT("Close"));
	m_Close->SetToolTip(wxT("Apply and Close"));

	wxBoxSizer* sButtons = new wxBoxSizer(wxHORIZONTAL);
	//sButtons->Add(m_About, 0, wxALL, 5); // there is no about
	sButtons->AddStretchSpacer();
	sButtons->Add(m_Apply, 0, (wxALL), 0);
	sButtons->Add(m_Close, 0, (wxLEFT), 5);	
	///////////////////////////////


	////////////////////////////////////////////
	// Set sizers and layout
	// ----------------
	m_MainSizer = new wxBoxSizer(wxVERTICAL);
	m_MainSizer->Add(m_Notebook, 1, wxEXPAND | wxALL, 5);
	m_MainSizer->Add(sButtons, 0, wxEXPAND | (wxLEFT | wxRIGHT | wxDOWN), 5);

	m_Controller[1]->Enable(false);
	m_Controller[2]->Enable(false);
	m_Controller[3]->Enable(false);

	this->SetSizer(m_MainSizer);
	this->Layout();

	Fit();
	Center();

	ControlsCreated = true;
	/////////////////////////////////
}
/////////////////////////////////



// ===================================================
/* Do use real wiimote */
// ----------------
void ConfigDialog::DoConnectReal()
{
	g_Config.bConnectRealWiimote = m_ConnectRealWiimote[Page]->IsChecked();

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
	// There is no need for this if no game is running
	if(!g_EmulatorRunning) return; 

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
	case ID_CONNECT_REAL:
		DoConnectReal();
		break;
	case ID_USE_REAL:
		g_Config.bUseRealWiimote = m_UseRealWiimote[Page]->IsChecked();
		//if(g_Config.bUseRealWiimote) WiiMoteReal::SetDataReportingMode();
		if(g_Config.bUseRealWiimote) WiiMoteReal::ClearEvents();		
		break;

	case ID_SIDEWAYSDPAD:
		g_Config.bSidewaysDPad = m_SidewaysDPad[Page]->IsChecked();
		break;
	case ID_WIDESCREEN:
		g_Config.bWideScreen = m_WideScreen[Page]->IsChecked();
		break;


	//////////////////////////
	// Extensions
	// -----------
	case ID_NUNCHUCKCONNECTED:
		// Don't allow two extensions at the same time
		if(m_ClassicControllerConnected[Page]->IsChecked())
		{
			m_ClassicControllerConnected[Page]->SetValue(false);
			g_Config.bClassicControllerConnected = false;
			// Disconnect the extension so that the game recognize the change
			DoExtensionConnectedDisconnected();
			/* It doesn't seem to be needed but shouldn't it at least take 25 ms to
			   reconnect an extension after we disconnected another? */
			if(g_EmulatorRunning) sleep(25);
		}

		// Update status
		g_Config.bNunchuckConnected = m_NunchuckConnected[Page]->IsChecked();

		// Copy the calibration data
		memcpy(WiiMoteEmu::g_RegExt + 0x20, WiiMoteEmu::nunchuck_calibration,
			sizeof(WiiMoteEmu::nunchuck_calibration));
		memcpy(WiiMoteEmu::g_RegExt + 0xfa, WiiMoteEmu::nunchuck_id, sizeof(WiiMoteEmu::nunchuck_id));

		// Generate connect/disconnect status event
		DoExtensionConnectedDisconnected();
		break;

	case ID_CLASSICCONTROLLERCONNECTED:
		// Don't allow two extensions at the same time
		if(m_NunchuckConnected[Page]->IsChecked())
		{
			m_NunchuckConnected[Page]->SetValue(false);
			g_Config.bNunchuckConnected = false;
			// Disconnect the extension so that the game recognize the change
			DoExtensionConnectedDisconnected();
		}
		g_Config.bClassicControllerConnected = m_ClassicControllerConnected[Page]->IsChecked();

		// Copy the calibration data
		memcpy(WiiMoteEmu::g_RegExt + 0x20, WiiMoteEmu::classic_calibration,
			sizeof(WiiMoteEmu::classic_calibration));
		memcpy(WiiMoteEmu::g_RegExt + 0xfa, WiiMoteEmu::classic_id, sizeof(WiiMoteEmu::classic_id));
		// Generate connect/disconnect status event
		DoExtensionConnectedDisconnected();
		break;


	//////////////////////////
	// Recording
	// -----------
	case ID_UPDATE_REAL:
		g_Config.bUpdateRealWiimote = m_UpdateMeters->IsChecked();
		break;
	case ID_NEUTRAL_CHOICE:
		g_Config.iAccNeutralX = m_AccNeutralChoice[0]->GetSelection();
		g_Config.iAccNeutralY = m_AccNeutralChoice[1]->GetSelection();
		g_Config.iAccNeutralZ = m_AccNeutralChoice[2]->GetSelection();
		//g_Config.iAccNunNeutralX = m_AccNunNeutralChoice[0]->GetSelection();
		//g_Config.iAccNunNeutralY = m_AccNunNeutralChoice[1]->GetSelection();
		//g_Config.iAccNunNeutralZ = m_AccNunNeutralChoice[2]->GetSelection();
		break;		

	case IDC_RECORD + 1:
	case IDC_RECORD + 2:
	case IDC_RECORD + 3:
	case IDC_RECORD + 4:
	case IDC_RECORD + 5:
	case IDC_RECORD + 6:
	case IDC_RECORD + 7:
	case IDC_RECORD + 8:
	case IDC_RECORD + 9:
	case IDC_RECORD + 10:
	case IDC_RECORD + 11:
	case IDC_RECORD + 12:
	case IDC_RECORD + 13:
	case IDC_RECORD + 14:
	case IDC_RECORD + 15:
		// Check if any of the other choice boxes has the same hotkey
		for (int i = 1; i < (RECORDING_ROWS + 1); i++)
		{
			int CurrentChoiceBox = (event.GetId() - IDC_RECORD);
			if (i == CurrentChoiceBox) continue;
			if (m_RecordHotKey[i]->GetSelection() == m_RecordHotKey[CurrentChoiceBox]->GetSelection()) m_RecordHotKey[i]->SetSelection(10);
			Console::Print("HotKey: %i %i\n",
				m_RecordHotKey[i]->GetSelection(), m_RecordHotKey[CurrentChoiceBox]->GetSelection());
		}
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
	Console::Print("UpdateGUI: \n");

	/* We can't allow different values for this one if we are using the real and emulated wiimote
	   side by side so that we can switch between between during gameplay. We update the checked
	   or unchecked values from the g_Config settings, and we make sure they are up to date with
	   unplugged and reinserted extensions. */	
	m_NunchuckConnected[Page]->SetValue(g_Config.bNunchuckConnected);
	m_ClassicControllerConnected[Page]->SetValue(g_Config.bClassicControllerConnected);
	m_NunchuckConnected[Page]->Enable(!(g_RealWiiMotePresent && g_Config.bConnectRealWiimote && g_EmulatorRunning));
	m_ClassicControllerConnected[Page]->Enable(!(g_RealWiiMotePresent && g_Config.bConnectRealWiimote && g_EmulatorRunning));

	/* I have disabled this option during a running game because it's enough to be able to switch
	   between using and not using then. To also use the connect option during a running game would
	   mean that the wiimote must be sent the current reporting mode and the channel ID after it
	   has been initialized. Functions for that are basically already in place so these two options
	   could possibly be simplified to one option. */
	m_ConnectRealWiimote[Page]->Enable(!g_EmulatorRunning);
	m_UseRealWiimote[Page]->Enable((g_RealWiiMotePresent && g_Config.bConnectRealWiimote) || !g_EmulatorRunning);

	// Linux has no FindItem()
	#ifdef _WIN32
		// Disable all recording buttons
		for(int i = IDB_RECORD + 1; i < (IDB_RECORD + RECORDING_ROWS + 1); i++)
			if(ControlsCreated) m_Notebook->FindItem(i)->Enable(!(m_bWaitForRecording || m_bRecording));
	#endif
}
