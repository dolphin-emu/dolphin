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
#include "StringUtil.h"

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
	EVT_CHECKBOX(ID_NUNCHUCKCONNECTED, ConfigDialog::GeneralSettingsChanged)	
	EVT_CHECKBOX(ID_CLASSICCONTROLLERCONNECTED, ConfigDialog::GeneralSettingsChanged)

	EVT_CHECKBOX(ID_CONNECT_REAL, ConfigDialog::GeneralSettingsChanged)
	EVT_CHECKBOX(ID_USE_REAL, ConfigDialog::GeneralSettingsChanged)
	EVT_CHECKBOX(ID_UPDATE_REAL, ConfigDialog::GeneralSettingsChanged)

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
	EVT_COMBOBOX(IDC_JOYNAME, ConfigDialog::GeneralSettingsChanged)	
	EVT_COMBOBOX(ID_TRIGGER_TYPE, ConfigDialog::GeneralSettingsChanged)	
	EVT_COMBOBOX(ID_TILT_INPUT, ConfigDialog::GeneralSettingsChanged)	
	EVT_COMBOBOX(ID_TILT_RANGE_ROLL, ConfigDialog::GeneralSettingsChanged)
	EVT_COMBOBOX(ID_TILT_RANGE_PITCH, ConfigDialog::GeneralSettingsChanged)
	EVT_COMBOBOX(IDCB_LEFT_DIAGONAL, ConfigDialog::GeneralSettingsChanged)
	EVT_COMBOBOX(IDCB_DEAD_ZONE_LEFT, ConfigDialog::GeneralSettingsChanged)
	EVT_COMBOBOX(IDCB_DEAD_ZONE_RIGHT, ConfigDialog::GeneralSettingsChanged)
	EVT_CHECKBOX(IDC_LEFT_C2S, ConfigDialog::GeneralSettingsChanged)
	EVT_CHECKBOX(ID_TILT_INVERT_ROLL, ConfigDialog::GeneralSettingsChanged)
	EVT_CHECKBOX(ID_TILT_INVERT_PITCH, ConfigDialog::GeneralSettingsChanged)
	EVT_COMBOBOX(IDCB_NUNCHUCK_STICK, ConfigDialog::GeneralSettingsChanged)
	EVT_COMBOBOX(IDCB_CC_LEFT_STICK, ConfigDialog::GeneralSettingsChanged)
	EVT_COMBOBOX(IDCB_CC_RIGHT_STICK, ConfigDialog::GeneralSettingsChanged)
	EVT_COMBOBOX(IDCB_CC_TRIGGERS, ConfigDialog::GeneralSettingsChanged)
	
	// Wiimote
	EVT_BUTTON(IDB_WM_A, ConfigDialog::OnButtonClick) EVT_BUTTON(IDB_WM_B, ConfigDialog::OnButtonClick)
	EVT_BUTTON(IDB_WM_1, ConfigDialog::OnButtonClick) EVT_BUTTON(IDB_WM_2, ConfigDialog::OnButtonClick)
	EVT_BUTTON(IDB_WM_P, ConfigDialog::OnButtonClick) EVT_BUTTON(IDB_WM_M, ConfigDialog::OnButtonClick) EVT_BUTTON(IDB_WM_H, ConfigDialog::OnButtonClick)
	EVT_BUTTON(IDB_WM_L, ConfigDialog::OnButtonClick) EVT_BUTTON(IDB_WM_R, ConfigDialog::OnButtonClick)
	EVT_BUTTON(IDB_WM_U, ConfigDialog::OnButtonClick) EVT_BUTTON(IDB_WM_D, ConfigDialog::OnButtonClick)
	EVT_BUTTON(IDB_WM_SHAKE, ConfigDialog::OnButtonClick)
	EVT_BUTTON(IDB_WM_PITCH_L, ConfigDialog::OnButtonClick) EVT_BUTTON(IDB_WM_PITCH_R, ConfigDialog::OnButtonClick)
	// IR cursor
	EVT_COMMAND_SCROLL(IDS_WIDTH, ConfigDialog::GeneralSettingsChangedScroll)
	EVT_COMMAND_SCROLL(IDS_HEIGHT, ConfigDialog::GeneralSettingsChangedScroll)
	EVT_COMMAND_SCROLL(IDS_LEFT, ConfigDialog::GeneralSettingsChangedScroll)
	EVT_COMMAND_SCROLL(IDS_TOP, ConfigDialog::GeneralSettingsChangedScroll)

	// Nunchuck
	EVT_BUTTON(IDB_NC_Z, ConfigDialog::OnButtonClick) EVT_BUTTON(IDB_NC_C, ConfigDialog::OnButtonClick)
	EVT_BUTTON(IDB_NC_L, ConfigDialog::OnButtonClick) EVT_BUTTON(IDB_NC_R, ConfigDialog::OnButtonClick)
	EVT_BUTTON(IDB_NC_U, ConfigDialog::OnButtonClick) EVT_BUTTON(IDB_NC_D, ConfigDialog::OnButtonClick)
	EVT_BUTTON(IDB_NC_SHAKE, ConfigDialog::OnButtonClick)

	// Classic Controller
	EVT_BUTTON(IDB_CC_A, ConfigDialog::OnButtonClick) EVT_BUTTON(IDB_CC_B, ConfigDialog::OnButtonClick) EVT_BUTTON(IDB_CC_X, ConfigDialog::OnButtonClick) EVT_BUTTON(IDB_CC_Y, ConfigDialog::OnButtonClick)
	EVT_BUTTON(IDB_CC_P, ConfigDialog::OnButtonClick) EVT_BUTTON(IDB_CC_M, ConfigDialog::OnButtonClick) EVT_BUTTON(IDB_CC_H, ConfigDialog::OnButtonClick)
	EVT_BUTTON(IDB_CC_TL, ConfigDialog::OnButtonClick) EVT_BUTTON(IDB_CC_ZL, ConfigDialog::OnButtonClick) EVT_BUTTON(IDB_CC_ZR, ConfigDialog::OnButtonClick) EVT_BUTTON(IDB_CC_TR, ConfigDialog::OnButtonClick)
	EVT_BUTTON(IDB_CC_DL, ConfigDialog::OnButtonClick) EVT_BUTTON(IDB_CC_DU, ConfigDialog::OnButtonClick) EVT_BUTTON(IDB_CC_DR, ConfigDialog::OnButtonClick) EVT_BUTTON(IDB_CC_DD, ConfigDialog::OnButtonClick)
	EVT_BUTTON(IDB_CC_DL, ConfigDialog::OnButtonClick) EVT_BUTTON(IDB_CC_DU, ConfigDialog::OnButtonClick) EVT_BUTTON(IDB_CC_DR, ConfigDialog::OnButtonClick) EVT_BUTTON(IDB_CC_DD, ConfigDialog::OnButtonClick)
	EVT_BUTTON(IDB_CC_LL, ConfigDialog::OnButtonClick) EVT_BUTTON(IDB_CC_LU, ConfigDialog::OnButtonClick) EVT_BUTTON(IDB_CC_LR, ConfigDialog::OnButtonClick) EVT_BUTTON(IDB_CC_LD, ConfigDialog::OnButtonClick)
	EVT_BUTTON(IDB_CC_RL, ConfigDialog::OnButtonClick) EVT_BUTTON(IDB_CC_RU, ConfigDialog::OnButtonClick) EVT_BUTTON(IDB_CC_RR, ConfigDialog::OnButtonClick) EVT_BUTTON(IDB_CC_RD, ConfigDialog::OnButtonClick)
	
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
	ClickedButton = NULL;

	g_Config.Load();
	CreateGUIControls();
	LoadFile();
	// Set control values
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

	// Save the key
	g_Pressed = event.GetKeyCode();

	// Escape a recording event
	if (event.GetKeyCode() == WXK_ESCAPE)
	{
		m_bWaitForRecording = false;
		m_bRecording = false;
		UpdateGUI();
	}

	// ----------------------------------------------------
	// Handle the keyboard key mapping
	// ------------------
	std::string StrKey;
	if(ClickedButton != NULL)
	{
		// Allow the escape key to set a blank key
		if (g_Pressed == WXK_ESCAPE)
		{
			SaveKeyboardMapping(Page, ClickedButton->GetId(), -1);
			SetButtonText(ClickedButton->GetId(), "");
			ClickedButton = NULL;
			return;
		}

		#ifdef _WIN32
			BYTE keyState[256];
			GetKeyboardState(keyState);
			for (int i = 1; i < 256; ++i)
			{
				if ((keyState[i] & 0x80) != 0)
				{
					// Use the left and right specific keys instead of the common ones
					if (i == VK_SHIFT || i == VK_CONTROL || i == VK_MENU) continue;
					// Update the button label
					char KeyStr[64] = {0}; strcpy(KeyStr, InputCommon::VKToString(i).c_str());
					SetButtonText(ClickedButton->GetId(), KeyStr);
					// Save the setting
					SaveKeyboardMapping(Page, ClickedButton->GetId(), i);
				}
			}
		#elif defined(HAVE_X11) && HAVE_X11
			//pad[page].keyForControl[ClickedButton->GetId()] = wxCharCodeWXToX(event.GetKeyCode());
			//ClickedButton->SetLabel(wxString::Format(_T("%c"), event.GetKeyCode()));
		#endif
	}

	// Remove the button control pointer
	ClickedButton = NULL;
	// ---------------------------
}

// Input button clicked
void ConfigDialog::OnButtonClick(wxCommandEvent& event)
{
	//Console::Print("OnButtonClick: %i\n", g_Pressed);

	// Don't allow space to start a new Press Key option, that will interfer with setting a key to space
	if (g_Pressed == WXK_SPACE) { g_Pressed = 0; return; }

	// Reset the old label
	if(ClickedButton) ClickedButton->SetLabel(OldLabel);

	// Create the button object
	ClickedButton = (wxButton *)event.GetEventObject();
	OldLabel = ClickedButton->GetLabel();
	ClickedButton->SetLabel(wxT("<Press Key>"));
	// Allow Tab and directional keys to
	ClickedButton->SetWindowStyle(wxWANTS_CHARS);
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
		// Save the settings for the current pad
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

	Console::Print("WiiMoteEmu::PadMapping[%i].ID = %i\n", Page, m_Joyname[Page]->GetSelection());
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
	dc.SetPen(*wxRED_PEN);
	dc.SetBrush(*wxRED_BRUSH);

	dc.Clear();
	dc.DrawRectangle(0, 0, w, h);
	dc.SelectObject(wxNullBitmap);
	return bitmap;
}
wxBitmap ConfigDialog::CreateBitmapDeadZone(int Radius)
{
	wxBitmap bitmap(Radius*2, Radius*2);
	wxMemoryDC dc;
	dc.SelectObject(bitmap);

	// Set outline and fill colors
	dc.SetPen(*wxLIGHT_GREY_PEN);
	dc.SetBrush(*wxLIGHT_GREY_BRUSH);
	
	//dc.SetBackground(*wxGREEN_BRUSH);
	dc.Clear();
	dc.DrawCircle(Radius, Radius, Radius);
	//dc.SelectObject(wxNullBitmap);
	return bitmap;
}
wxBitmap ConfigDialog::CreateBitmapClear()
{
	wxBitmap bitmap(BoxW, BoxH);
	wxMemoryDC dc;
	dc.SelectObject(bitmap);

	// Set outline and fill colors
	//dc.SetBrush(*wxTRANSPARENT_BRUSH);
	
	dc.Clear();
	//dc.DrawRectangle(0, 0, BoxW, BoxH);
	//dc.SelectObject(wxNullBitmap);
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
	if (WiiMoteEmu::NumGoodPads > 0)
	{
		for (int x = 0; x < (int)WiiMoteEmu::joyinfo.size(); x++)
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
	StrTilt.Add(wxString::FromAscii("Analog 1"));
	StrTilt.Add(wxString::FromAscii("Analog 2"));
	StrTilt.Add(wxString::FromAscii("Triggers"));
	// The range is in degrees and are set at even 5 degrees values
	wxArrayString StrTiltRangeRoll, StrTiltRangePitch;
	for (int i = 0; i < 37; i++)
		StrTiltRangeRoll.Add(wxString::Format(wxT("%i"), i*5));
	for (int i = 0; i < 37; i++)
		StrTiltRangePitch.Add(wxString::Format(wxT("%i"), i*5));

	// The Trigger type list
	wxArrayString StrTriggerType;
	StrTriggerType.Add(wxString::FromAscii("SDL")); // -0x8000 to 0x7fff
	StrTriggerType.Add(wxString::FromAscii("XInput")); // 0x00 to 0xff

	// The Nunchuck stick list
	wxArrayString StrNunchuck;
	StrNunchuck.Add(wxString::FromAscii("Keyboard"));
	StrNunchuck.Add(wxString::FromAscii("Analog 1"));
	StrNunchuck.Add(wxString::FromAscii("Analog 2"));

	// The Classic Controller triggers list
	wxArrayString StrCcTriggers;
	StrCcTriggers.Add(wxString::FromAscii("Keyboard"));
	StrCcTriggers.Add(wxString::FromAscii("Triggers"));

	// A small type font
	wxFont m_SmallFont(7, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL);
	///////////////////////////////////////

	////////////////////////////////////////////////////////////////////////////////
	// This can take a few seconds so we show a progress bar for it
	// ----------------	
	wxProgressDialog dialog(_T("Opening Wii Remote Configuration"),
				wxT("Loading controls..."),
				6, // range
				this, // parent
				wxPD_APP_MODAL |
				// wxPD_AUTO_HIDE | -- try this as well
				wxPD_ELAPSED_TIME |
				wxPD_ESTIMATED_TIME |
				wxPD_REMAINING_TIME |
				wxPD_SMOOTH // - makes indeterminate mode bar on WinXP very small
				);
	// I'm not sure what parent this refers to
	dialog.CenterOnParent();
	///////////////////////////////////////

	/* Populate all four pages. Page 2, 3 and 4 are currently disabled since we can't use more than one
	   Wiimote at the moment */
	for (int i = 0; i < 4; i++)
	{

		////////////////////////////////////////////////////
		// General and basic Settings
		// ----------------

		// Configuration controls sizes
		static const int TxtW = 50, TxtH = 19, ChW = 257, BtW = 75, BtH = 20;

		// Basic Settings
		m_WiimoteOnline[i] = new wxCheckBox(m_Controller[i], IDC_WIMOTE_ON, wxT("Wiimote On"), wxDefaultPosition, wxSize(ChW, -1));
		// Emulated Wiimote
		m_SidewaysDPad[i] = new wxCheckBox(m_Controller[i], ID_SIDEWAYSDPAD, wxT("Sideways D-Pad"), wxDefaultPosition, wxSize(ChW, -1));
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
		m_ConnectRealWiimote[0]->SetValue(g_Config.bConnectRealWiimote);
		m_UseRealWiimote[0]->SetValue(g_Config.bUseRealWiimote);

		m_WiimoteOnline[0]->Enable(false);
		m_WiiMotionPlusConnected[0]->Enable(false);
		m_BalanceBoardConnected[0]->Enable(false);
		m_GuitarHeroGuitarConnected[0]->Enable(false);
		m_GuitarHeroWorldTourDrumsConnected[0]->Enable(false);

		// Tooltips
		m_WiimoteOnline[i]->SetToolTip(wxString::Format(wxT("Decide if Wiimote %i shall be detected by the game"), i));
		m_ConnectRealWiimote[i]->SetToolTip(wxT("Connected to the real wiimote. This can not be changed during gameplay."));
		m_UseRealWiimote[i]->SetToolTip(wxT(
			"Use the real Wiimote in the game. This can be changed during gameplay. This can not be selected"
			" when a recording is to be done. No status in this window will be updated when this is checked."));
		
		// -----------------------------------------------
		// Screen size
		// ---------------------
		// Controls
		m_TextScreenWidth[i] = new wxStaticText(m_Controller[i], wxID_ANY, wxT("Width: 000"));
		m_TextScreenHeight[i] = new wxStaticText(m_Controller[i], wxID_ANY, wxT("Height: 000"));
		m_TextScreenLeft[i] = new wxStaticText(m_Controller[i], wxID_ANY, wxT("Left: 000"));
		m_TextScreenTop[i] = new wxStaticText(m_Controller[i], wxID_ANY, wxT("Top: 000"));
		m_TextAR[i] = new wxStaticText(m_Controller[i], wxID_ANY, wxT("Aspect Ratio"));
				
		m_SliderWidth[i] = new wxSlider(m_Controller[i], IDS_WIDTH, 0, 100, 923, wxDefaultPosition, wxSize(75, -1));
		m_SliderHeight[i] = new wxSlider(m_Controller[i], IDS_HEIGHT, 0, 0, 727, wxDefaultPosition, wxSize(75, -1));
		m_SliderLeft[i] = new wxSlider(m_Controller[i], IDS_LEFT, 0, 100, 500, wxDefaultPosition, wxSize(75, -1));
		m_SliderTop[i] = new wxSlider(m_Controller[i], IDS_TOP, 0, 0, 500, wxDefaultPosition, wxSize(75, -1));
		//m_ScreenSize = new wxCheckBox(m_Controller[i], IDC_SCREEN_SIZE, wxT("Adjust screen size and position"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);

		m_CheckAR43[i] = new wxCheckBox(m_Controller[i], wxID_ANY, wxT("4:3"), wxDefaultPosition, wxSize(-1, -1), 0, wxDefaultValidator);
		m_CheckAR169[i] = new wxCheckBox(m_Controller[i], wxID_ANY, wxT("16:9"), wxDefaultPosition, wxSize(-1, -1), 0, wxDefaultValidator);
		m_Crop[i] = new wxCheckBox(m_Controller[i], wxID_ANY, wxT("Crop"), wxDefaultPosition, wxSize(-1, -1), 0, wxDefaultValidator);

		// Sizers
		m_SizerIRPointerWidth[i] = new wxBoxSizer(wxHORIZONTAL);
		m_SizerIRPointerWidth[i]->Add(m_TextScreenLeft[i], 0, wxEXPAND | (wxTOP), 3);
		m_SizerIRPointerWidth[i]->Add(m_SliderLeft[i], 0, wxEXPAND | (wxRIGHT), 0);
		m_SizerIRPointerWidth[i]->Add(m_TextScreenWidth[i], 0, wxEXPAND | (wxTOP), 3);
		m_SizerIRPointerWidth[i]->Add(m_SliderWidth[i], 0, wxEXPAND | (wxLEFT), 0);

		m_SizerIRPointerHeight[i] = new wxBoxSizer(wxHORIZONTAL);
		m_SizerIRPointerHeight[i]->Add(m_TextScreenTop[i], 0, wxEXPAND | (wxTOP), 3);
		m_SizerIRPointerHeight[i]->Add(m_SliderTop[i], 0, wxEXPAND | (wxRIGHT), 0);
		m_SizerIRPointerHeight[i]->Add(m_TextScreenHeight[i], 0, wxEXPAND | (wxTOP), 3);
		m_SizerIRPointerHeight[i]->Add(m_SliderHeight[i], 0, wxEXPAND | (wxLEFT), 0);

		m_SizerIRPointerScreen[i] = new wxBoxSizer(wxHORIZONTAL);
		m_SizerIRPointerScreen[i]->Add(m_TextAR[i], 0, wxEXPAND | (wxTOP), 0);
		m_SizerIRPointerScreen[i]->Add(m_CheckAR43[i], 0, wxEXPAND | (wxLEFT), 5);
		m_SizerIRPointerScreen[i]->Add(m_CheckAR169[i], 0, wxEXPAND | (wxLEFT), 5);
		m_SizerIRPointerScreen[i]->Add(m_Crop[i], 0, wxEXPAND | (wxLEFT), 5);

		m_SizerIRPointer[i] = new wxStaticBoxSizer(wxVERTICAL, m_Controller[i], wxT("IR pointer"));
		//m_SizerIRPointer[i]->Add(m_ScreenSize[i], 0, wxEXPAND | (wxALL), 5);
		m_SizerIRPointer[i]->Add(m_SizerIRPointerWidth[i], 0, wxEXPAND | (wxLEFT | wxDOWN | wxRIGHT), 5);
		m_SizerIRPointer[i]->Add(m_SizerIRPointerHeight[i], 0, wxEXPAND | (wxLEFT | wxDOWN | wxRIGHT), 5);
		m_SizerIRPointer[i]->Add(m_SizerIRPointerScreen[i], 0, wxEXPAND | (wxLEFT | wxDOWN | wxRIGHT), 5);

		// Default values
		m_CheckAR43[i]->SetValue(g_Config.bKeepAR43);
		m_CheckAR169[i]->SetValue(g_Config.bKeepAR169);
		m_Crop[i]->SetValue(g_Config.bCrop);

		// These are changed from the graphics plugin settings, so they are just here to show the loaded status
		m_TextAR[i]->Enable(false);
		m_CheckAR43[i]->Enable(false);
		m_CheckAR169[i]->Enable(false);
		m_Crop[i]->Enable(false);		

		// Tool tips
		//m_ScreenSize[i]->SetToolTip(wxT("Use the adjusted screen size."));
		// -------------------------------

		// --------------------------------------------------------------------
		// Row 1 Sizers: General settings
		// -----------------------------
		m_SizeBasic[i] = new wxStaticBoxSizer(wxVERTICAL, m_Controller[i], wxT("General Settings"));
		m_SizeEmu[i] = new wxStaticBoxSizer(wxVERTICAL, m_Controller[i], wxT("Emulated Wiimote"));
		m_SizeExtensions[i] = new wxStaticBoxSizer(wxVERTICAL, m_Controller[i], wxT("Emulated Extension"));
		m_SizeReal[i] = new wxStaticBoxSizer(wxVERTICAL, m_Controller[i], wxT("Real Wiimote"));

		m_SizeBasicPadding[i] = new wxBoxSizer(wxVERTICAL); m_SizeBasic[i]->Add(m_SizeBasicPadding[i], 0, wxEXPAND | (wxALL), 5);
		m_SizeBasicPadding[i]->Add(m_WiimoteOnline[i], 0, wxEXPAND | (wxUP), 2);

		m_SizeEmuPadding[i] = new wxBoxSizer(wxVERTICAL); m_SizeEmu[i]->Add(m_SizeEmuPadding[i], 0, wxEXPAND | (wxALL), 5);
		m_SizeEmuPadding[i]->Add(m_SidewaysDPad[i], 0, wxEXPAND | (wxUP), 0);

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

		m_SizeBasicGeneralLeft[i]->Add(m_SizeReal[i], 0, wxEXPAND | (wxUP), 0);
		m_SizeBasicGeneralLeft[i]->Add(m_SizeExtensions[i], 0, wxEXPAND | (wxUP), 5);

		m_SizeBasicGeneralRight[i]->Add(m_SizeBasic[i], 0, wxEXPAND | (wxUP), 0);
		m_SizeBasicGeneralRight[i]->Add(m_SizeEmu[i], 0, wxEXPAND | (wxUP), 5);
		m_SizeBasicGeneralRight[i]->Add(m_SizerIRPointer[i], 0, wxEXPAND | (wxUP), 5);

		m_SizeBasicGeneral[i]->Add(m_SizeBasicGeneralLeft[i], 0, wxEXPAND | (wxUP), 0);
		m_SizeBasicGeneral[i]->Add(m_SizeBasicGeneralRight[i], 0, wxEXPAND | (wxLEFT), 5);
		// ------------------------

		///////////////////////////




		////////////////////////////////////////////////////////////////////////
		// Gamepad input
		// ----------------

		// --------------------------------------------------------------------
		// Controller
		// -----------------------------

		// Controller
		m_Joyname[i] = new wxComboBox(m_Controller[i], IDC_JOYNAME, StrJoyname[0], wxDefaultPosition, wxSize(200, -1), StrJoyname, wxCB_READONLY);
	
		// Circle to square
		m_CheckC2S[i] = new wxCheckBox(m_Controller[i], IDC_LEFT_C2S, wxT("Circle To Square"));

		// The drop down menu for the circle to square adjustment
		m_CheckC2SLabel[i] = new wxStaticText(m_Controller[i], wxID_ANY, wxT("Diagonal"));
		wxArrayString asStatusInSet;
			asStatusInSet.Add(wxT("100%"));
			asStatusInSet.Add(wxT("95%"));
			asStatusInSet.Add(wxT("90%"));
			asStatusInSet.Add(wxT("85%"));
			asStatusInSet.Add(wxT("80%"));
		m_ComboDiagonal[i] = new wxComboBox(m_Controller[i], IDCB_LEFT_DIAGONAL, asStatusInSet[0], wxDefaultPosition, wxDefaultSize, asStatusInSet, wxCB_READONLY);

		// Dead zone
		m_ComboDeadZoneLabel[i] = new wxStaticText(m_Controller[i], wxID_ANY, wxT("Dead Zone"));
		wxArrayString TextDeadZone;
		for (int j = 0; j <= 50; j++) TextDeadZone.Add(wxString::Format(wxT("%i%%"), j));
		m_ComboDeadZoneLeft[i] = new wxComboBox(m_Controller[i], IDCB_DEAD_ZONE_LEFT, TextDeadZone[0], wxDefaultPosition, wxDefaultSize, TextDeadZone, wxCB_READONLY);
		m_ComboDeadZoneRight[i] = new wxComboBox(m_Controller[i], IDCB_DEAD_ZONE_RIGHT, TextDeadZone[0], wxDefaultPosition, wxDefaultSize, TextDeadZone, wxCB_READONLY);

		// Tooltips
		m_Joyname[i]->SetToolTip(wxT("Save your settings and configure another joypad"));
		m_CheckC2S[i]->SetToolTip(wxT(
			"This will convert a circular stick radius to a square stick radius."
			" This can be useful for the pitch and roll emulation."
			));
		m_CheckC2SLabel[i]->SetToolTip(wxT(
			"To produce a perfect square circle in the 'Out' window you have to manually set"
			"\nyour diagonal values here from what is shown in the 'In' window."
			));

		// Sizers
		m_gDeadZone[i] = new wxBoxSizer(wxVERTICAL);
		m_gDeadZoneHoriz[i] = new wxBoxSizer(wxHORIZONTAL);
		m_gDeadZoneHoriz[i]->Add(m_ComboDeadZoneLeft[i], 0, (wxUP), 0);
		m_gDeadZoneHoriz[i]->Add(m_ComboDeadZoneRight[i], 0, (wxUP), 0);
		m_gDeadZone[i]->Add(m_ComboDeadZoneLabel[i], 0, wxALIGN_CENTER | (wxUP), 0);	
		m_gDeadZone[i]->Add(m_gDeadZoneHoriz[i], 0, wxALIGN_CENTER | (wxUP), 2);

		m_gCircle2Square[i] = new wxBoxSizer(wxHORIZONTAL);
		m_gCircle2Square[i]->Add(m_CheckC2SLabel[i], 0, (wxUP), 4);
		m_gCircle2Square[i]->Add(m_ComboDiagonal[i], 0, (wxLEFT), 2);

		m_gCircle2SquareVert[i] = new wxBoxSizer(wxVERTICAL);
		m_gCircle2SquareVert[i]->Add(m_CheckC2S[i], 0, wxALIGN_CENTER | (wxUP), 0);
		m_gCircle2SquareVert[i]->Add(m_gCircle2Square[i], 0, wxALIGN_CENTER | (wxUP), 2);

		m_gC2SDeadZone[i] = new wxBoxSizer(wxHORIZONTAL);
		m_gC2SDeadZone[i]->Add(m_gDeadZone[i], 0, (wxUP), 0);
		m_gC2SDeadZone[i]->Add(m_gCircle2SquareVert[i], 0, (wxLEFT), 8);

		m_gJoyname[i] = new wxStaticBoxSizer (wxVERTICAL, m_Controller[i], wxT("Gamepad"));
		m_gJoyname[i]->AddStretchSpacer();
		m_gJoyname[i]->Add(m_Joyname[i], 0, wxALIGN_CENTER | (wxLEFT | wxRIGHT | wxDOWN), 5);
		m_gJoyname[i]->Add(m_gC2SDeadZone[i], 0, wxALIGN_CENTER | (wxLEFT | wxRIGHT | wxDOWN), 5);
		m_gJoyname[i]->AddStretchSpacer();


		// --------------------------------------------------------------------
		// Tilt Wiimote
		// -----------------------------
		/**/
		// Controls
		m_TiltComboInput[i] = new wxComboBox(m_Controller[i], ID_TILT_INPUT, StrTilt[0], wxDefaultPosition, wxDefaultSize, StrTilt, wxCB_READONLY);
		m_TiltComboRangeRoll[i] = new wxComboBox(m_Controller[i], ID_TILT_RANGE_ROLL, StrTiltRangeRoll[0], wxDefaultPosition, wxDefaultSize, StrTiltRangeRoll, wxCB_READONLY);
		m_TiltComboRangePitch[i] = new wxComboBox(m_Controller[i], ID_TILT_RANGE_PITCH, StrTiltRangePitch[0], wxDefaultPosition, wxDefaultSize, StrTiltRangePitch, wxCB_READONLY);
		m_TiltTextRoll[i] = new wxStaticText(m_Controller[i], wxID_ANY, wxT("Roll"));
		m_TiltTextPitch[i] = new wxStaticText(m_Controller[i], wxID_ANY, wxT("Pitch"));
		m_TiltInvertRoll[i] = new wxCheckBox(m_Controller[i], ID_TILT_INVERT_ROLL, wxT("Invert"));
		m_TiltInvertPitch[i] = new wxCheckBox(m_Controller[i], ID_TILT_INVERT_PITCH, wxT("Invert"));

		// Sizers
		m_TiltGrid[i] = new wxGridBagSizer(0, 0);
		m_TiltGrid[i]->Add(m_TiltTextRoll[i], wxGBPosition(0, 0), wxGBSpan(1, 1), (wxTOP), 4);
		m_TiltGrid[i]->Add(m_TiltComboRangeRoll[i], wxGBPosition(0, 1), wxGBSpan(1, 1), (wxLEFT), 2);
		m_TiltGrid[i]->Add(m_TiltInvertRoll[i], wxGBPosition(0, 2), wxGBSpan(1, 1), (wxLEFT | wxTOP), 4);

		m_TiltGrid[i]->Add(m_TiltTextPitch[i], wxGBPosition(1, 0), wxGBSpan(1, 1), (wxTOP), 6);
		m_TiltGrid[i]->Add(m_TiltComboRangePitch[i], wxGBPosition(1, 1), wxGBSpan(1, 1), (wxLEFT | wxTOP | wxDOWN), 2);
		m_TiltGrid[i]->Add(m_TiltInvertPitch[i], wxGBPosition(1, 2), wxGBSpan(1, 1), (wxLEFT | wxTOP), 4);

		// For additional padding options if needed
		//m_TiltHoriz[i] = new wxBoxSizer(wxHORIZONTAL);

		m_gTilt[i] = new wxStaticBoxSizer (wxVERTICAL, m_Controller[i], wxT("Roll and pitch"));
		m_gTilt[i]->AddStretchSpacer();
		m_gTilt[i]->Add(m_TiltComboInput[i], 0, wxEXPAND | (wxLEFT | wxRIGHT | wxDOWN), 5);
		m_gTilt[i]->Add(m_TiltGrid[i], 0, wxEXPAND | (wxLEFT | wxRIGHT), 5);
		m_gTilt[i]->AddStretchSpacer();

		//Set values
		m_TiltComboInput[i]->SetSelection(g_Config.Trigger.Type);
		m_TiltComboRangeRoll[i]->SetValue(wxString::Format(wxT("%i"), g_Config.Trigger.Range.Roll));
		m_TiltComboRangePitch[i]->SetValue(wxString::Format(wxT("%i"), g_Config.Trigger.Range.Pitch));

		// Tooltips
		m_TiltComboInput[i]->SetToolTip(wxT("Control tilting by an analog gamepad stick, an analog trigger or the keyboard."));		
		m_TiltComboRangeRoll[i]->SetToolTip(wxT("The maximum roll in degrees. Set to 0 to turn off."));	
		m_TiltComboRangePitch[i]->SetToolTip(wxT("The maximum pitch in degrees. Set to 0 to turn off."));

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

		m_SizeAnalogTriggerStatusBox[i] = new wxGridBagSizer(0, 0);
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
		m_SizeAnalogTriggerHorizInput[i]->Add(m_tAnalogTriggerInput[i], 0, (wxUP), 3);
		m_SizeAnalogTriggerHorizInput[i]->Add(m_TriggerType[i], 0, (wxLEFT), 2);

		// The status text boxes
		m_SizeAnalogTriggerVertLeft[i]->AddStretchSpacer();
		m_SizeAnalogTriggerVertLeft[i]->Add(m_SizeAnalogTriggerStatusBox[i]);
		m_SizeAnalogTriggerVertLeft[i]->AddStretchSpacer();

		// The config grid and the input type choice box
		m_SizeAnalogTriggerVertRight[i]->Add(m_SizeAnalogTriggerHorizConfig[i], 0, (wxUP), 2);
		m_SizeAnalogTriggerVertRight[i]->Add(m_SizeAnalogTriggerHorizInput[i], 0, (wxUP | wxDOWN), 4);

		m_gTrigger[i]->Add(m_SizeAnalogTriggerVertLeft[i], 0, wxEXPAND | (wxLEFT | wxRIGHT), 5);
		m_gTrigger[i]->Add(m_SizeAnalogTriggerVertRight[i], 0, wxEXPAND | (wxLEFT | wxRIGHT), 5);

		// --------------------------------------------------------------------
		// Row 2 Sizers: Connected pads, tilt, triggers
		// -----------------------------
		m_HorizControllerTilt[i] = new wxBoxSizer(wxHORIZONTAL);
		m_HorizControllerTilt[i]->Add(m_gJoyname[i], 0, wxALIGN_CENTER | wxEXPAND, 0);
		m_HorizControllerTilt[i]->Add(m_gTilt[i], 0, wxEXPAND | (wxLEFT), 5);
		m_HorizControllerTilt[i]->Add(m_gTrigger[i], 0, wxEXPAND | (wxLEFT), 5);

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
		m_bmpDeadZoneLeftIn[i] = new wxStaticBitmap(m_pLeftInStatus[i], wxID_ANY, CreateBitmapDeadZone(0), wxDefaultPosition, wxDefaultSize);
		m_bmpDotLeftIn[i] = new wxStaticBitmap(m_pLeftInStatus[i], wxID_ANY, CreateBitmapDot(), wxPoint(BoxW / 2, BoxH / 2), wxDefaultSize);

		m_pLeftOutStatus[i] = new wxPanel(m_Controller[i], wxID_ANY, wxDefaultPosition, wxDefaultSize);
		m_bmpSquareLeftOut[i] = new wxStaticBitmap(m_pLeftOutStatus[i], wxID_ANY, CreateBitmap(), wxDefaultPosition, wxDefaultSize);
		m_bmpDotLeftOut[i] = new wxStaticBitmap(m_pLeftOutStatus[i], wxID_ANY, CreateBitmapDot(), wxPoint(BoxW / 2, BoxH / 2), wxDefaultSize);

		m_pRightInStatus[i] = new wxPanel(m_Controller[i], wxID_ANY, wxDefaultPosition, wxDefaultSize);
		m_bmpSquareRightIn[i] = new wxStaticBitmap(m_pRightInStatus[i], wxID_ANY, CreateBitmap(), wxDefaultPosition, wxDefaultSize);
		m_bmpDeadZoneRightIn[i] = new wxStaticBitmap(m_pRightInStatus[i], wxID_ANY, CreateBitmapDeadZone(0), wxDefaultPosition, wxDefaultSize);
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
		//m_HorizControllers[i]->AddStretchSpacer();
		m_HorizControllers[i]->AddSpacer(17);
		m_HorizControllers[i]->Add(m_gAnalogLeft[i]);
		m_HorizControllers[i]->Add(m_gAnalogRight[i], 0, (wxLEFT), 5);
		//m_HorizControllers[i]->AddStretchSpacer();
		///////////////////////////


		////////////////////////////////////////////////////////////////////////
		// Keyboard mapping
		// ----------------
		
		// --------------------------------------------------------------------
		// Wiimote
		// -----------------------------

		m_tWmA[i] = new wxStaticText(m_Controller[i], wxID_ANY, wxT("A"));
		m_tWmB[i] = new wxStaticText(m_Controller[i], wxID_ANY, wxT("B"));
		m_tWm1[i] = new wxStaticText(m_Controller[i], wxID_ANY, wxT("1"));
		m_tWm2[i] = new wxStaticText(m_Controller[i], wxID_ANY, wxT("2"));
		m_tWmP[i] = new wxStaticText(m_Controller[i], wxID_ANY, wxT("+"));
		m_tWmM[i] = new wxStaticText(m_Controller[i], wxID_ANY, wxT("- ")); // Intentional space
		m_tWmH[i] = new wxStaticText(m_Controller[i], wxID_ANY, wxT("Home"));
		m_tWmL[i] = new wxStaticText(m_Controller[i], wxID_ANY, wxT("Left"));
		m_tWmR[i] = new wxStaticText(m_Controller[i], wxID_ANY, wxT("Right"));
		m_tWmU[i] = new wxStaticText(m_Controller[i], wxID_ANY, wxT("Up"));
		m_tWmD[i] = new wxStaticText(m_Controller[i], wxID_ANY, wxT("Down"));
		m_tWmShake[i] = new wxStaticText(m_Controller[i], wxID_ANY, wxT("Shake"));
		m_tWmPitchL[i] = new wxStaticText(m_Controller[i], wxID_ANY, wxT("Pitch Left"));
		m_tWmPitchR[i] = new wxStaticText(m_Controller[i], wxID_ANY, wxT("Pitch Right"));

		m_bWmA[i] = new wxButton(m_Controller[i], IDB_WM_A, wxEmptyString, wxDefaultPosition, wxSize(BtW, BtH));
		m_bWmB[i] = new wxButton(m_Controller[i], IDB_WM_B, wxEmptyString, wxDefaultPosition, wxSize(BtW, BtH));
		m_bWm1[i] = new wxButton(m_Controller[i], IDB_WM_1, wxEmptyString, wxDefaultPosition, wxSize(BtW, BtH));
		m_bWm2[i] = new wxButton(m_Controller[i], IDB_WM_2, wxEmptyString, wxDefaultPosition, wxSize(BtW, BtH));
		m_bWmP[i] = new wxButton(m_Controller[i], IDB_WM_P, wxEmptyString, wxDefaultPosition, wxSize(BtW, BtH));
		m_bWmM[i] = new wxButton(m_Controller[i], IDB_WM_M, wxEmptyString, wxDefaultPosition, wxSize(BtW, BtH));
		m_bWmH[i] = new wxButton(m_Controller[i], IDB_WM_H, wxEmptyString, wxDefaultPosition, wxSize(BtW, BtH));
		m_bWmL[i] = new wxButton(m_Controller[i], IDB_WM_L, wxEmptyString, wxDefaultPosition, wxSize(BtW, BtH));
		m_bWmR[i] = new wxButton(m_Controller[i], IDB_WM_R, wxEmptyString, wxDefaultPosition, wxSize(BtW, BtH));
		m_bWmU[i] = new wxButton(m_Controller[i], IDB_WM_U, wxEmptyString, wxDefaultPosition, wxSize(BtW, BtH));
		m_bWmD[i] = new wxButton(m_Controller[i], IDB_WM_D, wxEmptyString, wxDefaultPosition, wxSize(BtW, BtH));
		m_bWmShake[i] = new wxButton(m_Controller[i], IDB_WM_SHAKE, wxEmptyString, wxDefaultPosition, wxSize(BtW, BtH));
		m_bWmPitchL[i] = new wxButton(m_Controller[i], IDB_WM_PITCH_L, wxEmptyString, wxDefaultPosition, wxSize(BtW, BtH));
		m_bWmPitchR[i] = new wxButton(m_Controller[i], IDB_WM_PITCH_R, wxEmptyString, wxDefaultPosition, wxSize(BtW, BtH));

		// Set small font
		m_bWmA[i]->SetFont(m_SmallFont); m_bWmB[i]->SetFont(m_SmallFont);
		m_bWm1[i]->SetFont(m_SmallFont); m_bWm2[i]->SetFont(m_SmallFont);
		m_bWmP[i]->SetFont(m_SmallFont); m_bWmM[i]->SetFont(m_SmallFont); m_bWmH[i]->SetFont(m_SmallFont);
		m_bWmL[i]->SetFont(m_SmallFont); m_bWmR[i]->SetFont(m_SmallFont);
		m_bWmU[i]->SetFont(m_SmallFont); m_bWmD[i]->SetFont(m_SmallFont);
		m_bWmShake[i]->SetFont(m_SmallFont);
		m_bWmPitchL[i]->SetFont(m_SmallFont); m_bWmPitchR[i]->SetFont(m_SmallFont);
	
		// Sizers
		m_SWmVertLeft[i] = new wxBoxSizer(wxVERTICAL); m_SWmVertRight[i] = new wxBoxSizer(wxVERTICAL);
		m_SWmA[i] = new wxBoxSizer(wxHORIZONTAL); m_SWmB[i] = new wxBoxSizer(wxHORIZONTAL);
		m_SWm1[i] = new wxBoxSizer(wxHORIZONTAL); m_SWm2[i] = new wxBoxSizer(wxHORIZONTAL);
		m_SWmP[i] = new wxBoxSizer(wxHORIZONTAL); m_SWmM[i] = new wxBoxSizer(wxHORIZONTAL); m_SWmH[i] = new wxBoxSizer(wxHORIZONTAL);
		m_SWmL[i] = new wxBoxSizer(wxHORIZONTAL); m_SWmR[i] = new wxBoxSizer(wxHORIZONTAL);
		m_SWmU[i] = new wxBoxSizer(wxHORIZONTAL); m_SWmD[i] = new wxBoxSizer(wxHORIZONTAL);
		m_SWmShake[i] = new wxBoxSizer(wxHORIZONTAL);
		m_SWmPitchL[i] = new wxBoxSizer(wxHORIZONTAL); m_SWmPitchR[i] = new wxBoxSizer(wxHORIZONTAL);

		m_SWmA[i]->Add(m_tWmA[i], 0, (wxUP), 4); m_SWmA[i]->Add(m_bWmA[i], 0, (wxLEFT), 2);
		m_SWmB[i]->Add(m_tWmB[i], 0, (wxUP), 4); m_SWmB[i]->Add(m_bWmB[i], 0, (wxLEFT), 2);
		m_SWm1[i]->Add(m_tWm1[i], 0, (wxUP), 4); m_SWm1[i]->Add(m_bWm1[i], 0, (wxLEFT), 2);
		m_SWm2[i]->Add(m_tWm2[i], 0, (wxUP), 4); m_SWm2[i]->Add(m_bWm2[i], 0, (wxLEFT), 2);
		m_SWmP[i]->Add(m_tWmP[i], 0, (wxUP), 4); m_SWmP[i]->Add(m_bWmP[i], 0, (wxLEFT), 2);
		m_SWmM[i]->Add(m_tWmM[i], 0, (wxUP), 4); m_SWmM[i]->Add(m_bWmM[i], 0, (wxLEFT), 2);
		m_SWmH[i]->Add(m_tWmH[i], 0, (wxUP), 4); m_SWmH[i]->Add(m_bWmH[i], 0, (wxLEFT), 2);
		m_SWmL[i]->Add(m_tWmL[i], 0, (wxUP), 4); m_SWmL[i]->Add(m_bWmL[i], 0, (wxLEFT), 2);
		m_SWmR[i]->Add(m_tWmR[i], 0, (wxUP), 4); m_SWmR[i]->Add(m_bWmR[i], 0, (wxLEFT), 2);
		m_SWmU[i]->Add(m_tWmU[i], 0, (wxUP), 4); m_SWmU[i]->Add(m_bWmU[i], 0, (wxLEFT), 2);
		m_SWmD[i]->Add(m_tWmD[i], 0, (wxUP), 4); m_SWmD[i]->Add(m_bWmD[i], 0, (wxLEFT), 2);
		m_SWmShake[i]->Add(m_tWmShake[i], 0, (wxUP), 4); m_SWmShake[i]->Add(m_bWmShake[i], 0, (wxLEFT), 2);
		m_SWmPitchL[i]->Add(m_tWmPitchL[i], 0, (wxUP), 4); m_SWmPitchL[i]->Add(m_bWmPitchL[i], 0, (wxLEFT), 2);
		m_SWmPitchR[i]->Add(m_tWmPitchR[i], 0, (wxUP), 4); m_SWmPitchR[i]->Add(m_bWmPitchR[i], 0, (wxLEFT), 2);

		m_SWmVertLeft[i]->Add(m_SWmA[i], 0, wxALIGN_RIGHT | (wxALL), 1);
		m_SWmVertLeft[i]->Add(m_SWmB[i], 0, wxALIGN_RIGHT | (wxLEFT | wxRIGHT | wxDOWN), 1);
		m_SWmVertLeft[i]->Add(m_SWm1[i], 0, wxALIGN_RIGHT | (wxLEFT | wxRIGHT | wxDOWN), 1);
		m_SWmVertLeft[i]->Add(m_SWm2[i], 0, wxALIGN_RIGHT | (wxLEFT | wxRIGHT | wxDOWN), 1);
		m_SWmVertLeft[i]->Add(m_SWmP[i], 0, wxALIGN_RIGHT | (wxLEFT | wxRIGHT | wxDOWN), 1);
		m_SWmVertLeft[i]->Add(m_SWmM[i], 0, wxALIGN_RIGHT | (wxLEFT | wxRIGHT | wxDOWN), 1);
		m_SWmVertLeft[i]->Add(m_SWmH[i], 0, wxALIGN_RIGHT | (wxLEFT | wxRIGHT | wxDOWN), 1);

		m_SWmVertRight[i]->Add(m_SWmShake[i], 0, wxALIGN_RIGHT | (wxALL), 1);
		//m_SWmVertRight[i]->AddSpacer(3);
		m_SWmVertRight[i]->Add(m_SWmPitchL[i], 0, wxALIGN_RIGHT | (wxLEFT | wxRIGHT | wxDOWN), 1);
		m_SWmVertRight[i]->Add(m_SWmPitchR[i], 0, wxALIGN_RIGHT | (wxLEFT | wxRIGHT | wxDOWN), 1);
		//m_SWmVertRight[i]->AddSpacer(3);
		m_SWmVertRight[i]->Add(m_SWmL[i], 0, wxALIGN_RIGHT | (wxLEFT | wxRIGHT | wxDOWN), 1);
		m_SWmVertRight[i]->Add(m_SWmR[i], 0, wxALIGN_RIGHT | (wxLEFT | wxRIGHT | wxDOWN), 1);
		m_SWmVertRight[i]->Add(m_SWmU[i], 0, wxALIGN_RIGHT | (wxLEFT | wxRIGHT | wxDOWN), 1);
		m_SWmVertRight[i]->Add(m_SWmD[i], 0, wxALIGN_RIGHT | (wxLEFT | wxRIGHT | wxDOWN), 1);

		m_gWiimote[i] = new wxStaticBoxSizer (wxHORIZONTAL, m_Controller[i], wxT("Wiimote"));
		m_gWiimote[i]->Add(m_SWmVertLeft[i], 0, wxALIGN_RIGHT | (wxALL), 0);
		m_gWiimote[i]->Add(m_SWmVertRight[i], 0, wxALIGN_RIGHT | (wxLEFT), 5);
		m_gWiimote[i]->AddSpacer(1);

		// --------------------------------------------------------------------
		// Nunchuck
		// -----------------------------

		// Stick controls
		m_NunchuckTextStick[i] = new wxStaticText(m_Controller[i], wxID_ANY, wxT("Stick"));
		m_NunchuckComboStick[i] = new wxComboBox(m_Controller[i], IDCB_NUNCHUCK_STICK, StrNunchuck[0], wxDefaultPosition, wxDefaultSize, StrNunchuck, wxCB_READONLY);

		m_tNcZ[i] = new wxStaticText(m_Controller[i], wxID_ANY, wxT("Z"));
		m_tNcC[i] = new wxStaticText(m_Controller[i], wxID_ANY, wxT("C"));
		m_tNcL[i] = new wxStaticText(m_Controller[i], wxID_ANY, wxT("Left"));
		m_tNcR[i] = new wxStaticText(m_Controller[i], wxID_ANY, wxT("Right"));
		m_tNcU[i] = new wxStaticText(m_Controller[i], wxID_ANY, wxT("Up"));
		m_tNcD[i] = new wxStaticText(m_Controller[i], wxID_ANY, wxT("Down"));
		m_tNcShake[i] = new wxStaticText(m_Controller[i], wxID_ANY, wxT("Shake"));

		m_bNcZ[i] = new wxButton(m_Controller[i], IDB_NC_Z, wxEmptyString, wxDefaultPosition, wxSize(BtW, BtH));
		m_bNcC[i] = new wxButton(m_Controller[i], IDB_NC_C, wxEmptyString, wxDefaultPosition, wxSize(BtW, BtH));
		m_bNcL[i] = new wxButton(m_Controller[i], IDB_NC_L, wxEmptyString, wxDefaultPosition, wxSize(BtW, BtH));
		m_bNcR[i] = new wxButton(m_Controller[i], IDB_NC_R, wxEmptyString, wxDefaultPosition, wxSize(BtW, BtH));
		m_bNcU[i] = new wxButton(m_Controller[i], IDB_NC_U, wxEmptyString, wxDefaultPosition, wxSize(BtW, BtH));
		m_bNcD[i] = new wxButton(m_Controller[i], IDB_NC_D, wxEmptyString, wxDefaultPosition, wxSize(BtW, BtH));
		m_bNcShake[i] = new wxButton(m_Controller[i], IDB_NC_SHAKE, wxEmptyString, wxDefaultPosition, wxSize(BtW, BtH));

		// Set small font
		m_bNcShake[i]->SetFont(m_SmallFont);
		m_bNcZ[i]->SetFont(m_SmallFont);
		m_bNcC[i]->SetFont(m_SmallFont);
		m_bNcL[i]->SetFont(m_SmallFont); m_bNcR[i]->SetFont(m_SmallFont);
		m_bNcU[i]->SetFont(m_SmallFont); m_bNcD[i]->SetFont(m_SmallFont);
		m_bNcShake[i]->SetFont(m_SmallFont);

		// Sizers
		m_NunchuckStick[i] = new wxBoxSizer(wxHORIZONTAL);
		m_NunchuckStick[i]->Add(m_NunchuckTextStick[i], 0, (wxUP), 4);
		m_NunchuckStick[i]->Add(m_NunchuckComboStick[i], 0, (wxLEFT), 2);

		m_SNcZ[i] = new wxBoxSizer(wxHORIZONTAL); m_SNcC[i] = new wxBoxSizer(wxHORIZONTAL);
		m_SNcL[i] = new wxBoxSizer(wxHORIZONTAL); m_SNcR[i] = new wxBoxSizer(wxHORIZONTAL);
		m_SNcU[i] = new wxBoxSizer(wxHORIZONTAL); m_SNcD[i] = new wxBoxSizer(wxHORIZONTAL);
		m_SNcShake[i] = new wxBoxSizer(wxHORIZONTAL);

		m_SNcZ[i]->Add(m_tNcZ[i], 0, wxALIGN_RIGHT | (wxUP), 4); m_SNcZ[i]->Add(m_bNcZ[i], 0, wxALIGN_RIGHT | (wxLEFT), 2);
		m_SNcC[i]->Add(m_tNcC[i], 0, wxALIGN_RIGHT | (wxUP), 4); m_SNcC[i]->Add(m_bNcC[i], 0, wxALIGN_RIGHT | (wxLEFT), 2);
		m_SNcL[i]->Add(m_tNcL[i], 0, wxALIGN_RIGHT | (wxUP), 4); m_SNcL[i]->Add(m_bNcL[i], 0, wxALIGN_RIGHT | (wxLEFT), 2);
		m_SNcR[i]->Add(m_tNcR[i], 0, wxALIGN_RIGHT | (wxUP), 4); m_SNcR[i]->Add(m_bNcR[i], 0, wxALIGN_RIGHT | (wxLEFT), 2);
		m_SNcU[i]->Add(m_tNcU[i], 0, wxALIGN_RIGHT | (wxUP), 4); m_SNcU[i]->Add(m_bNcU[i], 0, wxALIGN_RIGHT | (wxLEFT), 2);
		m_SNcD[i]->Add(m_tNcD[i], 0, wxALIGN_RIGHT | (wxUP), 4); m_SNcD[i]->Add(m_bNcD[i], 0, wxALIGN_RIGHT | (wxLEFT), 2);
		m_SNcShake[i]->Add(m_tNcShake[i], 0, wxALIGN_RIGHT | (wxUP), 4); m_SNcShake[i]->Add(m_bNcShake[i], 0, wxALIGN_RIGHT | (wxLEFT), 2);

		// The parent sizer
		m_gNunchuck[i] = new wxStaticBoxSizer (wxVERTICAL, m_Controller[i], wxT("Nunchuck"));
		m_gNunchuck[i]->Add(m_NunchuckStick[i], 0, wxALIGN_CENTER | (wxALL), 2);
		m_gNunchuck[i]->AddSpacer(2);
		m_gNunchuck[i]->Add(m_SNcShake[i], 0, wxALIGN_RIGHT | (wxALL), 1);
		m_gNunchuck[i]->Add(m_SNcZ[i], 0, wxALIGN_RIGHT | (wxLEFT | wxRIGHT | wxDOWN), 1);
		m_gNunchuck[i]->Add(m_SNcC[i], 0, wxALIGN_RIGHT | (wxLEFT | wxRIGHT | wxDOWN), 1);
		m_gNunchuck[i]->Add(m_SNcL[i], 0, wxALIGN_RIGHT | (wxLEFT | wxRIGHT | wxDOWN), 1);
		m_gNunchuck[i]->Add(m_SNcR[i], 0, wxALIGN_RIGHT | (wxLEFT | wxRIGHT | wxDOWN), 1);
		m_gNunchuck[i]->Add(m_SNcU[i], 0, wxALIGN_RIGHT | (wxLEFT | wxRIGHT | wxDOWN), 1);
		m_gNunchuck[i]->Add(m_SNcD[i], 0, wxALIGN_RIGHT | (wxLEFT | wxRIGHT | wxDOWN), 1);

		//Set values
		m_NunchuckComboStick[i]->SetSelection(g_Config.Nunchuck.Type);

		// --------------------------------------------------------------------
		// Classic Controller
		// -----------------------------
		
		// Stick controls
		m_CcTextLeftStick[i] = new wxStaticText(m_Controller[i], wxID_ANY, wxT("Left stick"));
		m_CcComboLeftStick[i] = new wxComboBox(m_Controller[i], IDCB_CC_LEFT_STICK, StrNunchuck[0], wxDefaultPosition, wxDefaultSize, StrNunchuck, wxCB_READONLY);
		m_CcTextRightStick[i] = new wxStaticText(m_Controller[i], wxID_ANY, wxT("Right stick"));
		m_CcComboRightStick[i] = new wxComboBox(m_Controller[i], IDCB_CC_RIGHT_STICK, StrNunchuck[0], wxDefaultPosition, wxDefaultSize, StrNunchuck, wxCB_READONLY);
		m_CcTextTriggers[i] = new wxStaticText(m_Controller[i], wxID_ANY, wxT("Triggers"));
		m_CcComboTriggers[i] = new wxComboBox(m_Controller[i], IDCB_CC_TRIGGERS, StrCcTriggers[0], wxDefaultPosition, wxDefaultSize, StrCcTriggers, wxCB_READONLY);

		m_tCcA[i] = new wxStaticText(m_Controller[i], wxID_ANY, wxT("A"));
		m_tCcB[i] = new wxStaticText(m_Controller[i], wxID_ANY, wxT("B"));
		m_tCcX[i] = new wxStaticText(m_Controller[i], wxID_ANY, wxT("X"));
		m_tCcY[i] = new wxStaticText(m_Controller[i], wxID_ANY, wxT("Y"));
		m_tCcP[i] = new wxStaticText(m_Controller[i], wxID_ANY, wxT("+"));
		m_tCcM[i] = new wxStaticText(m_Controller[i], wxID_ANY, wxT("- "));
		m_tCcH[i] = new wxStaticText(m_Controller[i], wxID_ANY, wxT("Home"));

		m_tCcTl[i] = new wxStaticText(m_Controller[i], wxID_ANY, wxT("Left trigger"));
		m_tCcZl[i] = new wxStaticText(m_Controller[i], wxID_ANY, wxT("Left Z"));
		m_tCcZr[i] = new wxStaticText(m_Controller[i], wxID_ANY, wxT("Right Z"));
		m_tCcTr[i] = new wxStaticText(m_Controller[i], wxID_ANY, wxT("Right trigger"));

		m_tCcDl[i] = new wxStaticText(m_Controller[i], wxID_ANY, wxT("Digital Left")); // Digital pad
		m_tCcDu[i] = new wxStaticText(m_Controller[i], wxID_ANY, wxT("Digital Up"));
		m_tCcDr[i] = new wxStaticText(m_Controller[i], wxID_ANY, wxT("Digital Right"));
		m_tCcDd[i] = new wxStaticText(m_Controller[i], wxID_ANY, wxT("Digital Down"));
		m_tCcLl[i] = new wxStaticText(m_Controller[i], wxID_ANY, wxT("L Left")); // Left analog stick
		m_tCcLu[i] = new wxStaticText(m_Controller[i], wxID_ANY, wxT("L Up"));
		m_tCcLr[i] = new wxStaticText(m_Controller[i], wxID_ANY, wxT("L Right"));
		m_tCcLd[i] = new wxStaticText(m_Controller[i], wxID_ANY, wxT("L Down"));
		m_tCcRl[i] = new wxStaticText(m_Controller[i], wxID_ANY, wxT("R Left")); // Right analog stick
		m_tCcRu[i] = new wxStaticText(m_Controller[i], wxID_ANY, wxT("R Up"));
		m_tCcRr[i] = new wxStaticText(m_Controller[i], wxID_ANY, wxT("R Right"));
		m_tCcRd[i] = new wxStaticText(m_Controller[i], wxID_ANY, wxT("R Down"));

		m_bCcA[i] = new wxButton(m_Controller[i], IDB_CC_A, wxEmptyString, wxDefaultPosition, wxSize(BtW, BtH));
		m_bCcB[i] = new wxButton(m_Controller[i], IDB_CC_B, wxEmptyString, wxDefaultPosition, wxSize(BtW, BtH));
		m_bCcX[i] = new wxButton(m_Controller[i], IDB_CC_X, wxEmptyString, wxDefaultPosition, wxSize(BtW, BtH));
		m_bCcY[i] = new wxButton(m_Controller[i], IDB_CC_Y, wxEmptyString, wxDefaultPosition, wxSize(BtW, BtH));
		m_bCcP[i] = new wxButton(m_Controller[i], IDB_CC_P, wxEmptyString, wxDefaultPosition, wxSize(BtW, BtH));
		m_bCcM[i] = new wxButton(m_Controller[i], IDB_CC_M, wxEmptyString, wxDefaultPosition, wxSize(BtW, BtH));
		m_bCcH[i] = new wxButton(m_Controller[i], IDB_CC_H, wxEmptyString, wxDefaultPosition, wxSize(BtW, BtH));

		m_bCcTl[i] = new wxButton(m_Controller[i], IDB_CC_TL, wxEmptyString, wxDefaultPosition, wxSize(BtW, BtH));
		m_bCcZl[i] = new wxButton(m_Controller[i], IDB_CC_ZL, wxEmptyString, wxDefaultPosition, wxSize(BtW, BtH));
		m_bCcZr[i] = new wxButton(m_Controller[i], IDB_CC_ZR, wxEmptyString, wxDefaultPosition, wxSize(BtW, BtH));
		m_bCcTr[i] = new wxButton(m_Controller[i], IDB_CC_TR, wxEmptyString, wxDefaultPosition, wxSize(BtW, BtH));

		m_bCcDl[i] = new wxButton(m_Controller[i], IDB_CC_DL, wxEmptyString, wxDefaultPosition, wxSize(BtW, BtH)); // Digital pad
		m_bCcDu[i] = new wxButton(m_Controller[i], IDB_CC_DU, wxEmptyString, wxDefaultPosition, wxSize(BtW, BtH));
		m_bCcDr[i] = new wxButton(m_Controller[i], IDB_CC_DR, wxEmptyString, wxDefaultPosition, wxSize(BtW, BtH));
		m_bCcDd[i] = new wxButton(m_Controller[i], IDB_CC_DD, wxEmptyString, wxDefaultPosition, wxSize(BtW, BtH));
		m_bCcLl[i] = new wxButton(m_Controller[i], IDB_CC_LL, wxEmptyString, wxDefaultPosition, wxSize(BtW, BtH)); // Left analog stick
		m_bCcLu[i] = new wxButton(m_Controller[i], IDB_CC_LU, wxEmptyString, wxDefaultPosition, wxSize(BtW, BtH));
		m_bCcLr[i] = new wxButton(m_Controller[i], IDB_CC_LR, wxEmptyString, wxDefaultPosition, wxSize(BtW, BtH));
		m_bCcLd[i] = new wxButton(m_Controller[i], IDB_CC_LD, wxEmptyString, wxDefaultPosition, wxSize(BtW, BtH));
		m_bCcRl[i] = new wxButton(m_Controller[i], IDB_CC_RL, wxEmptyString, wxDefaultPosition, wxSize(BtW, BtH)); // Right analog stick
		m_bCcRu[i] = new wxButton(m_Controller[i], IDB_CC_RU, wxEmptyString, wxDefaultPosition, wxSize(BtW, BtH));
		m_bCcRr[i] = new wxButton(m_Controller[i], IDB_CC_RR, wxEmptyString, wxDefaultPosition, wxSize(BtW, BtH));
		m_bCcRd[i] = new wxButton(m_Controller[i], IDB_CC_RD, wxEmptyString, wxDefaultPosition, wxSize(BtW, BtH));

		// Set small font
		m_bCcA[i]->SetFont(m_SmallFont); m_bCcB[i]->SetFont(m_SmallFont);
		m_bCcX[i]->SetFont(m_SmallFont); m_bCcY[i]->SetFont(m_SmallFont);
		m_bCcP[i]->SetFont(m_SmallFont); m_bCcM[i]->SetFont(m_SmallFont); m_bCcH[i]->SetFont(m_SmallFont);
		m_bCcTl[i]->SetFont(m_SmallFont); m_bCcZl[i]->SetFont(m_SmallFont); m_bCcZr[i]->SetFont(m_SmallFont); m_bCcTr[i]->SetFont(m_SmallFont);
		m_bCcDl[i]->SetFont(m_SmallFont); m_bCcDu[i]->SetFont(m_SmallFont); m_bCcDr[i]->SetFont(m_SmallFont); m_bCcDd[i]->SetFont(m_SmallFont);
		m_bCcLl[i]->SetFont(m_SmallFont); m_bCcLu[i]->SetFont(m_SmallFont); m_bCcLr[i]->SetFont(m_SmallFont); m_bCcLd[i]->SetFont(m_SmallFont);
		m_bCcRl[i]->SetFont(m_SmallFont); m_bCcRu[i]->SetFont(m_SmallFont); m_bCcRr[i]->SetFont(m_SmallFont); m_bCcRd[i]->SetFont(m_SmallFont);

		// Sizers
		m_SCcLeftStick[i] = new wxBoxSizer(wxHORIZONTAL);
		m_SCcLeftStick[i]->Add(m_CcTextLeftStick[i], 0, (wxUP), 4);
		m_SCcLeftStick[i]->Add(m_CcComboLeftStick[i], 0, (wxLEFT), 2);
		
		m_SCcRightStick[i] = new wxBoxSizer(wxHORIZONTAL);
		m_SCcRightStick[i]->Add(m_CcTextRightStick[i], 0, (wxUP), 4);
		m_SCcRightStick[i]->Add(m_CcComboRightStick[i], 0, (wxLEFT), 2);

		m_SCcTriggers[i] = new wxBoxSizer(wxHORIZONTAL);
		m_SCcTriggers[i]->Add(m_CcTextTriggers[i], 0, (wxUP), 4);
		m_SCcTriggers[i]->Add(m_CcComboTriggers[i], 0, (wxLEFT), 2);

		m_SCcA[i] = new wxBoxSizer(wxHORIZONTAL); m_SCcB[i] = new wxBoxSizer(wxHORIZONTAL);
		m_SCcX[i] = new wxBoxSizer(wxHORIZONTAL); m_SCcY[i] = new wxBoxSizer(wxHORIZONTAL);
		m_SCcP[i] = new wxBoxSizer(wxHORIZONTAL); m_SCcM[i] = new wxBoxSizer(wxHORIZONTAL); m_SCcH[i] = new wxBoxSizer(wxHORIZONTAL);
		m_SCcTl[i] = new wxBoxSizer(wxHORIZONTAL); m_SCcZl[i] = new wxBoxSizer(wxHORIZONTAL); m_SCcZr[i] = new wxBoxSizer(wxHORIZONTAL); m_SCcTr[i] = new wxBoxSizer(wxHORIZONTAL);
		m_SCcDl[i] = new wxBoxSizer(wxHORIZONTAL); m_SCcDu[i] = new wxBoxSizer(wxHORIZONTAL); m_SCcDr[i] = new wxBoxSizer(wxHORIZONTAL); m_SCcDd[i] = new wxBoxSizer(wxHORIZONTAL);
		m_SCcLl[i] = new wxBoxSizer(wxHORIZONTAL); m_SCcLu[i] = new wxBoxSizer(wxHORIZONTAL); m_SCcLr[i] = new wxBoxSizer(wxHORIZONTAL); m_SCcLd[i] = new wxBoxSizer(wxHORIZONTAL);
		m_SCcRl[i] = new wxBoxSizer(wxHORIZONTAL); m_SCcRu[i] = new wxBoxSizer(wxHORIZONTAL); m_SCcRr[i] = new wxBoxSizer(wxHORIZONTAL); m_SCcRd[i] = new wxBoxSizer(wxHORIZONTAL);

		m_SCcA[i]->Add(m_tCcA[i], 0, wxALIGN_RIGHT | (wxUP), 4); m_SCcA[i]->Add(m_bCcA[i], 0, wxALIGN_RIGHT | (wxLEFT), 2);
		m_SCcB[i]->Add(m_tCcB[i], 0, wxALIGN_RIGHT | (wxUP), 4); m_SCcB[i]->Add(m_bCcB[i], 0, wxALIGN_RIGHT | (wxLEFT), 2);
		m_SCcX[i]->Add(m_tCcX[i], 0, wxALIGN_RIGHT | (wxUP), 4); m_SCcX[i]->Add(m_bCcX[i], 0, wxALIGN_RIGHT | (wxLEFT), 2);
		m_SCcY[i]->Add(m_tCcY[i], 0, wxALIGN_RIGHT | (wxUP), 4); m_SCcY[i]->Add(m_bCcY[i], 0, wxALIGN_RIGHT | (wxLEFT), 2);
		m_SCcP[i]->Add(m_tCcP[i], 0, wxALIGN_RIGHT | (wxUP), 4); m_SCcP[i]->Add(m_bCcP[i], 0, wxALIGN_RIGHT | (wxLEFT), 2);
		m_SCcM[i]->Add(m_tCcM[i], 0, wxALIGN_RIGHT | (wxUP), 4); m_SCcM[i]->Add(m_bCcM[i], 0, wxALIGN_RIGHT | (wxLEFT), 2);
		m_SCcH[i]->Add(m_tCcH[i], 0, wxALIGN_RIGHT | (wxUP), 4); m_SCcH[i]->Add(m_bCcH[i], 0, wxALIGN_RIGHT | (wxLEFT), 2);
		m_SCcTl[i]->Add(m_tCcTl[i], 0, wxALIGN_RIGHT | (wxUP), 4); m_SCcTl[i]->Add(m_bCcTl[i], 0, wxALIGN_RIGHT | (wxLEFT), 2);
		m_SCcZl[i]->Add(m_tCcZl[i], 0, wxALIGN_RIGHT | (wxUP), 4); m_SCcZl[i]->Add(m_bCcZl[i], 0, wxALIGN_RIGHT | (wxLEFT), 2);
		m_SCcZr[i]->Add(m_tCcZr[i], 0, wxALIGN_RIGHT | (wxUP), 4); m_SCcZr[i]->Add(m_bCcZr[i], 0, wxALIGN_RIGHT | (wxLEFT), 2);
		m_SCcTr[i]->Add(m_tCcTr[i], 0, wxALIGN_RIGHT | (wxUP), 4); m_SCcTr[i]->Add(m_bCcTr[i], 0, wxALIGN_RIGHT | (wxLEFT), 2);

		m_SCcDl[i]->Add(m_tCcDl[i], 0, wxALIGN_RIGHT | (wxUP), 4); m_SCcDl[i]->Add(m_bCcDl[i], 0, wxALIGN_RIGHT | (wxLEFT), 2);
		m_SCcDu[i]->Add(m_tCcDu[i], 0, wxALIGN_RIGHT | (wxUP), 4); m_SCcDu[i]->Add(m_bCcDu[i], 0, wxALIGN_RIGHT | (wxLEFT), 2);
		m_SCcDr[i]->Add(m_tCcDr[i], 0, wxALIGN_RIGHT | (wxUP), 4); m_SCcDr[i]->Add(m_bCcDr[i], 0, wxALIGN_RIGHT | (wxLEFT), 2);
		m_SCcDd[i]->Add(m_tCcDd[i], 0, wxALIGN_RIGHT | (wxUP), 4); m_SCcDd[i]->Add(m_bCcDd[i], 0, wxALIGN_RIGHT | (wxLEFT), 2);

		m_SCcLl[i]->Add(m_tCcLl[i], 0, wxALIGN_RIGHT | (wxUP), 4); m_SCcLl[i]->Add(m_bCcLl[i], 0, wxALIGN_RIGHT | (wxLEFT), 2);
		m_SCcLu[i]->Add(m_tCcLu[i], 0, wxALIGN_RIGHT | (wxUP), 4); m_SCcLu[i]->Add(m_bCcLu[i], 0, wxALIGN_RIGHT | (wxLEFT), 2);
		m_SCcLr[i]->Add(m_tCcLr[i], 0, wxALIGN_RIGHT | (wxUP), 4); m_SCcLr[i]->Add(m_bCcLr[i], 0, wxALIGN_RIGHT | (wxLEFT), 2);
		m_SCcLd[i]->Add(m_tCcLd[i], 0, wxALIGN_RIGHT | (wxUP), 4); m_SCcLd[i]->Add(m_bCcLd[i], 0, wxALIGN_RIGHT | (wxLEFT), 2);

		m_SCcRl[i]->Add(m_tCcRl[i], 0, wxALIGN_RIGHT | (wxUP), 4); m_SCcRl[i]->Add(m_bCcRl[i], 0, wxALIGN_RIGHT | (wxLEFT), 2);
		m_SCcRu[i]->Add(m_tCcRu[i], 0, wxALIGN_RIGHT | (wxUP), 4); m_SCcRu[i]->Add(m_bCcRu[i], 0, wxALIGN_RIGHT | (wxLEFT), 2);
		m_SCcRr[i]->Add(m_tCcRr[i], 0, wxALIGN_RIGHT | (wxUP), 4); m_SCcRr[i]->Add(m_bCcRr[i], 0, wxALIGN_RIGHT | (wxLEFT), 2);
		m_SCcRd[i]->Add(m_tCcRd[i], 0, wxALIGN_RIGHT | (wxUP), 4); m_SCcRd[i]->Add(m_bCcRd[i], 0, wxALIGN_RIGHT | (wxLEFT), 2);

		// The left parent
		m_SCcVertLeft[i] = new wxBoxSizer(wxVERTICAL);
		m_SCcVertLeft[i]->Add(m_SCcLeftStick[i], 0, wxALIGN_RIGHT | (wxALL), 2);
		m_SCcVertLeft[i]->Add(m_SCcRightStick[i], 0, wxALIGN_RIGHT | (wxLEFT | wxRIGHT | wxDOWN), 2);
		m_SCcVertLeft[i]->AddSpacer(2);
		m_SCcVertLeft[i]->Add(m_SCcDl[i], 0, wxALIGN_RIGHT | (wxLEFT | wxRIGHT | wxDOWN), 1);
		m_SCcVertLeft[i]->Add(m_SCcDu[i], 0, wxALIGN_RIGHT | (wxLEFT | wxRIGHT | wxDOWN), 1);
		m_SCcVertLeft[i]->Add(m_SCcDr[i], 0, wxALIGN_RIGHT | (wxLEFT | wxRIGHT | wxDOWN), 1);
		m_SCcVertLeft[i]->Add(m_SCcDd[i], 0, wxALIGN_RIGHT | (wxLEFT | wxRIGHT | wxDOWN), 1);
		m_SCcVertLeft[i]->Add(m_SCcTl[i], 0, wxALIGN_RIGHT | (wxLEFT | wxRIGHT | wxDOWN), 1);
		m_SCcVertLeft[i]->Add(m_SCcTr[i], 0, wxALIGN_RIGHT | (wxLEFT | wxRIGHT | wxDOWN), 1);

		// The middle parent
		m_SCcVertMiddle[i] = new wxBoxSizer(wxVERTICAL);
		m_SCcVertMiddle[i]->Add(m_SCcTriggers[i], 0, wxALIGN_RIGHT | (wxALL), 1);
		m_SCcVertLeft[i]->AddSpacer(2);
		m_SCcVertMiddle[i]->Add(m_SCcLl[i], 0, wxALIGN_RIGHT | (wxLEFT | wxRIGHT | wxDOWN), 1);
		m_SCcVertMiddle[i]->Add(m_SCcLu[i], 0, wxALIGN_RIGHT | (wxLEFT | wxRIGHT | wxDOWN), 1);
		m_SCcVertMiddle[i]->Add(m_SCcLr[i], 0, wxALIGN_RIGHT | (wxLEFT | wxRIGHT | wxDOWN), 1);
		m_SCcVertMiddle[i]->Add(m_SCcLd[i], 0, wxALIGN_RIGHT | (wxLEFT | wxRIGHT | wxDOWN), 1);
		m_SCcVertMiddle[i]->Add(m_SCcRl[i], 0, wxALIGN_RIGHT | (wxLEFT | wxRIGHT | wxDOWN), 1);
		m_SCcVertMiddle[i]->Add(m_SCcRd[i], 0, wxALIGN_RIGHT | (wxLEFT | wxRIGHT | wxDOWN), 1);
		m_SCcVertMiddle[i]->Add(m_SCcRr[i], 0, wxALIGN_RIGHT | (wxLEFT | wxRIGHT | wxDOWN), 1);
		m_SCcVertMiddle[i]->Add(m_SCcRu[i], 0, wxALIGN_RIGHT | (wxLEFT | wxRIGHT | wxDOWN), 1);

		// The right parent
		m_SCcVertRight[i] = new wxBoxSizer(wxVERTICAL);
		m_SCcVertRight[i]->Add(m_SCcA[i], 0, wxALIGN_RIGHT | (wxALL), 1);
		m_SCcVertRight[i]->Add(m_SCcB[i], 0, wxALIGN_RIGHT | (wxLEFT | wxRIGHT | wxDOWN), 1);
		m_SCcVertRight[i]->Add(m_SCcX[i], 0, wxALIGN_RIGHT | (wxLEFT | wxRIGHT | wxDOWN), 1);
		m_SCcVertRight[i]->Add(m_SCcY[i], 0, wxALIGN_RIGHT | (wxLEFT | wxRIGHT | wxDOWN), 1);
		m_SCcVertRight[i]->Add(m_SCcP[i], 0, wxALIGN_RIGHT | (wxLEFT | wxRIGHT | wxDOWN), 1);
		m_SCcVertRight[i]->Add(m_SCcM[i], 0, wxALIGN_RIGHT | (wxLEFT | wxRIGHT | wxDOWN), 1);
		m_SCcVertRight[i]->Add(m_SCcH[i], 0, wxALIGN_RIGHT | (wxLEFT | wxRIGHT | wxDOWN), 1);
		m_SCcVertRight[i]->Add(m_SCcZl[i], 0, wxALIGN_RIGHT | (wxLEFT | wxRIGHT | wxDOWN), 1);
		m_SCcVertRight[i]->Add(m_SCcZr[i], 0, wxALIGN_RIGHT | (wxLEFT | wxRIGHT | wxDOWN), 1);


		// The parent sizer
		m_gClassicController[i] = new wxStaticBoxSizer (wxHORIZONTAL, m_Controller[i], wxT("Classic Controller"));
		m_gClassicController[i]->Add(m_SCcVertLeft[i], 0, wxALIGN_RIGHT | (wxLEFT | wxRIGHT | wxDOWN), 1);
		m_gClassicController[i]->Add(m_SCcVertMiddle[i], 0, wxALIGN_RIGHT | (wxLEFT | wxRIGHT | wxDOWN), 1);
		m_gClassicController[i]->Add(m_SCcVertRight[i], 0, wxALIGN_RIGHT | (wxLEFT | wxRIGHT | wxDOWN), 1);

		//Set values
		m_CcComboLeftStick[i]->SetSelection(g_Config.ClassicController.LType);
		m_CcComboRightStick[i]->SetSelection(g_Config.ClassicController.RType);
		m_CcComboTriggers[i]->SetSelection(g_Config.ClassicController.TType);

		// --------------------------------------------------------------------
		// Row 4 Sizers
		// -----------------------------
		m_HorizControllerMapping[i] = new wxBoxSizer(wxHORIZONTAL);
		m_HorizControllerMapping[i]->AddStretchSpacer();
		m_HorizControllerMapping[i]->Add(m_gWiimote[i]);
		m_HorizControllerMapping[i]->Add(m_gNunchuck[i], 0, (wxLEFT), 5);
		m_HorizControllerMapping[i]->Add(m_gClassicController[i], 0, (wxLEFT), 5);
		m_HorizControllerMapping[i]->AddStretchSpacer();
		///////////////////////////


		////////////////////////////////////////////////////////////////
		// Set up sizers and layout
		// ----------------
		m_SizeParent[i] = new wxBoxSizer(wxVERTICAL);
		m_SizeParent[i]->Add(m_SizeBasicGeneral[i], 0, wxBORDER_STATIC | wxEXPAND | (wxALL), 5);
		m_SizeParent[i]->Add(m_HorizControllerTiltParent[i], 0, wxEXPAND | (wxLEFT | wxRIGHT | wxDOWN), 5);
		m_SizeParent[i]->Add(m_HorizControllers[i], 0, wxEXPAND | (wxLEFT | wxRIGHT | wxDOWN), 5);
		m_SizeParent[i]->Add(m_HorizControllerMapping[i], 0, wxEXPAND | (wxLEFT | wxRIGHT | wxDOWN), 5);

		// The sizer m_sMain will be expanded inside m_Controller, m_SizeParent will not
		m_sMain[i] = new wxBoxSizer(wxVERTICAL);
		m_sMain[i]->Add(m_SizeParent[i]);

		// Set the main sizer
		m_Controller[i]->SetSizer(m_sMain[i]);
		/////////////////////////////////

		// Update with the progress (i) and the message
		dialog.Update(i + 1, wxT("Loading notebook pages..."));
	}

	////////////////////////////////////////////
	// Movement recording
	// ----------------
	CreateGUIControlsRecording();
	/////////////////////////////////

	// Update with the progress (i) and the message (msg)
	dialog.Update(5, wxT("Loading notebook pages..."));
	//dialog.Close();

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

	UsingExtension = !UsingExtension;
	Console::Print("\nDoUseReal()  Connect extension: %i\n", !UsingExtension);
	DoExtensionConnectedDisconnected(UsingExtension ? 1 : 0);

	if(g_EmulatorRunning)
	{
		// Disable the checkbox for a moment
		SetCursor(wxCursor(wxCURSOR_WAIT));
		m_bEnableUseRealWiimote = false;
		// We may not need this if there is already a message queue that allows the nessesary timeout
		//sleep(100);

		/* Start the timer to allow the approximate time it takes for the Wiimote to come online
		   it would simpler to use sleep(1000) but that doesn't work because we need the functions in main.cpp
		   to work */
		m_TimeoutOnce->Start(1000, true);
	}
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
	case ID_TILT_RANGE_ROLL:
		m_TiltComboRangeRoll[Page]->GetValue().ToLong(&TmpValue); g_Config.Trigger.Range.Roll = TmpValue;
		break;
	case ID_TILT_RANGE_PITCH:
		m_TiltComboRangePitch[Page]->GetValue().ToLong(&TmpValue); g_Config.Trigger.Range.Pitch = TmpValue;
		break;
	case IDC_JOYNAME:
		DoChangeJoystick();
		break;
	case IDCB_NUNCHUCK_STICK:
		g_Config.Nunchuck.Type = m_NunchuckComboStick[Page]->GetSelection();
		break;
	case IDCB_CC_LEFT_STICK:
		g_Config.ClassicController.LType = m_CcComboLeftStick[Page]->GetSelection();
		break;
	case IDCB_CC_RIGHT_STICK:
		g_Config.ClassicController.RType = m_CcComboRightStick[Page]->GetSelection();
		break;
	case IDCB_CC_TRIGGERS:
		g_Config.ClassicController.TType = m_CcComboTriggers[Page]->GetSelection();
		break;

	// These are defined in PadMapping and we can run the same function to update all of them
	case IDCB_LEFT_DIAGONAL:
	case IDC_LEFT_C2S:
	case ID_TILT_INVERT_ROLL:
	case ID_TILT_INVERT_PITCH:
		SaveButtonMappingAll(Page);
		break;
	case IDCB_DEAD_ZONE_LEFT:
		SaveButtonMappingAll(Page);
		break;
	case IDCB_DEAD_ZONE_RIGHT:
		SaveButtonMappingAll(Page);
		break;

	//////////////////////////
	// Recording
	// -----------
	case ID_UPDATE_REAL:
		g_Config.bUpdateRealWiimote = m_UpdateMeters->IsChecked();
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
			if (m_RecordHotKeyWiimote[i]->GetSelection() == m_RecordHotKeyWiimote[CurrentChoiceBox]->GetSelection()) m_RecordHotKeyWiimote[i]->SetSelection(10);
			if (m_RecordHotKeyNunchuck[i]->GetSelection() == m_RecordHotKeyNunchuck[CurrentChoiceBox]->GetSelection()) m_RecordHotKeyNunchuck[i]->SetSelection(10);
			if (m_RecordHotKeyIR[i]->GetSelection() == m_RecordHotKeyIR[CurrentChoiceBox]->GetSelection()) m_RecordHotKeyIR[i]->SetSelection(10);
			
			//Console::Print("HotKey: %i %i\n",
			//	m_RecordHotKey[i]->GetSelection(), m_RecordHotKey[CurrentChoiceBox]->GetSelection());
		}
		break;
		/////////////////
	}
	g_Config.Save();
	UpdateGUI();
}

// =======================================================
// Apparently we need a scroll event version of this for the sliders
// -------------
void ConfigDialog::GeneralSettingsChangedScroll(wxScrollEvent& event)
{
	switch (event.GetId())
	{
	// IR cursor position
	case IDS_WIDTH:
		g_Config.iIRWidth = m_SliderWidth[Page]->GetValue();
		break;
	case IDS_HEIGHT:
		g_Config.iIRHeight = m_SliderHeight[Page]->GetValue();
		break;
	case IDS_LEFT:
		g_Config.iIRLeft = m_SliderLeft[Page]->GetValue();
		break;
	case IDS_TOP:
		g_Config.iIRTop = m_SliderTop[Page]->GetValue();
		break;
	}

	UpdateGUI();
}

// =======================================================
// Update the IR pointer calibration sliders
// -------------
void ConfigDialog::UpdateControls()
{
	// Update the slider position if a configuration has been loaded
	m_SliderWidth[Page]->SetValue(g_Config.iIRWidth);
	m_SliderHeight[Page]->SetValue(g_Config.iIRHeight);
	m_SliderLeft[Page]->SetValue(g_Config.iIRLeft);
	m_SliderTop[Page]->SetValue(g_Config.iIRTop);

	// Update the labels
	m_TextScreenWidth[Page]->SetLabel(wxString::Format(wxT("Width: %i"), g_Config.iIRWidth));
	m_TextScreenHeight[Page]->SetLabel(wxString::Format(wxT("Height: %i"), g_Config.iIRHeight));
	m_TextScreenLeft[Page]->SetLabel(wxString::Format(wxT("Left: %i"), g_Config.iIRLeft));
	m_TextScreenTop[Page]->SetLabel(wxString::Format(wxT("Top: %i"), g_Config.iIRTop));
}
// ==============================


// =======================================================
// Update the enabled/disabled status
// -------------
void ConfigDialog::UpdateGUI(int Slot)
{
	//Console::Print("UpdateGUI: \n");

	// Update the gamepad settings
	UpdateGUIButtonMapping(Page);

	// Update dead zone
	DoChangeDeadZone(true); DoChangeDeadZone(false);

	// Update the Wiimote IR pointer calibration
	UpdateControls();

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
	m_UseRealWiimote[Page]->Enable((m_bEnableUseRealWiimote && g_RealWiiMotePresent && g_Config.bConnectRealWiimote) || (!g_EmulatorRunning && g_Config.bConnectRealWiimote));

	// Linux has no FindItem()
	// Disable all pad items if no pads are detected
	if(ControlsCreated)
	{
		bool PadEnabled = WiiMoteEmu::NumGoodPads != 0;
		#ifdef _WIN32
			for(int i = IDB_ANALOG_LEFT_X; i <= IDB_TRIGGER_R; i++) m_Notebook->FindItem(i)->Enable(PadEnabled);
			m_Notebook->FindItem(IDC_JOYNAME)->Enable(PadEnabled);
			m_Notebook->FindItem(IDC_LEFT_C2S)->Enable(PadEnabled);
			m_Notebook->FindItem(IDCB_LEFT_DIAGONAL)->Enable(PadEnabled);
			m_Notebook->FindItem(IDCB_DEAD_ZONE_LEFT)->Enable(PadEnabled);
			m_Notebook->FindItem(IDCB_DEAD_ZONE_RIGHT)->Enable(PadEnabled);
			m_Notebook->FindItem(ID_TRIGGER_TYPE)->Enable(PadEnabled);
		#endif
	}		

	// Disable all recording buttons
	bool ActiveRecording = !(m_bWaitForRecording || m_bRecording);
	#ifdef _WIN32
		for(int i = IDB_RECORD + 1; i < (IDB_RECORD + RECORDING_ROWS + 1); i++)
			if(ControlsCreated) m_Notebook->FindItem(i)->Enable(ActiveRecording);
	#endif
}
