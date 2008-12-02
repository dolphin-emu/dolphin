//////////////////////////////////////////////////////////////////////////////////////////
// Project description
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
// Name: nJoy 
// Description: A Dolphin Compatible Input Plugin
//
// Author: Falcon4ever (nJoy@falcon4ever.com)
// Site: www.multigesture.net
// Copyright (C) 2003-2008 Dolphin Project.
//
//////////////////////////////////////////////////////////////////////////////////////////
//
// Licensetype: GNU General Public License (GPL)
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.
//
// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/
//
// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/
//
//////////////////////////////////////////////////////////////////////////////////////////

#include "ConfigBox.h"
#include "../nJoy.h"
#include "Images/controller.xpm"

extern CONTROLLER_INFO	*joyinfo;
extern CONTROLLER_MAPPING joysticks[4];
extern bool emulator_running;

static const char* ControllerType[] =
{
	"Joystick (default)",
	"Joystick (no hat)",
//	"Joytstick (xbox360)",	// Shoulder buttons -> axis
//	"Keyboard"				// Not supported yet, sorry F|RES ;( ...
};

BEGIN_EVENT_TABLE(ConfigBox,wxDialog)
	EVT_CLOSE(ConfigBox::OnClose)
	EVT_BUTTON(ID_ABOUT, ConfigBox::AboutClick)
	EVT_BUTTON(ID_OK, ConfigBox::OKClick)
	EVT_BUTTON(ID_CANCEL, ConfigBox::CancelClick)
	EVT_COMBOBOX(IDC_JOYNAME, ConfigBox::ChangeJoystick)
	EVT_COMBOBOX(IDC_CONTROLTYPE, ConfigBox::ChangeControllertype)
	EVT_NOTEBOOK_PAGE_CHANGED(ID_NOTEBOOK, ConfigBox::NotebookPageChanged)

	EVT_BUTTON(IDB_SHOULDER_L, ConfigBox::GetInputs)
	EVT_BUTTON(IDB_SHOULDER_R, ConfigBox::GetInputs)
	EVT_BUTTON(IDB_BUTTON_A, ConfigBox::GetInputs)
	EVT_BUTTON(IDB_BUTTON_B, ConfigBox::GetInputs)
	EVT_BUTTON(IDB_BUTTON_X, ConfigBox::GetInputs)
	EVT_BUTTON(IDB_BUTTON_Y, ConfigBox::GetInputs)
	EVT_BUTTON(IDB_BUTTON_Z, ConfigBox::GetInputs)
	EVT_BUTTON(IDB_BUTTONSTART, ConfigBox::GetInputs)
	EVT_BUTTON(IDB_BUTTONHALFPRESS, ConfigBox::GetInputs)
	EVT_BUTTON(IDB_DPAD_UP, ConfigBox::GetInputs)
	EVT_BUTTON(IDB_DPAD_DOWN, ConfigBox::GetInputs)
	EVT_BUTTON(IDB_DPAD_LEFT, ConfigBox::GetInputs)
	EVT_BUTTON(IDB_DPAD_RIGHT, ConfigBox::GetInputs)

	EVT_BUTTON(IDB_ANALOG_MAIN_X, ConfigBox::GetInputs)
	EVT_BUTTON(IDB_ANALOG_MAIN_Y, ConfigBox::GetInputs)
	EVT_BUTTON(IDB_ANALOG_SUB_X, ConfigBox::GetInputs)
	EVT_BUTTON(IDB_ANALOG_SUB_Y, ConfigBox::GetInputs)
END_EVENT_TABLE()

ConfigBox::ConfigBox(wxWindow *parent, wxWindowID id, const wxString &title, const wxPoint &position, const wxSize& size, long style)
: wxDialog(parent, id, title, position, size, style)
{
	notebookpage = 0;
	CreateGUIControls();
}

ConfigBox::~ConfigBox()
{
	// empty
} 

// Warning: horrible code below proceed at own risk!
void ConfigBox::CreateGUIControls()
{
	#ifndef _DEBUG		
		SetTitle(wxT("Configure: nJoy v"INPUT_VERSION" Input Plugin"));
	#else			
		SetTitle(wxT("Configure: nJoy v"INPUT_VERSION" (Debug) Input Plugin"));
	#endif
	
	SetIcon(wxNullIcon);	
	SetClientSize(637, 527);
	Center();

#ifndef _WIN32
	// Force a 8pt font so that it looks more or less "correct" regardless of the default font setting
	wxFont f(8,wxFONTFAMILY_DEFAULT,wxFONTSTYLE_NORMAL,wxFONTWEIGHT_NORMAL);
	SetFont(f);
#endif

	// Buttons
	m_About = new wxButton(this, ID_ABOUT, wxT("About"), wxPoint(5, 497), wxSize(75, 25), 0, wxDefaultValidator, wxT("About"));
	m_OK = new wxButton(this, ID_OK, wxT("OK"), wxPoint(475, 497), wxSize(75, 25), 0, wxDefaultValidator, wxT("OK"));
	m_Cancel = new wxButton(this, ID_CANCEL, wxT("Cancel"), wxPoint(556, 497), wxSize(75, 25), 0, wxDefaultValidator, wxT("Cancel"));

	// Notebook
	m_Notebook = new wxNotebook(this, ID_NOTEBOOK, wxPoint(6, 7),wxSize(625, 484));
	
	// Controller pages
	m_Controller[0] = new wxPanel(m_Notebook, ID_CONTROLLERPAGE1, wxPoint(0, 0), wxSize(600, 400));
	m_Notebook->AddPage(m_Controller[0], wxT("Controller 1"));
	m_Controller[1] = new wxPanel(m_Notebook, ID_CONTROLLERPAGE2, wxPoint(0, 0), wxSize(600, 400));
	m_Notebook->AddPage(m_Controller[1], wxT("Controller 2"));
	m_Controller[2] = new wxPanel(m_Notebook, ID_CONTROLLERPAGE3, wxPoint(0, 0), wxSize(600, 400));
	m_Notebook->AddPage(m_Controller[2], wxT("Controller 3"));
	m_Controller[3] = new wxPanel(m_Notebook, ID_CONTROLLERPAGE4, wxPoint(0, 0), wxSize(600, 400));
	m_Notebook->AddPage(m_Controller[3], wxT("Controller 4"));
	
	// Add controls
	wxArrayString arrayStringFor_Joyname;
	wxArrayString arrayStringFor_Controltype;
	wxArrayString arrayStringFor_Deadzone;
	
	// Search for devices and add the to the device list	
	if(Search_Devices())
	{
		for(int x = 0; x < SDL_NumJoysticks(); x++)
		{
			arrayStringFor_Joyname.Add(wxString::FromAscii(joyinfo[x].Name));
		}
	}
	else
	{
		arrayStringFor_Joyname.Add(wxString::FromAscii("No Joystick detected!"));
	}

	arrayStringFor_Controltype.Add(wxString::FromAscii(ControllerType[CTL_TYPE_JOYSTICK]));
	arrayStringFor_Controltype.Add(wxString::FromAscii(ControllerType[CTL_TYPE_JOYSTICK_NO_HAT]));
	// arrayStringFor_Controltype.Add(wxString::FromAscii(ControllerType[CTL_TYPE_JOYSTICK_XBOX360]));
	// arrayStringFor_Controltype.Add(wxString::FromAscii(ControllerType[CTL_TYPE_KEYBOARD]));

	char buffer [8];
	for(int x = 1; x <= 100; x++)
	{		
		sprintf (buffer, "%d %%", x);
		arrayStringFor_Deadzone.Add(wxString::FromAscii(buffer));
	}

	wxBitmap WxStaticBitmap1_BITMAP(ConfigBox_WxStaticBitmap1_XPM);

	for(int i=0; i<4 ;i++)
	{	

		// Groups
		m_gJoyname[i] = new wxStaticBox(m_Controller[i], IDG_JOYSTICK, wxT("Controller:"), wxPoint(5, 11), wxSize(608, 46));
		
		#ifdef _WIN32
		m_Joyattach[i] = new wxCheckBox(m_Controller[i], IDC_JOYATTACH, wxT("Controller attached"), wxPoint(495, 26), wxSize(109, 25), 0, wxDefaultValidator, wxT("Controller attached"));
		m_Joyname[i] = new wxComboBox(m_Controller[i], IDC_JOYNAME, wxEmptyString, wxPoint(12, 29), wxSize(476, 21), arrayStringFor_Joyname, 0, wxDefaultValidator, wxT("m_Joyname"));
		m_gExtrasettings[i] = new wxStaticBox(m_Controller[i], IDG_EXTRASETTINGS, wxT("Extra settings:"), wxPoint(104, 385), wxSize(155, 69));
		m_Deadzone[i] = new wxComboBox(m_Controller[i], IDC_DEADZONE, wxEmptyString, wxPoint(167, 400), wxSize(59, 21), arrayStringFor_Deadzone, 0, wxDefaultValidator, wxT("m_Deadzone"));
		m_gControllertype[i] = new wxStaticBox(m_Controller[i], IDG_CONTROLLERTYPE, wxT("Controller type:"), wxPoint(359, 383), wxSize(143, 44));
		m_Controltype[i] = new wxComboBox(m_Controller[i], IDC_CONTROLTYPE, wxEmptyString, wxPoint(366, 401), wxSize(131, 21), arrayStringFor_Controltype, 0, wxDefaultValidator, wxT("m_Controltype"));
		#else
		m_Joyattach[i] = new wxCheckBox(m_Controller[i], IDC_JOYATTACH, wxT("Controller attached"), wxPoint(470, 26), wxSize(140, 25), 0, wxDefaultValidator, wxT("Controller attached"));
		m_Joyname[i] = new wxComboBox(m_Controller[i], IDC_JOYNAME, wxEmptyString, wxPoint(10, 25), wxSize(450, 25), arrayStringFor_Joyname, 0, wxDefaultValidator, wxT("m_Joyname"));
		m_gExtrasettings[i] = new wxStaticBox(m_Controller[i], IDG_EXTRASETTINGS, wxT("Extra settings:"), wxPoint(100, 385), wxSize(155, 65));
		m_Deadzone[i] = new wxComboBox(m_Controller[i], IDC_DEADZONE, wxEmptyString, wxPoint(167, 398), wxSize(80, 25), arrayStringFor_Deadzone, 0, wxDefaultValidator, wxT("m_Deadzone"));
		m_gControllertype[i] = new wxStaticBox(m_Controller[i], IDG_CONTROLLERTYPE, wxT("Controller type:"), wxPoint(359, 383), wxSize(160, 44));
		m_Controltype[i] = new wxComboBox(m_Controller[i], IDC_CONTROLTYPE, wxEmptyString, wxPoint(364, 396), wxSize(150, 25), arrayStringFor_Controltype, 0, wxDefaultValidator, wxT("m_Controltype"));
		#endif
					
		// GUI left side buttons
		m_JoyShoulderL[i] = new wxTextCtrl(m_Controller[i], ID_SHOULDER_L, wxT("0"), wxPoint(6, 80), wxSize(59, 19), wxTE_READONLY | wxTE_CENTRE, wxDefaultValidator, wxT("0"));
		m_JoyShoulderL[i]->Enable(false);
		m_JoyAnalogMainX[i] = new wxTextCtrl(m_Controller[i], ID_ANALOG_MAIN_X, wxT("0"), wxPoint(6, 218), wxSize(59, 19), wxTE_READONLY | wxTE_CENTRE, wxDefaultValidator, wxT("0"));
		m_JoyAnalogMainX[i]->Enable(false);
		m_JoyAnalogMainY[i] = new wxTextCtrl(m_Controller[i], ID_ANALOG_MAIN_Y, wxT("0"), wxPoint(6, 255), wxSize(59, 19), wxTE_READONLY | wxTE_CENTRE, wxDefaultValidator, wxT("0"));
		m_JoyAnalogMainY[i]->Enable(false);
		m_JoyDpadUp[i] = new wxTextCtrl(m_Controller[i], ID_DPAD_UP, wxT("0"), wxPoint(6, 296), wxSize(59, 19), wxTE_READONLY | wxTE_CENTRE, wxDefaultValidator, wxT("0"));
		m_JoyDpadUp[i]->Enable(false);
		m_JoyDpadDown[i] = new wxTextCtrl(m_Controller[i], ID_DPAD_DOWN, wxT("0"), wxPoint(6, 333), wxSize(59, 19), wxTE_READONLY | wxTE_CENTRE, wxDefaultValidator, wxT("0"));
		m_JoyDpadDown[i]->Enable(false);
		m_JoyDpadLeft[i] = new wxTextCtrl(m_Controller[i], ID_DPAD_LEFT, wxT("0"), wxPoint(6, 369), wxSize(59, 19), wxTE_READONLY | wxTE_CENTRE, wxDefaultValidator, wxT("0"));
		m_JoyDpadLeft[i]->Enable(false);
		m_JoyDpadRight[i] = new wxTextCtrl(m_Controller[i], ID_DPAD_RIGHT, wxT("0"), wxPoint(6, 406), wxSize(59, 19), wxTE_READONLY | wxTE_CENTRE, wxDefaultValidator, wxT("0"));
		m_JoyDpadRight[i]->Enable(false);
		m_bJoyShoulderL[i] = new wxButton(m_Controller[i], IDB_SHOULDER_L, wxEmptyString, wxPoint(70, 82), wxSize(21, 14), 0, wxDefaultValidator, wxEmptyString);
		m_bJoyAnalogMainX[i] = new wxButton(m_Controller[i], IDB_ANALOG_MAIN_X, wxEmptyString, wxPoint(70, 220), wxSize(21, 14), 0, wxDefaultValidator, wxEmptyString);
		m_bJoyAnalogMainY[i] = new wxButton(m_Controller[i], IDB_ANALOG_MAIN_Y, wxEmptyString, wxPoint(70, 257), wxSize(21, 14), 0, wxDefaultValidator, wxEmptyString);
		m_bJoyDpadUp[i] = new wxButton(m_Controller[i], IDB_DPAD_UP, wxEmptyString, wxPoint(70, 298), wxSize(21, 14), 0, wxDefaultValidator, wxEmptyString);
		m_bJoyDpadDown[i] = new wxButton(m_Controller[i], IDB_DPAD_DOWN, wxEmptyString, wxPoint(70, 335), wxSize(21, 14), 0, wxDefaultValidator, wxEmptyString);
		m_bJoyDpadLeft[i] = new wxButton(m_Controller[i], IDB_DPAD_LEFT, wxEmptyString, wxPoint(70, 371), wxSize(21, 14), 0, wxDefaultValidator, wxEmptyString);
		m_bJoyDpadRight[i] = new wxButton(m_Controller[i], IDB_DPAD_RIGHT, wxEmptyString, wxPoint(70, 408), wxSize(21, 14), 0, wxDefaultValidator, wxEmptyString);
		
		m_textMainX[i] = new wxStaticText(m_Controller[i], IDT_ANALOG_MAIN_X, wxT("X-axis"), wxPoint(6, 204), wxDefaultSize, 0, wxT("X-axis"));
		m_textMainY[i] = new wxStaticText(m_Controller[i], IDT_ANALOG_MAIN_Y, wxT("Y-axis"), wxPoint(6, 241), wxDefaultSize, 0, wxT("Y-axis"));
		m_textDpadUp[i] = new wxStaticText(m_Controller[i], IDT_DPAD_UP, wxT("Up"), wxPoint(6, 282), wxDefaultSize, 0, wxT("Up"));
		m_textDpadDown[i] = new wxStaticText(m_Controller[i], IDT_DPAD_DOWN, wxT("Down"), wxPoint(6, 319), wxDefaultSize, 0, wxT("Down"));
		m_textDpadLeft[i] = new wxStaticText(m_Controller[i], IDT_DPAD_LEFT, wxT("Left"), wxPoint(6, 354), wxDefaultSize, 0, wxT("Left"));
		m_textDpadRight[i] = new wxStaticText(m_Controller[i], IDT_DPAD_RIGHT, wxT("Right"), wxPoint(6, 391), wxDefaultSize, 0, wxT("Right"));
		
		// GUI right side buttons
		m_JoyShoulderR[i] = new wxTextCtrl(m_Controller[i], ID_SHOULDER_R, wxT("0"), wxPoint(552, 106), wxSize(59, 19), wxTE_READONLY | wxTE_CENTRE, wxDefaultValidator, wxT("0"));
		m_JoyShoulderR[i]->Enable(false);
		m_JoyButtonA[i] = new wxTextCtrl(m_Controller[i], ID_BUTTON_A, wxT("0"), wxPoint(552, 280), wxSize(59, 19), wxTE_READONLY | wxTE_CENTRE, wxDefaultValidator, wxT("0"));
		m_JoyButtonA[i]->Enable(false);
		m_JoyButtonB[i] = new wxTextCtrl(m_Controller[i], ID_BUTTON_B, wxT("0"), wxPoint(552, 80), wxSize(59, 19), wxTE_READONLY | wxTE_CENTRE, wxDefaultValidator, wxT("0"));
		m_JoyButtonB[i]->Enable(false);
		m_JoyButtonX[i] = new wxTextCtrl(m_Controller[i], ID_BUTTON_X, wxT("0"), wxPoint(552, 242), wxSize(59, 19), wxTE_READONLY | wxTE_CENTRE, wxDefaultValidator, wxT("0"));
		m_JoyButtonX[i]->Enable(false);
		m_JoyButtonY[i] = new wxTextCtrl(m_Controller[i], ID_BUTTON_Y, wxT("0"), wxPoint(552, 171), wxSize(59, 19), wxTE_READONLY | wxTE_CENTRE, wxDefaultValidator, wxT("0"));
		m_JoyButtonY[i]->Enable(false);
		m_JoyButtonZ[i] = new wxTextCtrl(m_Controller[i], ID_BUTTON_Z, wxT("0"), wxPoint(552, 145), wxSize(59, 19), wxTE_READONLY | wxTE_CENTRE, wxDefaultValidator, wxT("0"));
		m_JoyButtonZ[i]->Enable(false);
		m_JoyAnalogSubX[i] = new wxTextCtrl(m_Controller[i], ID_ANALOG_SUB_X, wxT("0"), wxPoint(552, 351), wxSize(59, 19), wxTE_READONLY | wxTE_CENTRE, wxDefaultValidator, wxT("0"));
		m_JoyAnalogSubX[i]->Enable(false);
		m_JoyAnalogSubY[i] = new wxTextCtrl(m_Controller[i], ID_ANALOG_SUB_Y, wxT("0"), wxPoint(552, 388), wxSize(59, 19), wxTE_READONLY | wxTE_CENTRE, wxDefaultValidator, wxT("0"));
		m_JoyAnalogSubY[i]->Enable(false);
		m_bJoyShoulderR[i] = new wxButton(m_Controller[i], IDB_SHOULDER_R, wxEmptyString, wxPoint(526, 108), wxSize(21, 14), 0, wxDefaultValidator, wxEmptyString);
		m_bJoyButtonA[i] = new wxButton(m_Controller[i], IDB_BUTTON_A, wxEmptyString, wxPoint(526, 282), wxSize(21, 14), 0, wxDefaultValidator, wxEmptyString);
		m_bJoyButtonB[i] = new wxButton(m_Controller[i], IDB_BUTTON_B, wxEmptyString, wxPoint(526, 82), wxSize(21, 14), 0, wxDefaultValidator, wxEmptyString);
		m_bJoyButtonX[i] = new wxButton(m_Controller[i], IDB_BUTTON_X, wxEmptyString, wxPoint(526, 244), wxSize(21, 14), 0, wxDefaultValidator, wxEmptyString);
		m_bJoyButtonY[i] = new wxButton(m_Controller[i], IDB_BUTTON_Y, wxEmptyString, wxPoint(526, 173), wxSize(21, 14), 0, wxDefaultValidator, wxEmptyString);
		m_bJoyButtonZ[i] = new wxButton(m_Controller[i], IDB_BUTTON_Z, wxEmptyString, wxPoint(526, 147), wxSize(21, 14), 0, wxDefaultValidator, wxEmptyString);
		m_bJoyAnalogSubX[i] = new wxButton(m_Controller[i], IDB_ANALOG_SUB_X, wxEmptyString, wxPoint(526, 353), wxSize(21, 14), 0, wxDefaultValidator, wxEmptyString);
		m_bJoyAnalogSubY[i] = new wxButton(m_Controller[i], IDB_ANALOG_SUB_Y, wxEmptyString, wxPoint(526, 390), wxSize(21, 14), 0, wxDefaultValidator, wxEmptyString);
		
		m_textSubX[i] = new wxStaticText(m_Controller[i], IDT_ANALOG_SUB_X, wxT("X-axis"), wxPoint(552, 336), wxDefaultSize, 0, wxT("X-axis"));
		m_textSubY[i] = new wxStaticText(m_Controller[i], IDT_ANALOG_SUB_Y, wxT("Y-axis"), wxPoint(552, 373), wxDefaultSize, 0, wxT("Y-axis"));
		
		// GUI center button
		m_JoyButtonStart[i] = new wxTextCtrl(m_Controller[i], ID_BUTTONSTART, wxT("0"), wxPoint(278, 403), wxSize(59, 19), wxTE_READONLY | wxTE_CENTRE, wxDefaultValidator, wxT("0"));
		m_JoyButtonStart[i]->Enable(false);
		m_JoyButtonHalfpress[i] = new wxTextCtrl(m_Controller[i], ID_BUTTONHALFPRESS, wxT("0"), wxPoint(167, 424), wxSize(59, 19), wxTE_READONLY | wxTE_CENTRE, wxDefaultValidator, wxT("0"));
		m_JoyButtonHalfpress[i]->Enable(false);
		m_bJoyButtonStart[i] = new wxButton(m_Controller[i], IDB_BUTTONSTART, wxEmptyString, wxPoint(297, 385), wxSize(21, 14), 0, wxDefaultValidator, wxEmptyString);
		m_bJoyButtonHalfpress[i] = new wxButton(m_Controller[i], IDB_BUTTONHALFPRESS, wxEmptyString, wxPoint(231, 426), wxSize(21, 14), 0, wxDefaultValidator, wxEmptyString);
		
		#ifdef _WIN32
		m_textDeadzone[i] = new wxStaticText(m_Controller[i], IDT_DEADZONE, wxT("Deadzone"), wxPoint(116, 404), wxDefaultSize, 0, wxT("Deadzone"));		
		m_textHalfpress[i] = new wxStaticText(m_Controller[i], IDT_BUTTONHALFPRESS, wxT("Half press"), wxPoint(116, 428), wxDefaultSize, 0, wxT("Half press"));
		m_textWebsite[i] = new wxStaticText(m_Controller[i], IDT_WEBSITE, wxT("www.multigesture.net"), wxPoint(500, 438), wxDefaultSize, 0, wxT("www.multigesture.net"));
		#else
		m_textDeadzone[i] = new wxStaticText(m_Controller[i], IDT_DEADZONE, wxT("Deadzone"), wxPoint(105, 404), wxDefaultSize, 0, wxT("Deadzone"));		
		m_textHalfpress[i] = new wxStaticText(m_Controller[i], IDT_BUTTONHALFPRESS, wxT("Half press"), wxPoint(105, 428), wxDefaultSize, 0, wxT("Half press"));
		m_textWebsite[i] = new wxStaticText(m_Controller[i], IDT_WEBSITE, wxT("www.multigesture.net"), wxPoint(480, 438), wxDefaultSize, 0, wxT("www.multigesture.net"));
		#endif		
		
		// TODO: Controller image
		// Placeholder instead of bitmap
		// m_PlaceholderBMP[i] = new wxTextCtrl(m_Controller[i], ID_CONTROLLERPICTURE, wxT("BITMAP HERE PLZ KTHX!"), wxPoint(98, 75), wxSize(423, 306), wxTE_READONLY | wxTE_CENTRE, wxDefaultValidator, wxT("BITMAP HERE PLZ KTHX!"));
		// m_PlaceholderBMP[i]->Enable(false);
		
		// You can enable the bitmap here:
		// But it loads überslow on init... (only in windows, linux seems to load it fast!)
		// AAaaand the XPM file (256 colours) looks crappier than the real bitmap... so maybe we can find a way to use a bitmap?	
		m_controllerimage[i] = new wxStaticBitmap(m_Controller[i], ID_CONTROLLERPICTURE, WxStaticBitmap1_BITMAP, wxPoint(98, 75), wxSize(421,304));		
		
		if(emulator_running)
		{
			m_Joyname[i]->Enable(false);
			m_Joyattach[i]->Enable(false);
			m_Controltype[i]->Enable(false);			
		}
		
		SetControllerAll(i);
	}
}

void ConfigBox::OnClose(wxCloseEvent& /*event*/)
{
	EndModal(0);
}

void ConfigBox::AboutClick(wxCommandEvent& event)
{
// Call about dialog
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
#ifdef _WIN32
	wxWindow win;
	win.SetHWND((WXHWND)this->GetHWND());
	win.Enable(false);  

	AboutBox frame(&win);
	frame.ShowModal();

	win.Enable(true);
	win.SetHWND(0); 
#else
	AboutBox frame(NULL);
	frame.ShowModal();
#endif	
}

void ConfigBox::OKClick(wxCommandEvent& event)
{
	if (event.GetId() == ID_OK)
	{
		for(int i=0; i<4 ;i++)
			GetControllerAll(i);
		SaveConfig();	// save settings
		LoadConfig();	// reload settings
		Close(); 
	}
}

void ConfigBox::CancelClick(wxCommandEvent& event)
{
	if (event.GetId() == ID_CANCEL)
	{
		LoadConfig();	// reload settings
		Close(); 
	}
}

// Set dialog items
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
void ConfigBox::SetControllerAll(int controller)
{	
	// http://wiki.wxwidgets.org/Converting_everything_to_and_from_wxString
	wxString tmp;
	m_Joyname[controller]->SetSelection(joysticks[controller].ID);

	m_JoyShoulderL[controller]->SetValue(wxString::FromAscii(joysticks[controller].buttons[CTL_L_SHOULDER].c_str()));
	m_JoyShoulderR[controller]->SetValue(wxString::FromAscii(joysticks[controller].buttons[CTL_R_SHOULDER].c_str()));

	m_JoyButtonA[controller]->SetValue(wxString::FromAscii(joysticks[controller].buttons[CTL_A_BUTTON].c_str())); 
	m_JoyButtonB[controller]->SetValue(wxString::FromAscii(joysticks[controller].buttons[CTL_B_BUTTON].c_str())); 
	m_JoyButtonX[controller]->SetValue(wxString::FromAscii(joysticks[controller].buttons[CTL_X_BUTTON].c_str()));
	m_JoyButtonY[controller]->SetValue(wxString::FromAscii(joysticks[controller].buttons[CTL_Y_BUTTON].c_str())); 
	m_JoyButtonZ[controller]->SetValue(wxString::FromAscii(joysticks[controller].buttons[CTL_Z_TRIGGER].c_str())); 

	m_JoyButtonStart[controller]->SetValue(wxString::FromAscii(joysticks[controller].buttons[CTL_START].c_str())); 
	tmp << joysticks[controller].halfpress; m_JoyButtonHalfpress[controller]->SetValue(tmp); tmp.clear();

	m_JoyAnalogMainX[controller]->SetValue(wxString::FromAscii(joysticks[controller].buttons[CTL_MAIN_X].c_str()));
	m_JoyAnalogMainY[controller]->SetValue(wxString::FromAscii(joysticks[controller].buttons[CTL_MAIN_Y].c_str()));
	m_JoyAnalogSubX[controller]->SetValue(wxString::FromAscii(joysticks[controller].buttons[CTL_SUB_X].c_str()));
	m_JoyAnalogSubY[controller]->SetValue(wxString::FromAscii(joysticks[controller].buttons[CTL_SUB_Y].c_str())); 

	if(joysticks[controller].enabled)
		m_Joyattach[controller]->SetValue(TRUE);
	else
		m_Joyattach[controller]->SetValue(FALSE);
	
	m_Controltype[controller]->SetSelection(joysticks[controller].controllertype);
	m_Deadzone[controller]->SetSelection(joysticks[controller].deadzone);

	UpdateVisibleItems(controller);

	if(joysticks[controller].controllertype == CTL_TYPE_JOYSTICK)
	{
		tmp << joysticks[controller].dpad; m_JoyDpadUp[controller]->SetValue(tmp); tmp.clear();		
	}
	else
	{
		tmp << joysticks[controller].dpad2[CTL_D_PAD_UP]; m_JoyDpadUp[controller]->SetValue(tmp); tmp.clear();
		tmp << joysticks[controller].dpad2[CTL_D_PAD_DOWN]; m_JoyDpadDown[controller]->SetValue(tmp); tmp.clear();
		tmp << joysticks[controller].dpad2[CTL_D_PAD_LEFT]; m_JoyDpadLeft[controller]->SetValue(tmp); tmp.clear();
		tmp << joysticks[controller].dpad2[CTL_D_PAD_RIGHT]; m_JoyDpadRight[controller]->SetValue(tmp); tmp.clear();
	}	
}

// Get dialog items
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
void ConfigBox::GetControllerAll(int controller)
{
	wxString tmp;
	char value[64];
	long lvalue;

	joysticks[controller].ID = m_Joyname[controller]->GetSelection();

	joysticks[controller].buttons[CTL_L_SHOULDER] = std::string(m_JoyShoulderL[controller]->GetValue().mb_str()); tmp.clear();
	joysticks[controller].buttons[CTL_R_SHOULDER] = std::string(m_JoyShoulderR[controller]->GetValue().mb_str()); tmp.clear();

	joysticks[controller].buttons[CTL_A_BUTTON] = std::string(m_JoyButtonA[controller]->GetValue().mb_str()); tmp.clear();
	joysticks[controller].buttons[CTL_B_BUTTON] = std::string(m_JoyButtonB[controller]->GetValue().mb_str()); tmp.clear();
	joysticks[controller].buttons[CTL_X_BUTTON] = std::string(m_JoyButtonX[controller]->GetValue().mb_str()); tmp.clear();
	joysticks[controller].buttons[CTL_Y_BUTTON] = std::string(m_JoyButtonY[controller]->GetValue().mb_str()); tmp.clear();
	joysticks[controller].buttons[CTL_Z_TRIGGER] = std::string(m_JoyButtonZ[controller]->GetValue().mb_str()); tmp.clear();
	joysticks[controller].buttons[CTL_START] = std::string(m_JoyButtonStart[controller]->GetValue().mb_str()); tmp.clear();

	m_JoyButtonHalfpress[controller]->GetValue().ToLong(&lvalue); joysticks[controller].halfpress = lvalue; tmp.clear();

	if(joysticks[controller].controllertype == CTL_TYPE_JOYSTICK)
	{
		m_JoyDpadUp[controller]->GetValue().ToLong(&lvalue); joysticks[controller].dpad = lvalue; tmp.clear();
	}
	else
	{
		m_JoyDpadUp[controller]->GetValue().ToLong(&lvalue); joysticks[controller].dpad2[CTL_D_PAD_UP] = lvalue; tmp.clear();
		m_JoyDpadDown[controller]->GetValue().ToLong(&lvalue); joysticks[controller].dpad2[CTL_D_PAD_DOWN] = lvalue; tmp.clear();
		m_JoyDpadLeft[controller]->GetValue().ToLong(&lvalue); joysticks[controller].dpad2[CTL_D_PAD_LEFT] = lvalue; tmp.clear();
		m_JoyDpadRight[controller]->GetValue().ToLong(&lvalue); joysticks[controller].dpad2[CTL_D_PAD_RIGHT] = lvalue; tmp.clear(); 	
	}

	joysticks[controller].buttons[CTL_MAIN_X] = std::string(m_JoyAnalogMainX[controller]->GetValue().mb_str()); tmp.clear();
	joysticks[controller].buttons[CTL_MAIN_Y] = std::string(m_JoyAnalogMainY[controller]->GetValue().mb_str()); tmp.clear();
	joysticks[controller].buttons[CTL_SUB_X] = std::string(m_JoyAnalogSubX[controller]->GetValue().mb_str()); tmp.clear();
	joysticks[controller].buttons[CTL_SUB_Y] = std::string(m_JoyAnalogSubY[controller]->GetValue().mb_str()); tmp.clear();

	joysticks[controller].enabled = m_Joyattach[controller]->GetValue();

	joysticks[controller].controllertype = m_Controltype[controller]->GetSelection();
	joysticks[controller].deadzone = m_Deadzone[controller]->GetSelection();
}

void ConfigBox::UpdateVisibleItems(int controller)
{	
	if(joysticks[controller].controllertype)	
	{
		m_JoyDpadDown[controller]->Show(TRUE);
		m_JoyDpadLeft[controller]->Show(TRUE);
		m_JoyDpadRight[controller]->Show(TRUE);

		m_bJoyDpadDown[controller]->Show(TRUE);
		m_bJoyDpadLeft[controller]->Show(TRUE);
		m_bJoyDpadRight[controller]->Show(TRUE);
		
		m_textDpadUp[controller]->Show(TRUE);
		m_textDpadDown[controller]->Show(TRUE);
		m_textDpadLeft[controller]->Show(TRUE);
		m_textDpadRight[controller]->Show(TRUE);		
	}
	else
	{	
		m_JoyDpadDown[controller]->Show(FALSE);
		m_JoyDpadLeft[controller]->Show(FALSE);
		m_JoyDpadRight[controller]->Show(FALSE);

		m_bJoyDpadDown[controller]->Show(FALSE);
		m_bJoyDpadLeft[controller]->Show(FALSE);
		m_bJoyDpadRight[controller]->Show(FALSE);
				
		m_textDpadUp[controller]->Show(FALSE);
		m_textDpadDown[controller]->Show(FALSE);
		m_textDpadLeft[controller]->Show(FALSE);
		m_textDpadRight[controller]->Show(FALSE);		
	}	
}

void ConfigBox::ChangeJoystick(wxCommandEvent& event)
{
	joysticks[0].ID = m_Joyname[0]->GetSelection();
	joysticks[1].ID = m_Joyname[1]->GetSelection();
	joysticks[2].ID = m_Joyname[2]->GetSelection();
	joysticks[3].ID = m_Joyname[3]->GetSelection();	
}
		
void ConfigBox::ChangeControllertype(wxCommandEvent& event)
{
	joysticks[0].controllertype = m_Controltype[0]->GetSelection();
	joysticks[1].controllertype = m_Controltype[1]->GetSelection();
	joysticks[2].controllertype = m_Controltype[2]->GetSelection();
	joysticks[3].controllertype = m_Controltype[3]->GetSelection();

	for(int i=0; i<4 ;i++)
		UpdateVisibleItems(i);	
}
	
void ConfigBox::NotebookPageChanged(wxNotebookEvent& event)
{	
	notebookpage = event.GetSelection();
}

void ConfigBox::SetButtonText(int id, char text[128])
{
	int controller = notebookpage;

	switch(id)
	{
		case IDB_SHOULDER_L:
		{
			m_JoyShoulderL[controller]->SetValue(wxString::FromAscii(text));
		}
		break;

		case IDB_SHOULDER_R:
		{
			m_JoyShoulderR[controller]->SetValue(wxString::FromAscii(text));
		}
		break;
		
		case IDB_BUTTON_A:
		{
			m_JoyButtonA[controller]->SetValue(wxString::FromAscii(text));
		}
		break;
		
		case IDB_BUTTON_B:
		{
			m_JoyButtonB[controller]->SetValue(wxString::FromAscii(text));
		}
		break;

		case IDB_BUTTON_X:
		{
			m_JoyButtonX[controller]->SetValue(wxString::FromAscii(text));
		}
		break;

		case IDB_BUTTON_Y:
		{
			m_JoyButtonY[controller]->SetValue(wxString::FromAscii(text));
		}
		break;

		case IDB_BUTTON_Z:
		{
			m_JoyButtonZ[controller]->SetValue(wxString::FromAscii(text));
		}
		break;

		case IDB_BUTTONSTART:
		{
			m_JoyButtonStart[controller]->SetValue(wxString::FromAscii(text));
		}
		break;

		case IDB_BUTTONHALFPRESS:
		{
			m_JoyButtonHalfpress[controller]->SetValue(wxString::FromAscii(text));
		}
		break;

		case IDB_DPAD_UP:
		{
			m_JoyDpadUp[controller]->SetValue(wxString::FromAscii(text));
		}
		break;

		case IDB_DPAD_DOWN:
		{
			m_JoyDpadDown[controller]->SetValue(wxString::FromAscii(text));
		}
		break;

		case IDB_DPAD_LEFT:
		{
			m_JoyDpadLeft[controller]->SetValue(wxString::FromAscii(text));
		}
		break;

		case IDB_DPAD_RIGHT:
		{
			m_JoyDpadRight[controller]->SetValue(wxString::FromAscii(text));
		}
		break;

		case IDB_ANALOG_MAIN_X:
		{
			m_JoyAnalogMainX[controller]->SetValue(wxString::FromAscii(text));
		}
		break;

		case IDB_ANALOG_MAIN_Y:
		{
			m_JoyAnalogMainY[controller]->SetValue(wxString::FromAscii(text));
		}
		break;

		case IDB_ANALOG_SUB_X:
		{
			m_JoyAnalogSubX[controller]->SetValue(wxString::FromAscii(text));
		}
		break;

		case IDB_ANALOG_SUB_Y:
		{
			m_JoyAnalogSubY[controller]->SetValue(wxString::FromAscii(text));
		}
		break;

		default:
			break;
	}
}
void ConfigBox::GetInputs(wxCommandEvent& event)
{
	int ID = event.GetId();
	int controller = notebookpage;
	
	// DPAD type check!
	if(ID == IDB_DPAD_UP)
		if(joysticks[controller].controllertype == 0)
		{
			GetHats(ID);
			return;
		}
	
	SDL_Joystick *joy = SDL_JoystickOpen(joysticks[controller].ID);

	char format[128];
	int buttons = SDL_JoystickNumButtons(joy);
	int axes = SDL_JoystickNumAxes(joy);
	char type = 'N'; // B, A, H, N = Nan
	bool waiting = true;
	bool succeed = false;
	int pressed = 0;
	Sint16 value;

	int counter1 = 0;
	int counter2 = 10;
		
	sprintf(format, "[%d]", counter2);
	SetButtonText(ID, format);
	wxWindow::Update();	// win only? doesnt seem to work in linux...

	while(waiting)
	{			
		SDL_JoystickUpdate();
		for(int b = 0; b < buttons; b++)
		{			
			if(SDL_JoystickGetButton(joy, b))
			{
				pressed = b;	
				waiting = false;
				succeed = true;
				type = 'B';
				goto InputEnd;
			}			
		}
		for(int b = 0; b < axes; b++)
		{		
			value = SDL_JoystickGetAxis(joy, b);
			if(value < -10000 || value > 10000)
			{
				pressed = b;	
				waiting = false;
				succeed = true;
				type = 'A';
				goto InputEnd;
			}			
		}	
InputEnd:
		counter1++;
		if(counter1==100)
		{
			counter1=0;
			counter2--;
			
			sprintf(format, "[%d]", counter2);
			SetButtonText(ID, format);
			wxWindow::Update();	// win only? doesnt seem to work in linux...
			
			if(counter2<0)
				waiting = false;
		}	
		SLEEP(10);
	}
	if(type == 'A')
		sprintf(format, "%c%c%d" , type, value < 0 ? '-' : '+',  succeed ? pressed : -1);
	else
		sprintf(format, "%c%d" , type, succeed ? pressed : -1);
	SetButtonText(ID, format);
	
	if(SDL_JoystickOpened(joysticks[controller].ID))
		SDL_JoystickClose(joy);
}

// Wait for D-Pad
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯
void ConfigBox::GetHats(int ID)
{	
	int controller = notebookpage;

	SDL_Joystick *joy;
	joy=SDL_JoystickOpen(joysticks[controller].ID);

	char format[128];
	int hats = SDL_JoystickNumHats(joy);
	bool waiting = true;
	bool succeed = false;
	int pressed = 0;

	int counter1 = 0;
	int counter2 = 10;
	
	sprintf(format, "[%d]", counter2);
	SetButtonText(ID, format);
	wxWindow::Update();	// win only? doesnt seem to work in linux...

	while(waiting)
	{			
		SDL_JoystickUpdate();
		for(int b = 0; b < hats; b++)
		{			
			if(SDL_JoystickGetHat(joy, b))
			{
				pressed = b;	
				waiting = false;
				succeed = true;
				break;
			}			
		}

		counter1++;
		if(counter1==100)
		{
			counter1=0;
			counter2--;
			
			sprintf(format, "[%d]", counter2);
			SetButtonText(ID, format);
			wxWindow::Update();	// win only? doesnt seem to work in linux...

			if(counter2<0)
				waiting = false;
		}	
		SLEEP(10);
	}

	sprintf(format, "H%d", succeed ? pressed : -1);
	SetButtonText(ID, format);

	if(SDL_JoystickOpened(joysticks[controller].ID))
		SDL_JoystickClose(joy);
}
