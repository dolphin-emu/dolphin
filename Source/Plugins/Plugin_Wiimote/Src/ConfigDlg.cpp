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
#include "EmuDefinitions.h" // for joyinfo
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

	// Recording
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

	// Gamepad
	EVT_COMBOBOX(ID_TRIGGER_TYPE, ConfigDialog::GeneralSettingsChanged)	
	EVT_COMBOBOX(ID_TILT_INPUT, ConfigDialog::GeneralSettingsChanged)	
	EVT_COMBOBOX(ID_TILT_RANGE, ConfigDialog::GeneralSettingsChanged)	
	EVT_COMBOBOX(IDC_JOYNAME, ConfigDialog::GeneralSettingsChanged)
	

	EVT_BUTTON(IDB_ANALOG_LEFT_X, ConfigDialog::GetButtons)
	EVT_BUTTON(IDB_ANALOG_LEFT_Y, ConfigDialog::GetButtons)
	EVT_BUTTON(IDB_ANALOG_RIGHT_X, ConfigDialog::GetButtons)
	EVT_BUTTON(IDB_ANALOG_RIGHT_Y, ConfigDialog::GetButtons)
	EVT_BUTTON(IDB_TRIGGER_L, ConfigDialog::GetButtons)
	EVT_BUTTON(IDB_TRIGGER_R, ConfigDialog::GetButtons)

	EVT_TIMER(IDTM_UPDATE, ConfigDialog::Update)
	EVT_TIMER(IDTM_UPDATE_ONCE, ConfigDialog::UpdateOnce)
	EVT_TIMER(IDTM_SHUTDOWN, ConfigDialog::ShutDown)
	EVT_TIMER(IDTM_BUTTON, ConfigDialog::OnButtonTimer)
	EVT_TIMER(IDTM_UPDATE_PAD, ConfigDialog::UpdatePad)
END_EVENT_TABLE()
//////////////////////////////////////


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
		m_TimeoutOnce = new wxTimer(this, IDTM_UPDATE_ONCE);
		m_ButtonMappingTimer = new wxTimer(this, IDTM_BUTTON);
		m_UpdatePad = new wxTimer(this, IDTM_UPDATE_PAD);

		// Reset values
		m_bWaitForRecording = false;
		m_bRecording = false;
		GetButtonWaitingID = 0; GetButtonWaitingTimer = 0;

		// Start the permanent timer
		const int TimesPerSecond = 30;
		m_UpdatePad->Start( floor((double)(1000 / TimesPerSecond)) );
	#endif

	ControlsCreated = false;
	m_bEnableUseRealWiimote = true;
	Page = 0;
	m_vRecording.resize(RECORDING_ROWS + 1);

	g_Config.Load();
	CreateGUIControls();
	LoadFile();
	UpdateGUI();

	wxTheApp->Connect(wxID_ANY, wxEVT_KEY_DOWN, // Keyboard
		wxKeyEventHandler(ConfigDialog::OnKeyDown),
		(wxObject*)0, this);
	wxTheApp->Connect(wxID_ANY, wxEVT_KEY_UP,
		wxKeyEventHandler(ConfigDialog::OnKeyDown),
		(wxObject*)0, this);
}

ConfigDialog::~ConfigDialog()
{	
}

void ConfigDialog::OnKeyDown(wxKeyEvent& event)
{
	event.Skip();

	// Save the key
	g_Pressed = event.GetKeyCode();

	// Escape a recording event
	if (event.GetKeyCode() == WXK_ESCAPE)
	{
		m_bWaitForRecording = false;
		m_bRecording = false;
		UpdateGUI();
	}
}

void ConfigDialog::OnClose(wxCloseEvent& event)
{
	g_FrameOpen = false;
	m_UpdatePad->Stop();
	SaveFile();
	g_Config.Save();	
	//SuccessAlert("Saved\n");
	if (!g_EmulatorRunning) Shutdown();
	// This will let the Close() function close and remove the wxDialog
	event.Skip();
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
		// Wait for the Wiimote thread to stop, then close and shutdown
		if(!g_EmulatorRunning)
		{
			WiiMoteReal::g_Shutdown = true;
			m_ShutDownTimer->Start(10);
		}
		// Close directly
		else
		{
			Close();
		}
		break;
	case ID_APPLY:
		SaveButtonMappingAll(Page);
		g_Config.Save();
		SaveFile();
		WiiMoteEmu::LoadRecordedMovements();
		break;
	}
}

void ConfigDialog::AboutClick(wxCommandEvent& WXUNUSED (event))
{
}

// Execute a delayed function
void ConfigDialog::UpdateOnce(wxTimerEvent& event)
{
	switch(event.GetId())
	{
	case IDTM_UPDATE_ONCE:
		// Reenable the checkbox
		m_bEnableUseRealWiimote = true;
		SetCursor(wxCursor(wxCURSOR_ARROW));
		UpdateGUI();
		break;
	}
}
//////////////////////////////////////



////////////////////////////////////////////////////////////////////////////
// Save Settings
/* ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯

   Saving is currently done when:

   1. Closing the configuration window
   2. Changing the gamepad
   3. When the gamepad is enabled or disbled

   Input: ChangePad needs to be used when we change the pad for a slot. Slot needs to be used when
   we only want to save changes to one slot.
*/
void ConfigDialog::DoSave(bool ChangePad, int Slot)
{
	// Replace "" with "-1" before we are saving
	ToBlank(false);

	if(ChangePad)
	{
		// Since we are selecting the pad to save to by the Id we can't update it when we change the pad
		for(int i = 0; i < 4; i++) SaveButtonMapping(i, true);
		
		g_Config.Save(Slot);
		// Now we can update the ID
		WiiMoteEmu::PadMapping[Page].ID = m_Joyname[Page]->GetSelection();
	}
	else
	{
		// Update PadMapping[] from the GUI controls
		for(int i = 0; i < 4; i++) SaveButtonMapping(i);
		g_Config.Save(Slot);
	}		

	// Then change it back to ""
	ToBlank();
}
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

	m_Notebook->AddPage(m_Controller[0], wxT("Wiimote 1"));
	m_Notebook->AddPage(m_Controller[1], wxT("Wiimote 2"));
	m_Notebook->AddPage(m_Controller[2], wxT("Wiimote 3"));
	m_Notebook->AddPage(m_Controller[3], wxT("Wiimote 4"));
	m_Notebook->AddPage(m_PageRecording, wxT("Recording"));
	///////////////////////////////


	////////////////////////////////////////////////////////////////////////////////
	// Text lists
	// ----------------	

	// Search for devices and add them to the device list
	wxArrayString StrJoyname; // The string array
	if(WiiMoteEmu::NumGoodPads > 0)
	{
		for(int x = 0; x < WiiMoteEmu::joyinfo.size(); x++)
			StrJoyname.Add(wxString::FromAscii(WiiMoteEmu::joyinfo[x].Name.c_str()));
	}
	else
	{
		StrJoyname.Add(wxString::FromAscii("<No Gamepad Detected>"));
	}

	// The tilt list
	wxArrayString StrTilt;
	StrTilt.Add(wxString::FromAscii("<Off>"));
	StrTilt.Add(wxString::FromAscii("Keyboard"));
	StrTilt.Add(wxString::FromAscii("Analog stick"));
	StrTilt.Add(wxString::FromAscii("Triggers"));
	// The range is in degrees and are set at even 5 degrees values
	wxArrayString StrTiltRange;
	for (int i = 2; i < 19; i++) StrTiltRange.Add(wxString::Format(wxT("%i"), i*5));

	// The Trigger type list
	wxArrayString StrTriggerType;
	StrTriggerType.Add(wxString::FromAscii("SDL")); // -0x8000 to 0x7fff
	StrTriggerType.Add(wxString::FromAscii("XInput")); // 0x00 to 0xff
	///////////////////////////////////////


	/* Populate all four pages. Page 2, 3 and 4 are currently disabled since we can't use more than one
	   Wiimote at the moment */
	for (int i = 0; i < 4; i++)
	{
		////////////////////////////////////////////////////
		// General and basic Settings
		// ----------------

		// Configuration controls
		static const int TxtW = 50, TxtH = 19, ChW = 245;

		// Basic Settings
		m_WiimoteOnline[i] = new wxCheckBox(m_Controller[i], IDC_JOYATTACH, wxT("Wiimote On"), wxDefaultPosition, wxSize(ChW, -1));
		// Emulated Wiimote
		m_SidewaysDPad[i] = new wxCheckBox(m_Controller[i], ID_SIDEWAYSDPAD, wxT("Sideways D-Pad"), wxDefaultPosition, wxSize(ChW, -1));
		m_WideScreen[i] = new wxCheckBox(m_Controller[i], ID_WIDESCREEN, wxT("WideScreen Mode (for correct aiming)"));
		// Extension
		m_WiiMotionPlusConnected[i] = new wxCheckBox(m_Controller[i], wxID_ANY, wxT("Wii Motion Plus Connected"), wxDefaultPosition, wxSize(ChW, -1), 0, wxDefaultValidator);
		m_NunchuckConnected[i] = new wxCheckBox(m_Controller[i], ID_NUNCHUCKCONNECTED, wxT("Nunchuck Connected"));
		m_ClassicControllerConnected[i] = new wxCheckBox(m_Controller[i], ID_CLASSICCONTROLLERCONNECTED, wxT("Classic Controller Connected"));
		m_BalanceBoardConnected[i] = new wxCheckBox(m_Controller[i], wxID_ANY, wxT("Balance Board Connected"));
		m_GuitarHeroGuitarConnected[i] = new wxCheckBox(m_Controller[i], wxID_ANY, wxT("Guitar Hero Guitar Connected"));
		m_GuitarHeroWorldTourDrumsConnected[i] = new wxCheckBox(m_Controller[i], wxID_ANY, wxT("Guitar Hero World Tour Drums Connected"));
		// Real Wiimote
		m_ConnectRealWiimote[i] = new wxCheckBox(m_Controller[i], ID_CONNECT_REAL, wxT("Connect Real Wiimote"), wxDefaultPosition, wxSize(ChW, -1));
		m_UseRealWiimote[i] = new wxCheckBox(m_Controller[i], ID_USE_REAL, wxT("Use Real Wiimote"));

		// Default values
		m_WiimoteOnline[0]->SetValue(true);
		m_NunchuckConnected[0]->SetValue(g_Config.bNunchuckConnected);
		m_ClassicControllerConnected[0]->SetValue(g_Config.bClassicControllerConnected);
		m_SidewaysDPad[0]->SetValue(g_Config.bSidewaysDPad);
		m_WideScreen[0]->SetValue(g_Config.bWideScreen);	
		m_ConnectRealWiimote[0]->SetValue(g_Config.bConnectRealWiimote);
		m_UseRealWiimote[0]->SetValue(g_Config.bUseRealWiimote);

		m_WiimoteOnline[0]->Enable(false);
		m_WiiMotionPlusConnected[0]->Enable(false);
		m_BalanceBoardConnected[0]->Enable(false);
		m_GuitarHeroGuitarConnected[0]->Enable(false);
		m_GuitarHeroWorldTourDrumsConnected[0]->Enable(false);

		// Sizers
		m_SizeBasic[i] = new wxStaticBoxSizer(wxVERTICAL, m_Controller[i], wxT("General Settings"));
		m_SizeEmu[i] = new wxStaticBoxSizer(wxVERTICAL, m_Controller[i], wxT("Emulated Wiimote"));
		m_SizeExtensions[i] = new wxStaticBoxSizer(wxVERTICAL, m_Controller[i], wxT("Emulated Extension"));
		m_SizeReal[i] = new wxStaticBoxSizer(wxVERTICAL, m_Controller[i], wxT("Real Wiimote"));

		m_SizeBasicPadding[i] = new wxBoxSizer(wxVERTICAL); m_SizeBasic[i]->Add(m_SizeBasicPadding[i], 0, wxEXPAND | (wxALL), 5);
		m_SizeBasicPadding[i]->Add(m_WiimoteOnline[i], 0, wxEXPAND | (wxUP), 2);

		m_SizeEmuPadding[i] = new wxBoxSizer(wxVERTICAL); m_SizeEmu[i]->Add(m_SizeEmuPadding[i], 0, wxEXPAND | (wxALL), 5);
		m_SizeEmuPadding[i]->Add(m_SidewaysDPad[i], 0, wxEXPAND | (wxUP), 0);
		m_SizeEmuPadding[i]->Add(m_WideScreen[i], 0, wxEXPAND | (wxUP), 2);

		m_SizeRealPadding[i] = new wxBoxSizer(wxVERTICAL); m_SizeReal[i]->Add(m_SizeRealPadding[i], 0, wxEXPAND | (wxALL), 5);
		m_SizeRealPadding[i]->Add(m_ConnectRealWiimote[i], 0, wxEXPAND | (wxUP), 0);
		m_SizeRealPadding[i]->Add(m_UseRealWiimote[i], 0, wxEXPAND | (wxUP), 2);

		m_SizeExtensionsPadding[i] = new wxBoxSizer(wxVERTICAL); m_SizeExtensions[i]->Add(m_SizeExtensionsPadding[i], 0, wxEXPAND | (wxALL), 5);
		m_SizeExtensionsPadding[i]->Add(m_WiiMotionPlusConnected[i], 0, (wxUP), 0);
		m_SizeExtensionsPadding[i]->Add(m_NunchuckConnected[i], 0, (wxUP), 2);
		m_SizeExtensionsPadding[i]->Add(m_ClassicControllerConnected[i], 0, (wxUP), 2);
		m_SizeExtensionsPadding[i]->Add(m_BalanceBoardConnected[i], 0, (wxUP), 2);
		m_SizeExtensionsPadding[i]->Add(m_GuitarHeroGuitarConnected[i], 0, (wxUP), 2);
		m_SizeExtensionsPadding[i]->Add(m_GuitarHeroWorldTourDrumsConnected[i], 0, (wxUP), 2);

		m_SizeBasicGeneral[i] = new wxBoxSizer(wxHORIZONTAL);
		m_SizeBasicGeneralLeft[i] = new wxBoxSizer(wxVERTICAL);
		m_SizeBasicGeneralRight[i] = new wxBoxSizer(wxVERTICAL);

		m_SizeBasicGeneralLeft[i]->Add(m_SizeBasic[i], 0, wxEXPAND | (wxUP), 0);
		m_SizeBasicGeneralLeft[i]->Add(m_SizeReal[i], 0, wxEXPAND | (wxUP), 5);
		m_SizeBasicGeneralLeft[i]->Add(m_SizeEmu[i], 0, wxEXPAND | (wxUP), 5);		

		m_SizeBasicGeneralRight[i]->Add(m_SizeExtensions[i], 0, wxEXPAND | (wxUP), 0);

		m_SizeBasicGeneral[i]->Add(m_SizeBasicGeneralLeft[i], 0, wxEXPAND | (wxUP), 0);
		m_SizeBasicGeneral[i]->Add(m_SizeBasicGeneralRight[i], 0, wxEXPAND | (wxLEFT), 10);

		// Tooltips
		m_WiimoteOnline[i]->SetToolTip(wxString::Format(wxT("Decide if Wiimote %i shall be detected by the game"), i));
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
		m_Joyname[i] = new wxComboBox(m_Controller[i], IDC_JOYNAME, StrJoyname[0], wxDefaultPosition, wxSize(225, -1), StrJoyname, wxCB_READONLY);

		m_gJoyname[i] = new wxStaticBoxSizer (wxHORIZONTAL, m_Controller[i], wxT("Gamepad"));
		m_gJoyname[i]->Add(m_Joyname[i], 0, wxALIGN_CENTER | (wxLEFT | wxRIGHT | wxDOWN), 5);

		// Tooltips
		m_Joyname[i]->SetToolTip(wxT("Save your settings and configure another joypad"));
		

		// --------------------------------------------------------------------
		// Tilt Wiimote
		// -----------------------------
		/**/
		// Controls
		m_TiltComboInput[i] = new wxComboBox(m_Controller[i], ID_TILT_INPUT, StrTilt[0], wxDefaultPosition, wxDefaultSize, StrTilt, wxCB_READONLY);
		m_TiltComboRange[i] = new wxComboBox(m_Controller[i], ID_TILT_RANGE, StrTiltRange[0], wxDefaultPosition, wxDefaultSize, StrTiltRange, wxCB_READONLY);
		m_TiltText[i] = new wxStaticText(m_Controller[i], wxID_ANY, wxT("Range"));

		m_TiltHoriz[i] = new wxBoxSizer(wxHORIZONTAL);
		m_TiltHoriz[i]->Add(m_TiltText[i], 0, (wxLEFT | wxTOP), 4);
		m_TiltHoriz[i]->Add(m_TiltComboRange[i], 0, (wxLEFT | wxRIGHT), 5);
		
		m_gTilt[i] = new wxStaticBoxSizer (wxVERTICAL, m_Controller[i], wxT("Tilt Wiimote"));
		m_gTilt[i]->AddStretchSpacer();
		m_gTilt[i]->Add(m_TiltComboInput[i], 0, wxEXPAND | (wxLEFT | wxRIGHT | wxDOWN), 5);
		m_gTilt[i]->Add(m_TiltHoriz[i], 0, wxEXPAND | (wxLEFT | wxRIGHT), 5);
		m_gTilt[i]->AddStretchSpacer();

		//Set values
		m_TiltComboInput[i]->SetSelection(g_Config.Trigger.Type);
		m_TiltComboRange[i]->SetValue(wxString::Format(wxT("%i"), g_Config.Trigger.Range));

		// Tooltips
		m_TiltComboInput[i]->SetToolTip(wxT("Control tilting by an analog gamepad stick, an analog trigger or the keyboard."));		
		m_TiltComboRange[i]->SetToolTip(wxT("The maximum tilt in degrees"));	

		// --------------------------------------------------------------------
		// Analog triggers
		// -----------------------------
		/**/
		m_gTrigger[i] = new wxStaticBoxSizer (wxHORIZONTAL, m_Controller[i], wxT("Triggers"));
		
		m_TriggerStatusL[i]= new wxStaticText(m_Controller[i], wxID_ANY, wxT("Left: "));
		m_TriggerStatusR[i]= new wxStaticText(m_Controller[i], wxID_ANY, wxT("Right: "));
		m_TriggerStatusLx[i]= new wxStaticText(m_Controller[i], wxID_ANY, wxT("000"), wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT);
		m_TriggerStatusRx[i]= new wxStaticText(m_Controller[i], wxID_ANY, wxT("000"), wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT);

		m_tAnalogTriggerInput[i] = new wxStaticText(m_Controller[i], wxID_ANY, wxT("Input"));
		m_tAnalogTriggerL[i] = new wxStaticText(m_Controller[i], wxID_ANY, wxT("Left"));
		m_tAnalogTriggerR[i] = new wxStaticText(m_Controller[i], wxID_ANY, wxT("Right"));

		m_AnalogTriggerL[i] = new wxTextCtrl(m_Controller[i], ID_TRIGGER_L, wxT(""), wxDefaultPosition, wxSize(TxtW, TxtH), wxTE_READONLY | wxTE_CENTRE);
		m_AnalogTriggerR[i] = new wxTextCtrl(m_Controller[i], ID_TRIGGER_R, wxT(""), wxDefaultPosition, wxSize(TxtW, TxtH), wxTE_READONLY | wxTE_CENTRE);

		m_AnalogTriggerL[i]->Enable(false);
		m_AnalogTriggerR[i]->Enable(false);

		m_bAnalogTriggerL[i] = new wxButton(m_Controller[i], IDB_TRIGGER_L, wxEmptyString, wxDefaultPosition, wxSize(21, 14));
		m_bAnalogTriggerR[i] = new wxButton(m_Controller[i], IDB_TRIGGER_R, wxEmptyString, wxDefaultPosition, wxSize(21, 14));

		m_TriggerType[i] = new wxComboBox(m_Controller[i], ID_TRIGGER_TYPE, StrTriggerType[0], wxDefaultPosition, wxDefaultSize, StrTriggerType, wxCB_READONLY);

		m_SizeAnalogTriggerStatusBox[i]  = new wxGridBagSizer(0, 0);
		m_SizeAnalogTriggerHorizConfig[i] = new wxGridBagSizer(0, 0);	
		m_SizeAnalogTriggerVertLeft[i] = new wxBoxSizer(wxVERTICAL);
		m_SizeAnalogTriggerVertRight[i] = new wxBoxSizer(wxVERTICAL);
		m_SizeAnalogTriggerHorizInput[i] = new wxBoxSizer(wxHORIZONTAL);

		// The status text boxes
		m_SizeAnalogTriggerStatusBox[i]->Add(m_TriggerStatusL[i], wxGBPosition(0, 0), wxGBSpan(1, 1), (wxUP), 0);
		m_SizeAnalogTriggerStatusBox[i]->Add(m_TriggerStatusLx[i], wxGBPosition(0, 1), wxGBSpan(1, 1), (wxUP), 0);
		m_SizeAnalogTriggerStatusBox[i]->Add(m_TriggerStatusR[i], wxGBPosition(1, 0), wxGBSpan(1, 1), (wxUP), 0);
		m_SizeAnalogTriggerStatusBox[i]->Add(m_TriggerStatusRx[i], wxGBPosition(1, 1), wxGBSpan(1, 1), (wxUP), 0);

		m_SizeAnalogTriggerHorizConfig[i]->Add(m_tAnalogTriggerL[i], wxGBPosition(0, 0), wxGBSpan(1, 1), (wxUP), 2);
		m_SizeAnalogTriggerHorizConfig[i]->Add(m_AnalogTriggerL[i], wxGBPosition(0, 1), wxGBSpan(1, 1), (wxLEFT | wxRIGHT), 2);
		m_SizeAnalogTriggerHorizConfig[i]->Add(m_bAnalogTriggerL[i], wxGBPosition(0, 2), wxGBSpan(1, 1), (wxUP), 2);
		m_SizeAnalogTriggerHorizConfig[i]->Add(m_tAnalogTriggerR[i], wxGBPosition(1, 0), wxGBSpan(1, 1), (wxUP), 4);
		m_SizeAnalogTriggerHorizConfig[i]->Add(m_AnalogTriggerR[i], wxGBPosition(1, 1), wxGBSpan(1, 1), (wxLEFT | wxUP | wxRIGHT), 2);
		m_SizeAnalogTriggerHorizConfig[i]->Add(m_bAnalogTriggerR[i], wxGBPosition(1, 2), wxGBSpan(1, 1), (wxUP), 5);

		// The choice box and its name label
		m_SizeAnalogTriggerHorizInput[i]->Add(m_tAnalogTriggerInput[i], 0, (wxUP), 2);
		m_SizeAnalogTriggerHorizInput[i]->Add(m_TriggerType[i], 0, (wxLEFT), 2);

		// The status text boxes
		m_SizeAnalogTriggerVertLeft[i]->AddStretchSpacer();
		m_SizeAnalogTriggerVertLeft[i]->Add(m_SizeAnalogTriggerStatusBox[i]);
		m_SizeAnalogTriggerVertLeft[i]->AddStretchSpacer();

		// The config grid and the input type choice box
		m_SizeAnalogTriggerVertRight[i]->Add(m_SizeAnalogTriggerHorizConfig[i], 0, (wxUP), 0);
		m_SizeAnalogTriggerVertRight[i]->Add(m_SizeAnalogTriggerHorizInput[i], 0, (wxUP | wxDOWN), 4);

		m_gTrigger[i]->Add(m_SizeAnalogTriggerVertLeft[i], 0, wxEXPAND | (wxLEFT | wxRIGHT), 5);
		m_gTrigger[i]->Add(m_SizeAnalogTriggerVertRight[i], 0, (wxLEFT | wxRIGHT), 5);


		// --------------------------------------------------------------------
		// Row 2 Sizers: Connected pads, tilt, triggers
		// -----------------------------
		m_HorizControllerTilt[i] = new wxBoxSizer(wxHORIZONTAL);
		m_HorizControllerTilt[i]->Add(m_gJoyname[i], 0, wxALIGN_CENTER | wxEXPAND, 0);
		m_HorizControllerTilt[i]->Add(m_gTilt[i], 0, wxEXPAND | (wxLEFT), 5);
		m_HorizControllerTilt[i]->Add(m_gTrigger[i], 0, (wxLEFT), 5);

		m_HorizControllerTiltParent[i] = new wxBoxSizer(wxBOTH);
		m_HorizControllerTiltParent[i]->Add(m_HorizControllerTilt[i]);



		// --------------------------------------------------------------------
		// Analog sticks
		// -----------------------------
		
		// Status panels
		m_TStatusLeftIn[i] = new wxStaticText(m_Controller[i], wxID_ANY, wxT("In"));
		m_TStatusLeftOut[i] = new wxStaticText(m_Controller[i], wxID_ANY, wxT("Out"));
		m_TStatusRightIn[i] = new wxStaticText(m_Controller[i], wxID_ANY, wxT("In"));
		m_TStatusRightOut[i] = new wxStaticText(m_Controller[i], wxID_ANY, wxT("Out"));

		m_pLeftInStatus[i] = new wxPanel(m_Controller[i], wxID_ANY, wxDefaultPosition, wxDefaultSize);
		m_bmpSquareLeftIn[i] = new wxStaticBitmap(m_pLeftInStatus[i], wxID_ANY, CreateBitmap(), wxDefaultPosition, wxDefaultSize);
		m_bmpDotLeftIn[i] = new wxStaticBitmap(m_pLeftInStatus[i], wxID_ANY, CreateBitmapDot(), wxPoint(BoxW / 2, BoxH / 2), wxDefaultSize);

		m_pLeftOutStatus[i] = new wxPanel(m_Controller[i], wxID_ANY, wxDefaultPosition, wxDefaultSize);
		m_bmpSquareLeftOut[i] = new wxStaticBitmap(m_pLeftOutStatus[i], wxID_ANY, CreateBitmap(), wxDefaultPosition, wxDefaultSize);
		m_bmpDotLeftOut[i] = new wxStaticBitmap(m_pLeftOutStatus[i], wxID_ANY, CreateBitmapDot(), wxPoint(BoxW / 2, BoxH / 2), wxDefaultSize);

		m_pRightInStatus[i] = new wxPanel(m_Controller[i], wxID_ANY, wxDefaultPosition, wxDefaultSize);
		m_bmpSquareRightIn[i] = new wxStaticBitmap(m_pRightInStatus[i], wxID_ANY, CreateBitmap(), wxDefaultPosition, wxDefaultSize);
		m_bmpDotRightIn[i] = new wxStaticBitmap(m_pRightInStatus[i], wxID_ANY, CreateBitmapDot(), wxPoint(BoxW / 2, BoxH / 2), wxDefaultSize);

		m_pRightOutStatus[i] = new wxPanel(m_Controller[i], wxID_ANY, wxDefaultPosition, wxDefaultSize);
		m_bmpSquareRightOut[i] = new wxStaticBitmap(m_pRightOutStatus[i], wxID_ANY, CreateBitmap(), wxDefaultPosition, wxDefaultSize);
		m_bmpDotRightOut[i] = new wxStaticBitmap(m_pRightOutStatus[i], wxID_ANY, CreateBitmapDot(), wxPoint(BoxW / 2, BoxH / 2), wxDefaultSize);

		m_AnalogLeftX[i] = new wxTextCtrl(m_Controller[i], ID_ANALOG_LEFT_X, wxT(""), wxDefaultPosition, wxSize(TxtW, TxtH), wxTE_READONLY | wxTE_CENTRE);
		m_AnalogLeftY[i] = new wxTextCtrl(m_Controller[i], ID_ANALOG_LEFT_Y, wxT(""), wxDefaultPosition, wxSize(TxtW, TxtH), wxTE_READONLY | wxTE_CENTRE);
		m_AnalogRightX[i] = new wxTextCtrl(m_Controller[i], ID_ANALOG_RIGHT_X, wxT(""), wxDefaultPosition, wxSize(TxtW, TxtH), wxTE_READONLY | wxTE_CENTRE);
		m_AnalogRightY[i] = new wxTextCtrl(m_Controller[i], ID_ANALOG_RIGHT_Y, wxT(""), wxDefaultPosition, wxSize(TxtW, TxtH), wxTE_READONLY | wxTE_CENTRE);

		m_AnalogLeftX[i]->Enable(false);
		m_AnalogLeftY[i]->Enable(false);
		m_AnalogRightX[i]->Enable(false);
		m_AnalogRightY[i]->Enable(false);

		m_bAnalogLeftX[i] = new wxButton(m_Controller[i], IDB_ANALOG_LEFT_X, wxEmptyString, wxDefaultPosition, wxSize(21, 14));
		m_bAnalogLeftY[i] = new wxButton(m_Controller[i], IDB_ANALOG_LEFT_Y, wxEmptyString, wxDefaultPosition, wxSize(21, 14));
		m_bAnalogRightX[i] = new wxButton(m_Controller[i], IDB_ANALOG_RIGHT_X, wxEmptyString, wxDefaultPosition, wxSize(21, 14));
		m_bAnalogRightY[i] = new wxButton(m_Controller[i], IDB_ANALOG_RIGHT_Y, wxEmptyString, wxDefaultPosition, wxSize(21, 14));

		m_SizeAnalogLeft[i] = new wxBoxSizer(wxVERTICAL); m_SizeAnalogLeftHorizX[i] = new wxBoxSizer(wxHORIZONTAL); m_SizeAnalogLeftHorizY[i] = new wxBoxSizer(wxHORIZONTAL);
		m_SizeAnalogRight[i] = new wxBoxSizer(wxVERTICAL); m_SizeAnalogRightHorizX[i] = new wxBoxSizer(wxHORIZONTAL); m_SizeAnalogRightHorizY[i] = new wxBoxSizer(wxHORIZONTAL);

		m_tAnalogX[i] = new wxStaticText(m_Controller[i], wxID_ANY, wxT("X-Axis"));
		m_tAnalogX[i + 4] = new wxStaticText(m_Controller[i], wxID_ANY, wxT("X-Axis"));
		m_tAnalogY[i] = new wxStaticText(m_Controller[i], wxID_ANY, wxT("Y-Axis"));
		m_tAnalogY[i + 4] = new wxStaticText(m_Controller[i], wxID_ANY, wxT("Y-Axis"));

		// Configuration sizers
		m_SizeAnalogLeftHorizX[i]->Add(m_tAnalogX[i], 0, (wxUP), 2);
		m_SizeAnalogLeftHorizX[i]->Add(m_AnalogLeftX[i], 0, (wxRIGHT), 2);
		m_SizeAnalogLeftHorizX[i]->Add(m_bAnalogLeftX[i], 0, (wxUP), 2);
		m_SizeAnalogLeftHorizY[i]->Add(m_tAnalogY[i], 0, (wxUP), 4);
		m_SizeAnalogLeftHorizY[i]->Add(m_AnalogLeftY[i], 0, (wxUP | wxRIGHT), 2);
		m_SizeAnalogLeftHorizY[i]->Add(m_bAnalogLeftY[i], 0, (wxUP), 4);

		m_SizeAnalogRightHorizX[i]->Add(m_tAnalogX[i + 4], 0, (wxUP), 2);
		m_SizeAnalogRightHorizX[i]->Add(m_AnalogRightX[i], 0, (wxRIGHT), 2);
		m_SizeAnalogRightHorizX[i]->Add(m_bAnalogRightX[i], 0, (wxUP), 2);
		m_SizeAnalogRightHorizY[i]->Add(m_tAnalogY[i + 4], 0, (wxUP), 4);
		m_SizeAnalogRightHorizY[i]->Add(m_AnalogRightY[i], 0, (wxUP | wxRIGHT), 2);
		m_SizeAnalogRightHorizY[i]->Add(m_bAnalogRightY[i], 0, (wxUP), 4);

		m_SizeAnalogLeft[i]->AddStretchSpacer();
		m_SizeAnalogLeft[i]->Add(m_SizeAnalogLeftHorizX[i], 0, (wxUP), 0);
		m_SizeAnalogLeft[i]->Add(m_SizeAnalogLeftHorizY[i], 0, (wxUP), 0);
		m_SizeAnalogLeft[i]->AddStretchSpacer();
		m_SizeAnalogRight[i]->AddStretchSpacer();
		m_SizeAnalogRight[i]->Add(m_SizeAnalogRightHorizX[i], 0, (wxUP), 0);
		m_SizeAnalogRight[i]->Add(m_SizeAnalogRightHorizY[i], 0, (wxUP), 0);
		m_SizeAnalogRight[i]->AddStretchSpacer();

		// Status sizers
		m_GridLeftStick[i] = new wxGridBagSizer(0, 0);
		m_GridLeftStick[i]->Add(m_pLeftInStatus[i], wxGBPosition(0, 0), wxGBSpan(1, 1), wxALL, 0);
		m_GridLeftStick[i]->Add(m_pLeftOutStatus[i], wxGBPosition(0, 1), wxGBSpan(1, 1), wxLEFT, 5);
		m_GridLeftStick[i]->Add(m_TStatusLeftIn[i], wxGBPosition(1, 0), wxGBSpan(1, 1), wxALL, 0);
		m_GridLeftStick[i]->Add(m_TStatusLeftOut[i], wxGBPosition(1, 1), wxGBSpan(1, 1), wxLEFT, 5);

		m_GridRightStick[i] = new wxGridBagSizer(0, 0);
		m_GridRightStick[i]->Add(m_pRightInStatus[i], wxGBPosition(0, 0), wxGBSpan(1, 1), wxALL, 0);
		m_GridRightStick[i]->Add(m_pRightOutStatus[i], wxGBPosition(0, 1), wxGBSpan(1, 1), wxLEFT, 5);
		m_GridRightStick[i]->Add(m_TStatusRightIn[i], wxGBPosition(1, 0), wxGBSpan(1, 1), wxALL, 0);
		m_GridRightStick[i]->Add(m_TStatusRightOut[i], wxGBPosition(1, 1), wxGBSpan(1, 1), wxLEFT, 5);

		m_gAnalogLeft[i] = new wxStaticBoxSizer (wxHORIZONTAL, m_Controller[i], wxT("Analog 1 (In) (Out)"));
		m_gAnalogLeft[i]->Add(m_GridLeftStick[i], 0, (wxLEFT | wxRIGHT), 5);
		m_gAnalogLeft[i]->Add(m_SizeAnalogLeft[i], 0, wxEXPAND | wxALIGN_CENTER_VERTICAL, 0);

		m_gAnalogRight[i] = new wxStaticBoxSizer (wxHORIZONTAL, m_Controller[i], wxT("Analog 2 (In) (Out)"));
		m_gAnalogRight[i]->Add(m_GridRightStick[i], 0, (wxLEFT | wxRIGHT), 5);
		m_gAnalogRight[i]->Add(m_SizeAnalogRight[i], 0, wxEXPAND | wxALIGN_CENTER_VERTICAL, 0);

		
		// --------------------------------------------------------------------
		// Row 3 Sizers
		// -----------------------------
		m_HorizControllers[i] = new wxBoxSizer(wxHORIZONTAL);
		m_HorizControllers[i]->Add(m_gAnalogLeft[i]);
		m_HorizControllers[i]->Add(m_gAnalogRight[i], 0, (wxLEFT), 5);
		///////////////////////////


		////////////////////////////////////////////////////////////////////////
		// Keyboard mapping
		// ----------------
		
		// --------------------------------------------------------------------
		// Wiimote
		// -----------------------------
		/*
		m_WmA[i] = new wxTextCtrl(m_Controller[i], ID_WM_A, wxT(""), wxDefaultPosition, wxDefaultSize, wxTE_READONLY | wxTE_CENTRE);
		m_WmB[i] = new wxTextCtrl(m_Controller[i], ID_WM_A, wxT(""), wxDefaultPosition, wxDefaultSize, wxTE_READONLY | wxTE_CENTRE);
		m_Wm1[i] = new wxTextCtrl(m_Controller[i], ID_WM_A, wxT(""), wxDefaultPosition, wxDefaultSize, wxTE_READONLY | wxTE_CENTRE);
		m_Wm2[i] = new wxTextCtrl(m_Controller[i], ID_WM_A, wxT(""), wxDefaultPosition, wxDefaultSize, wxTE_READONLY | wxTE_CENTRE);
		m_WmP[i] = new wxTextCtrl(m_Controller[i], ID_WM_A, wxT(""), wxDefaultPosition, wxDefaultSize, wxTE_READONLY | wxTE_CENTRE);
		m_WmM[i] = new wxTextCtrl(m_Controller[i], ID_WM_A, wxT(""), wxDefaultPosition, wxDefaultSize, wxTE_READONLY | wxTE_CENTRE);
		m_WmH[i] = new wxTextCtrl(m_Controller[i], ID_WM_A, wxT(""), wxDefaultPosition, wxDefaultSize, wxTE_READONLY | wxTE_CENTRE);
		m_WmL[i] = new wxTextCtrl(m_Controller[i], ID_WM_A, wxT(""), wxDefaultPosition, wxDefaultSize, wxTE_READONLY | wxTE_CENTRE);
		m_WmR[i] = new wxTextCtrl(m_Controller[i], ID_WM_A, wxT(""), wxDefaultPosition, wxDefaultSize, wxTE_READONLY | wxTE_CENTRE);
		m_WmU[i] = new wxTextCtrl(m_Controller[i], ID_WM_A, wxT(""), wxDefaultPosition, wxDefaultSize, wxTE_READONLY | wxTE_CENTRE);
		m_WmD[i] = new wxTextCtrl(m_Controller[i], ID_WM_A, wxT(""), wxDefaultPosition, wxDefaultSize, wxTE_READONLY | wxTE_CENTRE);

		m_tWmA[i] = new wxStaticText(m_Controller[i], wxID_ANY, wxT("A"));
		m_tWmB[i] = new wxStaticText(m_Controller[i], wxID_ANY, wxT("B"));
		m_tWm1[i] = new wxStaticText(m_Controller[i], wxID_ANY, wxT("1"));
		m_tWm2[i] = new wxStaticText(m_Controller[i], wxID_ANY, wxT("2"));
		m_tWmP[i] = new wxStaticText(m_Controller[i], wxID_ANY, wxT("P"));
		m_tWmM[i] = new wxStaticText(m_Controller[i], wxID_ANY, wxT("M"));
		m_tWmH[i] = new wxStaticText(m_Controller[i], wxID_ANY, wxT("H"));
		m_tWmL[i] = new wxStaticText(m_Controller[i], wxID_ANY, wxT("Left"));
		m_tWmR[i] = new wxStaticText(m_Controller[i], wxID_ANY, wxT("Right"));
		m_tWmU[i] = new wxStaticText(m_Controller[i], wxID_ANY, wxT("Up"));
		m_tWmD[i] = new wxStaticText(m_Controller[i], wxID_ANY, wxT("Down"));

		m_bWmA[i] = new wxButton(m_Controller[i], IDB_WM_A);
		m_bWmB[i] = new wxButton(m_Controller[i], IDB_WM_B);
		m_bWm1[i] = new wxButton(m_Controller[i], IDB_WM_1);
		m_bWm2[i] = new wxButton(m_Controller[i], IDB_WM_2);
		m_bWmP[i] = new wxButton(m_Controller[i], IDB_WM_P);
		m_bWmM[i] = new wxButton(m_Controller[i], IDB_WM_M);
		m_bWmL[i] = new wxButton(m_Controller[i], IDB_WM_L);
		m_bWmR[i] = new wxButton(m_Controller[i], IDB_WM_R);
		m_bWmU[i] = new wxButton(m_Controller[i], IDB_WM_U);
		m_bWmD[i] = new wxButton(m_Controller[i], IDB_WM_D);	

		// Disable
		m_WmA[i]->Enable(false);
		m_WmB[i]->Enable(false);
		m_Wm1[i]->Enable(false);
		m_Wm2[i]->Enable(false);
		m_WmP[i]->Enable(false);
		m_WmM[i]->Enable(false);
		m_WmL[i]->Enable(false);
		m_WmR[i]->Enable(false);
		m_WmU[i]->Enable(false);
		m_WmD[i]->Enable(false);
		

		// --------------------------------------------------------------------
		// Nunchuck
		// -----------------------------

		m_NuZ[i] = new wxTextCtrl(m_Controller[i], ID_WM_A, wxT(""), wxDefaultPosition, wxDefaultSize, wxTE_READONLY | wxTE_CENTRE);
		m_NuC[i] = new wxTextCtrl(m_Controller[i], ID_WM_A, wxT(""), wxDefaultPosition, wxDefaultSize, wxTE_READONLY | wxTE_CENTRE);
		m_NuL[i] = new wxTextCtrl(m_Controller[i], ID_WM_A, wxT(""), wxDefaultPosition, wxDefaultSize, wxTE_READONLY | wxTE_CENTRE);
		m_NuR[i] = new wxTextCtrl(m_Controller[i], ID_WM_A, wxT(""), wxDefaultPosition, wxDefaultSize, wxTE_READONLY | wxTE_CENTRE);
		m_NuU[i] = new wxTextCtrl(m_Controller[i], ID_WM_A, wxT(""), wxDefaultPosition, wxDefaultSize, wxTE_READONLY | wxTE_CENTRE);
		m_NuD[i] = new wxTextCtrl(m_Controller[i], ID_WM_A, wxT(""), wxDefaultPosition, wxDefaultSize, wxTE_READONLY | wxTE_CENTRE);

		m_tNuZ[i] = new wxStaticText(m_Controller[i], wxID_ANY, wxT("Z"));
		m_tNuC[i] = new wxStaticText(m_Controller[i], wxID_ANY, wxT("C"));
		m_tNuL[i] = new wxStaticText(m_Controller[i], wxID_ANY, wxT("Left"));
		m_tNuR[i] = new wxStaticText(m_Controller[i], wxID_ANY, wxT("Right"));
		m_tNuU[i] = new wxStaticText(m_Controller[i], wxID_ANY, wxT("Up"));
		m_tNuD[i] = new wxStaticText(m_Controller[i], wxID_ANY, wxT("Down"));

		m_bNuZ[i] = new wxButton(m_Controller[i], IDB_WM_Z);
		m_bNuC[i] = new wxButton(m_Controller[i], IDB_WM_C);
		m_bNuL[i] = new wxButton(m_Controller[i], IDB_WM_L);
		m_bNuR[i] = new wxButton(m_Controller[i], IDB_WM_R);
		m_bNuU[i] = new wxButton(m_Controller[i], IDB_WM_U);
		m_bNuD[i] = new wxButton(m_Controller[i], IDB_WM_D);	

		// Disable
		m_NuZ[i]->Enable(false);
		m_NuC[i]->Enable(false);
		m_NuL[i]->Enable(false);
		m_NuR[i]->Enable(false);
		m_NuU[i]->Enable(false);
		m_NuD[i]->Enable(false);

		// --------------------------------------------------------------------
		// Classic Controller
		// -----------------------------

		m_ClY[i] = new wxTextCtrl(m_Controller[i], ID_WM_A, wxT(""), wxDefaultPosition, wxDefaultSize, wxTE_READONLY | wxTE_CENTRE);
		m_ClX[i] = new wxTextCtrl(m_Controller[i], ID_WM_A, wxT(""), wxDefaultPosition, wxDefaultSize, wxTE_READONLY | wxTE_CENTRE);
		m_ClA[i] = new wxTextCtrl(m_Controller[i], ID_WM_A, wxT(""), wxDefaultPosition, wxDefaultSize, wxTE_READONLY | wxTE_CENTRE);
		m_ClB[i] = new wxTextCtrl(m_Controller[i], ID_WM_A, wxT(""), wxDefaultPosition, wxDefaultSize, wxTE_READONLY | wxTE_CENTRE);
		m_ClLx[i] = new wxTextCtrl(m_Controller[i], ID_WM_A, wxT(""), wxDefaultPosition, wxDefaultSize, wxTE_READONLY | wxTE_CENTRE);
		m_ClLy[i] = new wxTextCtrl(m_Controller[i], ID_WM_A, wxT(""), wxDefaultPosition, wxDefaultSize, wxTE_READONLY | wxTE_CENTRE);

		m_tClY[i] = new wxStaticText(m_Controller[i], wxID_ANY, wxT("Z"));
		m_tClX[i] = new wxStaticText(m_Controller[i], wxID_ANY, wxT("C"));
		m_tClA[i] = new wxStaticText(m_Controller[i], wxID_ANY, wxT("Left"));
		m_tClB[i] = new wxStaticText(m_Controller[i], wxID_ANY, wxT("Right"));
		m_tClLx[i] = new wxStaticText(m_Controller[i], wxID_ANY, wxT("Up"));
		m_tClLy[i] = new wxStaticText(m_Controller[i], wxID_ANY, wxT("Down"));

		m_bClY[i] = new wxButton(m_Controller[i], IDB_WM_Z);
		m_bClX[i] = new wxButton(m_Controller[i], IDB_WM_C);
		m_bClA[i] = new wxButton(m_Controller[i], IDB_WM_L);
		m_bClB[i] = new wxButton(m_Controller[i], IDB_WM_R);
		m_bClLx[i] = new wxButton(m_Controller[i], IDB_WM_U);
		m_bClLy[i] = new wxButton(m_Controller[i], IDB_WM_D);	

		// Disable
		m_ClY[i]->Enable(false);
		m_ClX[i]->Enable(false);
		m_ClA[i]->Enable(false);
		m_ClB[i]->Enable(false);
		m_ClLx[i]->Enable(false);
		m_ClLy[i]->Enable(false);
		*/
		///////////////////////////


		////////////////////////////////////////////////////////////////
		// Set up sizers and layout
		// ----------------
		m_SizeParent[i] = new wxBoxSizer(wxVERTICAL);
		m_SizeParent[i]->Add(m_SizeBasicGeneral[i], 0, wxBORDER_STATIC | wxEXPAND | (wxALL), 5);
		m_SizeParent[i]->Add(m_HorizControllerTiltParent[i], 0, wxEXPAND | (wxLEFT | wxRIGHT | wxDOWN), 5);
		m_SizeParent[i]->Add(m_HorizControllers[i], 0, wxEXPAND | (wxLEFT | wxRIGHT | wxDOWN), 5);

		// The sizer m_sMain will be expanded inside m_Controller, m_SizeParent will not
		m_sMain[i] = new wxBoxSizer(wxVERTICAL);
		m_sMain[i]->Add(m_SizeParent[i]);

		// Set the main sizer
		m_Controller[i]->SetSizer(m_sMain[i]);
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

	// Center the window if there is room for it
	#ifdef _WIN32
		if (GetSystemMetrics(SM_CYFULLSCREEN) > 800)
			Center();
	#endif

	ControlsCreated = true;
	/////////////////////////////////
}
/////////////////////////////////



// ===================================================
/* Do connect real wiimote */
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
		Console::Print("Post Message: %i\n", g_RealWiiMoteInitialized);
		if (g_RealWiiMoteInitialized)
		{
			WiiMoteReal::Shutdown();
			/*
			if (g_WiimoteUnexpectedDisconnect)
			{
				#ifdef _WIN32
					PostMessage(g_ParentHWND, WM_USER, WIIMOTE_RECONNECT, 0);
					g_WiimoteUnexpectedDisconnect = false;
				#endif
			}
			*/
		}
	}
}


// ===================================================
/* Do use real wiimote. We let the game set up the real Wiimote reporting mode and init the Extension when we change
   want to use it again. */
// ----------------
void ConfigDialog::DoUseReal()
{
	// Clear any eventual events in the Wiimote queue
	WiiMoteReal::ClearEvents();

	// Are we using an extension now? The report that it's removed, then reconnected.
	bool UsingExtension = false;
	if (g_Config.bNunchuckConnected || g_Config.bClassicControllerConnected)
		UsingExtension = true;

	Console::Print("\nDoUseReal()  Connect extension: %i\n", !UsingExtension);
	DoExtensionConnectedDisconnected(UsingExtension ? 0 : 1);

	// Disable the checkbox for a moment
	SetCursor(wxCursor(wxCURSOR_WAIT));
	m_bEnableUseRealWiimote = false;
	// We don't need this, there is already a message queue that allows the nessesary timeout
	//sleep(100);

	UsingExtension = !UsingExtension;
	Console::Print("\nDoUseReal()  Connect extension: %i\n", !UsingExtension);
	DoExtensionConnectedDisconnected(UsingExtension ? 1 : 0);

	/* Start the timer to allow the approximate time it takes for the Wiimote to come online
	   it would simpler to use sleep(1000) but that doesn't work because we need the functions in main.cpp
	   to work */
	m_TimeoutOnce->Start(1000, true);
}

// ===================================================
/* Generate connect/disconnect status event */
// ----------------
void ConfigDialog::DoExtensionConnectedDisconnected(int Extension)
{
	// There is no need for this if no game is running
	if(!g_EmulatorRunning) return; 

	u8 DataFrame[8]; // make a blank report for it
	wm_request_status *rs = (wm_request_status*)DataFrame;

	// Check if a game is running, in that case change the status
	if(WiiMoteEmu::g_ReportingChannel > 0)
		WiiMoteEmu::WmRequestStatus(WiiMoteEmu::g_ReportingChannel, rs, Extension);
}

// ===================================================
/* Change settings */
// ----------------
void ConfigDialog::GeneralSettingsChanged(wxCommandEvent& event)
{
	long TmpValue;

	switch (event.GetId())
	{
	case ID_CONNECT_REAL:
		DoConnectReal();
		break;
	case ID_USE_REAL:
		// Enable the Wiimote thread
		g_Config.bUseRealWiimote = m_UseRealWiimote[Page]->IsChecked();
		if(g_Config.bUseRealWiimote) DoUseReal();		
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
		WiiMoteEmu::SetDefaultExtensionRegistry();

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
		WiiMoteEmu::SetDefaultExtensionRegistry();
		// Generate connect/disconnect status event
		DoExtensionConnectedDisconnected();
		break;

	//////////////////////////
	// Gamepad
	// -----------
	case ID_TRIGGER_TYPE:
		WiiMoteEmu::PadMapping[Page].triggertype = m_TriggerType[Page]->GetSelection();
		break;
	case ID_TILT_INPUT:
		g_Config.Trigger.Type = m_TiltComboInput[Page]->GetSelection();
		break;
	case ID_TILT_RANGE:
		m_TiltComboRange[Page]->GetValue().ToLong(&TmpValue); g_Config.Trigger.Range = TmpValue;
		break;
	case IDC_JOYNAME:
		DoChangeJoystick();
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
void ConfigDialog::UpdateGUI(int Slot)
{
	//Console::Print("UpdateGUI: \n");

	// Update the gamepad settings
	UpdateGUIButtonMapping(Page);

	/* We only allow a change of extension if we are not currently using the real Wiimote, if it's in use the status will be updated
	   from the data scanning functions in main.cpp */
	bool AllowExtensionChange = !(g_RealWiiMotePresent && g_Config.bConnectRealWiimote && g_Config.bUseRealWiimote && g_EmulatorRunning);
	m_NunchuckConnected[Page]->SetValue(g_Config.bNunchuckConnected);
	m_ClassicControllerConnected[Page]->SetValue(g_Config.bClassicControllerConnected);
	m_NunchuckConnected[Page]->Enable(AllowExtensionChange);
	m_ClassicControllerConnected[Page]->Enable(AllowExtensionChange);

	/* I have disabled this option during a running game because it's enough to be able to switch
	   between using and not using then. To also use the connect option during a running game would
	   mean that the wiimote must be sent the current reporting mode and the channel ID after it
	   has been initialized. Functions for that are basically already in place so these two options
	   could possibly be simplified to one option. */
	m_ConnectRealWiimote[Page]->Enable(!g_EmulatorRunning);
	m_UseRealWiimote[Page]->Enable((m_bEnableUseRealWiimote && g_RealWiiMotePresent && g_Config.bConnectRealWiimote) || !g_EmulatorRunning);

	// Linux has no FindItem()
	#ifdef _WIN32
		// Disable all recording buttons
		for(int i = IDB_RECORD + 1; i < (IDB_RECORD + RECORDING_ROWS + 1); i++)
			if(ControlsCreated) m_Notebook->FindItem(i)->Enable(!(m_bWaitForRecording || m_bRecording));
	#endif
}
