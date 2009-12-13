// Copyright (C) 2003 Dolphin Project.

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

	EVT_TIMER(IDTM_BUTTON, WiimotePadConfigDialog::OnButtonTimer)
	EVT_TIMER(IDTM_UPDATE_PAD, WiimotePadConfigDialog::UpdatePad)

	EVT_COMBOBOX(IDC_JOYNAME, WiimotePadConfigDialog::GeneralSettingsChanged)
	EVT_CHECKBOX(IDC_RUMBLE, WiimotePadConfigDialog::GeneralSettingsChanged)
	EVT_COMBOBOX(IDC_RUMBLE_STRENGTH, WiimotePadConfigDialog::GeneralSettingsChanged)
	EVT_COMBOBOX(IDC_DEAD_ZONE_LEFT, WiimotePadConfigDialog::GeneralSettingsChanged)
	EVT_COMBOBOX(IDC_DEAD_ZONE_RIGHT, WiimotePadConfigDialog::GeneralSettingsChanged)
	EVT_COMBOBOX(IDC_STICK_DIAGONAL, WiimotePadConfigDialog::GeneralSettingsChanged)
	EVT_CHECKBOX(IDC_STICK_C2S, WiimotePadConfigDialog::GeneralSettingsChanged)
	EVT_COMBOBOX(IDC_TILT_INPUT, WiimotePadConfigDialog::GeneralSettingsChanged)	
	EVT_COMBOBOX(IDC_TILT_RANGE_ROLL, WiimotePadConfigDialog::GeneralSettingsChanged)
	EVT_COMBOBOX(IDC_TILT_RANGE_PITCH, WiimotePadConfigDialog::GeneralSettingsChanged)
	EVT_CHECKBOX(IDC_TILT_INVERT_ROLL, WiimotePadConfigDialog::GeneralSettingsChanged)
	EVT_CHECKBOX(IDC_TILT_INVERT_PITCH, WiimotePadConfigDialog::GeneralSettingsChanged)
	EVT_COMBOBOX(IDC_TRIGGER_TYPE, WiimotePadConfigDialog::GeneralSettingsChanged)	
	EVT_COMBOBOX(IDC_NUNCHUCK_STICK, WiimotePadConfigDialog::GeneralSettingsChanged)
	EVT_COMBOBOX(IDC_CC_LEFT_STICK, WiimotePadConfigDialog::GeneralSettingsChanged)
	EVT_COMBOBOX(IDC_CC_RIGHT_STICK, WiimotePadConfigDialog::GeneralSettingsChanged)
	EVT_COMBOBOX(IDC_CC_TRIGGERS, WiimotePadConfigDialog::GeneralSettingsChanged)
	EVT_COMBOBOX(IDC_GH3_ANALOG, WiimotePadConfigDialog::GeneralSettingsChanged)

	// Analog
	EVT_BUTTON(IDB_ANALOG_LEFT_X, WiimotePadConfigDialog::GetButtons) EVT_BUTTON(IDB_ANALOG_LEFT_Y, WiimotePadConfigDialog::GetButtons)
	EVT_BUTTON(IDB_ANALOG_RIGHT_X, WiimotePadConfigDialog::GetButtons) EVT_BUTTON(IDB_ANALOG_RIGHT_Y, WiimotePadConfigDialog::GetButtons)
	EVT_BUTTON(IDB_TRIGGER_L, WiimotePadConfigDialog::GetButtons) EVT_BUTTON(IDB_TRIGGER_R, WiimotePadConfigDialog::GetButtons)

	// Wiimote
	EVT_BUTTON(IDB_WM_A, WiimotePadConfigDialog::OnButtonClick) EVT_BUTTON(IDB_WM_B, WiimotePadConfigDialog::OnButtonClick)
	EVT_BUTTON(IDB_WM_1, WiimotePadConfigDialog::OnButtonClick) EVT_BUTTON(IDB_WM_2, WiimotePadConfigDialog::OnButtonClick)
	EVT_BUTTON(IDB_WM_P, WiimotePadConfigDialog::OnButtonClick) EVT_BUTTON(IDB_WM_M, WiimotePadConfigDialog::OnButtonClick)
	EVT_BUTTON(IDB_WM_H, WiimotePadConfigDialog::OnButtonClick)
	EVT_BUTTON(IDB_WM_L, WiimotePadConfigDialog::OnButtonClick) EVT_BUTTON(IDB_WM_R, WiimotePadConfigDialog::OnButtonClick)
	EVT_BUTTON(IDB_WM_U, WiimotePadConfigDialog::OnButtonClick) EVT_BUTTON(IDB_WM_D, WiimotePadConfigDialog::OnButtonClick)
	EVT_BUTTON(IDB_WM_ROLL_L, WiimotePadConfigDialog::OnButtonClick) EVT_BUTTON(IDB_WM_ROLL_R, WiimotePadConfigDialog::OnButtonClick)
	EVT_BUTTON(IDB_WM_PITCH_U, WiimotePadConfigDialog::OnButtonClick) EVT_BUTTON(IDB_WM_PITCH_D, WiimotePadConfigDialog::OnButtonClick)
	EVT_BUTTON(IDB_WM_SHAKE, WiimotePadConfigDialog::OnButtonClick)

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

	// Guitar Hero 3
	EVT_BUTTON(IDB_GH3_GREEN, WiimotePadConfigDialog::OnButtonClick)
	EVT_BUTTON(IDB_GH3_RED, WiimotePadConfigDialog::OnButtonClick)
	EVT_BUTTON(IDB_GH3_YELLOW, WiimotePadConfigDialog::OnButtonClick)
	EVT_BUTTON(IDB_GH3_BLUE, WiimotePadConfigDialog::OnButtonClick)
	EVT_BUTTON(IDB_GH3_ORANGE, WiimotePadConfigDialog::OnButtonClick)
	EVT_BUTTON(IDB_GH3_PLUS, WiimotePadConfigDialog::OnButtonClick)
	EVT_BUTTON(IDB_GH3_MINUS, WiimotePadConfigDialog::OnButtonClick)
	EVT_BUTTON(IDB_GH3_WHAMMY, WiimotePadConfigDialog::OnButtonClick)
	EVT_BUTTON(IDB_GH3_ANALOG_LEFT, WiimotePadConfigDialog::OnButtonClick)
	EVT_BUTTON(IDB_GH3_ANALOG_RIGHT, WiimotePadConfigDialog::OnButtonClick)
	EVT_BUTTON(IDB_GH3_ANALOG_UP, WiimotePadConfigDialog::OnButtonClick)
	EVT_BUTTON(IDB_GH3_ANALOG_DOWN, WiimotePadConfigDialog::OnButtonClick)
	EVT_BUTTON(IDB_GH3_STRUM_UP, WiimotePadConfigDialog::OnButtonClick)
	EVT_BUTTON(IDB_GH3_STRUM_DOWN, WiimotePadConfigDialog::OnButtonClick)

END_EVENT_TABLE()

WiimotePadConfigDialog::WiimotePadConfigDialog(wxWindow *parent, wxWindowID id, const wxString &title,
						   const wxPoint &position, const wxSize& size, long style)
: wxDialog(parent, id, title, position, size, style)
{
#if wxUSE_TIMER
	m_ButtonMappingTimer = new wxTimer(this, IDTM_BUTTON);
	m_UpdatePadTimer = new wxTimer(this, IDTM_UPDATE_PAD);

	// Reset values
	GetButtonWaitingID = 0;
	GetButtonWaitingTimer = 0;

	// Start the permanent timer
	const int TimesPerSecond = 10;
	m_UpdatePadTimer->Start(1000 / TimesPerSecond);
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

WiimotePadConfigDialog::~WiimotePadConfigDialog()
{
	if (m_ButtonMappingTimer)
	{
		delete m_ButtonMappingTimer;
		m_ButtonMappingTimer = NULL;
	}
	if (m_UpdatePadTimer)
	{
		delete m_UpdatePadTimer;
		m_UpdatePadTimer = NULL;
	}
}

void WiimotePadConfigDialog::OnKeyDown(wxKeyEvent& event)
{
	event.Skip();

	if(ClickedButton != NULL)
	{
		// Save the key
		g_Pressed = event.GetKeyCode();
		// Handle the keyboard key mapping
		char keyStr[128] = {0};

		// Allow the escape key to set a blank key
		if (g_Pressed == WXK_ESCAPE)
		{
			SaveKeyboardMapping(Page, ClickedButton->GetId(), -1);
			SetButtonText(ClickedButton->GetId(), "");
		}
		else
		{
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
					strcpy(keyStr, InputCommon::VKToString(i).c_str());
					SetButtonText(ClickedButton->GetId(), keyStr);
					// Save the setting
					SaveKeyboardMapping(Page, ClickedButton->GetId(), i);
					break;
				}
			}
		#elif defined(HAVE_X11) && HAVE_X11
			g_Pressed = InputCommon::wxCharCodeWXToX(g_Pressed);
			InputCommon::XKeyToString(g_Pressed, keyStr);
			SetButtonText(ClickedButton->GetId(), keyStr);
			SaveKeyboardMapping(Page, ClickedButton->GetId(), g_Pressed);
		#endif
		}
		m_ButtonMappingTimer->Stop();
		GetButtonWaitingTimer = 0;
		GetButtonWaitingID = 0;
		ClickedButton = NULL;
	}
}

// Input button clicked
void WiimotePadConfigDialog::OnButtonClick(wxCommandEvent& event)
{
	//DEBUG_LOG(WIIMOTE, "OnButtonClick: %i", g_Pressed);

	if (m_ButtonMappingTimer->IsRunning()) return;

	// Don't allow space to start a new Press Key option, that will interfer with setting a key to space
	if (g_Pressed == WXK_SPACE) { g_Pressed = 0; return; }

	// Create the button object
	ClickedButton = (wxButton *)event.GetEventObject();
	// Save old label so we can revert back
	OldLabel = ClickedButton->GetLabel();
	ClickedButton->SetWindowStyle(wxWANTS_CHARS);
	ClickedButton->SetLabel(wxT("<Press Key>"));
	DoGetButtons(ClickedButton->GetId());
}


void WiimotePadConfigDialog::OnClose(wxCloseEvent& event)
{
	if (m_UpdatePadTimer)
		m_UpdatePadTimer->Stop();
	if (m_ButtonMappingTimer)
		m_ButtonMappingTimer->Stop();
	SaveButtonMappingAll(Page);
	EndModal(wxID_CLOSE);
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
		for(int i = 0; i < 1; i++)	// Only got 1 wiimote yet
			SaveButtonMapping(i, true);
		// Save the settings for the current pad
		g_Config.Save(Slot);
		// Now we can update the ID
		WiiMoteEmu::PadMapping[Page].ID = m_Joyname[Page]->GetSelection();
	}
	else
	{
		// Update PadMapping[] from the GUI controls
		for(int i = 0; i < 1; i++) // Only got 1 wiimote yet
			SaveButtonMapping(i);
		g_Config.Save(Slot);
	}		

	// Then change it back to ""
	ToBlank();

	DEBUG_LOG(WIIMOTE, "WiiMoteEmu::PadMapping[%i].ID = %i", Page, m_Joyname[Page]->GetSelection());
}

// Bitmap box and dot
wxBitmap WiimotePadConfigDialog::CreateBitmap()
{
	BoxW = 70, BoxH = 70;
	wxBitmap bitmap(BoxW, BoxH);
	wxMemoryDC dc;
	dc.SelectObject(bitmap);

	// Set outline and fill colors
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
	dc.SelectObject(wxNullBitmap);
	return bitmap;
}

wxBitmap WiimotePadConfigDialog::CreateBitmapClear()
{
	wxBitmap bitmap(BoxW, BoxH);
	wxMemoryDC dc;
	dc.SelectObject(bitmap);
	
	dc.Clear();
	dc.SelectObject(wxNullBitmap);
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

	wxArrayString TextDeadZone;
	for (int i = 0; i <= 50; i++)
		TextDeadZone.Add(wxString::Format(wxT("%i%%"), i));

	wxArrayString StrDiagonal;
		StrDiagonal.Add(wxT("100%"));
		StrDiagonal.Add(wxT("95%"));
		StrDiagonal.Add(wxT("90%"));
		StrDiagonal.Add(wxT("85%"));
		StrDiagonal.Add(wxT("80%"));

	wxArrayString StrRumble;
	for (int i = 0; i <= 10; i++)
		StrRumble.Add(wxString::Format(wxT("%i%%"), i*10));

	// The tilt list
	wxArrayString StrTilt;
	StrTilt.Add(wxString::FromAscii("<Off>"));
	StrTilt.Add(wxString::FromAscii("Keyboard"));
	StrTilt.Add(wxString::FromAscii("Analog 1"));
	StrTilt.Add(wxString::FromAscii("Analog 2"));
	StrTilt.Add(wxString::FromAscii("Triggers"));

	// The range is in degrees and are set at even 5 degrees values
	wxArrayString StrTiltRangeRoll, StrTiltRangePitch;
	StrTiltRangeRoll.Add(wxString::Format(wxT("Free Swing")));
	for (int i = 1; i < 37; i++)
		StrTiltRangeRoll.Add(wxString::Format(wxT("%i"), i*5));
	StrTiltRangePitch.Add(wxString::Format(wxT("Free Swing")));
	for (int i = 1; i < 37; i++)
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

	// GH3 stick list
	wxArrayString StrAnalogArray;
	StrAnalogArray.Add(wxString::FromAscii("Analog 1"));
	StrAnalogArray.Add(wxString::FromAscii("Analog 2"));

	static const wxChar* anText[] =
	{
		wxT("Left X-Axis"),
		wxT("Left Y-Axis"),
		wxT("Right X-Axis"),
		wxT("Right Y-Axis"),
		wxT("Left Trigger"),
		wxT("Right Trigger"),
	};

	static const wxChar* wmText[] =
	{
		wxT("A"),
		wxT("B"),
		wxT("1"),
		wxT("2"),
		wxT("+"),
		wxT("- "), // Intentional space
		wxT("Home"),
		wxT("Left"),
		wxT("Right"),
		wxT("Up"),
		wxT("Down"),
		wxT("Roll Left"),
		wxT("Roll Right"),
		wxT("Pitch Up"),
		wxT("Pitch Down"),
		wxT("Shake"),
	};

	static const wxChar* ncText[] =
	{
		wxT("Z"),
		wxT("C"),
		wxT("Left"),
		wxT("Right"),
		wxT("Up"),
		wxT("Down"),
		wxT("Shake"),
	};

	static const wxChar* classicText[] =
	{
		 wxT("A"),
		 wxT("B"),
		 wxT("X"),
		 wxT("Y"),
		 wxT("+"),
		 wxT("- "),
		 wxT("Home"),
		 wxT("Left trigger"),
		 wxT("Right trigger"),
		 wxT("Left Z"),
		 wxT("Right Z"),
		 wxT("Digital Left"),
		 wxT("Digital Right"),
		 wxT("Digital Up"),
		 wxT("Digital Down"),
		 wxT("L Left"),
		 wxT("L Right"),
		 wxT("L Up"),
		 wxT("L Down"),
		 wxT("R Left"),
		 wxT("R Right"),
		 wxT("R Up"),
		 wxT("R Down"),
	};

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
		wxT("Right"),
		wxT("Up"),
		wxT("Down"),
		wxT("Strum Up"),
		wxT("Strum Down"),
	};

	// Configuration controls sizes
	static const int TxtW = 50, TxtH = 20, BtW = 70, BtH = 20;
	// A small type font
	wxFont m_SmallFont(7, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL);

	m_Notebook = new wxNotebook(this, ID_NOTEBOOK, wxDefaultPosition, wxDefaultSize);

	for (int i = 0; i < MAX_WIIMOTES; i++)
	{
		m_Controller[i] = new wxPanel(m_Notebook, ID_CONTROLLERPAGE1 + i, wxDefaultPosition, wxDefaultSize);
		m_Notebook->AddPage(m_Controller[i], wxString::Format(wxT("Wiimote %d"), i+1));

		// Controller
		m_Joyname[i] = new wxComboBox(m_Controller[i], IDC_JOYNAME, StrJoyname[0], wxDefaultPosition, wxSize(200, -1), StrJoyname, wxCB_READONLY);
		m_Joyname[i]->SetToolTip(wxT("Save your settings and configure another joypad"));

		// Dead zone
		m_ComboDeadZoneLabel[i] = new wxStaticText(m_Controller[i], wxID_ANY, wxT("Dead Zone"));
		m_ComboDeadZoneLeft[i] = new wxComboBox(m_Controller[i], IDC_DEAD_ZONE_LEFT, TextDeadZone[0], wxDefaultPosition,  wxSize(50, -1), TextDeadZone, wxCB_READONLY);
		m_ComboDeadZoneRight[i] = new wxComboBox(m_Controller[i], IDC_DEAD_ZONE_RIGHT, TextDeadZone[0], wxDefaultPosition,  wxSize(50, -1), TextDeadZone, wxCB_READONLY);

		// Circle to square
		m_CheckC2S[i] = new wxCheckBox(m_Controller[i], IDC_STICK_C2S, wxT("Circle To Square"));
		m_CheckC2S[i]->SetToolTip(wxT("This will convert a circular stick radius to a square stick radius.\n")
			wxT("This can be useful for the pitch and roll emulation."));

		// The drop down menu for the circle to square adjustment
		m_DiagonalLabel[i] = new wxStaticText(m_Controller[i], wxID_ANY, wxT("Diagonal"));
		m_DiagonalLabel[i]->SetToolTip(wxT("To produce a perfect square circle in the 'Out' window you have to manually set\n")
			wxT("your diagonal values here from what is shown in the 'In' window."));
		m_ComboDiagonal[i] = new wxComboBox(m_Controller[i], IDC_STICK_DIAGONAL, StrDiagonal[0], wxDefaultPosition, wxSize(50, -1), StrDiagonal, wxCB_READONLY);
		
		// Rumble
		m_CheckRumble[i] = new wxCheckBox(m_Controller[i], IDC_RUMBLE, wxT("Rumble"));
		m_RumbleStrengthLabel[i] = new wxStaticText(m_Controller[i], wxID_ANY, wxT("Strength"));
		m_RumbleStrength[i] = new wxComboBox(m_Controller[i], IDC_RUMBLE_STRENGTH, StrRumble[0], wxDefaultPosition, wxSize(50, -1), StrRumble, wxCB_READONLY);

		// Sizers
		m_sDeadZoneHoriz[i] = new wxBoxSizer(wxHORIZONTAL);
		m_sDeadZoneHoriz[i]->Add(m_ComboDeadZoneLeft[i], 0, (wxUP), 0);
		m_sDeadZoneHoriz[i]->Add(m_ComboDeadZoneRight[i], 0, (wxUP), 0);

		m_sDeadZone[i] = new wxBoxSizer(wxVERTICAL);
		m_sDeadZone[i]->Add(m_ComboDeadZoneLabel[i], 0, wxALIGN_CENTER | (wxUP), 0);	
		m_sDeadZone[i]->Add(m_sDeadZoneHoriz[i], 0, wxALIGN_CENTER | (wxUP), 2);

		m_sDiagonal[i] = new wxBoxSizer(wxHORIZONTAL);
		m_sDiagonal[i]->Add(m_DiagonalLabel[i], 0, (wxUP), 4);
		m_sDiagonal[i]->Add(m_ComboDiagonal[i], 0, (wxLEFT), 2);

		m_sCircle2Square[i] = new wxBoxSizer(wxVERTICAL);
		m_sCircle2Square[i]->Add(m_CheckC2S[i], 0, wxALIGN_CENTER | (wxUP), 0);
		m_sCircle2Square[i]->Add(m_sDiagonal[i], 0, wxALIGN_CENTER | (wxUP), 2);

		m_sC2SDeadZone[i] = new wxBoxSizer(wxHORIZONTAL);
		m_sC2SDeadZone[i]->Add(m_sDeadZone[i], 0, (wxUP), 0);
		m_sC2SDeadZone[i]->Add(m_sCircle2Square[i], 0, (wxLEFT), 8);

		m_sJoyname[i] = new wxBoxSizer(wxVERTICAL);
		m_sJoyname[i]->Add(m_Joyname[i], 0, wxALIGN_CENTER | (wxUP | wxDOWN), 4);
		m_sJoyname[i]->Add(m_sC2SDeadZone[i], 0, (wxUP | wxDOWN), 4);

		m_sRumble[i] = new wxBoxSizer(wxVERTICAL);
		m_sRumble[i]->Add(m_CheckRumble[i], 0, wxALIGN_CENTER | (wxUP | wxDOWN), 8);
		m_sRumble[i]->Add(m_RumbleStrengthLabel[i], 0, wxALIGN_CENTER | (wxUP), 4);
		m_sRumble[i]->Add(m_RumbleStrength[i], 0, (wxUP | wxDOWN), 2);

		m_gJoyPad[i] = new wxStaticBoxSizer (wxHORIZONTAL, m_Controller[i], wxT("Gamepad"));
		m_gJoyPad[i]->AddStretchSpacer();
		m_gJoyPad[i]->Add(m_sJoyname[i], 0, wxEXPAND | (wxLEFT | wxRIGHT | wxDOWN), 5);
		m_gJoyPad[i]->Add(m_sRumble[i], 0, wxEXPAND | (wxLEFT | wxRIGHT | wxDOWN), 5);
		m_gJoyPad[i]->AddStretchSpacer();

		// Tilt Wiimote
		m_TiltComboInput[i] = new wxComboBox(m_Controller[i], IDC_TILT_INPUT, StrTilt[0], wxDefaultPosition,  wxSize(100, -1), StrTilt, wxCB_READONLY);
		m_TiltComboInput[i]->SetSelection(g_Config.Tilt.Type);
		m_TiltComboInput[i]->SetToolTip(wxT("Control tilting by an analog gamepad stick, an analog trigger or the keyboard."));		

		m_TiltTextRoll[i] = new wxStaticText(m_Controller[i], wxID_ANY, wxT("Roll L/R"));
		m_TiltTextPitch[i] = new wxStaticText(m_Controller[i], wxID_ANY, wxT("Pitch U/D"));

		m_TiltComboRangeRoll[i] = new wxComboBox(m_Controller[i], IDC_TILT_RANGE_ROLL, StrTiltRangeRoll[0], wxDefaultPosition,  wxSize(75, -1), StrTiltRangeRoll, wxCB_READONLY);
		m_TiltComboRangeRoll[i]->SetValue(wxString::Format(wxT("%i"), g_Config.Tilt.Range.Roll));
		m_TiltComboRangeRoll[i]->SetToolTip(wxT("The maximum roll in degrees."));	
		m_TiltComboRangePitch[i] = new wxComboBox(m_Controller[i], IDC_TILT_RANGE_PITCH, StrTiltRangePitch[0], wxDefaultPosition,  wxSize(75, -1), StrTiltRangePitch, wxCB_READONLY);
		m_TiltComboRangePitch[i]->SetValue(wxString::Format(wxT("%i"), g_Config.Tilt.Range.Pitch));
		m_TiltComboRangePitch[i]->SetToolTip(wxT("The maximum pitch in degrees."));

		m_TiltInvertRoll[i] = new wxCheckBox(m_Controller[i], IDC_TILT_INVERT_ROLL, wxT("Invert"));
		m_TiltInvertPitch[i] = new wxCheckBox(m_Controller[i], IDC_TILT_INVERT_PITCH, wxT("Invert"));

		// Sizers
		m_sGridTilt[i] = new wxGridBagSizer(0, 0);
		m_sGridTilt[i]->Add(m_TiltTextRoll[i], wxGBPosition(0, 0), wxGBSpan(1, 1), (wxTOP), 4);
		m_sGridTilt[i]->Add(m_TiltComboRangeRoll[i], wxGBPosition(0, 1), wxGBSpan(1, 1), (wxLEFT), 2);
		m_sGridTilt[i]->Add(m_TiltInvertRoll[i], wxGBPosition(0, 2), wxGBSpan(1, 1), (wxLEFT | wxTOP), 4);
		m_sGridTilt[i]->Add(m_TiltTextPitch[i], wxGBPosition(1, 0), wxGBSpan(1, 1), (wxTOP), 4);
		m_sGridTilt[i]->Add(m_TiltComboRangePitch[i], wxGBPosition(1, 1), wxGBSpan(1, 1), (wxLEFT | wxTOP), 2);
		m_sGridTilt[i]->Add(m_TiltInvertPitch[i], wxGBPosition(1, 2), wxGBSpan(1, 1), (wxLEFT | wxTOP), 4);

		m_gTilt[i] = new wxStaticBoxSizer (wxVERTICAL, m_Controller[i], wxT("Tilt and Swing"));
		m_gTilt[i]->AddStretchSpacer();
		m_gTilt[i]->Add(m_TiltComboInput[i], 0, wxEXPAND | (wxLEFT | wxRIGHT | wxDOWN), 5);
		m_gTilt[i]->Add(m_sGridTilt[i], 0, wxEXPAND | (wxLEFT | wxRIGHT), 5);
		m_gTilt[i]->AddStretchSpacer();

		// Row 1 Sizers: Connected pads, tilt
		m_sHorizController[i] = new wxBoxSizer(wxHORIZONTAL);
		m_sHorizController[i]->Add(m_gJoyPad[i], 0, wxEXPAND | (wxLEFT), 5);
		m_sHorizController[i]->Add(m_gTilt[i], 0, wxEXPAND | (wxLEFT), 5);


		// Stick Status Panels
		m_tStatusLeftIn[i] = new wxStaticText(m_Controller[i], wxID_ANY, wxT("In"));
		m_tStatusLeftOut[i] = new wxStaticText(m_Controller[i], wxID_ANY, wxT("Out"));
		m_tStatusRightIn[i] = new wxStaticText(m_Controller[i], wxID_ANY, wxT("In"));
		m_tStatusRightOut[i] = new wxStaticText(m_Controller[i], wxID_ANY, wxT("Out"));

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

		// Sizers
		m_sGridStickLeft[i] = new wxGridBagSizer(0, 0);
		m_sGridStickLeft[i]->Add(m_pLeftInStatus[i], wxGBPosition(0, 0), wxGBSpan(1, 1), wxALL, 0);
		m_sGridStickLeft[i]->Add(m_pLeftOutStatus[i], wxGBPosition(0, 1), wxGBSpan(1, 1), wxLEFT, 10);
		m_sGridStickLeft[i]->Add(m_tStatusLeftIn[i], wxGBPosition(1, 0), wxGBSpan(1, 1), wxALL, 0);
		m_sGridStickLeft[i]->Add(m_tStatusLeftOut[i], wxGBPosition(1, 1), wxGBSpan(1, 1), wxLEFT, 10);

		m_sGridStickRight[i] = new wxGridBagSizer(0, 0);
		m_sGridStickRight[i]->Add(m_pRightInStatus[i], wxGBPosition(0, 0), wxGBSpan(1, 1), wxALL, 0);
		m_sGridStickRight[i]->Add(m_pRightOutStatus[i], wxGBPosition(0, 1), wxGBSpan(1, 1), wxLEFT, 10);
		m_sGridStickRight[i]->Add(m_tStatusRightIn[i], wxGBPosition(1, 0), wxGBSpan(1, 1), wxALL, 0);
		m_sGridStickRight[i]->Add(m_tStatusRightOut[i], wxGBPosition(1, 1), wxGBSpan(1, 1), wxLEFT, 10);

		m_gStickLeft[i] = new wxStaticBoxSizer (wxHORIZONTAL, m_Controller[i], wxT("Analog 1 Status (In) (Out)"));
		m_gStickLeft[i]->Add(m_sGridStickLeft[i], 0, (wxLEFT | wxRIGHT), 5);

		m_gStickRight[i] = new wxStaticBoxSizer (wxHORIZONTAL, m_Controller[i], wxT("Analog 2 Status (In) (Out)"));
		m_gStickRight[i]->Add(m_sGridStickRight[i], 0, (wxLEFT | wxRIGHT), 5);

		// Trigger Status Panels		
		m_TriggerL[i]= new wxStaticText(m_Controller[i], wxID_ANY, wxT("Left: "));
		m_TriggerR[i]= new wxStaticText(m_Controller[i], wxID_ANY, wxT("Right: "));
		m_TriggerStatusL[i]= new wxStaticText(m_Controller[i], wxID_ANY, wxT("000"), wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT);
		m_TriggerStatusR[i]= new wxStaticText(m_Controller[i], wxID_ANY, wxT("000"), wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT);
		m_tTriggerSource[i]= new wxStaticText(m_Controller[i], wxID_ANY, wxT("Input Source"));
		m_TriggerType[i] = new wxComboBox(m_Controller[i], IDC_TRIGGER_TYPE, StrTriggerType[0], wxDefaultPosition,  wxSize(70, -1), StrTriggerType, wxCB_READONLY);

		// Sizers
		m_sGridTrigger[i] = new wxGridBagSizer(0, 0);
		m_sGridTrigger[i]->Add(m_TriggerL[i], wxGBPosition(0, 0), wxGBSpan(1, 1), (wxTOP), 4);
		m_sGridTrigger[i]->Add(m_TriggerStatusL[i], wxGBPosition(0, 1), wxGBSpan(1, 1), (wxLEFT | wxTOP), 4);
		m_sGridTrigger[i]->Add(m_TriggerR[i], wxGBPosition(1, 0), wxGBSpan(1, 1), (wxTOP), 4);
		m_sGridTrigger[i]->Add(m_TriggerStatusR[i], wxGBPosition(1, 1), wxGBSpan(1, 1), (wxLEFT | wxTOP), 4);

		m_gTriggers[i] = new wxStaticBoxSizer (wxVERTICAL, m_Controller[i], wxT("Triggers Status"));
		m_gTriggers[i]->AddStretchSpacer();
		m_gTriggers[i]->Add(m_sGridTrigger[i], 0, wxEXPAND | (wxLEFT | wxRIGHT), 5);
		m_gTriggers[i]->Add(m_tTriggerSource[i], 0, wxEXPAND | (wxUP | wxLEFT | wxRIGHT), 5);
		m_gTriggers[i]->Add(m_TriggerType[i], 0, wxEXPAND | (wxUP | wxLEFT | wxRIGHT), 5);
		m_gTriggers[i]->AddStretchSpacer();

		// Row 2 Sizers: Analog status
		m_sHorizStatus[i] = new wxBoxSizer(wxHORIZONTAL);
		m_sHorizStatus[i]->Add(m_gStickLeft[i], 0, wxEXPAND | (wxLEFT), 5);
		m_sHorizStatus[i]->Add(m_gStickRight[i], 0, wxEXPAND | (wxLEFT), 5);
		m_sHorizStatus[i]->Add(m_gTriggers[i], 0, wxEXPAND | (wxLEFT), 5);


		// Analog Axes and Triggers Mapping
		m_sAnalogLeft[i] = new wxBoxSizer(wxVERTICAL);
		m_sAnalogMiddle[i] = new wxBoxSizer(wxVERTICAL);		
		m_sAnalogRight[i] = new wxBoxSizer(wxVERTICAL);

		for (int x = 0; x < AN_CONTROLS; x++)
		{
			m_statictext_Analog[x][i] = new wxStaticText(m_Controller[i], wxID_ANY, anText[x]);
			m_Button_Analog[x][i] = new wxButton(m_Controller[i], x + IDB_ANALOG_LEFT_X, wxEmptyString, wxDefaultPosition, wxSize(BtW, BtH));
			m_Button_Analog[x][i]->SetFont(m_SmallFont);
			m_Sizer_Analog[x][i] = new wxBoxSizer(wxHORIZONTAL);
			m_Sizer_Analog[x][i]->Add(m_statictext_Analog[x][i], 0, (wxUP), 4);
			m_Sizer_Analog[x][i]->Add(m_Button_Analog[x][i], 0, (wxLEFT), 2);
			if (x < 2) // Make some balance
					m_sAnalogLeft[i]->Add(m_Sizer_Analog[x][i], 0, wxALIGN_RIGHT | (wxLEFT | wxRIGHT | wxDOWN), 1);
			else if (x < 4)
					m_sAnalogMiddle[i]->Add(m_Sizer_Analog[x][i], 0, wxALIGN_RIGHT | (wxLEFT | wxRIGHT | wxDOWN), 1);
			else
					m_sAnalogRight[i]->Add(m_Sizer_Analog[x][i], 0, wxALIGN_RIGHT | (wxLEFT | wxRIGHT | wxDOWN), 1);
		}

		m_gAnalog[i] = new wxStaticBoxSizer (wxHORIZONTAL, m_Controller[i], wxT("Analog Axes and Triggers"));
		m_gAnalog[i]->Add(m_sAnalogLeft[i], 0, wxALIGN_RIGHT | (wxALL), 0);
		m_gAnalog[i]->Add(m_sAnalogMiddle[i], 0, wxALIGN_RIGHT | (wxLEFT), 5);
		m_gAnalog[i]->Add(m_sAnalogRight[i], 0, wxALIGN_RIGHT | (wxLEFT), 5);
		m_gAnalog[i]->AddSpacer(1);

		// Row 3 Sizes: Analog Mapping
		m_sHorizAnalogMapping[i] = new wxBoxSizer(wxHORIZONTAL);
		m_sHorizAnalogMapping[i]->Add(m_gAnalog[i], 0, (wxLEFT), 5);


		// Wiimote Mapping
		m_sWmVertLeft[i] = new wxBoxSizer(wxVERTICAL);
		m_sWmVertRight[i] = new wxBoxSizer(wxVERTICAL);

		for (int x = 0; x < WM_CONTROLS; x++)
		{
				m_statictext_Wiimote[x][i] = new wxStaticText(m_Controller[i], wxID_ANY, wmText[x]);
				m_Button_Wiimote[x][i] = new wxButton(m_Controller[i], x + IDB_WM_A, wxEmptyString, wxDefaultPosition, wxSize(BtW, BtH));
				m_Button_Wiimote[x][i]->SetFont(m_SmallFont);
				m_Sizer_Wiimote[x][i] = new wxBoxSizer(wxHORIZONTAL);
				m_Sizer_Wiimote[x][i]->Add(m_statictext_Wiimote[x][i], 0, (wxUP), 4);
				m_Sizer_Wiimote[x][i]->Add(m_Button_Wiimote[x][i], 0, (wxLEFT), 2);
				if (x < 7 || x == WM_CONTROLS -1) // Make some balance
					m_sWmVertLeft[i]->Add(m_Sizer_Wiimote[x][i], 0, wxALIGN_RIGHT | (wxLEFT | wxRIGHT | wxDOWN), 1);
				else
					m_sWmVertRight[i]->Add(m_Sizer_Wiimote[x][i], 0, wxALIGN_RIGHT | (wxLEFT | wxRIGHT | wxDOWN), 1);
		}

		m_gWiimote[i] = new wxStaticBoxSizer (wxHORIZONTAL, m_Controller[i], wxT("Wiimote"));
		m_gWiimote[i]->Add(m_sWmVertLeft[i], 0, wxALIGN_RIGHT | (wxALL), 0);
		m_gWiimote[i]->Add(m_sWmVertRight[i], 0, wxALIGN_RIGHT | (wxLEFT), 5);
		m_gWiimote[i]->AddSpacer(1);

		// Extension Mapping
		if(g_Config.iExtensionConnected == EXT_NUNCHUCK)
		{
			// Stick controls
			m_NunchuckTextStick[i] = new wxStaticText(m_Controller[i], wxID_ANY, wxT("Stick"));
			m_NunchuckComboStick[i] = new wxComboBox(m_Controller[i], IDC_NUNCHUCK_STICK, StrNunchuck[0], wxDefaultPosition, wxSize(70, -1), StrNunchuck, wxCB_READONLY);
			m_NunchuckComboStick[i]->SetSelection(g_Config.Nunchuck.Type);

			m_sNunchuckStick[i] = new wxBoxSizer(wxHORIZONTAL);
			m_sNunchuckStick[i]->Add(m_NunchuckTextStick[i], 0, (wxUP), 4);
			m_sNunchuckStick[i]->Add(m_NunchuckComboStick[i], 0, (wxLEFT), 2);

			m_gNunchuck[i] = new wxStaticBoxSizer (wxVERTICAL, m_Controller[i], wxT("Nunchuck"));
			m_gNunchuck[i]->Add(m_sNunchuckStick[i], 0, wxALIGN_RIGHT | (wxLEFT | wxRIGHT | wxDOWN), 2);
			m_gNunchuck[i]->AddSpacer(2);
			
			for (int x = 0; x < NC_CONTROLS; x++)
			{
				m_statictext_NunChuck[x][i] = new wxStaticText(m_Controller[i], wxID_ANY, ncText[x]);
				m_Button_NunChuck[x][i] = new wxButton(m_Controller[i], x + IDB_NC_Z, wxEmptyString, wxDefaultPosition, wxSize(BtW, BtH));
				m_Button_NunChuck[x][i]->SetFont(m_SmallFont);
				m_Sizer_NunChuck[x][i] = new wxBoxSizer(wxHORIZONTAL);
				m_Sizer_NunChuck[x][i]->Add(m_statictext_NunChuck[x][i], 0, wxALIGN_RIGHT | (wxUP), 4);
				m_Sizer_NunChuck[x][i]->Add(m_Button_NunChuck[x][i], 0, wxALIGN_RIGHT | (wxLEFT), 2);
				m_gNunchuck[i]->Add(m_Sizer_NunChuck[x][i], 0, wxALIGN_RIGHT | (wxLEFT | wxRIGHT | wxDOWN), 1);
			}
		}
		else if(g_Config.iExtensionConnected == EXT_CLASSIC_CONTROLLER)
		{
			// Stick controls
			m_CcTextLeftStick[i] = new wxStaticText(m_Controller[i], wxID_ANY, wxT("Left stick"));
			m_CcComboLeftStick[i] = new wxComboBox(m_Controller[i], IDC_CC_LEFT_STICK, StrNunchuck[0], wxDefaultPosition,  wxSize(70, -1), StrNunchuck, wxCB_READONLY);
			m_CcComboLeftStick[i]->SetSelection(g_Config.ClassicController.LType);
			m_CcTextRightStick[i] = new wxStaticText(m_Controller[i], wxID_ANY, wxT("Right stick"));
			m_CcComboRightStick[i] = new wxComboBox(m_Controller[i], IDC_CC_RIGHT_STICK, StrNunchuck[0], wxDefaultPosition,  wxSize(70, -1), StrNunchuck, wxCB_READONLY);
			m_CcComboRightStick[i]->SetSelection(g_Config.ClassicController.RType);
			m_CcTextTriggers[i] = new wxStaticText(m_Controller[i], wxID_ANY, wxT("Triggers"));
			m_CcComboTriggers[i] = new wxComboBox(m_Controller[i], IDC_CC_TRIGGERS, StrCcTriggers[0], wxDefaultPosition,  wxSize(70, -1), StrCcTriggers, wxCB_READONLY);
			m_CcComboTriggers[i]->SetSelection(g_Config.ClassicController.TType);

			for (int x = 0; x < CC_CONTROLS; x++)
			{
				m_statictext_Classic[x][i] = new wxStaticText(m_Controller[i], wxID_ANY, classicText[x]);
				m_Button_Classic[x][i] = new wxButton(m_Controller[i], x + IDB_CC_A, wxEmptyString, wxDefaultPosition, wxSize(BtW, BtH));
				m_Button_Classic[x][i]->SetFont(m_SmallFont);
				m_Sizer_Classic[x][i] = new wxBoxSizer(wxHORIZONTAL);
				m_Sizer_Classic[x][i]->Add(m_statictext_Classic[x][i], 0, wxALIGN_RIGHT | (wxUP), 4);
				m_Sizer_Classic[x][i]->Add(m_Button_Classic[x][i], 0, wxALIGN_RIGHT | (wxLEFT), 2);
			}

			// Sizers
			m_sCcLeftStick[i] = new wxBoxSizer(wxHORIZONTAL);
			m_sCcLeftStick[i]->Add(m_CcTextLeftStick[i], 0, (wxUP), 4);
			m_sCcLeftStick[i]->Add(m_CcComboLeftStick[i], 0, (wxLEFT), 2);
		
			m_sCcRightStick[i] = new wxBoxSizer(wxHORIZONTAL);
			m_sCcRightStick[i]->Add(m_CcTextRightStick[i], 0, (wxUP), 4);
			m_sCcRightStick[i]->Add(m_CcComboRightStick[i], 0, (wxLEFT), 2);

			m_sCcTriggers[i] = new wxBoxSizer(wxHORIZONTAL);
			m_sCcTriggers[i]->Add(m_CcTextTriggers[i], 0, (wxUP), 4);
			m_sCcTriggers[i]->Add(m_CcComboTriggers[i], 0, (wxLEFT), 2);

			m_sCcVertLeft[i] = new wxBoxSizer(wxVERTICAL);
			m_sCcVertLeft[i]->Add(m_sCcLeftStick[i], 0, wxALIGN_RIGHT | (wxALL), 2);
			m_sCcVertLeft[i]->AddSpacer(2);
			for (int x = IDB_CC_LL; x <= IDB_CC_LD; x++)
				m_sCcVertLeft[i]->Add(m_Sizer_Classic[x - IDB_CC_A][i], 0, wxALIGN_RIGHT | (wxLEFT | wxRIGHT | wxDOWN), 1);
			for (int x = IDB_CC_DL; x <= IDB_CC_DD; x++)
				m_sCcVertLeft[i]->Add(m_Sizer_Classic[x - IDB_CC_A][i], 0, wxALIGN_RIGHT | (wxLEFT | wxRIGHT | wxDOWN), 1);

			m_sCcVertMiddle[i] = new wxBoxSizer(wxVERTICAL);
			m_sCcVertMiddle[i]->Add(m_sCcRightStick[i], 0, wxALIGN_RIGHT | (wxALL), 2);
			m_sCcVertMiddle[i]->AddSpacer(2);
			for (int x = IDB_CC_RL; x <= IDB_CC_RD; x++)
				m_sCcVertMiddle[i]->Add(m_Sizer_Classic[x - IDB_CC_A][i], 0, wxALIGN_RIGHT | (wxLEFT | wxRIGHT | wxDOWN), 1);
			for (int x = IDB_CC_A; x <= IDB_CC_Y; x++)
				m_sCcVertMiddle[i]->Add(m_Sizer_Classic[x - IDB_CC_A][i], 0, wxALIGN_RIGHT | (wxLEFT | wxRIGHT | wxDOWN), 1);

			m_sCcVertRight[i] = new wxBoxSizer(wxVERTICAL);
			m_sCcVertRight[i]->Add(m_sCcTriggers[i], 0, wxALIGN_RIGHT | (wxALL), 2);
			m_sCcVertRight[i]->AddSpacer(2);
			for (int x = IDB_CC_TL; x <= IDB_CC_ZR; x++)
				m_sCcVertRight[i]->Add(m_Sizer_Classic[x - IDB_CC_A][i], 0, wxALIGN_RIGHT | (wxLEFT | wxRIGHT | wxDOWN), 1);
			for (int x = IDB_CC_P; x <= IDB_CC_H; x++)
				m_sCcVertRight[i]->Add(m_Sizer_Classic[x - IDB_CC_A][i], 0, wxALIGN_RIGHT | (wxLEFT | wxRIGHT | wxDOWN), 1);

			m_gClassicController[i] = new wxStaticBoxSizer (wxHORIZONTAL, m_Controller[i], wxT("Classic Controller"));
			m_gClassicController[i]->Add(m_sCcVertLeft[i], 0, wxALIGN_RIGHT | (wxLEFT | wxRIGHT | wxDOWN), 1);
			m_gClassicController[i]->Add(m_sCcVertMiddle[i], 0, wxALIGN_RIGHT | (wxLEFT | wxRIGHT | wxDOWN), 1);
			m_gClassicController[i]->Add(m_sCcVertRight[i], 0, wxALIGN_RIGHT | (wxLEFT | wxRIGHT | wxDOWN), 1);
		}
		else if(g_Config.iExtensionConnected == EXT_GUITARHERO3_CONTROLLER)
		{			
			// Stick controls
			m_tGH3Analog[i] = new wxStaticText(m_Controller[i], wxID_ANY, wxT("Joystick"));
			m_GH3ComboAnalog[i] = new wxComboBox(m_Controller[i], IDC_GH3_ANALOG, StrAnalogArray[0], wxDefaultPosition, wxSize(70, -1), StrAnalogArray, wxCB_READONLY);
			m_GH3ComboAnalog[i]->SetSelection(g_Config.GH3Controller.AType);

			for (int x = 0; x < GH3_CONTROLS; x++)
			{
				m_statictext_GH3[x][i] = new wxStaticText(m_Controller[i], wxID_ANY, gh3Text[x]);
				m_Button_GH3[x][i] = new wxButton(m_Controller[i], x, wxEmptyString, wxDefaultPosition, wxSize(BtW, BtH));
				m_Sizer_GH3[x][i] = new wxBoxSizer(wxHORIZONTAL);
				m_Sizer_GH3[x][i]->Add(m_statictext_GH3[x][i], 0, (wxUP), 4);
				m_Sizer_GH3[x][i]->Add(m_Button_GH3[x][i], 0, (wxLEFT), 2);
			}

			// Sizers
			m_sGH3Analog[i] = new wxBoxSizer(wxHORIZONTAL);
			m_sGH3Analog[i]->Add(m_tGH3Analog[i], 0, (wxUP), 4);
			m_sGH3Analog[i]->Add(m_GH3ComboAnalog[i], 0, (wxLEFT), 2);

			m_sGH3VertLeft[i] = new wxBoxSizer(wxVERTICAL);
			m_sGH3VertLeft[i]->Add(m_sGH3Analog[i], 0, wxALIGN_RIGHT | (wxALL), 2);
			m_sGH3VertLeft[i]->AddSpacer(2);
			for (int x = IDB_GH3_ANALOG_LEFT; x <= IDB_GH3_STRUM_DOWN; x++)
				m_sGH3VertLeft[i]->Add(m_Sizer_GH3[x - IDB_GH3_GREEN][i], 0, wxALIGN_RIGHT | (wxLEFT | wxRIGHT | wxDOWN), 1);

			m_sGH3VertRight[i] = new wxBoxSizer(wxVERTICAL);
			for (int x = IDB_GH3_GREEN; x <= IDB_GH3_WHAMMY; x++)
				m_sGH3VertRight[i]->Add(m_Sizer_GH3[x - IDB_GH3_GREEN][i], 0, wxALIGN_RIGHT | (wxLEFT | wxRIGHT | wxDOWN), 1);

			m_gGuitarHero3Controller[i] = new wxStaticBoxSizer (wxHORIZONTAL, m_Controller[i], wxT("Guitar Hero 3 Controller"));
			m_gGuitarHero3Controller[i]->Add(m_sGH3VertLeft[i], 0, wxALIGN_RIGHT | (wxLEFT | wxRIGHT | wxDOWN), 1);
			m_gGuitarHero3Controller[i]->Add(m_sGH3VertRight[i], 0, wxALIGN_RIGHT | (wxLEFT | wxRIGHT | wxDOWN), 1);
		}

		// Row 4 Sizers: Wiimote and Extension Mapping
		m_sHorizControllerMapping[i] = new wxBoxSizer(wxHORIZONTAL);
		m_sHorizControllerMapping[i]->Add(m_gWiimote[i], 0, (wxLEFT), 5);
		switch(g_Config.iExtensionConnected)
		{
		case EXT_NUNCHUCK:
			m_sHorizControllerMapping[i]->Add(m_gNunchuck[i], 0, (wxLEFT), 5);
			break;
		case EXT_CLASSIC_CONTROLLER:
			m_sHorizControllerMapping[i]->Add(m_gClassicController[i], 0, (wxLEFT), 5);
			break;
		case EXT_GUITARHERO3_CONTROLLER:
			m_sHorizControllerMapping[i]->Add(m_gGuitarHero3Controller[i], 0, (wxLEFT), 5);
			break;
		default:
			break;
		}

		// Set up sizers and layout

		// The sizer m_sMain will be expanded inside m_Controller
		m_sMain[i] = new wxBoxSizer(wxVERTICAL);
		m_sMain[i]->Add(m_sHorizController[i], 0, wxEXPAND | (wxLEFT | wxRIGHT | wxDOWN), 5);
		m_sMain[i]->Add(m_sHorizStatus[i], 0, wxEXPAND | (wxLEFT | wxRIGHT | wxDOWN), 5);
		m_sMain[i]->Add(m_sHorizAnalogMapping[i], 0, wxEXPAND | (wxLEFT | wxRIGHT | wxDOWN), 5);
		m_sMain[i]->Add(m_sHorizControllerMapping[i], 0, wxEXPAND | (wxLEFT | wxRIGHT | wxDOWN), 5);

		// Set the main sizer
		m_Controller[i]->SetSizer(m_sMain[i]);
	}

	m_Apply = new wxButton(this, ID_APPLY, wxT("Apply"));
	m_Close = new wxButton(this, ID_CLOSE, wxT("Close"));

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
		if (GetSystemMetrics(SM_CYFULLSCREEN) > 600)
			Center();
	#endif

	ControlsCreated = true;
}

void WiimotePadConfigDialog::GeneralSettingsChanged(wxCommandEvent& event)
{
	long TmpValue;

	switch (event.GetId())
	{
	case IDC_JOYNAME:
		DoChangeJoystick();
		break;
	case IDC_TILT_INPUT:
		g_Config.Tilt.Type = m_TiltComboInput[Page]->GetSelection();
		break;
	case IDC_TILT_RANGE_ROLL:
		if (m_TiltComboRangeRoll[Page]->GetSelection() == 0)
			g_Config.Tilt.Range.Roll = 0;
		else
		{
			m_TiltComboRangeRoll[Page]->GetValue().ToLong(&TmpValue);
			g_Config.Tilt.Range.Roll = TmpValue;
		}
		break;
	case IDC_TILT_RANGE_PITCH:
		if (m_TiltComboRangePitch[Page]->GetSelection() == 0)
			g_Config.Tilt.Range.Pitch = 0;
		else
		{
			m_TiltComboRangePitch[Page]->GetValue().ToLong(&TmpValue);
			g_Config.Tilt.Range.Pitch = TmpValue;
		}
		break;
	case IDC_NUNCHUCK_STICK:
		g_Config.Nunchuck.Type = m_NunchuckComboStick[Page]->GetSelection();
		break;
	case IDC_CC_LEFT_STICK:
		g_Config.ClassicController.LType = m_CcComboLeftStick[Page]->GetSelection();
		break;
	case IDC_CC_RIGHT_STICK:
		g_Config.ClassicController.RType = m_CcComboRightStick[Page]->GetSelection();
		break;
	case IDC_CC_TRIGGERS:
		g_Config.ClassicController.TType = m_CcComboTriggers[Page]->GetSelection();
		break;
	case IDC_GH3_ANALOG:
		g_Config.GH3Controller.AType = m_GH3ComboAnalog[Page]->GetSelection();
		break;
	// These are defined in PadMapping and we can run the same function to update all of them
	case IDC_DEAD_ZONE_LEFT:
	case IDC_DEAD_ZONE_RIGHT:
	case IDC_STICK_DIAGONAL:
	case IDC_STICK_C2S:
	case IDC_RUMBLE:
	case IDC_RUMBLE_STRENGTH:
	case IDC_TILT_INVERT_ROLL:
	case IDC_TILT_INVERT_PITCH:
	case IDC_TRIGGER_TYPE:
		SaveButtonMappingAll(Page);
		break;
	}
	g_Config.Save();
	UpdateGUI();
}

void WiimotePadConfigDialog::UpdateGUI(int Slot)
{
	UpdateGUIButtonMapping(Page);
	DoChangeDeadZone(true);
	DoChangeDeadZone(false);

	// Linux has no FindItem()
	// Disable all pad items if no pads are detected
	if(ControlsCreated)
	{
		bool PadEnabled = WiiMoteEmu::NumGoodPads != 0;
		#ifdef _WIN32
			m_Notebook->FindItem(IDC_JOYNAME)->Enable(PadEnabled);
			m_Notebook->FindItem(IDC_DEAD_ZONE_LEFT)->Enable(PadEnabled);
			m_Notebook->FindItem(IDC_DEAD_ZONE_RIGHT)->Enable(PadEnabled);
			m_Notebook->FindItem(IDC_STICK_C2S)->Enable(PadEnabled);
			m_Notebook->FindItem(IDC_STICK_DIAGONAL)->Enable(PadEnabled);
			m_Notebook->FindItem(IDC_RUMBLE)->Enable(PadEnabled);
			m_Notebook->FindItem(IDC_RUMBLE_STRENGTH)->Enable(PadEnabled);
			m_Notebook->FindItem(IDC_TRIGGER_TYPE)->Enable(PadEnabled);
			for(int i = IDB_ANALOG_LEFT_X; i <= IDB_TRIGGER_R; i++)
				m_Notebook->FindItem(i)->Enable(PadEnabled);
		#endif
	}		
}

