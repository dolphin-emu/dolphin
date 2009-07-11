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

#include "Common.h"
#include "ConfigDlg.h"
#include "../PadSimple.h"

#ifdef _WIN32
#include "XInput.h"
#include "../../../../Core/InputCommon/Src/DirectInputBase.h" // Core

DInput m_dinput;
#endif

BEGIN_EVENT_TABLE(PADConfigDialogSimple,wxDialog)
	EVT_CLOSE(PADConfigDialogSimple::OnClose)
	EVT_BUTTON(ID_CLOSE,PADConfigDialogSimple::OnCloseClick)
	EVT_BUTTON(ID_PAD_ABOUT,PADConfigDialogSimple::DllAbout)

	EVT_CHECKBOX(ID_X360PAD,PADConfigDialogSimple::ControllerSettingsChanged)
	EVT_CHOICE(ID_X360PAD_CHOICE,PADConfigDialogSimple::ControllerSettingsChanged)
	EVT_CHECKBOX(ID_RUMBLE,PADConfigDialogSimple::ControllerSettingsChanged)
	EVT_CHECKBOX(ID_DISABLE,PADConfigDialogSimple::ControllerSettingsChanged)

	// Input recording
	#ifdef RERECORDING
		EVT_CHECKBOX(ID_RECORDING,PADConfigDialogSimple::ControllerSettingsChanged)
		EVT_CHECKBOX(ID_PLAYBACK,PADConfigDialogSimple::ControllerSettingsChanged)
		EVT_BUTTON(ID_SAVE_RECORDING,PADConfigDialogSimple::ControllerSettingsChanged)	
	#endif

	EVT_BUTTON(CTL_A,PADConfigDialogSimple::OnButtonClick)
	EVT_BUTTON(CTL_B,PADConfigDialogSimple::OnButtonClick)
	EVT_BUTTON(CTL_X,PADConfigDialogSimple::OnButtonClick)
	EVT_BUTTON(CTL_Y,PADConfigDialogSimple::OnButtonClick)
	EVT_BUTTON(CTL_Z,PADConfigDialogSimple::OnButtonClick)
	EVT_BUTTON(CTL_START,PADConfigDialogSimple::OnButtonClick)
	EVT_BUTTON(CTL_L,PADConfigDialogSimple::OnButtonClick)
	EVT_BUTTON(CTL_R,PADConfigDialogSimple::OnButtonClick)
	EVT_BUTTON(CTL_MAINUP,PADConfigDialogSimple::OnButtonClick)
	EVT_BUTTON(CTL_MAINDOWN,PADConfigDialogSimple::OnButtonClick)
	EVT_BUTTON(CTL_MAINLEFT,PADConfigDialogSimple::OnButtonClick)
	EVT_BUTTON(CTL_MAINRIGHT,PADConfigDialogSimple::OnButtonClick)
	EVT_BUTTON(CTL_SUBUP,PADConfigDialogSimple::OnButtonClick)
	EVT_BUTTON(CTL_SUBDOWN,PADConfigDialogSimple::OnButtonClick)
	EVT_BUTTON(CTL_SUBLEFT,PADConfigDialogSimple::OnButtonClick)
	EVT_BUTTON(CTL_SUBRIGHT,PADConfigDialogSimple::OnButtonClick)
	EVT_BUTTON(CTL_DPADUP,PADConfigDialogSimple::OnButtonClick)
	EVT_BUTTON(CTL_DPADDOWN,PADConfigDialogSimple::OnButtonClick)
	EVT_BUTTON(CTL_DPADLEFT,PADConfigDialogSimple::OnButtonClick)
	EVT_BUTTON(CTL_DPADRIGHT,PADConfigDialogSimple::OnButtonClick)
	EVT_BUTTON(CTL_HALFPRESS,PADConfigDialogSimple::OnButtonClick)
END_EVENT_TABLE()

PADConfigDialogSimple::PADConfigDialogSimple(wxWindow *parent, wxWindowID id, const wxString &title, const wxPoint &position, const wxSize& size, long style)
: wxDialog(parent, id, title, position, size, style)
{
#ifdef _WIN32
	m_dinput.Init((HWND)parent);
#endif
	ClickedButton = NULL;
	CreateGUIControls();
	Fit();

	// Connect keydown to the window
	wxTheApp->Connect(wxID_ANY, wxEVT_KEY_DOWN,
		wxKeyEventHandler(PADConfigDialogSimple::OnKeyDown),
		(wxObject*)NULL, this);
}

PADConfigDialogSimple::~PADConfigDialogSimple()
{
}


//////////////////////////////////////////////////////////////////////////////////////////
// Create input button controls
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
inline void AddControl(wxPanel *pan, wxButton **button, wxStaticBoxSizer *sizer,
					   const char *name, int ctl, int controller)
{
	wxBoxSizer *hButton = new wxBoxSizer(wxHORIZONTAL);
	char keyStr[10] = {0};

	// Add the label
	hButton->Add(new wxStaticText(pan, 0, wxString::FromAscii(name), 
				 wxDefaultPosition, wxDefaultSize), 0,
				 wxALIGN_CENTER_VERTICAL|wxALL);

	// Give it the mapped key name
#ifdef _WIN32
	DInput::DIKToString(pad[controller].keyForControl[ctl], keyStr);	
#elif defined(HAVE_X11) && HAVE_X11
	InputCommon::XKeyToString(pad[controller].keyForControl[ctl], keyStr);
#endif

	// Add the button to its sizer
	*button = new wxButton(pan, ctl, wxString::FromAscii(keyStr), 
		                   wxDefaultPosition, wxDefaultSize, wxWANTS_CHARS);
	hButton->Add(*button, 0, wxALIGN_RIGHT|wxALL);
	sizer->Add(hButton, 0, wxALIGN_RIGHT|wxALL);
}
////////////////////////////////////


void PADConfigDialogSimple::CreateGUIControls()
{
	// Notebook
	m_Notebook = new wxNotebook(this, ID_NOTEBOOK, wxDefaultPosition, wxDefaultSize);
	
	// Controller pages
	m_Controller[0] = new wxPanel(m_Notebook, ID_CONTROLLERPAGE1, wxDefaultPosition, wxDefaultSize);
	m_Notebook->AddPage(m_Controller[0], wxT("Controller 1"));
	m_Controller[1] = new wxPanel(m_Notebook, ID_CONTROLLERPAGE2, wxDefaultPosition, wxDefaultSize);
	m_Notebook->AddPage(m_Controller[1], wxT("Controller 2"));
	m_Controller[2] = new wxPanel(m_Notebook, ID_CONTROLLERPAGE3, wxDefaultPosition, wxDefaultSize);
	m_Notebook->AddPage(m_Controller[2], wxT("Controller 3"));
	m_Controller[3] = new wxPanel(m_Notebook, ID_CONTROLLERPAGE4, wxDefaultPosition, wxDefaultSize);
	m_Notebook->AddPage(m_Controller[3], wxT("Controller 4"));
	
	// Standard buttons
	m_Close = new wxButton(this, ID_CLOSE, wxT("Close"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	m_About = new wxButton(this, ID_PAD_ABOUT, wxT("About"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	
	// Put notebook and standard buttons in sizers
	wxBoxSizer* sSButtons;
	sSButtons = new wxBoxSizer(wxHORIZONTAL);
	sSButtons->Add(m_About,0,wxALL, 5);
	sSButtons->Add(0, 0, 1, wxEXPAND, 5);
	sSButtons->Add(m_Close, 0, wxALL, 5);
	
	wxBoxSizer* sMain;
	sMain = new wxBoxSizer(wxVERTICAL);
	sMain->Add(m_Notebook, 1, wxEXPAND|wxALL, 5);
	sMain->Add(sSButtons, 0, wxEXPAND, 5);
	
	this->SetSizer(sMain);
	this->Layout();

#ifdef _WIN32
	// Add connected XPads
	for (int x = 0; x < 4; x++)
	{
		XINPUT_STATE xstate;
		DWORD xresult = XInputGetState(x, &xstate);

		if (xresult == ERROR_SUCCESS)
		{
			arrayStringFor_X360Pad.Add(wxString::Format("%i", x+1));
		}
	}
#endif
	
	for(int i = 0; i < 4; i++)
	{	
		// --------------------------------------------------------------------
		// Settings
		// -----------------------------
		// Main horizontal container
		sDevice[i] = new wxBoxSizer(wxHORIZONTAL);

		sbDevice[i] = new wxStaticBoxSizer(wxVERTICAL, m_Controller[i], wxT("Controller Settings"));
		m_Disable[i] = new wxCheckBox(m_Controller[i], ID_DISABLE, wxT("Disable when Dolphin is not in focus"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
		sbDevice[i]->Add(m_Disable[i], 0, wxEXPAND|wxALL, 1);

#ifdef _WIN32
		m_SizeXInput[i] = new wxStaticBoxSizer(wxHORIZONTAL, m_Controller[i], wxT("XInput Pad"));
		m_X360Pad[i] = new wxCheckBox(m_Controller[i], ID_X360PAD, wxT("Enable X360Pad"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
		m_X360PadC[i] = new wxChoice(m_Controller[i], ID_X360PAD_CHOICE, wxDefaultPosition, wxDefaultSize, arrayStringFor_X360Pad, 0, wxDefaultValidator);
		m_Rumble[i] = new wxCheckBox(m_Controller[i], ID_RUMBLE, wxT("Enable rumble"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);

		m_SizeXInput[i]->Add(m_X360Pad[i], 0, wxEXPAND | wxALL, 1);
		m_SizeXInput[i]->Add(m_X360PadC[i], 0, wxEXPAND | wxALL, 1);
		m_SizeXInput[i]->Add(m_Rumble[i], 0, wxEXPAND | wxALL, 1);
#endif
		// Set values
		m_Disable[i]->SetValue(pad[i].bDisable);

#ifdef _WIN32
		// Check if any XInput pad was found
		if (arrayStringFor_X360Pad.IsEmpty())
		{
			m_X360Pad[i]->SetLabel(wxT("Enable X360Pad - No pad connected"));
			m_X360Pad[i]->SetValue(false);
			m_X360Pad[i]->Enable(false);
			pad[i].bEnableXPad = false;
			m_X360PadC[i]->Hide();
			m_Rumble[i]->Hide();
		}
		else
		{
			m_X360Pad[i]->SetValue(pad[i].bEnableXPad);
			m_X360PadC[i]->SetSelection(pad[i].XPadPlayer);
			m_X360PadC[i]->Enable(m_X360Pad[i]->IsChecked());
			m_Rumble[i]->SetValue(pad[i].bRumble);
			m_Rumble[i]->Enable(m_X360Pad[i]->IsChecked());
		}
#endif
		
		// Sizers
		sDevice[i]->Add(sbDevice[i], 0, wxEXPAND | wxALL, 1);
		//sDevice[i]->AddStretchSpacer();
#ifdef _WIN32
		sDevice[i]->Add(m_SizeXInput[i], 0, wxEXPAND | wxALL, 1);
#endif
		// -----------------------------------


		/////////////////////////////////////////////////////////////////////////////////////
		// Rerecording
		// ¯¯¯¯¯¯¯¯¯
		#ifdef RERECORDING
		// Create controls
		m_SizeRecording[i] = new wxStaticBoxSizer(wxVERTICAL, m_Controller[i], wxT("Input Recording"));
		m_CheckRecording[i] = new wxCheckBox(m_Controller[i], ID_RECORDING, wxT("Record input"));
		m_CheckPlayback[i] = new wxCheckBox(m_Controller[i], ID_PLAYBACK, wxT("Play back input"));
		m_BtnSaveRecording[i] = new wxButton(m_Controller[i], ID_SAVE_RECORDING, wxT("Save recording"), wxDefaultPosition, wxDefaultSize);

		// Tool tips
		m_CheckRecording[i]->SetToolTip(wxT("Your recording will be saved to pad-record.bin in the Dolphin dir when you stop the game"));
		m_CheckPlayback[i]->SetToolTip(wxT("Play back the pad-record.bin file from the Dolphin dir"));
		m_BtnSaveRecording[i]->SetToolTip(wxT(
			"This will save the current recording to pad-record.bin. Your recording will\n"
			"also be automatically saved every 60 * 10 frames. And when you shut down the\n"
			"game."));

		// Sizers
		m_SizeRecording[i]->Add(m_CheckRecording[i], 0, wxEXPAND | wxALL, 1);
		m_SizeRecording[i]->Add(m_CheckPlayback[i], 0, wxEXPAND | wxALL, 1);
		m_SizeRecording[i]->Add(m_BtnSaveRecording[i], 0, wxEXPAND | wxALL, 1);

		// Only enable these options for pad 0
		m_CheckRecording[i]->Enable(false); m_CheckRecording[0]->Enable(true);
		m_CheckPlayback[i]->Enable(false); m_CheckPlayback[0]->Enable(true);
		m_BtnSaveRecording[i]->Enable(false); m_BtnSaveRecording[0]->Enable(true);
		// Don't allow saving when we are not recording
		m_BtnSaveRecording[i]->Enable(g_EmulatorRunning && pad[0].bRecording);
		sDevice[i]->Add(m_SizeRecording[i], 0, wxEXPAND | wxALL, 1);

		// Set values
		m_CheckRecording[0]->SetValue(pad[0].bRecording);
		m_CheckPlayback[0]->SetValue(pad[0].bPlayback);

		//DEBUG_LOG(CONSOLE, "m_CheckRecording: %i\n", pad[0].bRecording, pad[0].bPlayback);
		#endif
		//////////////////////////////////////
	

		// --------------------------------------------------------------------
		// Buttons
		// -----------------------------
		sButtons[i] = new wxStaticBoxSizer(wxVERTICAL, m_Controller[i], wxT("Buttons"));

		AddControl(m_Controller[i], &(m_ButtonA[i]), sButtons[i], "A: ", CTL_A, i);
		AddControl(m_Controller[i], &(m_ButtonB[i]), sButtons[i], "B: ", CTL_B, i);
		AddControl(m_Controller[i], &(m_ButtonX[i]), sButtons[i], "X: ", CTL_X, i);
		AddControl(m_Controller[i], &(m_ButtonY[i]), sButtons[i], "Y: ", CTL_Y, i);
		AddControl(m_Controller[i], &(m_ButtonZ[i]), sButtons[i], "Z: ", CTL_Z, i);
		AddControl(m_Controller[i], &(m_ButtonStart[i]), sButtons[i], "Start: ", CTL_START, i);

		sTriggers[i] = new wxStaticBoxSizer(wxVERTICAL, m_Controller[i], wxT("Triggers"));

		AddControl(m_Controller[i], &(m_ButtonL[i]), sTriggers[i], "        L: ", CTL_L, i);
		AddControl(m_Controller[i], &(m_ButtonR[i]), sTriggers[i], "        R: ", CTL_R, i);

		sModifiers[i] = new wxStaticBoxSizer(wxVERTICAL, m_Controller[i], wxT("Modifiers"));

		AddControl(m_Controller[i], &(m_HalfPress[i]), sModifiers[i], "1/2 Press: ", CTL_HALFPRESS, i);

		sStick[i] = new wxStaticBoxSizer(wxVERTICAL, m_Controller[i], wxT("Main Stick"));

		AddControl(m_Controller[i], &(m_StickUp[i]), sStick[i], "Up: ", CTL_MAINUP, i);
		AddControl(m_Controller[i], &(m_StickDown[i]), sStick[i], "Down: ", CTL_MAINDOWN, i);
		AddControl(m_Controller[i], &(m_StickLeft[i]), sStick[i], "Left: ", CTL_MAINLEFT, i);
		AddControl(m_Controller[i], &(m_StickRight[i]), sStick[i], "Right: ", CTL_MAINRIGHT, i);

		sDPad[i] = new wxStaticBoxSizer(wxVERTICAL, m_Controller[i], wxT("D-Pad"));

		AddControl(m_Controller[i], &(m_DPadUp[i]), sDPad[i], "Up: ", CTL_DPADUP, i);
		AddControl(m_Controller[i], &(m_DPadDown[i]), sDPad[i], "Down: ", CTL_DPADDOWN, i);
		AddControl(m_Controller[i], &(m_DPadLeft[i]), sDPad[i], "Left: ", CTL_DPADLEFT, i);
		AddControl(m_Controller[i], &(m_DPadRight[i]), sDPad[i], "Right: ", CTL_DPADRIGHT, i);

		sCStick[i] = new wxStaticBoxSizer(wxVERTICAL, m_Controller[i], wxT("C-Stick"));

		AddControl(m_Controller[i], &(m_CStickUp[i]), sCStick[i], "Up: ", CTL_SUBUP, i);
		AddControl(m_Controller[i], &(m_CStickDown[i]), sCStick[i], "Down: ", CTL_SUBDOWN, i);
		AddControl(m_Controller[i], &(m_CStickLeft[i]), sCStick[i], "Left: ", CTL_SUBLEFT, i);
		AddControl(m_Controller[i], &(m_CStickRight[i]), sCStick[i], "Right: ", CTL_SUBRIGHT, i);

		// --------------------------------------------------------------------
		// Sizers
		// -----------------------------
		sPage[i] = new wxGridBagSizer(0, 0);
		sPage[i]->SetFlexibleDirection(wxBOTH);
		sPage[i]->SetNonFlexibleGrowMode(wxFLEX_GROWMODE_SPECIFIED);
		sPage[i]->Add(sDevice[i], wxGBPosition(0, 0), wxGBSpan(1, 5), wxEXPAND|wxALL, 1);
		sPage[i]->Add(sButtons[i], wxGBPosition(1, 0), wxGBSpan(2, 1), wxALL, 1);
		sPage[i]->Add(sTriggers[i], wxGBPosition(1, 1), wxGBSpan(1, 1), wxEXPAND|wxALL, 1);
		sPage[i]->Add(sModifiers[i], wxGBPosition(2, 1), wxGBSpan(1, 1), wxALL, 1);
		sPage[i]->Add(sStick[i], wxGBPosition(1, 2), wxGBSpan(2, 1), wxALL, 1);
		sPage[i]->Add(sDPad[i], wxGBPosition(1, 3), wxGBSpan(2, 1), wxALL, 1);
		sPage[i]->Add(sCStick[i], wxGBPosition(1, 4), wxGBSpan(2, 1), wxALL, 1);
		m_Controller[i]->SetSizer(sPage[i]);
		sPage[i]->Layout();
	}
}

void PADConfigDialogSimple::OnClose(wxCloseEvent& event)
{
#ifdef _WIN32
	m_dinput.Free();
#endif
	EndModal(0);
}

void PADConfigDialogSimple::OnKeyDown(wxKeyEvent& event)
{
	char keyStr[10] = {0};
	if(ClickedButton != NULL)
	{
		// Get the selected notebook page
		int page = m_Notebook->GetSelection();

#ifdef _WIN32
		m_dinput.Read();
		for(int i = 0; i < 255; i++)
		{
			if(m_dinput.diks[i])
			{
				// Save the mapped key, the wxButtons have the Id 0 to 21
				pad[page].keyForControl[ClickedButton->GetId()] = i;
				// Get the key name
				DInput::DIKToString(i, keyStr);
				ClickedButton->SetLabel(wxString::FromAscii(keyStr));
				break;
			}
		}
#elif defined(HAVE_X11) && HAVE_X11
		pad[page].keyForControl[ClickedButton->GetId()] = InputCommon::wxCharCodeWXToX(event.GetKeyCode());

		InputCommon::XKeyToString(pad[page].keyForControl[ClickedButton->GetId()], keyStr);
		ClickedButton->SetLabel(wxString::FromAscii(keyStr));
#endif
		ClickedButton->Disconnect();
	}
	// Reset 
	ClickedButton = NULL;
	//event.Skip();
}

// We have clicked a button
void PADConfigDialogSimple::OnButtonClick(wxCommandEvent& event)
{
	// Check if the Space key was set, to solve the problem that the Space key calls this function
	#ifdef _WIN32
		if (m_dinput.diks[DIK_SPACE]) { m_dinput.diks[DIK_SPACE] = 0; return; }
	#endif

	// If we come here again before any key was set
	if(ClickedButton) ClickedButton->SetLabel(oldLabel);

	// Save the old button label so we can reapply it if necessary
	ClickedButton = (wxButton *)event.GetEventObject();
	oldLabel = ClickedButton->GetLabel();
	ClickedButton->SetLabel(_("Press Key"));

	ClickedButton->SetWindowStyle(wxWANTS_CHARS);
}

void PADConfigDialogSimple::OnCloseClick(wxCommandEvent& event)
{
	Close();
}

void PADConfigDialogSimple::ControllerSettingsChanged(wxCommandEvent& event)
{
	int page = m_Notebook->GetSelection();

	switch (event.GetId())
	{
	// General settings
	case ID_DISABLE:
		pad[page].bDisable = m_Disable[page]->GetValue();
		break;

	// XInput
	case ID_X360PAD:
		pad[page].bEnableXPad = event.IsChecked();
		m_Rumble[page]->Enable(event.IsChecked());
		m_X360PadC[page]->Enable(event.IsChecked());
		break;
	case ID_X360PAD_CHOICE:
		pad[page].XPadPlayer = event.GetSelection();
		break;		
	case ID_RUMBLE:
		pad[page].bRumble = m_Rumble[page]->GetValue();
		break;

	// Input recording
	#ifdef RERECORDING
		case ID_RECORDING:
			pad[page].bRecording = m_CheckRecording[page]->GetValue();
			// Turn off the other option
			pad[page].bPlayback = false; m_CheckPlayback[page]->SetValue(false);
			break;
		case ID_PLAYBACK:
			pad[page].bPlayback = m_CheckPlayback[page]->GetValue();
			// Turn off the other option
			pad[page].bRecording = false; m_CheckRecording[page]->SetValue(false);
			break;
		case ID_SAVE_RECORDING:
			// Double check again that we are still running a game
			if (g_EmulatorRunning) SaveRecord();
			break;
	#endif		
	}
}

void PADConfigDialogSimple::DllAbout(wxCommandEvent& event)
{
	wxString message;
#ifdef _WIN32
	message = _("A simple keyboard and XInput plugin for dolphin.");
#else
	message = _("A simple keyboard plugin for dolphin.");
#endif

	wxMessageBox(_T("Dolphin PadSimple Plugin\nBy ector and F|RES\n\n" + message),
		_T("Dolphin PadSimple"), wxOK, this);
}
