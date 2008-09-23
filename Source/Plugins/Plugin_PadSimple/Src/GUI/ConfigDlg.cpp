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


#include "ConfigDlg.h"
#include "../PadSimple.h"
//#include "../DirectInputBase.h"

BEGIN_EVENT_TABLE(ConfigDialog,wxDialog)
	EVT_CLOSE(ConfigDialog::OnClose)
	EVT_BUTTON(ID_CLOSE,ConfigDialog::OnCloseClick)
	EVT_CHECKBOX(ID_ATTACHED,ConfigDialog::AttachedCheck)
	EVT_CHECKBOX(ID_DISABLE,ConfigDialog::DisableCheck)
	EVT_CHECKBOX(ID_RUMBLE,ConfigDialog::RumbleCheck)
	EVT_BUTTON(CTL_A,ConfigDialog::OnButtonClick)
	EVT_BUTTON(CTL_B,ConfigDialog::OnButtonClick)
	EVT_BUTTON(CTL_X,ConfigDialog::OnButtonClick)
	EVT_BUTTON(CTL_Y,ConfigDialog::OnButtonClick)
	EVT_BUTTON(CTL_Z,ConfigDialog::OnButtonClick)
	EVT_BUTTON(CTL_START,ConfigDialog::OnButtonClick)
	EVT_BUTTON(CTL_TRIGGER_L,ConfigDialog::OnButtonClick)
	EVT_BUTTON(CTL_L,ConfigDialog::OnButtonClick)
	EVT_BUTTON(CTL_TRIGGER_R,ConfigDialog::OnButtonClick)
	EVT_BUTTON(CTL_R,ConfigDialog::OnButtonClick)
	EVT_BUTTON(CTL_MAINUP,ConfigDialog::OnButtonClick)
	EVT_BUTTON(CTL_MAINDOWN,ConfigDialog::OnButtonClick)
	EVT_BUTTON(CTL_MAINLEFT,ConfigDialog::OnButtonClick)
	EVT_BUTTON(CTL_MAINRIGHT,ConfigDialog::OnButtonClick)
	EVT_BUTTON(CTL_HALFMAIN,ConfigDialog::OnButtonClick)
	EVT_BUTTON(CTL_SUBUP,ConfigDialog::OnButtonClick)
	EVT_BUTTON(CTL_SUBDOWN,ConfigDialog::OnButtonClick)
	EVT_BUTTON(CTL_SUBLEFT,ConfigDialog::OnButtonClick)
	EVT_BUTTON(CTL_SUBRIGHT,ConfigDialog::OnButtonClick)
	EVT_BUTTON(CTL_HALFSUB,ConfigDialog::OnButtonClick)
	EVT_BUTTON(CTL_DPADUP,ConfigDialog::OnButtonClick)
	EVT_BUTTON(CTL_DPADDOWN,ConfigDialog::OnButtonClick)
	EVT_BUTTON(CTL_DPADLEFT,ConfigDialog::OnButtonClick)
	EVT_BUTTON(CTL_DPADRIGHT,ConfigDialog::OnButtonClick)
END_EVENT_TABLE()

ConfigDialog::ConfigDialog(wxWindow *parent, wxWindowID id, const wxString &title, const wxPoint &position, const wxSize& size, long style)
: wxDialog(parent, id, title, position, size, style)
{
	clickedButton = NULL;
	CreateGUIControls();
}

ConfigDialog::~ConfigDialog()
{
} 

void ConfigDialog::CreateGUIControls()
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
	
	// Put notebook and standard buttons in sizers
	wxBoxSizer* sSButtons;
	sSButtons = new wxBoxSizer(wxHORIZONTAL);
	sSButtons->Add(0, 0, 1, wxEXPAND, 5);
	sSButtons->Add(m_Close, 0, wxALL, 5);
	
	wxBoxSizer* sMain;
	sMain = new wxBoxSizer(wxVERTICAL);
	sMain->Add(m_Notebook, 1, wxEXPAND|wxALL, 5);
	sMain->Add(sSButtons, 0, wxEXPAND, 5);
	
	this->SetSizer(sMain);
	this->Layout();

	wxArrayString arrayStringFor_DeviceName;

	for(int i = 0; i < 4; i++)
	{
		sDevice[i] = new wxStaticBoxSizer(wxVERTICAL, m_Controller[i], wxT("Controller:"));
		sDeviceTop[i] = new wxBoxSizer(wxHORIZONTAL);
		sDeviceBottom[i] = new wxBoxSizer(wxHORIZONTAL);
		m_DeviceName[i] = new wxChoice(m_Controller[i], ID_DEVICENAME, wxDefaultPosition, wxDefaultSize, arrayStringFor_DeviceName, 0, wxDefaultValidator);
		m_Attached[i] = new wxCheckBox(m_Controller[i], ID_ATTACHED, wxT("Controller attached"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
		m_Disable[i] = new wxCheckBox(m_Controller[i], ID_DISABLE, wxT("Disable when window looses focus"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
		m_Rumble[i] = new wxCheckBox(m_Controller[i], ID_RUMBLE, wxT("Enable rumble"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
		m_Attached[i]->SetValue(pad[i].attached);
		m_Disable[i]->SetValue(pad[i].disable);
		m_Rumble[i]->SetValue(pad[i].rumble);
		m_Rumble[i]->Show(pad[i].type);
		
		sDeviceTop[i]->Add(m_DeviceName[i], 1, wxEXPAND|wxALL, 1);
		sDeviceTop[i]->Add(m_Attached[i], 0, wxEXPAND|wxALL, 1);
		sDeviceBottom[i]->AddStretchSpacer(1);
		sDeviceBottom[i]->Add(m_Disable[i], 0, wxEXPAND|wxALL, 1);
		sDeviceBottom[i]->Add(m_Rumble[i], 0, wxEXPAND|wxALL, 1);
		sDeviceBottom[i]->AddStretchSpacer(1);
		sDevice[i]->Add(sDeviceTop[i], 0, wxEXPAND|wxALL, 1);
		sDevice[i]->Add(sDeviceBottom[i], 0, wxEXPAND|wxALL, 1);

		sButtons[i] = new wxStaticBoxSizer(wxVERTICAL, m_Controller[i], wxT("Buttons:"));
		m_ButtonA[i] = new wxButton(m_Controller[i], CTL_A, wxT("A: "), wxDefaultPosition, wxDefaultSize, wxBU_LEFT);
		m_ButtonB[i] = new wxButton(m_Controller[i], CTL_B, wxT("B: "), wxDefaultPosition, wxDefaultSize, wxBU_LEFT);
		m_ButtonX[i] = new wxButton(m_Controller[i], CTL_X, wxT("X: "), wxDefaultPosition, wxDefaultSize, wxBU_LEFT);
		m_ButtonY[i] = new wxButton(m_Controller[i], CTL_Y, wxT("Y: "), wxDefaultPosition, wxDefaultSize, wxBU_LEFT);
		m_ButtonZ[i] = new wxButton(m_Controller[i], CTL_Z, wxT("Z: "), wxDefaultPosition, wxDefaultSize, wxBU_LEFT);
		m_ButtonStart[i] = new wxButton(m_Controller[i], CTL_START, wxT("Start: "), wxDefaultPosition, wxDefaultSize, wxBU_LEFT);

		sButtons[i]->Add(m_ButtonA[i], 0, wxEXPAND|wxALL);
		sButtons[i]->Add(m_ButtonB[i], 0, wxEXPAND|wxALL);
		sButtons[i]->Add(m_ButtonX[i], 0, wxEXPAND|wxALL);
		sButtons[i]->Add(m_ButtonY[i], 0, wxEXPAND|wxALL);
		sButtons[i]->Add(m_ButtonZ[i], 0, wxEXPAND|wxALL);
		sButtons[i]->Add(m_ButtonStart[i], 0, wxEXPAND|wxALL);

		sTriggerL[i] = new wxStaticBoxSizer(wxVERTICAL, m_Controller[i], wxT("L Trigger:"));
		m_TriggerL[i] = new wxButton(m_Controller[i], CTL_TRIGGER_L, wxT("Analog: "), wxDefaultPosition, wxDefaultSize, wxBU_LEFT);
		m_ButtonL[i] = new wxButton(m_Controller[i], CTL_L, wxT("Click: "), wxDefaultPosition, wxDefaultSize, wxBU_LEFT);
		sTriggerR[i] = new wxStaticBoxSizer(wxVERTICAL, m_Controller[i], wxT("R Trigger:"));
		m_TriggerR[i] = new wxButton(m_Controller[i], CTL_TRIGGER_R, wxT("Analog: "), wxDefaultPosition, wxDefaultSize, wxBU_LEFT);
		m_ButtonR[i] = new wxButton(m_Controller[i], CTL_R, wxT("Click: "), wxDefaultPosition, wxDefaultSize, wxBU_LEFT);

		sTriggerL[i]->Add(m_TriggerL[i], 0, wxEXPAND|wxALL);
		sTriggerL[i]->Add(m_ButtonL[i], 0, wxEXPAND|wxALL);
		sTriggerR[i]->Add(m_TriggerR[i], 0, wxEXPAND|wxALL);
		sTriggerR[i]->Add(m_ButtonR[i], 0, wxEXPAND|wxALL);

		sStick[i] = new wxStaticBoxSizer(wxVERTICAL, m_Controller[i], wxT("Main Stick:"));
		m_StickUp[i] = new wxButton(m_Controller[i], CTL_MAINUP, wxT("Up: "), wxDefaultPosition, wxDefaultSize, wxBU_LEFT);
		m_StickDown[i] = new wxButton(m_Controller[i], CTL_MAINDOWN, wxT("Down: "), wxDefaultPosition, wxDefaultSize, wxBU_LEFT);
		m_StickLeft[i] = new wxButton(m_Controller[i], CTL_MAINLEFT, wxT("Left: "), wxDefaultPosition, wxDefaultSize, wxBU_LEFT);
		m_StickRight[i] = new wxButton(m_Controller[i], CTL_MAINRIGHT, wxT("Right: "), wxDefaultPosition, wxDefaultSize, wxBU_LEFT);

		sStick[i]->Add(m_StickUp[i], 0, wxEXPAND|wxALL);
		sStick[i]->Add(m_StickDown[i], 0, wxEXPAND|wxALL);
		sStick[i]->Add(m_StickLeft[i], 0, wxEXPAND|wxALL);
		sStick[i]->Add(m_StickRight[i], 0, wxEXPAND|wxALL);

		sDPad[i] = new wxStaticBoxSizer(wxVERTICAL, m_Controller[i], wxT("D-Pad:"));
		m_DPadUp[i] = new wxButton(m_Controller[i], CTL_DPADUP, wxT("Up: "), wxDefaultPosition, wxDefaultSize, wxBU_LEFT);
		m_DPadDown[i] = new wxButton(m_Controller[i], CTL_DPADDOWN, wxT("Down: "), wxDefaultPosition, wxDefaultSize, wxBU_LEFT);
		m_DPadLeft[i] = new wxButton(m_Controller[i], CTL_DPADLEFT, wxT("Left: "), wxDefaultPosition, wxDefaultSize, wxBU_LEFT);
		m_DPadRight[i] = new wxButton(m_Controller[i], CTL_DPADRIGHT, wxT("Right: "), wxDefaultPosition, wxDefaultSize, wxBU_LEFT);

		sDPad[i]->Add(m_DPadUp[i], 0, wxEXPAND|wxALL);
		sDPad[i]->Add(m_DPadDown[i], 0, wxEXPAND|wxALL);
		sDPad[i]->Add(m_DPadLeft[i], 0, wxEXPAND|wxALL);
		sDPad[i]->Add(m_DPadRight[i], 0, wxEXPAND|wxALL);

		sCStick[i] = new wxStaticBoxSizer(wxVERTICAL, m_Controller[i], wxT("C-Stick:"));
		m_CStickUp[i] = new wxButton(m_Controller[i], CTL_SUBUP, wxT("Up: "), wxDefaultPosition, wxDefaultSize, wxBU_LEFT);
		m_CStickDown[i] = new wxButton(m_Controller[i], CTL_SUBDOWN, wxT("Down: "), wxDefaultPosition, wxDefaultSize, wxBU_LEFT);
		m_CStickLeft[i] = new wxButton(m_Controller[i], CTL_SUBLEFT, wxT("Left: "), wxDefaultPosition, wxDefaultSize, wxBU_LEFT);
		m_CStickRight[i] = new wxButton(m_Controller[i], CTL_SUBRIGHT, wxT("Right: "), wxDefaultPosition, wxDefaultSize, wxBU_LEFT);

		sCStick[i]->Add(m_CStickUp[i], 0, wxEXPAND|wxALL);
		sCStick[i]->Add(m_CStickDown[i], 0, wxEXPAND|wxALL);
		sCStick[i]->Add(m_CStickLeft[i], 0, wxEXPAND|wxALL);
		sCStick[i]->Add(m_CStickRight[i], 0, wxEXPAND|wxALL);

		sPage[i] = new wxGridBagSizer(0, 0);
		sPage[i]->SetFlexibleDirection(wxBOTH);
		sPage[i]->SetNonFlexibleGrowMode(wxFLEX_GROWMODE_SPECIFIED);
		sPage[i]->Add(sDevice[i], wxGBPosition(0, 0), wxGBSpan(1, 5), wxEXPAND|wxALL, 1);
		sPage[i]->Add(sButtons[i], wxGBPosition(1, 0), wxGBSpan(3, 1), wxALL, 1);
		sPage[i]->Add(sTriggerL[i], wxGBPosition(1, 1), wxGBSpan(1, 1), wxALL, 1);
		sPage[i]->Add(sTriggerR[i], wxGBPosition(2, 1), wxGBSpan(1, 1), wxALL, 1);
		sPage[i]->Add(sStick[i], wxGBPosition(1, 2), wxGBSpan(2, 1), wxALL, 1);
		sPage[i]->Add(sDPad[i], wxGBPosition(1, 3), wxGBSpan(2, 1), wxALL, 1);
		sPage[i]->Add(sCStick[i], wxGBPosition(1, 4), wxGBSpan(2, 1), wxALL, 1);
		m_Controller[i]->SetSizer(sPage[i]);
		sPage[i]->Layout();
	}

	clickedButton->Connect(wxID_ANY, wxEVT_KEY_DOWN,
                               wxKeyEventHandler(ConfigDialog::OnKeyDown),
                               (wxObject*)NULL, this);
	SetIcon(wxNullIcon);
	Fit();
}

void ConfigDialog::OnClose(wxCloseEvent& event)
{
	EndModal(0);
}
void ConfigDialog::OnKeyDown(wxKeyEvent& event)
{
	if(clickedButton != NULL)
	{
		int page = m_Notebook->GetSelection();
		pad[page].keyForControl[clickedButton->GetId()] = event.GetKeyCode();
		clickedButton->SetLabel(wxString::Format(wxT("%i"), event.GetKeyCode()));
	}
	//this is here to see if the event gets processed at all...so far, it doesn't
	m_ButtonA[0]->SetLabel(wxString::Format(wxT("%i"), event.GetKeyCode()));
	//clickedButton = NULL;
	event.Skip();
}
void ConfigDialog::OnCloseClick(wxCommandEvent& event)
{
	Close();
}

void ConfigDialog::AttachedCheck(wxCommandEvent& event)
{
	int page = m_Notebook->GetSelection();
	pad[page].attached = m_Attached[page]->GetValue();
}

void ConfigDialog::DisableCheck(wxCommandEvent& event)
{
	int page = m_Notebook->GetSelection();
	pad[page].disable = m_Disable[page]->GetValue();
}

void ConfigDialog::RumbleCheck(wxCommandEvent& event)
{
	int page = m_Notebook->GetSelection();
	pad[page].rumble = m_Rumble[page]->GetValue();
}

void ConfigDialog::OnButtonClick(wxCommandEvent& event)
{
	clickedButton = (wxButton *)event.GetEventObject();
	//wxString oldLabel = clickedButton->GetLabel();
	clickedButton->SetLabel(wxString::FromAscii("Press Key"));

	//clickedButton->SetLabel(wxString::Format(wxT("%i"), keyPress));

	//clickedButton->SetLabel(wxString::Format(wxT("%s  %i"), oldLabel, keyPress));

}
