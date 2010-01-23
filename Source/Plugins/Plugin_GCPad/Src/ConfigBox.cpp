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


#include "math.h" // System
#include "ConfigBox.h"
#include "Config.h"
#include "GCPad.h"
#if defined(HAVE_X11) && HAVE_X11
	#include <X11/Xlib.h>
	#include <X11/Xutil.h>
	#include <X11/keysym.h>
	#include <X11/XKBlib.h>
	#include "X11InputBase.h"
#endif

// The wxWidgets class
BEGIN_EVENT_TABLE(GCPadConfigDialog,wxDialog)
	EVT_CLOSE(GCPadConfigDialog::OnClose)
	EVT_BUTTON(ID_OK, GCPadConfigDialog::OnCloseClick)
	EVT_BUTTON(ID_CANCEL, GCPadConfigDialog::OnCloseClick)
	EVT_NOTEBOOK_PAGE_CHANGED(ID_NOTEBOOK, GCPadConfigDialog::NotebookPageChanged)

	EVT_COMBOBOX(IDC_JOYNAME, GCPadConfigDialog::ChangeSettings)	
	EVT_CHECKBOX(IDC_RUMBLE, GCPadConfigDialog::ChangeSettings)
	EVT_COMBOBOX(IDC_RUMBLE_STRENGTH, GCPadConfigDialog::ChangeSettings)
	EVT_COMBOBOX(IDC_DEAD_ZONE_LEFT, GCPadConfigDialog::ChangeSettings)
	EVT_COMBOBOX(IDC_DEAD_ZONE_RIGHT, GCPadConfigDialog::ChangeSettings)
	EVT_COMBOBOX(IDC_STICK_DIAGONAL, GCPadConfigDialog::ChangeSettings)
	EVT_CHECKBOX(IDC_STICK_S2C, GCPadConfigDialog::ChangeSettings)
	EVT_COMBOBOX(IDC_TRIGGER_TYPE, GCPadConfigDialog::ChangeSettings)
	EVT_COMBOBOX(IDC_STICK_SOURCE, GCPadConfigDialog::ChangeSettings)
	EVT_COMBOBOX(IDC_CSTICK_SOURCE, GCPadConfigDialog::ChangeSettings)
	EVT_COMBOBOX(IDC_TRIGGER_SOURCE, GCPadConfigDialog::ChangeSettings)

	EVT_BUTTON(IDB_ANALOG_LEFT_X, GCPadConfigDialog::OnAxisClick)
	EVT_BUTTON(IDB_ANALOG_LEFT_Y, GCPadConfigDialog::OnAxisClick)
	EVT_BUTTON(IDB_ANALOG_RIGHT_X, GCPadConfigDialog::OnAxisClick)
	EVT_BUTTON(IDB_ANALOG_RIGHT_Y, GCPadConfigDialog::OnAxisClick)
	EVT_BUTTON(IDB_TRIGGER_L, GCPadConfigDialog::OnAxisClick)
	EVT_BUTTON(IDB_TRIGGER_R, GCPadConfigDialog::OnAxisClick)

	EVT_BUTTON(IDB_BTN_A, GCPadConfigDialog::OnButtonClick)
	EVT_BUTTON(IDB_BTN_B, GCPadConfigDialog::OnButtonClick)
	EVT_BUTTON(IDB_BTN_X, GCPadConfigDialog::OnButtonClick)
	EVT_BUTTON(IDB_BTN_Y, GCPadConfigDialog::OnButtonClick)
	EVT_BUTTON(IDB_BTN_Z, GCPadConfigDialog::OnButtonClick)
	EVT_BUTTON(IDB_BTN_START, GCPadConfigDialog::OnButtonClick)
	EVT_BUTTON(IDB_DPAD_UP, GCPadConfigDialog::OnButtonClick)
	EVT_BUTTON(IDB_DPAD_DOWN, GCPadConfigDialog::OnButtonClick)
	EVT_BUTTON(IDB_DPAD_LEFT, GCPadConfigDialog::OnButtonClick)
	EVT_BUTTON(IDB_DPAD_RIGHT, GCPadConfigDialog::OnButtonClick)
	EVT_BUTTON(IDB_MAIN_UP, GCPadConfigDialog::OnButtonClick)
	EVT_BUTTON(IDB_MAIN_DOWN, GCPadConfigDialog::OnButtonClick)
	EVT_BUTTON(IDB_MAIN_LEFT, GCPadConfigDialog::OnButtonClick)
	EVT_BUTTON(IDB_MAIN_RIGHT, GCPadConfigDialog::OnButtonClick)
	EVT_BUTTON(IDB_SUB_UP, GCPadConfigDialog::OnButtonClick)
	EVT_BUTTON(IDB_SUB_DOWN, GCPadConfigDialog::OnButtonClick)
	EVT_BUTTON(IDB_SUB_LEFT, GCPadConfigDialog::OnButtonClick)
	EVT_BUTTON(IDB_SUB_RIGHT, GCPadConfigDialog::OnButtonClick)
	EVT_BUTTON(IDB_SHDR_L, GCPadConfigDialog::OnButtonClick)
	EVT_BUTTON(IDB_SHDR_R, GCPadConfigDialog::OnButtonClick)
	EVT_BUTTON(IDB_SHDR_SEMI_L, GCPadConfigDialog::OnButtonClick)
	EVT_BUTTON(IDB_SHDR_SEMI_R, GCPadConfigDialog::OnButtonClick)

#if wxUSE_TIMER
	EVT_TIMER(IDTM_UPDATE_PAD, GCPadConfigDialog::UpdatePadInfo)
	EVT_TIMER(IDTM_BUTTON, GCPadConfigDialog::OnButtonTimer)
#endif
END_EVENT_TABLE()

GCPadConfigDialog::GCPadConfigDialog(wxWindow *parent, wxWindowID id, const wxString &title,
	const wxPoint &position, const wxSize& size, long style)
	: wxDialog(parent, id, title, position, size, style)
{
	// Define values
	m_ControlsCreated = false;
	m_Page = 0;

	// Create controls
	CreateGUIControls();

#if wxUSE_TIMER
	m_UpdatePadTimer = new wxTimer(this, IDTM_UPDATE_PAD);
	m_ButtonMappingTimer = new wxTimer(this, IDTM_BUTTON);

	// Reset values
	g_Pressed = 0;
	GetButtonWaitingID = 0;
	GetButtonWaitingTimer = 0;

	if (NumGoodPads)
	{
		// Start the constant timer
		int TimesPerSecond = 10;
		m_UpdatePadTimer->Start(1000 / TimesPerSecond);
	}
#endif

	UpdateGUI();

	wxTheApp->Connect(wxID_ANY, wxEVT_KEY_DOWN, // Keyboard
		wxKeyEventHandler(GCPadConfigDialog::OnKeyDown),
		(wxObject*)0, this);
}

GCPadConfigDialog::~GCPadConfigDialog()
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
void GCPadConfigDialog::NotebookPageChanged(wxNotebookEvent& event)
{	
	// Update the global variable
	m_Page = event.GetSelection();
	// Update GUI
	UpdateGUI();
}

// Close window
void GCPadConfigDialog::OnClose(wxCloseEvent& event)
{
	// Allow wxWidgets to close the window
	//event.Skip();

	// Stop the timer
	if (m_UpdatePadTimer)
		m_UpdatePadTimer->Stop();
	if (m_ButtonMappingTimer)
		m_ButtonMappingTimer->Stop();

	EndModal(wxID_CLOSE);
}

// Button Click
void GCPadConfigDialog::OnCloseClick(wxCommandEvent& event)
{
	switch (event.GetId())
	{
	case ID_OK:
		g_Config.Save();
		Close(); // Call OnClose()
		break;
	case ID_CANCEL:
		g_Config.Load();
		Close(); // Call OnClose()
		break;
	}
}

void GCPadConfigDialog::SaveButtonMapping(int Id, int Key)
{
	if (IDB_ANALOG_LEFT_X <= Id && Id <= IDB_TRIGGER_R)
	{
		GCMapping[m_Page].AxisMapping.Code[Id - IDB_ANALOG_LEFT_X] = Key;
	}
	else if (IDB_BTN_A <= Id && Id <= IDB_SHDR_SEMI_R)
	{
		GCMapping[m_Page].Button[Id - IDB_BTN_A] = Key;
	}
}

void GCPadConfigDialog::OnKeyDown(wxKeyEvent& event)
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
			int XKey = InputCommon::wxCharCodeWXToX(g_Pressed);
			InputCommon::XKeyToString(XKey, keyStr);
			SetButtonText(ClickedButton->GetId(),
                                      wxString::FromAscii(keyStr));
			SaveButtonMapping(ClickedButton->GetId(), XKey);
		#endif
		}
		m_ButtonMappingTimer->Stop();
		GetButtonWaitingTimer = 0;
		GetButtonWaitingID = 0;
		ClickedButton = NULL;
	}
}

// Configure button mapping
void GCPadConfigDialog::OnButtonClick(wxCommandEvent& event)
{
	event.Skip();

	// Don't allow space to start a new Press Key option, that will interfer with setting a key to space
	if (g_Pressed == WXK_SPACE) { g_Pressed = 0; return; }

	if (m_ButtonMappingTimer->IsRunning()) return;

	// Create the button object
	ClickedButton = (wxButton *)event.GetEventObject();
	// Save old label so we can revert back
	OldLabel = ClickedButton->GetLabel();
	ClickedButton->SetWindowStyle(wxWANTS_CHARS);
	ClickedButton->SetLabel(wxT("<Press Key>"));
	DoGetButtons(ClickedButton->GetId());
}

// Configure axis mapping
void GCPadConfigDialog::OnAxisClick(wxCommandEvent& event)
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

void GCPadConfigDialog::ChangeSettings(wxCommandEvent& event)
{
	int id = event.GetId();

	switch (id)
	{
	case IDC_JOYNAME:
		GCMapping[m_Page].ID = m_Joyname[m_Page]->GetSelection();
		GCMapping[m_Page].joy = joyinfo.at(GCMapping[m_Page].ID).joy;
		break;
	case IDC_DEAD_ZONE_LEFT:
		GCMapping[m_Page].DeadZoneL = m_ComboDeadZoneLeft[m_Page]->GetSelection();
		break;
	case IDC_DEAD_ZONE_RIGHT:
		GCMapping[m_Page].DeadZoneR = m_ComboDeadZoneRight[m_Page]->GetSelection();
		break;
	case IDC_STICK_DIAGONAL:
		GCMapping[m_Page].Diagonal = 100 - m_ComboDiagonal[m_Page]->GetSelection() * 5;
		break;
	case IDC_STICK_S2C:
		GCMapping[m_Page].bSquare2Circle = m_CheckS2C[m_Page]->IsChecked();
		break;
	case IDC_RUMBLE:
		GCMapping[m_Page].Rumble = m_CheckRumble[m_Page]->IsChecked();
		break;
	case IDC_RUMBLE_STRENGTH:
		GCMapping[m_Page].RumbleStrength = m_RumbleStrength[m_Page]->GetSelection() * 10;
		break;
	case IDC_TRIGGER_TYPE:
		GCMapping[m_Page].TriggerType = m_TriggerType[m_Page]->GetSelection();
		break;
	case IDC_STICK_SOURCE:
		GCMapping[m_Page].Stick.Main = m_Combo_StickSrc[m_Page]->GetSelection();
		break;
	case IDC_CSTICK_SOURCE:
		GCMapping[m_Page].Stick.Sub = m_Combo_CStickSrc[m_Page]->GetSelection();
		break;
	case IDC_TRIGGER_SOURCE:
		GCMapping[m_Page].Stick.Shoulder = m_Combo_TriggerSrc[m_Page]->GetSelection();
		break;
	}

	UpdateGUI();
}

void GCPadConfigDialog::UpdateGUI()
{
	if(!m_ControlsCreated)
		return;

	// Disable all pad items if no pads are detected
	bool PadEnabled = NumGoodPads != 0;

	m_Joyname[m_Page]->Enable(PadEnabled);
	m_ComboDeadZoneLeft[m_Page]->Enable(PadEnabled);
	m_ComboDeadZoneRight[m_Page]->Enable(PadEnabled);
	m_CheckS2C[m_Page]->Enable(PadEnabled);
	m_ComboDiagonal[m_Page]->Enable(PadEnabled);
	m_CheckRumble[m_Page]->Enable(PadEnabled);
	m_RumbleStrength[m_Page]->Enable(PadEnabled);
	m_TriggerType[m_Page]->Enable(PadEnabled);
	for(int i = 0; i <= IDB_TRIGGER_R - IDB_ANALOG_LEFT_X; i++)
		m_Button_Analog[i][m_Page]->Enable(PadEnabled);

	wxString tmp;

	m_Joyname[m_Page]->SetSelection(GCMapping[m_Page].ID);
	m_ComboDeadZoneLeft[m_Page]->SetSelection(GCMapping[m_Page].DeadZoneL);
	m_ComboDeadZoneRight[m_Page]->SetSelection(GCMapping[m_Page].DeadZoneR);
	m_ComboDiagonal[m_Page]->SetSelection((100 - GCMapping[m_Page].Diagonal) / 5);
	m_CheckS2C[m_Page]->SetValue(GCMapping[m_Page].bSquare2Circle);
	m_CheckRumble[m_Page]->SetValue(GCMapping[m_Page].Rumble);
	m_RumbleStrength[m_Page]->SetSelection(GCMapping[m_Page].RumbleStrength / 10);
	m_TriggerType[m_Page]->SetSelection(GCMapping[m_Page].TriggerType);

	m_Combo_StickSrc[m_Page]->SetSelection(GCMapping[m_Page].Stick.Main);
	m_Combo_CStickSrc[m_Page]->SetSelection(GCMapping[m_Page].Stick.Sub);
	m_Combo_TriggerSrc[m_Page]->SetSelection(GCMapping[m_Page].Stick.Shoulder);

	for (int i = 0; i <= IDB_TRIGGER_R - IDB_ANALOG_LEFT_X; i++)
	{
		tmp << GCMapping[m_Page].AxisMapping.Code[i];
		m_Button_Analog[i][m_Page]->SetLabel(tmp);
		tmp.clear();
	}

#ifdef _WIN32
	for (int x = 0; x <= IDB_SHDR_SEMI_R - IDB_BTN_A; x++)
	{
		m_Button_GC[x][m_Page]->SetLabel(wxString::FromAscii(
		InputCommon::VKToString(GCMapping[m_Page].Button[x + EGC_A]).c_str()));
	}
#elif defined(HAVE_X11) && HAVE_X11
	char keyStr[10] = {0};
	for (int x = 0; x <= IDB_SHDR_SEMI_R - IDB_BTN_A; x++)
	{
		InputCommon::XKeyToString(GCMapping[m_Page].Button[x + EGC_A], keyStr);
		m_Button_GC[x][m_Page]->SetLabel(wxString::FromAscii(keyStr));
	}
#endif

	DoChangeDeadZone();
}

void GCPadConfigDialog::CreateGUIControls()
{
	// Search for devices and add them to the device list
	wxArrayString StrJoyname; // The string array
	if (NumGoodPads > 0)
	{
		for (int i = 0; i < NumPads; i++)
			StrJoyname.Add(wxString::FromAscii(joyinfo[i].Name.c_str()));
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
		StrRumble.Add(wxString::Format(wxT("%i%%"), i * 10));

	wxArrayString StrSource;
	StrSource.Add(wxT("Keyboard"));
	StrSource.Add(wxT("Analog 1"));
	StrSource.Add(wxT("Analog 2"));
	StrSource.Add(wxT("Triggers"));

	// The Trigger type list
	wxArrayString StrTriggerType;
	StrTriggerType.Add(wxT("SDL")); // -0x8000 to 0x7fff
	StrTriggerType.Add(wxT("XInput")); // 0x00 to 0xff

	static const wxChar* anText[] =
	{
		wxT("Left X-Axis"),
		wxT("Left Y-Axis"),
		wxT("Right X-Axis"),
		wxT("Right Y-Axis"),
		wxT("Left Trigger"),
		wxT("Right Trigger"),
	};

	static const wxChar* padText[] =
	{
		wxT("A"),
		wxT("B"),
		wxT("X"),
		wxT("Y"),
		wxT("Z"),
		wxT("Start"),

		wxT("Up"),	// D-Pad
		wxT("Down"),
		wxT("Left"),
		wxT("Right"),

		wxT("Up"),	// Main Stick
		wxT("Down"),
		wxT("Left"),
		wxT("Right"),

		wxT("Up"),	// C-Stick
		wxT("Down"),
		wxT("Left"),
		wxT("Right"),

		wxT("L"),	// Triggers
		wxT("R"),
		wxT("Semi-L"),
		wxT("Semi-R"),
	};

	// Configuration controls sizes
	static const int TxtW = 50, TxtH = 20, BtW = 70, BtH = 20;
	// A small type font
	wxFont m_SmallFont(7, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL);

	m_Notebook = new wxNotebook(this, ID_NOTEBOOK, wxDefaultPosition, wxDefaultSize);

	for (int i = 0; i < 4; i++)
	{
		m_Controller[i] = new wxPanel(m_Notebook, ID_CONTROLLERPAGE1 + i, wxDefaultPosition, wxDefaultSize);
		m_Notebook->AddPage(m_Controller[i], wxString::Format(wxT("Gamecube Pad %d"), i+1));

		// Controller
		m_Joyname[i] = new wxComboBox(m_Controller[i], IDC_JOYNAME, StrJoyname[0], wxDefaultPosition, wxSize(400, -1), StrJoyname, wxCB_READONLY);

		// Dead zone
		m_ComboDeadZoneLabel[i] = new wxStaticText(m_Controller[i], wxID_ANY, wxT("Dead Zone"));
		m_ComboDeadZoneLeft[i] = new wxComboBox(m_Controller[i], IDC_DEAD_ZONE_LEFT, TextDeadZone[0], wxDefaultPosition,  wxSize(50, -1), TextDeadZone, wxCB_READONLY);
		m_ComboDeadZoneRight[i] = new wxComboBox(m_Controller[i], IDC_DEAD_ZONE_RIGHT, TextDeadZone[0], wxDefaultPosition,  wxSize(50, -1), TextDeadZone, wxCB_READONLY);

		// Circle to square
		m_CheckS2C[i] = new wxCheckBox(m_Controller[i], IDC_STICK_S2C, wxT("Square To Circle"));
		m_CheckS2C[i]->SetToolTip(wxT("This will convert a square stick radius to a circle stick radius, which is\n")
			wxT("similar to the octagonal area that the original GameCube pad produces."));

		// The drop down menu for the circle to square adjustment
		m_DiagonalLabel[i] = new wxStaticText(m_Controller[i], wxID_ANY, wxT("Diagonal"));
		m_DiagonalLabel[i]->SetToolTip(wxT("To produce a perfect circle in the 'Out' window you have to manually set\n")
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

		m_sSquare2Circle[i] = new wxBoxSizer(wxVERTICAL);
		m_sSquare2Circle[i]->Add(m_CheckS2C[i], 0, wxALIGN_CENTER | (wxUP), 0);
		m_sSquare2Circle[i]->Add(m_sDiagonal[i], 0, wxALIGN_CENTER | (wxUP), 2);

		m_sRumbleStrength[i] = new wxBoxSizer(wxHORIZONTAL);
		m_sRumbleStrength[i]->Add(m_RumbleStrengthLabel[i], 0, (wxUP), 4);
		m_sRumbleStrength[i]->Add(m_RumbleStrength[i], 0, (wxLEFT), 2);

		m_sRumble[i] = new wxBoxSizer(wxVERTICAL);
		m_sRumble[i]->Add(m_CheckRumble[i], 0, wxALIGN_CENTER | (wxUP), 0);
		m_sRumble[i]->Add(m_sRumbleStrength[i], 0, wxALIGN_CENTER | (wxUP), 2);

		m_sS2CDeadZone[i] = new wxBoxSizer(wxHORIZONTAL);
		m_sS2CDeadZone[i]->Add(m_sDeadZone[i], 0, (wxUP), 0);
		m_sS2CDeadZone[i]->Add(m_sSquare2Circle[i], 0, (wxLEFT), 40);
		m_sS2CDeadZone[i]->Add(m_sRumble[i], 0, (wxLEFT), 40);

		m_gJoyPad[i] = new wxStaticBoxSizer (wxVERTICAL, m_Controller[i], wxT("Gamepad"));
		m_gJoyPad[i]->AddStretchSpacer();
		m_gJoyPad[i]->Add(m_Joyname[i], 0, wxEXPAND | (wxLEFT | wxRIGHT | wxDOWN), 5);
		m_gJoyPad[i]->Add(m_sS2CDeadZone[i], 0, wxALIGN_CENTER | (wxUP | wxDOWN), 4);
		m_gJoyPad[i]->AddStretchSpacer();

		// Row 1 Sizers: Connected pads, tilt
		m_sHorizJoypad[i] = new wxBoxSizer(wxHORIZONTAL);
		m_sHorizJoypad[i]->Add(m_gJoyPad[i], 0, wxEXPAND | (wxLEFT), 5);


		// Stick Status Panels
		m_tStatusLeftIn[i] = new wxStaticText(m_Controller[i], wxID_ANY, wxT("Not connected"));
		m_tStatusLeftOut[i] = new wxStaticText(m_Controller[i], wxID_ANY, wxT("Not connected"));
		m_tStatusRightIn[i] = new wxStaticText(m_Controller[i], wxID_ANY, wxT("Not connected"));
		m_tStatusRightOut[i] = new wxStaticText(m_Controller[i], wxID_ANY, wxT("Not connected"));

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
		m_tTriggerSource[i]= new wxStaticText(m_Controller[i], wxID_ANY, wxT("Trigger Source"));
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

		for (int x = 0; x < 6; x++)
		{
			m_Text_Analog[x][i] = new wxStaticText(m_Controller[i], wxID_ANY, anText[x]);
			m_Button_Analog[x][i] = new wxButton(m_Controller[i], x + IDB_ANALOG_LEFT_X, wxEmptyString, wxDefaultPosition, wxSize(BtW, BtH));
			m_Button_Analog[x][i]->SetFont(m_SmallFont);
			m_Sizer_Analog[x][i] = new wxBoxSizer(wxHORIZONTAL);
			m_Sizer_Analog[x][i]->Add(m_Text_Analog[x][i], 0, (wxUP), 4);
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

		// Row 3 Sizes: Analog Mapping
		m_sHorizAnalog[i] = new wxBoxSizer(wxHORIZONTAL);
		m_sHorizAnalog[i]->Add(m_gAnalog[i], 0, (wxLEFT), 5);


		// Pad Mapping
		m_gButton[i] = new wxStaticBoxSizer(wxVERTICAL, m_Controller[i], wxT("Buttons"));
		m_gDPad[i] = new wxStaticBoxSizer(wxVERTICAL, m_Controller[i], wxT("D-Pad"));
		m_gStick[i] = new wxStaticBoxSizer(wxVERTICAL, m_Controller[i], wxT("Main Stick"));
		m_Text_StickSrc[i] = new wxStaticText(m_Controller[i], wxID_ANY, wxT("Source"));
		m_Combo_StickSrc[i] = new wxComboBox(m_Controller[i], IDC_STICK_SOURCE, StrSource[0], wxDefaultPosition, wxSize(70, -1), StrSource, wxCB_READONLY);
		m_sStickSrc[i] = new wxBoxSizer(wxHORIZONTAL);
		m_sStickSrc[i]->Add(m_Text_StickSrc[i], 0, (wxUP), 4);
		m_sStickSrc[i]->Add(m_Combo_StickSrc[i], 0, (wxLEFT), 2);
		m_gStick[i]->Add(m_sStickSrc[i], 0, wxALIGN_RIGHT | (wxALL), 2);
		m_gStick[i]->AddSpacer(2);
		m_gCStick[i] = new wxStaticBoxSizer(wxVERTICAL, m_Controller[i], wxT("C-Stick"));
		m_Text_CStickSrc[i] = new wxStaticText(m_Controller[i], wxID_ANY, wxT("Source"));
		m_Combo_CStickSrc[i] = new wxComboBox(m_Controller[i], IDC_CSTICK_SOURCE, StrSource[0], wxDefaultPosition, wxSize(70, -1), StrSource, wxCB_READONLY);
		m_sCStickSrc[i] = new wxBoxSizer(wxHORIZONTAL);
		m_sCStickSrc[i]->Add(m_Text_CStickSrc[i], 0, (wxUP), 4);
		m_sCStickSrc[i]->Add(m_Combo_CStickSrc[i], 0, (wxLEFT), 2);
		m_gCStick[i]->Add(m_sCStickSrc[i], 0, wxALIGN_RIGHT | (wxALL), 2);
		m_gCStick[i]->AddSpacer(2);
		m_gTrigger[i] = new wxStaticBoxSizer(wxVERTICAL, m_Controller[i], wxT("Triggers"));
		m_Text_TriggerSrc[i] = new wxStaticText(m_Controller[i], wxID_ANY, wxT("Source"));
		m_Combo_TriggerSrc[i] = new wxComboBox(m_Controller[i], IDC_TRIGGER_SOURCE, StrSource[0], wxDefaultPosition, wxSize(70, -1), StrSource, wxCB_READONLY);
		m_sTriggerSrc[i] = new wxBoxSizer(wxHORIZONTAL);
		m_sTriggerSrc[i]->Add(m_Text_TriggerSrc[i], 0, (wxUP), 4);
		m_sTriggerSrc[i]->Add(m_Combo_TriggerSrc[i], 0, (wxLEFT), 2);
		m_gTrigger[i]->Add(m_sTriggerSrc[i], 0, wxALIGN_RIGHT | (wxALL), 2);
		m_gTrigger[i]->AddSpacer(2);

		for (int x = 0; x <= IDB_SHDR_SEMI_R - IDB_BTN_A; x++)
		{
			m_Text_Pad[x][i] = new wxStaticText(m_Controller[i], wxID_ANY, padText[x]);
			m_Button_GC[x][i] = new wxButton(m_Controller[i], x + IDB_BTN_A, wxEmptyString, wxDefaultPosition, wxSize(BtW, BtH));
			m_Button_GC[x][i]->SetFont(m_SmallFont);
			m_Sizer_Pad[x][i] = new wxBoxSizer(wxHORIZONTAL);
			m_Sizer_Pad[x][i]->Add(m_Text_Pad[x][i], 0, (wxUP), 4);
			m_Sizer_Pad[x][i]->Add(m_Button_GC[x][i], 0, (wxLEFT), 2);
			if (x <= IDB_BTN_START)
				m_gButton[i]->Add(m_Sizer_Pad[x - IDB_BTN_A][i], 0, wxALIGN_RIGHT | (wxLEFT | wxRIGHT | wxDOWN), 1);
			else if (x <= IDB_DPAD_RIGHT)
				m_gDPad[i]->Add(m_Sizer_Pad[x - IDB_BTN_A][i], 0, wxALIGN_RIGHT | (wxLEFT | wxRIGHT | wxDOWN), 1);
			else if (x <= IDB_MAIN_RIGHT)
				m_gStick[i]->Add(m_Sizer_Pad[x - IDB_BTN_A][i], 0, wxALIGN_RIGHT | (wxLEFT | wxRIGHT | wxDOWN), 1);
			else if (x <= IDB_SUB_RIGHT)
				m_gCStick[i]->Add(m_Sizer_Pad[x - IDB_BTN_A][i], 0, wxALIGN_RIGHT | (wxLEFT | wxRIGHT | wxDOWN), 1);
			else
				m_gTrigger[i]->Add(m_Sizer_Pad[x - IDB_BTN_A][i], 0, wxALIGN_RIGHT | (wxLEFT | wxRIGHT | wxDOWN), 1);
		}

		// Row 4 Sizers: Button Mapping
		m_sHorizMapping[i] = new wxBoxSizer(wxHORIZONTAL);
		m_sHorizMapping[i]->Add(m_gButton[i], 0, (wxLEFT), 5);
		m_sHorizMapping[i]->Add(m_gDPad[i], 0, (wxLEFT), 5);
		m_sHorizMapping[i]->Add(m_gStick[i], 0, (wxLEFT), 5);
		m_sHorizMapping[i]->Add(m_gCStick[i], 0, (wxLEFT), 5);
		m_sHorizMapping[i]->Add(m_gTrigger[i], 0, (wxLEFT), 5);


		// Set up sizers and layout
		// The sizer m_sMain will be expanded inside m_Controller
		m_sMain[i] = new wxBoxSizer(wxVERTICAL);
		m_sMain[i]->Add(m_sHorizJoypad[i], 0, wxEXPAND | (wxLEFT | wxRIGHT | wxDOWN), 5);
		m_sMain[i]->Add(m_sHorizStatus[i], 0, wxEXPAND | (wxLEFT | wxRIGHT | wxDOWN), 5);
		m_sMain[i]->Add(m_sHorizAnalog[i], 0, wxEXPAND | (wxLEFT | wxRIGHT | wxDOWN), 5);
		m_sMain[i]->Add(m_sHorizMapping[i], 0, wxEXPAND | (wxLEFT | wxRIGHT | wxDOWN), 5);

		// Set the main sizer
		m_Controller[i]->SetSizer(m_sMain[i]);
	}

	m_OK = new wxButton(this, ID_OK, wxT("OK"));
	m_OK->SetToolTip(wxT("Save changes and close"));
	m_Cancel = new wxButton(this, ID_CANCEL, wxT("Cancel"));
	m_Cancel->SetToolTip(wxT("Discard changes and close"));
	wxBoxSizer* m_DlgButton = new wxBoxSizer(wxHORIZONTAL);
	m_DlgButton->AddStretchSpacer();
	m_DlgButton->Add(m_OK, 0, (wxLEFT), 5);	
	m_DlgButton->Add(m_Cancel, 0, (wxLEFT), 5);

	m_MainSizer = new wxBoxSizer(wxVERTICAL);
	m_MainSizer->Add(m_Notebook, 1, wxEXPAND | wxALL, 5);
	m_MainSizer->Add(m_DlgButton, 0, wxEXPAND | (wxLEFT | wxRIGHT | wxDOWN), 5);

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

// Bitmap box and dot
wxBitmap GCPadConfigDialog::CreateBitmap()
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

wxBitmap GCPadConfigDialog::CreateBitmapDot()
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

wxBitmap GCPadConfigDialog::CreateBitmapDeadZone(int Radius)
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

wxBitmap GCPadConfigDialog::CreateBitmapClear()
{
	wxBitmap bitmap(BoxW, BoxH);
	wxMemoryDC dc;
	dc.SelectObject(bitmap);
	
	dc.Clear();
	dc.SelectObject(wxNullBitmap);
	return bitmap;
}
