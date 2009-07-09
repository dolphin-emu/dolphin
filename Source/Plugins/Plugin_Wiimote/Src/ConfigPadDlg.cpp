// Copyright (C) 2003-2009 Dolphin Project.

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

#include "wiimote_real.h" // for MAX_WIIMOTES
#include "wiimote_hid.h"
#include "main.h"
#include "ConfigPadDlg.h"
#include "ConfigBasicDlg.h"
#include "Config.h"
#include "EmuDefinitions.h" // for joyinfo

enum TriggerType
{
	CTL_TRIGGER_SDL = 0,
	CTL_TRIGGER_XINPUT
};

BEGIN_EVENT_TABLE(WiimotePadConfigDialog,wxDialog)
	EVT_CLOSE(WiimotePadConfigDialog::OnClose)
	EVT_BUTTON(ID_CLOSE, WiimotePadConfigDialog::CloseClick)
	EVT_BUTTON(ID_APPLY, WiimotePadConfigDialog::CloseClick)

	EVT_COMBOBOX(IDC_JOYNAME, WiimotePadConfigDialog::GeneralSettingsChanged)	
	EVT_COMBOBOX(ID_TRIGGER_TYPE, WiimotePadConfigDialog::GeneralSettingsChanged)	
	EVT_COMBOBOX(ID_TILT_INPUT, WiimotePadConfigDialog::GeneralSettingsChanged)	
	EVT_COMBOBOX(ID_TILT_RANGE_ROLL, WiimotePadConfigDialog::GeneralSettingsChanged)
	EVT_COMBOBOX(ID_TILT_RANGE_PITCH, WiimotePadConfigDialog::GeneralSettingsChanged)
	EVT_COMBOBOX(IDCB_LEFT_DIAGONAL, WiimotePadConfigDialog::GeneralSettingsChanged)
	EVT_COMBOBOX(IDCB_DEAD_ZONE_LEFT, WiimotePadConfigDialog::GeneralSettingsChanged)
	EVT_COMBOBOX(IDCB_DEAD_ZONE_RIGHT, WiimotePadConfigDialog::GeneralSettingsChanged)
	EVT_CHECKBOX(IDC_LEFT_C2S, WiimotePadConfigDialog::GeneralSettingsChanged)
	EVT_CHECKBOX(ID_TILT_INVERT_ROLL, WiimotePadConfigDialog::GeneralSettingsChanged)
	EVT_CHECKBOX(ID_TILT_INVERT_PITCH, WiimotePadConfigDialog::GeneralSettingsChanged)
	EVT_COMBOBOX(IDCB_NUNCHUCK_STICK, WiimotePadConfigDialog::GeneralSettingsChanged)
	EVT_COMBOBOX(IDCB_CC_LEFT_STICK, WiimotePadConfigDialog::GeneralSettingsChanged)
	EVT_COMBOBOX(IDCB_CC_RIGHT_STICK, WiimotePadConfigDialog::GeneralSettingsChanged)
	EVT_COMBOBOX(IDCB_CC_TRIGGERS, WiimotePadConfigDialog::GeneralSettingsChanged)
	
	// Wiimote
	EVT_BUTTON(IDB_WM_A, WiimotePadConfigDialog::OnButtonClick) EVT_BUTTON(IDB_WM_B, WiimotePadConfigDialog::OnButtonClick)
	EVT_BUTTON(IDB_WM_1, WiimotePadConfigDialog::OnButtonClick) EVT_BUTTON(IDB_WM_2, WiimotePadConfigDialog::OnButtonClick)
	EVT_BUTTON(IDB_WM_P, WiimotePadConfigDialog::OnButtonClick) EVT_BUTTON(IDB_WM_M, WiimotePadConfigDialog::OnButtonClick) EVT_BUTTON(IDB_WM_H, WiimotePadConfigDialog::OnButtonClick)
	EVT_BUTTON(IDB_WM_L, WiimotePadConfigDialog::OnButtonClick) EVT_BUTTON(IDB_WM_R, WiimotePadConfigDialog::OnButtonClick)
	EVT_BUTTON(IDB_WM_U, WiimotePadConfigDialog::OnButtonClick) EVT_BUTTON(IDB_WM_D, WiimotePadConfigDialog::OnButtonClick)
	EVT_BUTTON(IDB_WM_SHAKE, WiimotePadConfigDialog::OnButtonClick)
	EVT_BUTTON(IDB_WM_PITCH_L, WiimotePadConfigDialog::OnButtonClick) EVT_BUTTON(IDB_WM_PITCH_R, WiimotePadConfigDialog::OnButtonClick)

	// Nunchuck
	EVT_BUTTON(IDB_NC_Z, WiimotePadConfigDialog::OnButtonClick) EVT_BUTTON(IDB_NC_C, WiimotePadConfigDialog::OnButtonClick)
	EVT_BUTTON(IDB_NC_L, WiimotePadConfigDialog::OnButtonClick) EVT_BUTTON(IDB_NC_R, WiimotePadConfigDialog::OnButtonClick)
	EVT_BUTTON(IDB_NC_U, WiimotePadConfigDialog::OnButtonClick) EVT_BUTTON(IDB_NC_D, WiimotePadConfigDialog::OnButtonClick)
	EVT_BUTTON(IDB_NC_SHAKE, WiimotePadConfigDialog::OnButtonClick)

	// Classic Controller
	EVT_BUTTON(IDB_CC_A, WiimotePadConfigDialog::OnButtonClick) EVT_BUTTON(IDB_CC_B, WiimotePadConfigDialog::OnButtonClick) EVT_BUTTON(IDB_CC_X, WiimotePadConfigDialog::OnButtonClick) EVT_BUTTON(IDB_CC_Y, WiimotePadConfigDialog::OnButtonClick)
	EVT_BUTTON(IDB_CC_P, WiimotePadConfigDialog::OnButtonClick) EVT_BUTTON(IDB_CC_M, WiimotePadConfigDialog::OnButtonClick) EVT_BUTTON(IDB_CC_H, WiimotePadConfigDialog::OnButtonClick)
	EVT_BUTTON(IDB_CC_TL, WiimotePadConfigDialog::OnButtonClick) EVT_BUTTON(IDB_CC_ZL, WiimotePadConfigDialog::OnButtonClick) EVT_BUTTON(IDB_CC_ZR, WiimotePadConfigDialog::OnButtonClick) EVT_BUTTON(IDB_CC_TR, WiimotePadConfigDialog::OnButtonClick)
	EVT_BUTTON(IDB_CC_DL, WiimotePadConfigDialog::OnButtonClick) EVT_BUTTON(IDB_CC_DU, WiimotePadConfigDialog::OnButtonClick) EVT_BUTTON(IDB_CC_DR, WiimotePadConfigDialog::OnButtonClick) EVT_BUTTON(IDB_CC_DD, WiimotePadConfigDialog::OnButtonClick)
	EVT_BUTTON(IDB_CC_DL, WiimotePadConfigDialog::OnButtonClick) EVT_BUTTON(IDB_CC_DU, WiimotePadConfigDialog::OnButtonClick) EVT_BUTTON(IDB_CC_DR, WiimotePadConfigDialog::OnButtonClick) EVT_BUTTON(IDB_CC_DD, WiimotePadConfigDialog::OnButtonClick)
	EVT_BUTTON(IDB_CC_LL, WiimotePadConfigDialog::OnButtonClick) EVT_BUTTON(IDB_CC_LU, WiimotePadConfigDialog::OnButtonClick) EVT_BUTTON(IDB_CC_LR, WiimotePadConfigDialog::OnButtonClick) EVT_BUTTON(IDB_CC_LD, WiimotePadConfigDialog::OnButtonClick)
	EVT_BUTTON(IDB_CC_RL, WiimotePadConfigDialog::OnButtonClick) EVT_BUTTON(IDB_CC_RU, WiimotePadConfigDialog::OnButtonClick) EVT_BUTTON(IDB_CC_RR, WiimotePadConfigDialog::OnButtonClick) EVT_BUTTON(IDB_CC_RD, WiimotePadConfigDialog::OnButtonClick)

	EVT_BUTTON(IDB_GH3_GREEN, WiimotePadConfigDialog::OnButtonClick)
	EVT_BUTTON(IDB_GH3_RED, WiimotePadConfigDialog::OnButtonClick)
	EVT_BUTTON(IDB_GH3_YELLOW, WiimotePadConfigDialog::OnButtonClick)
	EVT_BUTTON(IDB_GH3_BLUE, WiimotePadConfigDialog::OnButtonClick)
	EVT_BUTTON(IDB_GH3_ORANGE, WiimotePadConfigDialog::OnButtonClick)
	EVT_BUTTON(IDB_GH3_PLUS, WiimotePadConfigDialog::OnButtonClick)
	EVT_BUTTON(IDB_GH3_MINUS, WiimotePadConfigDialog::OnButtonClick)
//	EVT_COMBOBOX(IDB_GH3_WHAMMY, WiimotePadConfigDialog::GeneralSettingsChanged)
	EVT_COMBOBOX(IDCB_GH3_ANALOG_STICK, WiimotePadConfigDialog::GeneralSettingsChanged)
	EVT_BUTTON(IDB_GH3_ANALOG_LEFT, WiimotePadConfigDialog::OnButtonClick)
	EVT_BUTTON(IDB_GH3_ANALOG_RIGHT, WiimotePadConfigDialog::OnButtonClick)
	EVT_BUTTON(IDB_GH3_ANALOG_UP, WiimotePadConfigDialog::OnButtonClick)
	EVT_BUTTON(IDB_GH3_ANALOG_DOWN, WiimotePadConfigDialog::OnButtonClick)
	EVT_BUTTON(IDB_GH3_STRUM_UP, WiimotePadConfigDialog::OnButtonClick)
	EVT_BUTTON(IDB_GH3_STRUM_DOWN, WiimotePadConfigDialog::OnButtonClick) 			


	EVT_BUTTON(IDB_ANALOG_LEFT_X, WiimotePadConfigDialog::GetButtons)
	EVT_BUTTON(IDB_ANALOG_LEFT_Y, WiimotePadConfigDialog::GetButtons)
	EVT_BUTTON(IDB_ANALOG_RIGHT_X, WiimotePadConfigDialog::GetButtons)
	EVT_BUTTON(IDB_ANALOG_RIGHT_Y, WiimotePadConfigDialog::GetButtons)
	EVT_BUTTON(IDB_TRIGGER_L, WiimotePadConfigDialog::GetButtons)
	EVT_BUTTON(IDB_TRIGGER_R, WiimotePadConfigDialog::GetButtons)
	EVT_TIMER(IDTM_BUTTON, WiimotePadConfigDialog::OnButtonTimer)
	EVT_TIMER(IDTM_UPDATE_PAD, WiimotePadConfigDialog::UpdatePad)
END_EVENT_TABLE()

WiimotePadConfigDialog::WiimotePadConfigDialog(wxWindow *parent, wxWindowID id, const wxString &title,
						   const wxPoint &position, const wxSize& size, long style)
: wxDialog(parent, id, title, position, size, style)
{
#if wxUSE_TIMER
	m_ButtonMappingTimer = new wxTimer(this, IDTM_BUTTON);
	m_UpdatePad = new wxTimer(this, IDTM_UPDATE_PAD);

	// Reset values
	GetButtonWaitingID = 0;
	GetButtonWaitingTimer = 0;

	// Start the permanent timer
	const int TimesPerSecond = 30;
	m_UpdatePad->Start( floor((double)(1000 / TimesPerSecond)) );
#endif

	ControlsCreated = false;
	Page = 0;
	ClickedButton = NULL;

	g_Config.Load();
	CreatePadGUIControls();
	SetBackgroundColour(m_Notebook->GetBackgroundColour());
	
	// Set control values
	UpdateGUI();

	wxTheApp->Connect(wxID_ANY, wxEVT_KEY_DOWN, // Keyboard
		wxKeyEventHandler(WiimotePadConfigDialog::OnKeyDown),
		(wxObject*)0, this);
}

void WiimotePadConfigDialog::OnKeyDown(wxKeyEvent& event)
{
	event.Skip();

	// Save the key
	g_Pressed = event.GetKeyCode();

	// Handle the keyboard key mapping
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
	ClickedButton = NULL;
}

// Input button clicked
void WiimotePadConfigDialog::OnButtonClick(wxCommandEvent& event)
{
	//INFO_LOG(CONSOLE, "OnButtonClick: %i\n", g_Pressed);

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


void WiimotePadConfigDialog::OnClose(wxCloseEvent& event)
{
	g_FrameOpen = false;
	SaveButtonMappingAll(Page);
	if(m_UpdatePad)
		m_UpdatePad->Stop();
	g_Config.Save();
	event.Skip();
}

void WiimotePadConfigDialog::CloseClick(wxCommandEvent& event)
{
	switch(event.GetId())
	{
	case ID_CLOSE:
		Close();
		break;
	case ID_APPLY:
		SaveButtonMappingAll(Page);
		g_Config.Save();
		break;
	}
}



void WiimotePadConfigDialog::DoSave(bool ChangePad, int Slot)
{
	// Replace "" with "-1" before we are saving
	ToBlank(false);

	if(ChangePad)
	{
		// Since we are selecting the pad to save to by the Id we can't update it when we change the pad
		for(int i = 0; i < 4; i++)
			SaveButtonMapping(i, true);
		// Save the settings for the current pad
		g_Config.Save(Slot);
		// Now we can update the ID
		WiiMoteEmu::PadMapping[Page].ID = m_Joyname[Page]->GetSelection();
	}
	else
	{
		// Update PadMapping[] from the GUI controls
		for(int i = 0; i < 4; i++)
			SaveButtonMapping(i);
		g_Config.Save(Slot);
	}		

	// Then change it back to ""
	ToBlank();

	INFO_LOG(CONSOLE, "WiiMoteEmu::PadMapping[%i].ID = %i\n", Page, m_Joyname[Page]->GetSelection());
}

// Bitmap box and dot
wxBitmap WiimotePadConfigDialog::CreateBitmap()
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
wxBitmap WiimotePadConfigDialog::CreateBitmapDot()
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
wxBitmap WiimotePadConfigDialog::CreateBitmapDeadZone(int Radius)
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
wxBitmap WiimotePadConfigDialog::CreateBitmapClear()
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



void WiimotePadConfigDialog::CreatePadGUIControls()
{


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


	m_Notebook = new wxNotebook(this, ID_NOTEBOOK, wxDefaultPosition, wxDefaultSize);

	for (int i = 0; i < MAX_WIIMOTES; i++)
	{
		m_Controller[i] = new wxPanel(m_Notebook, ID_CONTROLLERPAGE1 + i, wxDefaultPosition, wxDefaultSize);
		m_Notebook->AddPage(m_Controller[i], wxString::Format(wxT("Wiimote %d"), i+1));

		// A small type font
		wxFont m_SmallFont(7, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL);

		// Configuration controls sizes
		static const int TxtW = 50, TxtH = 19, BtW = 75, BtH = 20;


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


		// Tilt Wiimote

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

		// Analog triggers
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

		// Row 2 Sizers: Connected pads, tilt, triggers
		m_HorizControllerTilt[i] = new wxBoxSizer(wxHORIZONTAL);
		m_HorizControllerTilt[i]->Add(m_gJoyname[i], 0, wxALIGN_CENTER | wxEXPAND, 0);
		m_HorizControllerTilt[i]->Add(m_gTilt[i], 0, wxEXPAND | (wxLEFT), 5);
		m_HorizControllerTilt[i]->Add(m_gTrigger[i], 0, wxEXPAND | (wxLEFT), 5);

		m_HorizControllerTiltParent[i] = new wxBoxSizer(wxBOTH);
		m_HorizControllerTiltParent[i]->Add(m_HorizControllerTilt[i]);



		// Analog sticks
		
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

		
		// Row 3 Sizers
		m_HorizControllers[i] = new wxBoxSizer(wxHORIZONTAL);
		//m_HorizControllers[i]->AddStretchSpacer();
		m_HorizControllers[i]->AddSpacer(17);
		m_HorizControllers[i]->Add(m_gAnalogLeft[i]);
		m_HorizControllers[i]->Add(m_gAnalogRight[i], 0, (wxLEFT), 5);
		//m_HorizControllers[i]->AddStretchSpacer();



		// Keyboard mapping
		
		// Wiimote

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

		// Nunchuck
		if(g_Config.iExtensionConnected == EXT_NUNCHUCK)
		{
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
		}
		else if(g_Config.iExtensionConnected == EXT_CLASSIC_CONTROLLER)
		{		
			// Classic Controller
		
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
		}
		else if(g_Config.iExtensionConnected == EXT_GUITARHERO3_CONTROLLER)
		{
			// Stick controls
			m_tGH3_Analog[i] = new wxStaticText(m_Controller[i], wxID_ANY, wxT("Analog joystick"));
			m_GH3ComboAnalog[i] = new wxComboBox(m_Controller[i], IDCB_GH3_ANALOG_STICK, StrNunchuck[0], wxDefaultPosition, wxDefaultSize, StrNunchuck, wxCB_READONLY);

			static const wxChar* gh3Text[] =
			{
				wxT("Green"),
				wxT("Red"),
				wxT("Yellow"),
				wxT("Blue"),
				wxT("Orange"),
				wxT("+"),
				wxT("- "),
				wxT("Whammy"),
				wxT("Left"), //analog stick
				wxT("Up"),
				wxT("Right"),
				wxT("Down"),
				wxT("Strum Up"),
				wxT("Strum Down"),
			};

			for ( int x = 0; x < GH3_CONTROLS; x++)
				m_statictext_GH3[x][i] = new wxStaticText(m_Controller[i], wxID_ANY, gh3Text[x]);

			for ( int x = IDB_GH3_GREEN; x <= IDB_GH3_STRUM_DOWN; x++)
			{
				m_Button_GH3[x - IDB_GH3_GREEN][i] = new wxButton(m_Controller[i], x, wxEmptyString, wxDefaultPosition, wxSize(BtW, BtH));
				if (IDB_GH3_WHAMMY <= x && x <= IDB_GH3_ANALOG_DOWN)
					m_Button_GH3[x - IDB_GH3_GREEN][i]->Disable();
			}
			m_GH3ComboAnalog[i]->Disable();

			// Sizers
			m_sGH3_Analog[i] = new wxBoxSizer(wxHORIZONTAL);
			m_sGH3_Analog[i]->Add(m_tGH3_Analog[i], 0, (wxUP), 4);
			m_sGH3_Analog[i]->Add(m_GH3ComboAnalog[i], 0, (wxLEFT), 2);

			for ( int x = 0; x < GH3_CONTROLS; x++)
			{
				m_sizer_GH3[x][i] = new wxBoxSizer(wxHORIZONTAL);
				m_sizer_GH3[x][i]->Add(m_statictext_GH3[x][i], 0, (wxUP), 4);
				m_sizer_GH3[x][i]->Add(m_Button_GH3[x][i], 0, (wxLEFT), 2);
			}

			// The left parent
			m_SGH3VertLeft[i] = new wxBoxSizer(wxVERTICAL);
			m_SGH3VertLeft[i]->Add(m_sGH3_Analog[i], 0, wxALIGN_RIGHT | (wxALL), 2);

			for (int x = IDB_GH3_WHAMMY - IDB_GH3_GREEN; x <= IDB_GH3_ANALOG_DOWN - IDB_GH3_GREEN; x++)
				m_SGH3VertLeft[i]->Add(m_sizer_GH3[x][i], 0, wxALIGN_RIGHT | (wxLEFT | wxRIGHT | wxDOWN), 1);

			// The right parent
			m_SGH3VertRight[i] = new wxBoxSizer(wxVERTICAL);

			for (int x = 0; x <= IDB_GH3_MINUS - IDB_GH3_GREEN; x++)
				m_SGH3VertRight[i]->Add(m_sizer_GH3[x][i], 0, wxALIGN_RIGHT | (wxLEFT | wxRIGHT | wxDOWN), 1);

			m_SGH3VertRight[i]->Add(m_sizer_GH3[IDB_GH3_STRUM_UP - IDB_GH3_GREEN][i], 0, wxALIGN_RIGHT | (wxLEFT | wxRIGHT | wxDOWN), 1);
			m_SGH3VertRight[i]->Add(m_sizer_GH3[IDB_GH3_STRUM_DOWN - IDB_GH3_GREEN][i], 0, wxALIGN_RIGHT | (wxLEFT | wxRIGHT | wxDOWN), 1);

			// The parent sizer
			m_gGuitarHero3Controller[i] = new wxStaticBoxSizer (wxHORIZONTAL, m_Controller[i], wxT("Guitar Hero 3 Controller"));
			m_gGuitarHero3Controller[i]->Add(m_SGH3VertLeft[i], 0, wxALIGN_RIGHT | (wxLEFT | wxRIGHT | wxDOWN), 1);
			m_gGuitarHero3Controller[i]->Add(m_SGH3VertRight[i], 0, wxALIGN_RIGHT | (wxLEFT | wxRIGHT | wxDOWN), 1);
		}

		// Row 4 Sizers

		m_HorizControllerMapping[i] = new wxBoxSizer(wxHORIZONTAL);
		m_HorizControllerMapping[i]->Add(m_gWiimote[i], 0, (wxLEFT), 5);
		switch(g_Config.iExtensionConnected)
		{
		case EXT_NUNCHUCK:
			m_HorizControllerMapping[i]->AddStretchSpacer(2);
			m_HorizControllerMapping[i]->Add(m_gNunchuck[i], 0, (wxLEFT), 5);
			break;
		case EXT_CLASSIC_CONTROLLER:
			m_HorizControllerMapping[i]->Add(m_gClassicController[i], 0, (wxLEFT), 5);
			break;
		case EXT_GUITARHERO3_CONTROLLER:
			m_HorizControllerMapping[i]->Add(m_gGuitarHero3Controller[i], 0, (wxLEFT), 5);
			break;
		case EXT_GUITARHEROWT_DRUMS:
		default:
			break;
		}
		m_HorizControllerMapping[i]->AddStretchSpacer(2);

		// Set up sizers and layout
		m_SizeParent[i] = new wxBoxSizer(wxVERTICAL);
		m_SizeParent[i]->Add(m_HorizControllerTiltParent[i], 0, wxEXPAND | (wxLEFT | wxRIGHT | wxDOWN), 5);
		m_SizeParent[i]->Add(m_HorizControllers[i], 0, wxEXPAND | (wxLEFT | wxRIGHT | wxDOWN), 5);
		m_SizeParent[i]->Add(m_HorizControllerMapping[i], 0, wxEXPAND | (wxLEFT | wxRIGHT | wxDOWN), 5);

		// The sizer m_sMain will be expanded inside m_Controller, m_SizeParent will not
		m_sMain[i] = new wxBoxSizer(wxVERTICAL);
		m_sMain[i]->Add(m_SizeParent[i]);

		// Set the main sizer
		m_Controller[i]->SetSizer(m_sMain[i]);
	}



	m_Apply = new wxButton(this, ID_APPLY, wxT("Apply"));
	m_Close = new wxButton(this, ID_CLOSE, wxT("Close"));
	m_Close->SetToolTip(wxT("Apply and Close"));

	wxBoxSizer* sButtons = new wxBoxSizer(wxHORIZONTAL);
	sButtons->AddStretchSpacer();
	sButtons->Add(m_Apply, 0, (wxALL), 0);
	sButtons->Add(m_Close, 0, (wxLEFT), 5);	

	m_MainSizer = new wxBoxSizer(wxVERTICAL);
	m_MainSizer->Add(m_Notebook, 1, wxEXPAND | wxALL, 5);
	m_MainSizer->Add(sButtons, 0, wxEXPAND | (wxLEFT | wxRIGHT | wxDOWN), 5);

	for (int i = MAX_WIIMOTES - 1; i > 0; i--)
	{
		m_Controller[i]->Enable(false);
	}

	this->SetSizer(m_MainSizer);
	this->Layout();

	Fit();

	// Center the window if there is room for it
	#ifdef _WIN32
		if (GetSystemMetrics(SM_CYFULLSCREEN) > 800)
			Center();
	#endif

	ControlsCreated = true;
}


void WiimotePadConfigDialog::GeneralSettingsChanged(wxCommandEvent& event)
{
	long TmpValue;

	switch (event.GetId())
	{
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
	}
	g_Config.Save();
	UpdateGUI();
}

void WiimotePadConfigDialog::UpdateGUI(int Slot)
{
	UpdateGUIButtonMapping(Page);
	DoChangeDeadZone(true); DoChangeDeadZone(false);

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
}

