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
#include "WXInputBase.h"
#include "ConfigPadDlg.h"
#include "ConfigBasicDlg.h"
#include "Config.h"
#include "EmuDefinitions.h" // for joyinfo

BEGIN_EVENT_TABLE(WiimotePadConfigDialog, wxDialog)

	EVT_CLOSE(WiimotePadConfigDialog::OnClose)
	EVT_BUTTON(wxID_CLOSE, WiimotePadConfigDialog::CloseClick)
//	EVT_BUTTON(wxID_APPLY, WiimotePadConfigDialog::CloseClick)
	EVT_NOTEBOOK_PAGE_CHANGED(ID_NOTEBOOK, WiimotePadConfigDialog::NotebookPageChanged)

	EVT_TIMER(IDTM_BUTTON, WiimotePadConfigDialog::OnButtonTimer)
	EVT_TIMER(IDTM_UPDATE_PAD, WiimotePadConfigDialog::UpdatePadInfo)

	EVT_CHOICE(IDC_JOYNAME, WiimotePadConfigDialog::GeneralSettingsChanged)
	EVT_CHECKBOX(IDC_RUMBLE, WiimotePadConfigDialog::GeneralSettingsChanged)
	EVT_CHOICE(IDC_RUMBLE_STRENGTH, WiimotePadConfigDialog::GeneralSettingsChanged)
	EVT_CHOICE(IDC_DEAD_ZONE_LEFT, WiimotePadConfigDialog::GeneralSettingsChanged)
	EVT_CHOICE(IDC_DEAD_ZONE_RIGHT, WiimotePadConfigDialog::GeneralSettingsChanged)
	EVT_CHOICE(IDC_STICK_DIAGONAL, WiimotePadConfigDialog::GeneralSettingsChanged)
	EVT_CHECKBOX(IDC_STICK_C2S, WiimotePadConfigDialog::GeneralSettingsChanged)
	EVT_CHOICE(IDC_TILT_TYPE_WM, WiimotePadConfigDialog::GeneralSettingsChanged)
	EVT_CHOICE(IDC_TILT_TYPE_NC, WiimotePadConfigDialog::GeneralSettingsChanged)
	EVT_CHOICE(IDC_TILT_ROLL, WiimotePadConfigDialog::GeneralSettingsChanged)
	EVT_CHECKBOX(IDC_TILT_ROLL_SWING, WiimotePadConfigDialog::GeneralSettingsChanged)
	EVT_CHOICE(IDC_TILT_PITCH, WiimotePadConfigDialog::GeneralSettingsChanged)
	EVT_CHECKBOX(IDC_TILT_PITCH_SWING, WiimotePadConfigDialog::GeneralSettingsChanged)
	EVT_CHECKBOX(IDC_TILT_ROLL_INVERT, WiimotePadConfigDialog::GeneralSettingsChanged)
	EVT_CHECKBOX(IDC_TILT_PITCH_INVERT, WiimotePadConfigDialog::GeneralSettingsChanged)
	EVT_CHOICE(IDC_TRIGGER_TYPE, WiimotePadConfigDialog::GeneralSettingsChanged)
	EVT_CHOICE(IDC_NUNCHUCK_STICK, WiimotePadConfigDialog::GeneralSettingsChanged)
	EVT_CHOICE(IDC_CC_LEFT_STICK, WiimotePadConfigDialog::GeneralSettingsChanged)
	EVT_CHOICE(IDC_CC_RIGHT_STICK, WiimotePadConfigDialog::GeneralSettingsChanged)
	EVT_CHOICE(IDC_CC_TRIGGERS, WiimotePadConfigDialog::GeneralSettingsChanged)
	EVT_CHOICE(IDC_GH3_ANALOG, WiimotePadConfigDialog::GeneralSettingsChanged)

	// Analog
	EVT_BUTTON(IDB_ANALOG_LEFT_X, WiimotePadConfigDialog::OnAxisClick)
	EVT_BUTTON(IDB_ANALOG_LEFT_Y, WiimotePadConfigDialog::OnAxisClick)
	EVT_BUTTON(IDB_ANALOG_RIGHT_X, WiimotePadConfigDialog::OnAxisClick)
	EVT_BUTTON(IDB_ANALOG_RIGHT_Y, WiimotePadConfigDialog::OnAxisClick)
	EVT_BUTTON(IDB_TRIGGER_L, WiimotePadConfigDialog::OnAxisClick)
	EVT_BUTTON(IDB_TRIGGER_R, WiimotePadConfigDialog::OnAxisClick)

	// Wiimote
	EVT_BUTTON(IDB_WM_A, WiimotePadConfigDialog::OnButtonClick)
	EVT_BUTTON(IDB_WM_B, WiimotePadConfigDialog::OnButtonClick)
	EVT_BUTTON(IDB_WM_1, WiimotePadConfigDialog::OnButtonClick)
	EVT_BUTTON(IDB_WM_2, WiimotePadConfigDialog::OnButtonClick)
	EVT_BUTTON(IDB_WM_P, WiimotePadConfigDialog::OnButtonClick)
	EVT_BUTTON(IDB_WM_M, WiimotePadConfigDialog::OnButtonClick)
	EVT_BUTTON(IDB_WM_H, WiimotePadConfigDialog::OnButtonClick)
	EVT_BUTTON(IDB_WM_L, WiimotePadConfigDialog::OnButtonClick)
	EVT_BUTTON(IDB_WM_R, WiimotePadConfigDialog::OnButtonClick)
	EVT_BUTTON(IDB_WM_U, WiimotePadConfigDialog::OnButtonClick)
	EVT_BUTTON(IDB_WM_D, WiimotePadConfigDialog::OnButtonClick)
	EVT_BUTTON(IDB_WM_ROLL_L, WiimotePadConfigDialog::OnButtonClick)
	EVT_BUTTON(IDB_WM_ROLL_R, WiimotePadConfigDialog::OnButtonClick)
	EVT_BUTTON(IDB_WM_PITCH_U, WiimotePadConfigDialog::OnButtonClick)
	EVT_BUTTON(IDB_WM_PITCH_D, WiimotePadConfigDialog::OnButtonClick)
	EVT_BUTTON(IDB_WM_SHAKE, WiimotePadConfigDialog::OnButtonClick)

	// Nunchuck
	EVT_BUTTON(IDB_NC_Z, WiimotePadConfigDialog::OnButtonClick)
	EVT_BUTTON(IDB_NC_C, WiimotePadConfigDialog::OnButtonClick)
	EVT_BUTTON(IDB_NC_L, WiimotePadConfigDialog::OnButtonClick)
	EVT_BUTTON(IDB_NC_R, WiimotePadConfigDialog::OnButtonClick)
	EVT_BUTTON(IDB_NC_U, WiimotePadConfigDialog::OnButtonClick)
	EVT_BUTTON(IDB_NC_D, WiimotePadConfigDialog::OnButtonClick)
	EVT_BUTTON(IDB_NC_ROLL_L, WiimotePadConfigDialog::OnButtonClick)
	EVT_BUTTON(IDB_NC_ROLL_R, WiimotePadConfigDialog::OnButtonClick)
	EVT_BUTTON(IDB_NC_PITCH_U, WiimotePadConfigDialog::OnButtonClick)
	EVT_BUTTON(IDB_NC_PITCH_D, WiimotePadConfigDialog::OnButtonClick)
EVT_BUTTON(IDB_NC_SHAKE, WiimotePadConfigDialog::OnButtonClick)

	// Classic Controller
	EVT_BUTTON(IDB_CC_A, WiimotePadConfigDialog::OnButtonClick)
	EVT_BUTTON(IDB_CC_B, WiimotePadConfigDialog::OnButtonClick)
	EVT_BUTTON(IDB_CC_X, WiimotePadConfigDialog::OnButtonClick)
	EVT_BUTTON(IDB_CC_Y, WiimotePadConfigDialog::OnButtonClick)
	EVT_BUTTON(IDB_CC_P, WiimotePadConfigDialog::OnButtonClick)
	EVT_BUTTON(IDB_CC_M, WiimotePadConfigDialog::OnButtonClick)
	EVT_BUTTON(IDB_CC_H, WiimotePadConfigDialog::OnButtonClick)
	EVT_BUTTON(IDB_CC_TL, WiimotePadConfigDialog::OnButtonClick)
	EVT_BUTTON(IDB_CC_ZL, WiimotePadConfigDialog::OnButtonClick)
	EVT_BUTTON(IDB_CC_ZR, WiimotePadConfigDialog::OnButtonClick)
	EVT_BUTTON(IDB_CC_TR, WiimotePadConfigDialog::OnButtonClick)
	EVT_BUTTON(IDB_CC_DL, WiimotePadConfigDialog::OnButtonClick)
	EVT_BUTTON(IDB_CC_DU, WiimotePadConfigDialog::OnButtonClick)
	EVT_BUTTON(IDB_CC_DR, WiimotePadConfigDialog::OnButtonClick)
	EVT_BUTTON(IDB_CC_DD, WiimotePadConfigDialog::OnButtonClick)
	EVT_BUTTON(IDB_CC_DL, WiimotePadConfigDialog::OnButtonClick)
	EVT_BUTTON(IDB_CC_DU, WiimotePadConfigDialog::OnButtonClick)
	EVT_BUTTON(IDB_CC_DR, WiimotePadConfigDialog::OnButtonClick)
	EVT_BUTTON(IDB_CC_DD, WiimotePadConfigDialog::OnButtonClick)
	EVT_BUTTON(IDB_CC_LL, WiimotePadConfigDialog::OnButtonClick)
	EVT_BUTTON(IDB_CC_LU, WiimotePadConfigDialog::OnButtonClick)
	EVT_BUTTON(IDB_CC_LR, WiimotePadConfigDialog::OnButtonClick)
	EVT_BUTTON(IDB_CC_LD, WiimotePadConfigDialog::OnButtonClick)
	EVT_BUTTON(IDB_CC_RL, WiimotePadConfigDialog::OnButtonClick)
	EVT_BUTTON(IDB_CC_RU, WiimotePadConfigDialog::OnButtonClick)
	EVT_BUTTON(IDB_CC_RR, WiimotePadConfigDialog::OnButtonClick)
	EVT_BUTTON(IDB_CC_RD, WiimotePadConfigDialog::OnButtonClick)

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

WiimotePadConfigDialog::WiimotePadConfigDialog(wxWindow *parent, wxWindowID id,
		const wxString &title, const wxPoint &position, const wxSize& size, long style)
: wxDialog(parent, id, title, position, size, style)
{
	m_ControlsCreated = false;;
	CreatePadGUIControls();

#if wxUSE_TIMER
	m_ButtonMappingTimer = new wxTimer(this, IDTM_BUTTON);
	m_UpdatePadTimer = new wxTimer(this, IDTM_UPDATE_PAD);

	// Reset values
	g_Pressed = 0;
	ClickedButton = NULL;
	GetButtonWaitingID = 0;
	GetButtonWaitingTimer = 0;

	if (WiiMoteEmu::NumGoodPads)
	{
		// Start the permanent timer
		const int TimesPerSecond = 10;
		m_UpdatePadTimer->Start(1000 / TimesPerSecond);
	}
#endif

	m_Page = g_Config.CurrentPage;
	m_Notebook->ChangeSelection(m_Page);
	// Set control values
	UpdateGUI();
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

// Notebook page changed
void WiimotePadConfigDialog::NotebookPageChanged(wxNotebookEvent& event)
{	
	// Update the global variable
	m_Page = event.GetSelection();

	// Update GUI
	if (m_ControlsCreated)
		UpdateGUI();
}

void WiimotePadConfigDialog::OnClose(wxCloseEvent& event)
{
	if (m_UpdatePadTimer)
		m_UpdatePadTimer->Stop();
	if (m_ButtonMappingTimer)
		m_ButtonMappingTimer->Stop();

	g_Config.CurrentPage = m_Page;

	EndModal(wxID_CLOSE);
}

void WiimotePadConfigDialog::CloseClick(wxCommandEvent& event)
{
	switch(event.GetId())
	{
	case wxID_CLOSE:
		Close();
		break;
	case wxID_APPLY:
		break;
	}
}

// Save keyboard key mapping
void WiimotePadConfigDialog::SaveButtonMapping(int Id, int Key)
{
	if (IDB_ANALOG_LEFT_X <= Id && Id <= IDB_TRIGGER_R)
	{
		WiiMoteEmu::WiiMapping[m_Page].AxisMapping.Code[Id - IDB_ANALOG_LEFT_X] = Key;
	}
	else if (IDB_WM_A <= Id && Id <= IDB_GH3_STRUM_DOWN)
	{
		WiiMoteEmu::WiiMapping[m_Page].Button[Id - IDB_WM_A] = Key;
	}
}

void WiimotePadConfigDialog::OnKeyDown(wxKeyEvent& event)
{
	//event.Skip();

	if(ClickedButton != NULL)
	{
		// Save the key
		g_Pressed = event.GetKeyCode();
		// Handle the keyboard key mapping

		// Allow the escape key to set a blank key
		if (g_Pressed == WXK_ESCAPE)
		{
			SaveButtonMapping(ClickedButton->GetId(), -1);
			SetButtonText(ClickedButton->GetId(), wxString());
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
					SetButtonText(ClickedButton->GetId(),
						wxString::FromAscii(InputCommon::VKToString(i).c_str()));
					// Save the setting
					SaveButtonMapping(ClickedButton->GetId(), i);
					break;
				}
			}
		#elif defined(HAVE_X11) && HAVE_X11
			char keyStr[128] = {0};
			int XKey = InputCommon::wxCharCodeWXToX(g_Pressed);
			InputCommon::XKeyToString(XKey, keyStr);
			SetButtonText(ClickedButton->GetId(),
                                      wxString::FromAscii(keyStr));
			SaveButtonMapping(ClickedButton->GetId(), XKey);
		#elif defined(USE_WX) && USE_WX
			SetButtonText(ClickedButton->GetId(),
					InputCommon::WXKeyToString(g_Pressed));
			SaveButtonMapping(ClickedButton->GetId(), g_Pressed);
		#endif
		}
		EndGetButtons();
	}
}

// Input button clicked
void WiimotePadConfigDialog::OnButtonClick(wxCommandEvent& event)
{
	event.Skip();

	// Don't allow space to start a new Press Key option, that will interfer with setting a key to space
	if (g_Pressed == WXK_SPACE) { g_Pressed = 0; return; }

	if (m_ButtonMappingTimer->IsRunning()) return;

	wxTheApp->Connect(wxID_ANY, wxEVT_KEY_DOWN, // Keyboard
		wxKeyEventHandler(WiimotePadConfigDialog::OnKeyDown),
		(wxObject*)0, this);

	// Create the button object
	ClickedButton = (wxButton *)event.GetEventObject();
	// Save old label so we can revert back
	OldLabel = ClickedButton->GetLabel();
	ClickedButton->SetWindowStyle(wxWANTS_CHARS);
	ClickedButton->SetLabel(wxT("<Press Key>"));
	DoGetButtons(ClickedButton->GetId());
}

void WiimotePadConfigDialog::OnAxisClick(wxCommandEvent& event)
{
	event.Skip();

	if (m_ButtonMappingTimer->IsRunning()) return;

	ClickedButton = NULL;
	wxButton* pButton = (wxButton *)event.GetEventObject();
	OldLabel = pButton->GetLabel();
	pButton->SetWindowStyle(wxWANTS_CHARS);
	pButton->SetLabel(wxT("<Move Axis>"));
	DoGetButtons(pButton->GetId());
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
	int _Radius = Radius ? Radius : 1;

	wxBitmap bitmap(_Radius * 2, _Radius * 2);
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
		StrJoyname.Add(wxT("<No Gamepad Detected>"));
	}

	wxArrayString TextDeadZone;
	for (int i = 0; i <= 50; i++)
		TextDeadZone.Add(wxString::Format(wxT("%i%%"), i));

	wxArrayString StrDiagonal;
	for (int i = 0; i <= 10; i++)
		StrDiagonal.Add(wxString::Format(wxT("%i%%"), 100 - i * 5));

	wxArrayString StrRumble;
	for (int i = 0; i <= 10; i++)
		StrRumble.Add(wxString::Format(wxT("%i%%"), i*10));

	// The tilt list
	wxArrayString StrTilt;
	StrTilt.Add(wxT("Keyboard"));
	StrTilt.Add(wxT("Analog 1"));
	StrTilt.Add(wxT("Analog 2"));
	StrTilt.Add(wxT("Triggers"));

	// The range is in degrees and are set at even 5 degrees values
	wxArrayString StrTiltRangeRoll, StrTiltRangePitch;
	for (int i = 1; i <= 36; i++)
		StrTiltRangeRoll.Add(wxString::Format(wxT("%i"), i*5));
	for (int i = 1; i <= 36; i++)
		StrTiltRangePitch.Add(wxString::Format(wxT("%i"), i*5));

	// The Trigger type list
	wxArrayString StrTriggerType;
	StrTriggerType.Add(wxT("SDL")); // -0x8000 to 0x7fff
	StrTriggerType.Add(wxT("XInput")); // 0x00 to 0xff

	// The Nunchuck stick list
	wxArrayString StrNunchuck;
	StrNunchuck.Add(wxT("Keyboard"));
	StrNunchuck.Add(wxT("Analog 1"));
	StrNunchuck.Add(wxT("Analog 2"));

	// The Classic Controller triggers list
	wxArrayString StrCcTriggers;
	StrCcTriggers.Add(wxT("Keyboard"));
	StrCcTriggers.Add(wxT("Triggers"));

	// GH3 stick list
	wxArrayString StrAnalogArray;
	StrAnalogArray.Add(wxT("Analog 1"));
	StrAnalogArray.Add(wxT("Analog 2"));

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
		wxT("Roll Left"),
		wxT("Roll Right"),
		wxT("Pitch Up"),
		wxT("Pitch Down"),
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
	static const int BtW = 70, BtH = 20;
	// A small type font
	wxFont m_SmallFont(7, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL);

	m_Notebook = new wxNotebook(this, ID_NOTEBOOK, wxDefaultPosition, wxDefaultSize);

	for (int i = 0; i < MAX_WIIMOTES; i++)
	{
		wxPanel *m_Controller =
		   	new wxPanel(m_Notebook, ID_CONTROLLERPAGE1 + i, wxDefaultPosition, wxDefaultSize);
		m_Notebook->AddPage(m_Controller, wxString::Format(wxT("Wiimote %d"), i+1));

		// Controller
		m_Joyname[i] = new wxChoice(m_Controller, IDC_JOYNAME, wxDefaultPosition,
			   	wxSize(200, -1), StrJoyname, 0, wxDefaultValidator, StrJoyname[0]);
		m_Joyname[i]->SetToolTip(wxT("Save your settings and configure another joypad"));

		// Dead zone
		wxStaticText *m_ComboDeadZoneLabel = new wxStaticText(m_Controller, wxID_ANY, wxT("Dead Zone"));
		m_ComboDeadZoneLeft[i] = new wxChoice(m_Controller, IDC_DEAD_ZONE_LEFT,
				wxDefaultPosition, wxSize(50, -1), TextDeadZone, 0, wxDefaultValidator, TextDeadZone[0]);
		m_ComboDeadZoneRight[i] = new wxChoice(m_Controller, IDC_DEAD_ZONE_RIGHT,
				wxDefaultPosition, wxSize(50, -1), TextDeadZone, 0, wxDefaultValidator, TextDeadZone[0]);

		// Circle to square
		m_CheckC2S[i] = new wxCheckBox(m_Controller, IDC_STICK_C2S, wxT("Circle To Square"));
		m_CheckC2S[i]->SetToolTip(wxT("This will convert a circular stick radius to a square stick radius.\n")
			wxT("This can be useful for the pitch and roll emulation."));

		// The drop down menu for the circle to square adjustment
		wxStaticText *m_DiagonalLabel = new wxStaticText(m_Controller, wxID_ANY, wxT("Diagonal"));
		m_DiagonalLabel->SetToolTip(wxT("To produce a perfect square circle in the ")
				wxT("'Out' window you have to manually set\n")
				wxT("your diagonal values here from what is shown in the 'In' window."));
		m_ComboDiagonal[i] = new wxChoice(m_Controller, IDC_STICK_DIAGONAL,
				wxDefaultPosition, wxSize(50, -1), StrDiagonal, 0, wxDefaultValidator, StrDiagonal[0]);
		
		// Rumble
		m_CheckRumble[i] = new wxCheckBox(m_Controller, IDC_RUMBLE, wxT("Rumble"));
		wxStaticText *m_RumbleStrengthLabel = new wxStaticText(m_Controller, wxID_ANY, wxT("Strength"));
		m_RumbleStrength[i] = new wxChoice(m_Controller, IDC_RUMBLE_STRENGTH,
				wxDefaultPosition, wxSize(50, -1), StrRumble, 0, wxDefaultValidator, StrRumble[0]);

		// Sizers
		wxBoxSizer *m_sDeadZoneHoriz = new wxBoxSizer(wxHORIZONTAL);
		m_sDeadZoneHoriz->Add(m_ComboDeadZoneLeft[i], 0, (wxUP), 0);
		m_sDeadZoneHoriz->Add(m_ComboDeadZoneRight[i], 0, (wxUP), 0);

		wxBoxSizer *m_sDeadZone = new wxBoxSizer(wxVERTICAL);
		m_sDeadZone->Add(m_ComboDeadZoneLabel, 0, wxALIGN_CENTER | (wxUP), 0);	
		m_sDeadZone->Add(m_sDeadZoneHoriz, 0, wxALIGN_CENTER | (wxUP), 2);

		wxBoxSizer *m_sDiagonal = new wxBoxSizer(wxHORIZONTAL);
		m_sDiagonal->Add(m_DiagonalLabel, 0, (wxUP), 4);
		m_sDiagonal->Add(m_ComboDiagonal[i], 0, (wxLEFT), 2);

		wxBoxSizer *m_sCircle2Square = new wxBoxSizer(wxVERTICAL);
		m_sCircle2Square->Add(m_CheckC2S[i], 0, wxALIGN_CENTER | (wxUP), 0);
		m_sCircle2Square->Add(m_sDiagonal, 0, wxALIGN_CENTER | (wxUP), 2);

		wxBoxSizer *m_sC2SDeadZone = new wxBoxSizer(wxHORIZONTAL);
		m_sC2SDeadZone->Add(m_sDeadZone, 0, (wxUP), 0);
		m_sC2SDeadZone->Add(m_sCircle2Square, 0, (wxLEFT), 8);

		wxBoxSizer *m_sJoyname = new wxBoxSizer(wxVERTICAL);
		m_sJoyname->Add(m_Joyname[i], 0, wxALIGN_CENTER | (wxUP | wxDOWN), 4);
		m_sJoyname->Add(m_sC2SDeadZone, 0, (wxUP | wxDOWN), 4);

		wxBoxSizer *m_sRumble = new wxBoxSizer(wxVERTICAL);
		m_sRumble->Add(m_CheckRumble[i], 0, wxALIGN_CENTER | (wxUP | wxDOWN), 8);
		m_sRumble->Add(m_RumbleStrengthLabel, 0, wxALIGN_CENTER | (wxUP), 4);
		m_sRumble->Add(m_RumbleStrength[i], 0, (wxUP | wxDOWN), 2);

		wxStaticBoxSizer *m_gJoyPad =
			new wxStaticBoxSizer (wxHORIZONTAL, m_Controller, wxT("Gamepad"));
		m_gJoyPad->AddStretchSpacer();
		m_gJoyPad->Add(m_sJoyname, 0, wxEXPAND | (wxLEFT | wxRIGHT | wxDOWN), 5);
		m_gJoyPad->Add(m_sRumble, 0, wxEXPAND | (wxLEFT | wxRIGHT | wxDOWN), 5);
		m_gJoyPad->AddStretchSpacer();

		// Tilt Wiimote
		wxStaticText *m_tTiltTypeWM = new wxStaticText(m_Controller, wxID_ANY, wxT("Wiimote"));
		wxStaticText *m_tTiltTypeNC = new wxStaticText(m_Controller, wxID_ANY, wxT("Nunchuck"));

		m_TiltTypeWM[i] = new wxChoice(m_Controller, IDC_TILT_TYPE_WM,
				wxDefaultPosition, wxSize(70, -1), StrTilt, 0, wxDefaultValidator, StrTilt[0]);
		m_TiltTypeWM[i]->SetToolTip(wxT("Control Wiimote tilting by keyboard or stick or trigger"));

		m_TiltTypeNC[i] = new wxChoice(m_Controller, IDC_TILT_TYPE_NC,
				wxDefaultPosition, wxSize(70, -1), StrTilt, 0, wxDefaultValidator, StrTilt[0]);
		m_TiltTypeNC[i]->SetToolTip(wxT("Control Nunchuck tilting by keyboard or stick or trigger"));		

		wxStaticText *m_TiltTextRoll = new wxStaticText(m_Controller, wxID_ANY, wxT("Roll Left/Right"));
		wxStaticText *m_TiltTextPitch = new wxStaticText(m_Controller, wxID_ANY, wxT("Pitch Up/Down"));

		m_TiltComboRangeRoll[i] = new wxChoice(m_Controller, IDC_TILT_ROLL,
				wxDefaultPosition, wxSize(50, -1), StrTiltRangeRoll, 0,
				wxDefaultValidator, StrTiltRangeRoll[0]);
		m_TiltComboRangeRoll[i]->SetToolTip(wxT("The maximum Left/Righ Roll in degrees"));	
		m_TiltComboRangePitch[i] = new wxChoice(m_Controller, IDC_TILT_PITCH,
				wxDefaultPosition, wxSize(50, -1), StrTiltRangePitch, 0,
				wxDefaultValidator, StrTiltRangePitch[0]);
		m_TiltComboRangePitch[i]->SetToolTip(wxT("The maximum Up/Down Pitch in degrees"));
		m_TiltRollSwing[i] = new wxCheckBox(m_Controller, IDC_TILT_ROLL_SWING, wxT("Swing"));
		m_TiltRollSwing[i]->SetToolTip(wxT("Emulate Swing Left/Right instead of Roll Left/Right"));
		m_TiltPitchSwing[i] = new wxCheckBox(m_Controller, IDC_TILT_PITCH_SWING, wxT("Swing"));
		m_TiltPitchSwing[i]->SetToolTip(wxT("Emulate Swing Up/Down instead of Pitch Up/Down"));
		m_TiltRollInvert[i] = new wxCheckBox(m_Controller, IDC_TILT_ROLL_INVERT, wxT("Invert"));
		m_TiltRollInvert[i]->
			SetToolTip(wxT("Invert Left/Right direction (only effective for stick and trigger)"));
		m_TiltPitchInvert[i] = new wxCheckBox(m_Controller, IDC_TILT_PITCH_INVERT, wxT("Invert"));
		m_TiltPitchInvert[i]->
			SetToolTip(wxT("Invert Up/Down direction (only effective for stick and trigger)"));

		// Sizers
		wxBoxSizer *m_sTiltType = new wxBoxSizer(wxHORIZONTAL);
		m_sTiltType->Add(m_tTiltTypeWM, 0, wxEXPAND | (wxUP | wxDOWN | wxRIGHT), 4);
		m_sTiltType->Add(m_TiltTypeWM[i], 0, wxEXPAND | (wxDOWN | wxRIGHT), 4);
		m_sTiltType->Add(m_tTiltTypeNC, 0, wxEXPAND | (wxUP | wxDOWN | wxLEFT), 4);
		m_sTiltType->Add(m_TiltTypeNC[i], 0, wxEXPAND | (wxDOWN | wxLEFT), 4);

		wxGridBagSizer *m_sGridTilt = new wxGridBagSizer(0, 0);
		m_sGridTilt->Add(m_TiltTextRoll, wxGBPosition(0, 0), wxGBSpan(1, 1),
				(wxUP | wxRIGHT), 4);
		m_sGridTilt->Add(m_TiltComboRangeRoll[i], wxGBPosition(0, 1), wxGBSpan(1, 1),
				(wxLEFT | wxRIGHT), 4);
		m_sGridTilt->Add(m_TiltRollSwing[i], wxGBPosition(0, 2), wxGBSpan(1, 1),
				(wxUP | wxLEFT | wxRIGHT), 4);
		m_sGridTilt->Add(m_TiltRollInvert[i], wxGBPosition(0, 3), wxGBSpan(1, 1),
				(wxUP | wxLEFT), 4);
		m_sGridTilt->Add(m_TiltTextPitch, wxGBPosition(1, 0), wxGBSpan(1, 1),
				(wxUP | wxRIGHT), 4);
		m_sGridTilt->Add(m_TiltComboRangePitch[i], wxGBPosition(1, 1), wxGBSpan(1, 1),
				(wxLEFT | wxRIGHT), 4);
		m_sGridTilt->Add(m_TiltPitchSwing[i], wxGBPosition(1, 2), wxGBSpan(1, 1),
				(wxUP | wxLEFT | wxRIGHT), 4);
		m_sGridTilt->Add(m_TiltPitchInvert[i], wxGBPosition(1, 3), wxGBSpan(1, 1),
				(wxUP | wxLEFT), 4);

		wxStaticBoxSizer *m_gTilt = new wxStaticBoxSizer (wxVERTICAL, m_Controller, wxT("Tilt and Swing"));
		m_gTilt->AddStretchSpacer();
		m_gTilt->Add(m_sTiltType, 0, wxEXPAND | (wxLEFT | wxDOWN), 5);
		m_gTilt->Add(m_sGridTilt, 0, wxEXPAND | (wxLEFT), 5);
		m_gTilt->AddStretchSpacer();

		// Row 1 Sizers: Connected pads, tilt
		wxBoxSizer *m_sHorizController = new wxBoxSizer(wxHORIZONTAL);
		m_sHorizController->Add(m_gJoyPad, 0, wxEXPAND | (wxLEFT), 5);
		m_sHorizController->Add(m_gTilt, 0, wxEXPAND | (wxLEFT), 5);

		// Stick Status Panels
		m_tStatusLeftIn[i] = new wxStaticText(m_Controller, wxID_ANY, wxT("Not connected"));
		m_tStatusLeftOut[i] = new wxStaticText(m_Controller, wxID_ANY, wxT("Not connected"));
		m_tStatusRightIn[i] = new wxStaticText(m_Controller, wxID_ANY, wxT("Not connected"));
		m_tStatusRightOut[i] = new wxStaticText(m_Controller, wxID_ANY, wxT("Not connected"));

		wxPanel *m_pLeftInStatus = new wxPanel(m_Controller, wxID_ANY,
				wxDefaultPosition, wxDefaultSize);
		m_bmpSquareLeftIn[i] = new wxStaticBitmap(m_pLeftInStatus, wxID_ANY,
				CreateBitmap(), wxDefaultPosition, wxDefaultSize);
		m_bmpDeadZoneLeftIn[i] = new wxStaticBitmap(m_pLeftInStatus, wxID_ANY,
				CreateBitmapDeadZone(0), wxDefaultPosition, wxDefaultSize);
		m_bmpDotLeftIn[i] = new wxStaticBitmap(m_pLeftInStatus, wxID_ANY,
				CreateBitmapDot(), wxPoint(BoxW / 2, BoxH / 2), wxDefaultSize);

		wxPanel *m_pLeftOutStatus = new wxPanel(m_Controller, wxID_ANY, wxDefaultPosition, wxDefaultSize);
		m_bmpSquareLeftOut[i] = new wxStaticBitmap(m_pLeftOutStatus, wxID_ANY,
				CreateBitmap(), wxDefaultPosition, wxDefaultSize);
		m_bmpDotLeftOut[i] = new wxStaticBitmap(m_pLeftOutStatus, wxID_ANY,
				CreateBitmapDot(), wxPoint(BoxW / 2, BoxH / 2), wxDefaultSize);

		wxPanel *m_pRightInStatus = new wxPanel(m_Controller, wxID_ANY, wxDefaultPosition, wxDefaultSize);
		m_bmpSquareRightIn[i] = new wxStaticBitmap(m_pRightInStatus, wxID_ANY,
				CreateBitmap(), wxDefaultPosition, wxDefaultSize);
		m_bmpDeadZoneRightIn[i] = new wxStaticBitmap(m_pRightInStatus, wxID_ANY,
				CreateBitmapDeadZone(0), wxDefaultPosition, wxDefaultSize);
		m_bmpDotRightIn[i] = new wxStaticBitmap(m_pRightInStatus, wxID_ANY,
				CreateBitmapDot(), wxPoint(BoxW / 2, BoxH / 2), wxDefaultSize);

		wxPanel *m_pRightOutStatus = new wxPanel(m_Controller, wxID_ANY, wxDefaultPosition, wxDefaultSize);
		m_bmpSquareRightOut[i] = new wxStaticBitmap(m_pRightOutStatus, wxID_ANY,
				CreateBitmap(), wxDefaultPosition, wxDefaultSize);
		m_bmpDotRightOut[i] = new wxStaticBitmap(m_pRightOutStatus, wxID_ANY,
				CreateBitmapDot(), wxPoint(BoxW / 2, BoxH / 2), wxDefaultSize);

		// Sizers
		wxGridBagSizer *m_sGridStickLeft = new wxGridBagSizer(0, 0);
		m_sGridStickLeft->Add(m_pLeftInStatus, wxGBPosition(0, 0), wxGBSpan(1, 1), wxALL, 0);
		m_sGridStickLeft->Add(m_pLeftOutStatus, wxGBPosition(0, 1), wxGBSpan(1, 1), wxLEFT, 10);
		m_sGridStickLeft->Add(m_tStatusLeftIn[i], wxGBPosition(1, 0), wxGBSpan(1, 1), wxALL, 0);
		m_sGridStickLeft->Add(m_tStatusLeftOut[i], wxGBPosition(1, 1), wxGBSpan(1, 1), wxLEFT, 10);

		wxGridBagSizer *m_sGridStickRight = new wxGridBagSizer(0, 0);
		m_sGridStickRight->Add(m_pRightInStatus, wxGBPosition(0, 0), wxGBSpan(1, 1), wxALL, 0);
		m_sGridStickRight->Add(m_pRightOutStatus, wxGBPosition(0, 1), wxGBSpan(1, 1), wxLEFT, 10);
		m_sGridStickRight->Add(m_tStatusRightIn[i], wxGBPosition(1, 0), wxGBSpan(1, 1), wxALL, 0);
		m_sGridStickRight->Add(m_tStatusRightOut[i], wxGBPosition(1, 1), wxGBSpan(1, 1), wxLEFT, 10);

		wxStaticBoxSizer *m_gStickLeft = new wxStaticBoxSizer (wxHORIZONTAL, m_Controller,
				wxT("Analog 1 Status (In) (Out)"));
		m_gStickLeft->Add(m_sGridStickLeft, 0, (wxLEFT | wxRIGHT), 5);

		wxStaticBoxSizer *m_gStickRight = new wxStaticBoxSizer (wxHORIZONTAL, m_Controller,
				wxT("Analog 2 Status (In) (Out)"));
		m_gStickRight->Add(m_sGridStickRight, 0, (wxLEFT | wxRIGHT), 5);

		// Trigger Status Panels
		wxStaticText *m_TriggerL = new wxStaticText(m_Controller, wxID_ANY, wxT("Left:"));
		wxStaticText *m_TriggerR = new wxStaticText(m_Controller, wxID_ANY, wxT("Right:"));
		m_TriggerStatusL[i] = new wxStaticText(m_Controller, wxID_ANY, wxT("000"),
				wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT);
		m_TriggerStatusR[i] = new wxStaticText(m_Controller, wxID_ANY, wxT("000"),
				wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT);
		wxStaticText *m_tTriggerSource = new wxStaticText(m_Controller, wxID_ANY, wxT("Trigger Source"));
		m_TriggerType[i] = new wxChoice(m_Controller, IDC_TRIGGER_TYPE,
				wxDefaultPosition, wxSize(70, -1), StrTriggerType, 0,
				wxDefaultValidator, StrTriggerType[0]);

		// Sizers
		wxGridBagSizer *m_sGridTrigger = new wxGridBagSizer(0, 0);
		m_sGridTrigger->Add(m_TriggerL, wxGBPosition(0, 0), wxGBSpan(1, 1), (wxTOP), 4);
		m_sGridTrigger->Add(m_TriggerStatusL[i], wxGBPosition(0, 1), wxGBSpan(1, 1),
				(wxLEFT | wxTOP), 4);
		m_sGridTrigger->Add(m_TriggerR, wxGBPosition(1, 0), wxGBSpan(1, 1), (wxTOP), 4);
		m_sGridTrigger->Add(m_TriggerStatusR[i], wxGBPosition(1, 1), wxGBSpan(1, 1),
				(wxLEFT | wxTOP), 4);

		wxStaticBoxSizer *m_gTriggers =
			new wxStaticBoxSizer (wxVERTICAL, m_Controller, wxT("Triggers Status"));
		m_gTriggers->AddStretchSpacer();
		m_gTriggers->Add(m_sGridTrigger, 0, wxEXPAND | (wxLEFT | wxRIGHT), 5);
		m_gTriggers->Add(m_tTriggerSource, 0, wxEXPAND | (wxUP | wxLEFT | wxRIGHT), 5);
		m_gTriggers->Add(m_TriggerType[i], 0, wxEXPAND | (wxUP | wxLEFT | wxRIGHT), 5);
		m_gTriggers->AddStretchSpacer();

		// Row 2 Sizers: Analog status
		wxBoxSizer *m_sHorizStatus = new wxBoxSizer(wxHORIZONTAL);
		m_sHorizStatus->Add(m_gStickLeft, 0, wxEXPAND | (wxLEFT), 5);
		m_sHorizStatus->Add(m_gStickRight, 0, wxEXPAND | (wxLEFT), 5);
		m_sHorizStatus->Add(m_gTriggers, 0, wxEXPAND | (wxLEFT), 5);

		// Analog Axes and Triggers Mapping
		wxBoxSizer *m_sAnalogLeft = new wxBoxSizer(wxVERTICAL);
		wxBoxSizer *m_sAnalogMiddle = new wxBoxSizer(wxVERTICAL);		
		wxBoxSizer *m_sAnalogRight = new wxBoxSizer(wxVERTICAL);

		for (int x = 0; x < 6; x++)
		{
			wxStaticText *m_statictext_Analog =
				new wxStaticText(m_Controller, wxID_ANY, anText[x]);
			m_Button_Analog[x][i] = new wxButton(m_Controller, x + IDB_ANALOG_LEFT_X,
					wxEmptyString, wxDefaultPosition, wxSize(BtW, BtH));
			m_Button_Analog[x][i]->SetFont(m_SmallFont);
			wxBoxSizer *m_Sizer_Analog = new wxBoxSizer(wxHORIZONTAL);
			m_Sizer_Analog->Add(m_statictext_Analog, 0, (wxUP), 4);
			m_Sizer_Analog->Add(m_Button_Analog[x][i], 0, (wxLEFT), 2);
			if (x < 2) // Make some balance
				m_sAnalogLeft->Add(m_Sizer_Analog, 0,
						wxALIGN_RIGHT | (wxLEFT | wxRIGHT | wxDOWN), 1);
			else if (x < 4)
				m_sAnalogMiddle->Add(m_Sizer_Analog, 0,
						wxALIGN_RIGHT | (wxLEFT | wxRIGHT | wxDOWN), 1);
			else
				m_sAnalogRight->Add(m_Sizer_Analog, 0,
						wxALIGN_RIGHT | (wxLEFT | wxRIGHT | wxDOWN), 1);
		}

		wxStaticBoxSizer *m_gAnalog = new wxStaticBoxSizer (wxHORIZONTAL, m_Controller,
				wxT("Analog Axes and Triggers"));
		m_gAnalog->Add(m_sAnalogLeft, 0, wxALIGN_RIGHT | (wxALL), 0);
		m_gAnalog->Add(m_sAnalogMiddle, 0, wxALIGN_RIGHT | (wxLEFT), 5);
		m_gAnalog->Add(m_sAnalogRight, 0, wxALIGN_RIGHT | (wxLEFT), 5);
		m_gAnalog->AddSpacer(1);

		// Row 3 Sizes: Analog Mapping
		wxBoxSizer *m_sHorizAnalogMapping = new wxBoxSizer(wxHORIZONTAL);
		m_sHorizAnalogMapping->Add(m_gAnalog, 0, (wxLEFT), 5);

		// Wiimote Mapping
		wxBoxSizer *m_sWmVertLeft = new wxBoxSizer(wxVERTICAL);
		wxBoxSizer *m_sWmVertRight = new wxBoxSizer(wxVERTICAL);

		for (int x = 0; x <= IDB_WM_SHAKE - IDB_WM_A; x++)
		{
			wxStaticText *m_statictext_Wiimote = new wxStaticText(m_Controller, wxID_ANY, wmText[x]);
			m_Button_Wiimote[x][i] = new wxButton(m_Controller, x + IDB_WM_A,
					wxEmptyString, wxDefaultPosition, wxSize(BtW, BtH));
			m_Button_Wiimote[x][i]->SetFont(m_SmallFont);
			wxBoxSizer *m_Sizer_Wiimote = new wxBoxSizer(wxHORIZONTAL);
			m_Sizer_Wiimote->Add(m_statictext_Wiimote, 0, (wxUP), 4);
			m_Sizer_Wiimote->Add(m_Button_Wiimote[x][i], 0, (wxLEFT), 2);
			if (x < 7 || x == IDB_WM_SHAKE - IDB_WM_A) // Make some balance
				m_sWmVertLeft->Add(m_Sizer_Wiimote, 0,
						wxALIGN_RIGHT | (wxLEFT | wxRIGHT | wxDOWN), 1);
			else
				m_sWmVertRight->Add(m_Sizer_Wiimote, 0,
						wxALIGN_RIGHT | (wxLEFT | wxRIGHT | wxDOWN), 1);
		}

		wxStaticBoxSizer *m_gWiimote =
			new wxStaticBoxSizer (wxHORIZONTAL, m_Controller, wxT("Wiimote"));
		m_gWiimote->Add(m_sWmVertLeft, 0, (wxLEFT | wxRIGHT | wxDOWN), 1);
		m_gWiimote->Add(m_sWmVertRight, 0, (wxLEFT | wxRIGHT | wxDOWN), 1);

		// Row 4 Sizers: Wiimote and Extension Mapping
		wxBoxSizer *m_sHorizControllerMapping = new wxBoxSizer(wxHORIZONTAL);
		m_sHorizControllerMapping->Add(m_gWiimote, 0, (wxLEFT), 5);

		// Extension Mapping
		if(WiiMoteEmu::WiiMapping[i].iExtensionConnected == WiiMoteEmu::EXT_NUNCHUK)
		{
			// Stick controls
			wxStaticText *m_NunchuckTextStick =
				new wxStaticText(m_Controller, wxID_ANY, wxT("Stick"));
			m_NunchuckComboStick[i] = new wxChoice(m_Controller, IDC_NUNCHUCK_STICK,
					wxDefaultPosition, wxSize(70, -1), StrNunchuck, 0, wxDefaultValidator, StrNunchuck[0]);
			
			// Sizers
			wxBoxSizer *m_sNunchuckStick = new wxBoxSizer(wxHORIZONTAL);
			m_sNunchuckStick->Add(m_NunchuckTextStick, 0, (wxUP), 4);
			m_sNunchuckStick->Add(m_NunchuckComboStick[i], 0, (wxLEFT), 2);

			wxBoxSizer *m_sNCVertLeft = new wxBoxSizer(wxVERTICAL);
			wxBoxSizer *m_sNCVertRight = new wxBoxSizer(wxVERTICAL);
			m_sNCVertRight->Add(m_sNunchuckStick, 0, wxALIGN_RIGHT | (wxALL), 2);
			m_sNCVertRight->AddSpacer(2);

			for (int x = 0; x <= IDB_NC_SHAKE - IDB_NC_Z; x++)
			{
				wxStaticText *m_statictext_Nunchuk =
					new wxStaticText(m_Controller, wxID_ANY, ncText[x]);
				m_Button_NunChuck[x][i] = new wxButton(m_Controller, x + IDB_NC_Z,
						wxEmptyString, wxDefaultPosition, wxSize(BtW, BtH));
				m_Button_NunChuck[x][i]->SetFont(m_SmallFont);
				wxBoxSizer *m_Sizer_NunChuck = new wxBoxSizer(wxHORIZONTAL);
				m_Sizer_NunChuck->Add(m_statictext_Nunchuk, 0, (wxUP), 4);
				m_Sizer_NunChuck->Add(m_Button_NunChuck[x][i], 0, (wxLEFT), 2);

				if (x < IDB_NC_ROLL_L - IDB_NC_Z)	// Make some balance
					m_sNCVertLeft->Add(m_Sizer_NunChuck, 0,
							wxALIGN_RIGHT | (wxLEFT | wxRIGHT | wxDOWN), 1);
				else
					m_sNCVertRight->Add(m_Sizer_NunChuck, 0,
							wxALIGN_RIGHT | (wxLEFT | wxRIGHT | wxDOWN), 1);
			}

			wxStaticBoxSizer *m_gNunchuck =
				new wxStaticBoxSizer (wxHORIZONTAL, m_Controller, wxT("Nunchuck"));
			m_gNunchuck->Add(m_sNCVertLeft, 0, wxALIGN_RIGHT | (wxLEFT | wxRIGHT | wxDOWN), 1);
			m_gNunchuck->Add(m_sNCVertRight, 0, wxALIGN_RIGHT | (wxLEFT | wxRIGHT | wxDOWN), 1);

			m_sHorizControllerMapping->Add(m_gNunchuck, 0, (wxLEFT), 5);
		}
		else if(WiiMoteEmu::WiiMapping[i].iExtensionConnected == WiiMoteEmu::EXT_CLASSIC_CONTROLLER)
		{
			// Stick controls
			wxStaticText *m_CcTextLeftStick =
				new wxStaticText(m_Controller, wxID_ANY, wxT("Left stick"));
			m_CcComboLeftStick[i] = new wxChoice(m_Controller, IDC_CC_LEFT_STICK,
					wxDefaultPosition, wxSize(70, -1), StrNunchuck, 0, wxDefaultValidator, StrNunchuck[0]);
			wxStaticText *m_CcTextRightStick =
				new wxStaticText(m_Controller, wxID_ANY, wxT("Right stick"));
			m_CcComboRightStick[i] = new wxChoice(m_Controller, IDC_CC_RIGHT_STICK,
					wxDefaultPosition, wxSize(70, -1), StrNunchuck, 0, wxDefaultValidator, StrNunchuck[0]);
			wxStaticText *m_CcTextTriggers =
				new wxStaticText(m_Controller, wxID_ANY, wxT("Triggers"));
			m_CcComboTriggers[i] = new wxChoice(m_Controller, IDC_CC_TRIGGERS,
					wxDefaultPosition, wxSize(70, -1), StrCcTriggers, 0, wxDefaultValidator, StrCcTriggers[0]);

			for (int x = 0; x <= IDB_CC_RD - IDB_CC_A; x++)
			{
				wxStaticText *m_statictext_Classic =
					new wxStaticText(m_Controller, wxID_ANY, classicText[x]);
				m_Button_Classic[x][i] = new wxButton(m_Controller, x + IDB_CC_A,
						wxEmptyString, wxDefaultPosition, wxSize(BtW, BtH));
				m_Button_Classic[x][i]->SetFont(m_SmallFont);
				m_Sizer_Classic[x][i] = new wxBoxSizer(wxHORIZONTAL);
				m_Sizer_Classic[x][i]->Add(m_statictext_Classic, 0, wxALIGN_RIGHT | (wxUP), 4);
				m_Sizer_Classic[x][i]->Add(m_Button_Classic[x][i], 0, wxALIGN_RIGHT | (wxLEFT), 2);
			}

			// Sizers
			wxBoxSizer *m_sCcLeftStick = new wxBoxSizer(wxHORIZONTAL);
			m_sCcLeftStick->Add(m_CcTextLeftStick, 0, (wxUP), 4);
			m_sCcLeftStick->Add(m_CcComboLeftStick[i], 0, (wxLEFT), 2);
		
			wxBoxSizer *m_sCcRightStick = new wxBoxSizer(wxHORIZONTAL);
			m_sCcRightStick->Add(m_CcTextRightStick, 0, (wxUP), 4);
			m_sCcRightStick->Add(m_CcComboRightStick[i], 0, (wxLEFT), 2);

			wxBoxSizer *m_sCcTriggers = new wxBoxSizer(wxHORIZONTAL);
			m_sCcTriggers->Add(m_CcTextTriggers, 0, (wxUP), 4);
			m_sCcTriggers->Add(m_CcComboTriggers[i], 0, (wxLEFT), 2);

			wxBoxSizer *m_sCcVertLeft = new wxBoxSizer(wxVERTICAL);
			m_sCcVertLeft->Add(m_sCcLeftStick, 0, wxALIGN_RIGHT | (wxALL), 2);
			m_sCcVertLeft->AddSpacer(2);
			for (int x = IDB_CC_LL; x <= IDB_CC_LD; x++)
				m_sCcVertLeft->Add(m_Sizer_Classic[x - IDB_CC_A][i], 0,
						wxALIGN_RIGHT | (wxLEFT | wxRIGHT | wxDOWN), 1);
			for (int x = IDB_CC_DL; x <= IDB_CC_DD; x++)
				m_sCcVertLeft->Add(m_Sizer_Classic[x - IDB_CC_A][i], 0,
						wxALIGN_RIGHT | (wxLEFT | wxRIGHT | wxDOWN), 1);

			wxBoxSizer *m_sCcVertMiddle = new wxBoxSizer(wxVERTICAL);
			m_sCcVertMiddle->Add(m_sCcRightStick, 0, wxALIGN_RIGHT | (wxALL), 2);
			m_sCcVertMiddle->AddSpacer(2);
			for (int x = IDB_CC_RL; x <= IDB_CC_RD; x++)
				m_sCcVertMiddle->Add(m_Sizer_Classic[x - IDB_CC_A][i], 0,
						wxALIGN_RIGHT | (wxLEFT | wxRIGHT | wxDOWN), 1);
			for (int x = IDB_CC_A; x <= IDB_CC_Y; x++)
				m_sCcVertMiddle->Add(m_Sizer_Classic[x - IDB_CC_A][i], 0,
						wxALIGN_RIGHT | (wxLEFT | wxRIGHT | wxDOWN), 1);

			wxBoxSizer *m_sCcVertRight = new wxBoxSizer(wxVERTICAL);
			m_sCcVertRight->Add(m_sCcTriggers, 0, wxALIGN_RIGHT | (wxALL), 2);
			m_sCcVertRight->AddSpacer(2);
			for (int x = IDB_CC_TL; x <= IDB_CC_ZR; x++)
				m_sCcVertRight->Add(m_Sizer_Classic[x - IDB_CC_A][i], 0,
						wxALIGN_RIGHT | (wxLEFT | wxRIGHT | wxDOWN), 1);
			for (int x = IDB_CC_P; x <= IDB_CC_H; x++)
				m_sCcVertRight->Add(m_Sizer_Classic[x - IDB_CC_A][i], 0,
						wxALIGN_RIGHT | (wxLEFT | wxRIGHT | wxDOWN), 1);

			wxStaticBoxSizer *m_gClassicController = new wxStaticBoxSizer (wxHORIZONTAL,
					m_Controller, wxT("Classic Controller"));
			m_gClassicController->Add(m_sCcVertLeft, 0,
					wxALIGN_RIGHT | (wxLEFT | wxRIGHT | wxDOWN), 1);
			m_gClassicController->Add(m_sCcVertMiddle, 0,
					wxALIGN_RIGHT | (wxLEFT | wxRIGHT | wxDOWN), 1);
			m_gClassicController->Add(m_sCcVertRight, 0,
					wxALIGN_RIGHT | (wxLEFT | wxRIGHT | wxDOWN), 1);

			m_sHorizControllerMapping->Add(m_gClassicController, 0, (wxLEFT), 5);
		}
		else if(WiiMoteEmu::WiiMapping[i].iExtensionConnected == WiiMoteEmu::EXT_GUITARHERO)
		{			
			// Stick controls
			wxStaticText *m_tGH3Analog =
				new wxStaticText(m_Controller, wxID_ANY, wxT("Stick"));
			m_GH3ComboAnalog[i] = new wxChoice(m_Controller, IDC_GH3_ANALOG,
					wxDefaultPosition, wxSize(70, -1), StrAnalogArray, 0,
					wxDefaultValidator, StrAnalogArray[0]);

			// Sizers
			wxBoxSizer *m_sGH3Analog = new wxBoxSizer(wxHORIZONTAL);
			m_sGH3Analog->Add(m_tGH3Analog, 0, (wxUP), 4);
			m_sGH3Analog->Add(m_GH3ComboAnalog[i], 0, (wxLEFT), 2);

			wxBoxSizer *m_sGH3VertLeft = new wxBoxSizer(wxVERTICAL);
			m_sGH3VertLeft->Add(m_sGH3Analog, 0, wxALIGN_RIGHT | (wxALL), 2);
			m_sGH3VertLeft->AddSpacer(2);
			wxBoxSizer *m_sGH3VertRight = new wxBoxSizer(wxVERTICAL);

			for (int x = 0; x <= IDB_GH3_STRUM_DOWN - IDB_GH3_GREEN; x++)
			{
				wxStaticText *m_statictext_GH3 =
					new wxStaticText(m_Controller, wxID_ANY, gh3Text[x]);
				m_Button_GH3[x][i] = new wxButton(m_Controller, x,
						wxEmptyString, wxDefaultPosition, wxSize(BtW, BtH));
				m_Button_GH3[x][i]->SetFont(m_SmallFont);
				wxBoxSizer *m_Sizer_GH3 = new wxBoxSizer(wxHORIZONTAL);
				m_Sizer_GH3->Add(m_statictext_GH3, 0, (wxUP), 4);
				m_Sizer_GH3->Add(m_Button_GH3[x][i], 0, (wxLEFT), 2);

				if (x < IDB_GH3_ANALOG_LEFT - IDB_GH3_GREEN)
					m_sGH3VertRight->Add(m_Sizer_GH3, 0,
							wxALIGN_RIGHT | (wxLEFT | wxRIGHT | wxDOWN), 1);
				else
					m_sGH3VertLeft->Add(m_Sizer_GH3, 0,
							wxALIGN_RIGHT | (wxLEFT | wxRIGHT | wxDOWN), 1);
			}

			wxStaticBoxSizer *m_gGuitarHero3Controller = new wxStaticBoxSizer (wxHORIZONTAL,
					m_Controller, wxT("Guitar Hero 3 Controller"));
			m_gGuitarHero3Controller->Add(m_sGH3VertLeft, 0,
					wxALIGN_RIGHT | (wxLEFT | wxRIGHT | wxDOWN), 1);
			m_gGuitarHero3Controller->Add(m_sGH3VertRight, 0,
					wxALIGN_RIGHT | (wxLEFT | wxRIGHT | wxDOWN), 1);

			m_sHorizControllerMapping->Add(m_gGuitarHero3Controller, 0, (wxLEFT), 5);
		}

		// Set up sizers and layout
		// The sizer m_sMain will be expanded inside m_Controller
		wxBoxSizer *m_sMain = new wxBoxSizer(wxVERTICAL);
		m_sMain->Add(m_sHorizController, 0, wxEXPAND | (wxLEFT | wxRIGHT | wxDOWN), 5);
		m_sMain->Add(m_sHorizStatus, 0, wxEXPAND | (wxLEFT | wxRIGHT | wxDOWN), 5);
		m_sMain->Add(m_sHorizAnalogMapping, 0, wxEXPAND | (wxLEFT | wxRIGHT | wxDOWN), 5);
		m_sMain->Add(m_sHorizControllerMapping, 0, wxEXPAND | (wxLEFT | wxRIGHT | wxDOWN), 5);

		// Set the main sizer
		m_Controller->SetSizer(m_sMain);
	}

//	wxButton *m_Apply = new wxButton(this, wxID_APPLY, wxT("Apply"));
	wxButton *m_Close = new wxButton(this, wxID_CLOSE, wxT("Close"));

	wxBoxSizer* sButtons = new wxBoxSizer(wxHORIZONTAL);
	sButtons->AddStretchSpacer();
//	sButtons->Add(m_Apply, 0, (wxALL), 0);
	sButtons->Add(m_Close, 0, (wxLEFT), 5);

	wxBoxSizer *m_MainSizer = new wxBoxSizer(wxVERTICAL);
	m_MainSizer->Add(m_Notebook, 1, wxEXPAND | wxALL, 5);
	m_MainSizer->Add(sButtons, 0, wxEXPAND | (wxLEFT | wxRIGHT | wxDOWN), 5);

	SetSizer(m_MainSizer);
	Layout();
	Fit();
	// Center the window if there is room for it
	#ifdef _WIN32
		if (GetSystemMetrics(SM_CYFULLSCREEN) > 600)
			Center();
	#endif

	m_ControlsCreated = true;
}

void WiimotePadConfigDialog::GeneralSettingsChanged(wxCommandEvent& event)
{
	long TmpValue;
	int id = event.GetId();

	switch (id)
	{
	case IDC_JOYNAME:
		WiiMoteEmu::WiiMapping[m_Page].ID = m_Joyname[m_Page]->GetSelection();
		WiiMoteEmu::WiiMapping[m_Page].joy = WiiMoteEmu::joyinfo.at(WiiMoteEmu::WiiMapping[m_Page].ID).joy;
		break;
	case IDC_DEAD_ZONE_LEFT:
		WiiMoteEmu::WiiMapping[m_Page].DeadZoneL = m_ComboDeadZoneLeft[m_Page]->GetSelection();
		break;
	case IDC_DEAD_ZONE_RIGHT:
		WiiMoteEmu::WiiMapping[m_Page].DeadZoneR = m_ComboDeadZoneRight[m_Page]->GetSelection();
		break;
	case IDC_STICK_DIAGONAL:
		WiiMoteEmu::WiiMapping[m_Page].Diagonal = 100 - m_ComboDiagonal[m_Page]->GetSelection() * 5;
		break;
	case IDC_STICK_C2S:
		WiiMoteEmu::WiiMapping[m_Page].bCircle2Square = m_CheckC2S[m_Page]->IsChecked();
		break;
	case IDC_RUMBLE:
		WiiMoteEmu::WiiMapping[m_Page].Rumble = m_CheckRumble[m_Page]->IsChecked();
		break;
	case IDC_RUMBLE_STRENGTH:
		WiiMoteEmu::WiiMapping[m_Page].RumbleStrength = m_RumbleStrength[m_Page]->GetSelection() * 10;
		break;
	case IDC_TILT_TYPE_WM:
		WiiMoteEmu::WiiMapping[m_Page].Tilt.InputWM = m_TiltTypeWM[m_Page]->GetSelection();
		break;
	case IDC_TILT_TYPE_NC:
		WiiMoteEmu::WiiMapping[m_Page].Tilt.InputNC = m_TiltTypeNC[m_Page]->GetSelection();
		break;
	case IDC_TILT_ROLL:
	case IDC_TILT_ROLL_SWING:
		m_TiltComboRangeRoll[m_Page]->GetStringSelection().ToLong(&TmpValue);
		WiiMoteEmu::WiiMapping[m_Page].Tilt.RollDegree = TmpValue;
		WiiMoteEmu::WiiMapping[m_Page].Tilt.RollSwing = m_TiltRollSwing[m_Page]->IsChecked();
		WiiMoteEmu::WiiMapping[m_Page].Tilt.RollRange = (m_TiltRollSwing[m_Page]->IsChecked()) ? 0 : TmpValue;
		break;
	case IDC_TILT_PITCH:
	case IDC_TILT_PITCH_SWING:
		m_TiltComboRangePitch[m_Page]->GetStringSelection().ToLong(&TmpValue);
		WiiMoteEmu::WiiMapping[m_Page].Tilt.PitchDegree = TmpValue;
		WiiMoteEmu::WiiMapping[m_Page].Tilt.PitchSwing = m_TiltPitchSwing[m_Page]->IsChecked();
		WiiMoteEmu::WiiMapping[m_Page].Tilt.PitchRange = (m_TiltPitchSwing[m_Page]->IsChecked()) ? 0 : TmpValue;
		break;
	case IDC_TILT_ROLL_INVERT:
		WiiMoteEmu::WiiMapping[m_Page].Tilt.RollInvert = m_TiltRollInvert[m_Page]->IsChecked();
		break;
	case IDC_TILT_PITCH_INVERT:
		WiiMoteEmu::WiiMapping[m_Page].Tilt.PitchInvert = m_TiltPitchInvert[m_Page]->IsChecked();
		break;
	case IDC_TRIGGER_TYPE:
		WiiMoteEmu::WiiMapping[m_Page].TriggerType = m_TriggerType[m_Page]->GetSelection();
		break;
	case IDC_NUNCHUCK_STICK:
		WiiMoteEmu::WiiMapping[m_Page].Stick.NC = m_NunchuckComboStick[m_Page]->GetSelection();
		break;
	case IDC_CC_LEFT_STICK:
		WiiMoteEmu::WiiMapping[m_Page].Stick.CCL = m_CcComboLeftStick[m_Page]->GetSelection();
		break;
	case IDC_CC_RIGHT_STICK:
		WiiMoteEmu::WiiMapping[m_Page].Stick.CCR = m_CcComboRightStick[m_Page]->GetSelection();
		break;
	case IDC_CC_TRIGGERS:
		WiiMoteEmu::WiiMapping[m_Page].Stick.CCT = m_CcComboTriggers[m_Page]->GetSelection();
		break;
	case IDC_GH3_ANALOG:
		WiiMoteEmu::WiiMapping[m_Page].Stick.GH = m_GH3ComboAnalog[m_Page]->GetSelection();
		break;
	}
	UpdateGUI();
}

void WiimotePadConfigDialog::UpdateGUI()
{
	if(!m_ControlsCreated)
		return;

	// Disable all pad items if no pads are detected
	bool PadEnabled = WiiMoteEmu::NumGoodPads != 0;

	m_Joyname[m_Page]->Enable(PadEnabled);
	m_ComboDeadZoneLeft[m_Page]->Enable(PadEnabled);
	m_ComboDeadZoneRight[m_Page]->Enable(PadEnabled);
	m_CheckC2S[m_Page]->Enable(PadEnabled);
	m_ComboDiagonal[m_Page]->Enable(PadEnabled);
	m_CheckRumble[m_Page]->Enable(PadEnabled);
	m_RumbleStrength[m_Page]->Enable(PadEnabled);
	m_TriggerType[m_Page]->Enable(PadEnabled);
	for(int i = 0; i <= IDB_TRIGGER_R - IDB_ANALOG_LEFT_X; i++)
		m_Button_Analog[i][m_Page]->Enable(PadEnabled);

	wxString tmp;

	m_Joyname[m_Page]->SetSelection(WiiMoteEmu::WiiMapping[m_Page].ID);
	m_ComboDeadZoneLeft[m_Page]->SetSelection(WiiMoteEmu::WiiMapping[m_Page].DeadZoneL);
	m_ComboDeadZoneRight[m_Page]->SetSelection(WiiMoteEmu::WiiMapping[m_Page].DeadZoneR);
	m_ComboDiagonal[m_Page]->SetSelection((100 - WiiMoteEmu::WiiMapping[m_Page].Diagonal) / 5);
	m_CheckC2S[m_Page]->SetValue(WiiMoteEmu::WiiMapping[m_Page].bCircle2Square);
	m_CheckRumble[m_Page]->SetValue(WiiMoteEmu::WiiMapping[m_Page].Rumble);
	m_RumbleStrength[m_Page]->SetSelection(WiiMoteEmu::WiiMapping[m_Page].RumbleStrength / 10);
	m_TriggerType[m_Page]->SetSelection(WiiMoteEmu::WiiMapping[m_Page].TriggerType);
	m_TiltTypeWM[m_Page]->SetSelection(WiiMoteEmu::WiiMapping[m_Page].Tilt.InputWM);
	m_TiltTypeNC[m_Page]->SetSelection(WiiMoteEmu::WiiMapping[m_Page].Tilt.InputNC);
	m_TiltComboRangeRoll[m_Page]->
		SetSelection(WiiMoteEmu::WiiMapping[m_Page].Tilt.RollDegree / 5 - 1); // 5 to 180, step 5
	m_TiltComboRangeRoll[m_Page]->Enable(!WiiMoteEmu::WiiMapping[m_Page].Tilt.RollSwing);
	m_TiltComboRangePitch[m_Page]->
		SetSelection(WiiMoteEmu::WiiMapping[m_Page].Tilt.PitchDegree / 5 - 1); // 5 to 180, step 5
	m_TiltComboRangePitch[m_Page]->Enable(!WiiMoteEmu::WiiMapping[m_Page].Tilt.PitchSwing);
	m_TiltRollSwing[m_Page]->SetValue(WiiMoteEmu::WiiMapping[m_Page].Tilt.RollSwing);
	m_TiltPitchSwing[m_Page]->SetValue(WiiMoteEmu::WiiMapping[m_Page].Tilt.PitchSwing);
	m_TiltRollInvert[m_Page]->SetValue(WiiMoteEmu::WiiMapping[m_Page].Tilt.RollInvert);
	m_TiltPitchInvert[m_Page]->SetValue(WiiMoteEmu::WiiMapping[m_Page].Tilt.PitchInvert);

	for (int i = 0; i <= IDB_TRIGGER_R - IDB_ANALOG_LEFT_X; i++)
	{
		tmp << WiiMoteEmu::WiiMapping[m_Page].AxisMapping.Code[i];
		m_Button_Analog[i][m_Page]->SetLabel(tmp);
		tmp.clear();
	}

#ifdef _WIN32
	for (int x = 0; x <= IDB_WM_SHAKE - IDB_WM_A; x++)
	{
		m_Button_Wiimote[x][m_Page]->SetLabel(wxString::FromAscii(
		InputCommon::VKToString(WiiMoteEmu::WiiMapping[m_Page].Button[x + WiiMoteEmu::EWM_A]).c_str()));
	}
	if(WiiMoteEmu::WiiMapping[m_Page].iExtensionConnected == WiiMoteEmu::EXT_NUNCHUK)
	{
		m_NunchuckComboStick[m_Page]->SetSelection(WiiMoteEmu::WiiMapping[m_Page].Stick.NC);
		for (int x = 0; x <= IDB_NC_SHAKE - IDB_NC_Z; x++)
			m_Button_NunChuck[x][m_Page]->SetLabel(wxString::FromAscii(
			InputCommon::VKToString(WiiMoteEmu::WiiMapping[m_Page].Button[x + WiiMoteEmu::ENC_Z]).c_str()));
	}
	else if(WiiMoteEmu::WiiMapping[m_Page].iExtensionConnected == WiiMoteEmu::EXT_CLASSIC_CONTROLLER)
	{
		m_CcComboLeftStick[m_Page]->SetSelection(WiiMoteEmu::WiiMapping[m_Page].Stick.CCL);
		m_CcComboRightStick[m_Page]->SetSelection(WiiMoteEmu::WiiMapping[m_Page].Stick.CCR);
		m_CcComboTriggers[m_Page]->SetSelection(WiiMoteEmu::WiiMapping[m_Page].Stick.CCT);
		for (int x = 0; x <= IDB_CC_RD - IDB_CC_A; x++)
			m_Button_Classic[x][m_Page]->SetLabel(wxString::FromAscii(
			InputCommon::VKToString(WiiMoteEmu::WiiMapping[m_Page].Button[x + WiiMoteEmu::ECC_A]).c_str()));
	}
	else if(WiiMoteEmu::WiiMapping[m_Page].iExtensionConnected == WiiMoteEmu::EXT_GUITARHERO)
	{
		m_GH3ComboAnalog[m_Page]->SetSelection(WiiMoteEmu::WiiMapping[m_Page].Stick.GH);
		for (int x = 0; x <= IDB_GH3_STRUM_DOWN - IDB_GH3_GREEN; x++)
			m_Button_GH3[x][m_Page]->SetLabel(wxString::FromAscii(
			InputCommon::VKToString(WiiMoteEmu::WiiMapping[m_Page].Button[x + WiiMoteEmu::EGH_Green]).c_str()));
	}
#elif defined(HAVE_X11) && HAVE_X11
	char keyStr[10] = {0};
	for (int x = 0; x <= IDB_WM_SHAKE - IDB_WM_A; x++)
	{
		InputCommon::XKeyToString(WiiMoteEmu::WiiMapping[m_Page].Button[x + WiiMoteEmu::EWM_A], keyStr);
		m_Button_Wiimote[x][m_Page]->SetLabel(wxString::FromAscii(keyStr));
	}
	if(WiiMoteEmu::WiiMapping[m_Page].iExtensionConnected == WiiMoteEmu::EXT_NUNCHUK)
	{
		m_NunchuckComboStick[m_Page]->SetSelection(WiiMoteEmu::WiiMapping[m_Page].Stick.NC);
		for (int x = 0; x <= IDB_NC_SHAKE - IDB_NC_Z; x++)
		{
			InputCommon::XKeyToString(WiiMoteEmu::WiiMapping[m_Page].Button[x + WiiMoteEmu::ENC_Z], keyStr);
			m_Button_NunChuck[x][m_Page]->SetLabel(wxString::FromAscii(keyStr));
		}
	}
	else if(WiiMoteEmu::WiiMapping[m_Page].iExtensionConnected == WiiMoteEmu::EXT_CLASSIC_CONTROLLER)
	{
		m_CcComboLeftStick[m_Page]->SetSelection(WiiMoteEmu::WiiMapping[m_Page].Stick.CCL);
		m_CcComboRightStick[m_Page]->SetSelection(WiiMoteEmu::WiiMapping[m_Page].Stick.CCR);
		m_CcComboTriggers[m_Page]->SetSelection(WiiMoteEmu::WiiMapping[m_Page].Stick.CCT);
		for (int x = 0; x <= IDB_CC_RD - IDB_CC_A; x++)
		{
			InputCommon::XKeyToString(WiiMoteEmu::WiiMapping[m_Page].Button[x + WiiMoteEmu::ECC_A], keyStr);
			m_Button_Classic[x][m_Page]->SetLabel(wxString::FromAscii(keyStr));
		}
	}
	else if(WiiMoteEmu::WiiMapping[m_Page].iExtensionConnected == WiiMoteEmu::EXT_GUITARHERO)
	{
		m_GH3ComboAnalog[m_Page]->SetSelection(WiiMoteEmu::WiiMapping[m_Page].Stick.GH);
		for (int x = 0; x <= IDB_GH3_STRUM_DOWN - IDB_GH3_GREEN; x++)
		{
			InputCommon::XKeyToString(WiiMoteEmu::WiiMapping[m_Page].Button[x + WiiMoteEmu::EGH_Green], keyStr);
			m_Button_GH3[x][m_Page]->SetLabel(wxString::FromAscii(keyStr));
		}
	}
#elif defined(USE_WX) && USE_WX
	for (int x = 0; x <= IDB_WM_SHAKE - IDB_WM_A; x++)
	{
		m_Button_Wiimote[x][m_Page]-> \
			SetLabel(InputCommon::WXKeyToString(
				WiiMoteEmu::WiiMapping[m_Page]. \
					Button[x + WiiMoteEmu::EWM_A]));
	}
	if (WiiMoteEmu::WiiMapping[m_Page].iExtensionConnected ==
		WiiMoteEmu::EXT_NUNCHUK)
	{
		m_NunchuckComboStick[m_Page]-> \
			SetSelection(WiiMoteEmu::WiiMapping[m_Page].Stick.NC);

		for (int x = 0; x <= IDB_NC_SHAKE - IDB_NC_Z; x++)
		{
			m_Button_NunChuck[x][m_Page]-> \
				SetLabel(InputCommon::WXKeyToString(
					WiiMoteEmu::WiiMapping[m_Page]. \
						Button[x + WiiMoteEmu::ENC_Z]));
		}
	}
	else if (WiiMoteEmu::WiiMapping[m_Page].iExtensionConnected ==
		WiiMoteEmu::EXT_CLASSIC_CONTROLLER)
	{
		m_CcComboLeftStick[m_Page]-> \
			SetSelection(WiiMoteEmu::WiiMapping[m_Page].Stick.CCL);
		m_CcComboRightStick[m_Page]-> \
			SetSelection(WiiMoteEmu::WiiMapping[m_Page].Stick.CCR);
		m_CcComboTriggers[m_Page]-> \
			SetSelection(WiiMoteEmu::WiiMapping[m_Page].Stick.CCT);

		for (int x = 0; x <= IDB_CC_RD - IDB_CC_A; x++)
		{
			m_Button_Classic[x][m_Page]-> \
				SetLabel(InputCommon::WXKeyToString(
					WiiMoteEmu::WiiMapping[m_Page]. \
						Button[x + WiiMoteEmu::ECC_A]));
		}
	}

	else if(WiiMoteEmu::WiiMapping[m_Page].iExtensionConnected ==
		WiiMoteEmu::EXT_GUITARHERO)
	{
		m_GH3ComboAnalog[m_Page]-> \
			SetSelection(WiiMoteEmu::WiiMapping[m_Page].Stick.GH);

		for (int x = 0; x <= IDB_GH3_STRUM_DOWN - IDB_GH3_GREEN; x++)
		{
			m_Button_GH3[x][m_Page]-> \
				SetLabel(InputCommon::WXKeyToString(
					WiiMoteEmu::WiiMapping[m_Page]. \
						Button[x + WiiMoteEmu::EGH_Green]));
		}
	}
#endif

	DoChangeDeadZone();

}
