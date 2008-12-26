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


//////////////////////////////////////////////////////////////////////////////////////////
// Include
// ¯¯¯¯¯¯¯¯¯
#include "ConfigBox.h"
#include "../nJoy.h"
#include "Images/controller.xpm"

extern CONTROLLER_INFO	*joyinfo;
//extern CONTROLLER_MAPPING joysticks[4];
extern bool emulator_running;

static const char* ControllerType[] =
{
	"Joystick (default)",
	"Joystick (no hat)",
//	"Joytstick (xbox360)",	// Shoulder buttons -> axis
//	"Keyboard"				// Not supported yet, sorry F|RES ;( ...
};
////////////////////////


//////////////////////////////////////////////////////////////////////////////////////////
// The wxWidgets class
// ¯¯¯¯¯¯¯
BEGIN_EVENT_TABLE(ConfigBox,wxDialog)
	EVT_CLOSE(ConfigBox::OnClose)
	EVT_BUTTON(ID_ABOUT, ConfigBox::AboutClick)
	EVT_BUTTON(ID_OK, ConfigBox::OKClick)
	EVT_BUTTON(ID_CANCEL, ConfigBox::CancelClick)
	EVT_COMBOBOX(IDC_JOYNAME, ConfigBox::ChangeJoystick)
	EVT_COMBOBOX(IDC_CONTROLTYPE, ConfigBox::ChangeControllertype)
	EVT_NOTEBOOK_PAGE_CHANGED(ID_NOTEBOOK, ConfigBox::NotebookPageChanged)

	EVT_BUTTON(IDB_SHOULDER_L, ConfigBox::GetButtons)
	EVT_BUTTON(IDB_SHOULDER_R, ConfigBox::GetButtons)
	EVT_BUTTON(IDB_BUTTON_A, ConfigBox::GetButtons)
	EVT_BUTTON(IDB_BUTTON_B, ConfigBox::GetButtons)
	EVT_BUTTON(IDB_BUTTON_X, ConfigBox::GetButtons)
	EVT_BUTTON(IDB_BUTTON_Y, ConfigBox::GetButtons)
	EVT_BUTTON(IDB_BUTTON_Z, ConfigBox::GetButtons)
	EVT_BUTTON(IDB_BUTTONSTART, ConfigBox::GetButtons)
	EVT_BUTTON(IDB_BUTTONHALFPRESS, ConfigBox::GetButtons)
	EVT_BUTTON(IDB_DPAD_UP, ConfigBox::GetButtons)
	EVT_BUTTON(IDB_DPAD_DOWN, ConfigBox::GetButtons)
	EVT_BUTTON(IDB_DPAD_LEFT, ConfigBox::GetButtons)
	EVT_BUTTON(IDB_DPAD_RIGHT, ConfigBox::GetButtons)

	EVT_BUTTON(IDB_ANALOG_MAIN_X, ConfigBox::GetAxis)
	EVT_BUTTON(IDB_ANALOG_MAIN_Y, ConfigBox::GetAxis)
	EVT_BUTTON(IDB_ANALOG_SUB_X, ConfigBox::GetAxis)
	EVT_BUTTON(IDB_ANALOG_SUB_Y, ConfigBox::GetAxis)

	#if wxUSE_TIMER
		EVT_TIMER(wxID_ANY, ConfigBox::OnTimer)
	#endif
END_EVENT_TABLE()

ConfigBox::ConfigBox(wxWindow *parent, wxWindowID id, const wxString &title,
					 const wxPoint &position, const wxSize& size, long style)
	: wxDialog(parent, id, title, position, size, style)
	#if wxUSE_TIMER
	, m_timer(this)
	#endif
{
	notebookpage = 0;
	CreateGUIControls();

	#if wxUSE_TIMER
		m_timer.Start(1000);
	#endif
}

ConfigBox::~ConfigBox()
{
	// empty
} 
//////////////////////////////////


// Populate the config window
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
void ConfigBox::OnPaint( wxPaintEvent &event )
{
	event.Skip();

	for(int i=0; i<4 ;i++)
	{	
		wxPaintDC dcWin(m_pKeys[i]);
		PrepareDC( dcWin );
		dcWin.DrawBitmap( WxStaticBitmap1_BITMAP, 94, 0, true );
	}
}

// Populate the config window
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
void ConfigBox::CreateGUIControls()
{
	#ifndef _DEBUG		
		SetTitle(wxT("Configure: nJoy v"INPUT_VERSION" Input Plugin"));
	#else			
		SetTitle(wxT("Configure: nJoy v"INPUT_VERSION" (Debug) Input Plugin"));
	#endif
	
	SetIcon(wxNullIcon);

	//WxStaticBitmap1_BITMAP(ConfigBox_WxStaticBitmap1_XPM);

	

	//WxStaticBitmap1_BITMAP = new WxStaticBitmap1_BITMAP(ConfigBox_WxStaticBitmap1_XPM);

#ifndef _WIN32
	// Force a 8pt font so that it looks more or less "correct" regardless of the default font setting
	wxFont f(8,wxFONTFAMILY_DEFAULT,wxFONTSTYLE_NORMAL,wxFONTWEIGHT_NORMAL);
	SetFont(f);
#endif

	// Buttons
	m_About = new wxButton(this, ID_ABOUT, wxT("About"), wxDefaultPosition, wxSize(75, 25), 0, wxDefaultValidator, wxT("About"));
	m_OK = new wxButton(this, ID_OK, wxT("OK"), wxDefaultPosition, wxSize(75, 25), 0, wxDefaultValidator, wxT("OK"));
	m_Cancel = new wxButton(this, ID_CANCEL, wxT("Cancel"), wxDefaultPosition, wxSize(75, 25), 0, wxDefaultValidator, wxT("Cancel"));

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
	
	// Define bitmap for EVT_PAINT
	WxStaticBitmap1_BITMAP = wxBitmap(ConfigBox_WxStaticBitmap1_XPM);
	

	// Populate all four pages
	for(int i=0; i<4 ;i++)
	{	
		// --------------------------------------------------------------------
		// Populate keys sizer
		// -----------------------------
		// Set relative values for the keys
		int t = -75; // Top
		int l = -4; // Left
		m_sKeys[i] = new wxStaticBoxSizer( wxVERTICAL, m_Controller[i], wxT("Keys"));
		m_pKeys[i] = new wxPanel(m_Controller[i], ID_KEYSPANEL1 + i, wxDefaultPosition, wxSize(600, 400));
		//m_sKeys[i] = new wxStaticBox (m_Controller[i], IDG_JOYSTICK, wxT("Keys"), wxDefaultPosition, wxSize(608, 500));
		m_sKeys[i]->Add(m_pKeys[i], 0, (wxALL), 0); // margin = 0

		// --------------------------------------------------------------------
		// GameCube controller picture
		// -----------------------------
		// TODO: Controller image
		// Placeholder instead of bitmap
		// m_PlaceholderBMP[i] = new wxTextCtrl(m_Controller[i], ID_CONTROLLERPICTURE, wxT("BITMAP HERE PLZ KTHX!"), wxPoint(98, 75), wxSize(423, 306), wxTE_READONLY | wxTE_CENTRE, wxDefaultValidator, wxT("BITMAP HERE PLZ KTHX!"));
		// m_PlaceholderBMP[i]->Enable(false);
		
		/* You can enable the bitmap here. But it loads überslow on init... (only in windows, linux
		seems to load it fast!) AAaaand the XPM file (256 colours) looks crappier than the real bitmap...
		so maybe we can find a way to use a bitmap?	*/
		//m_controllerimage[i] = new wxStaticBitmap(m_pKeys[i], ID_CONTROLLERPICTURE, WxStaticBitmap1_BITMAP, wxPoint(l + 98, t + 75), wxSize(421,304));		
		//m_controllerimage[i] = new wxBitmap( WxStaticBitmap1_BITMAP );		

		 // Paint background. This allows objects to be visible on top of the picture
		m_pKeys[i]->Connect(wxID_ANY, wxEVT_PAINT,
			wxPaintEventHandler(ConfigBox::OnPaint),
			(wxObject*)0, this);


		// --------------------------------------------------------------------
		// Keys objects
		// -----------------------------
		// Left shoulder
		m_JoyShoulderL[i] = new wxTextCtrl(m_pKeys[i], ID_SHOULDER_L, wxT("0"), wxPoint(l + 6, t + 80), wxSize(59, 19), wxTE_READONLY | wxTE_CENTRE, wxDefaultValidator, wxT("0"));
		m_JoyShoulderL[i]->Enable(false);
		m_bJoyShoulderL[i] = new wxButton(m_pKeys[i], IDB_SHOULDER_L, wxEmptyString, wxPoint(l + 70, t + 82), wxSize(21, 14), 0, wxDefaultValidator, wxEmptyString);

		// Left analog
		int ALt = 170; int ALw = ALt + 14; int ALb = ALw + 2; // Set offset
		m_JoyAnalogMainX[i] = new wxTextCtrl(m_pKeys[i], ID_ANALOG_MAIN_X, wxT("0"), wxPoint(l + 6, t + ALw), wxSize(59, 19), wxTE_READONLY | wxTE_CENTRE, wxDefaultValidator, wxT("0"));
		m_JoyAnalogMainX[i]->Enable(false);
		m_JoyAnalogMainY[i] = new wxTextCtrl(m_pKeys[i], ID_ANALOG_MAIN_Y, wxT("0"), wxPoint(l + 6, t + ALw + 36), wxSize(59, 19), wxTE_READONLY | wxTE_CENTRE, wxDefaultValidator, wxT("0"));
		m_JoyAnalogMainY[i]->Enable(false);		
		m_bJoyAnalogMainX[i] = new wxButton(m_pKeys[i], IDB_ANALOG_MAIN_X, wxEmptyString, wxPoint(l + 70, t + ALb), wxSize(21, 14), 0, wxDefaultValidator, wxEmptyString);
		m_bJoyAnalogMainY[i] = new wxButton(m_pKeys[i], IDB_ANALOG_MAIN_Y, wxEmptyString, wxPoint(l + 70, t + ALb + 36), wxSize(21, 14), 0, wxDefaultValidator, wxEmptyString);
		m_textMainX[i] = new wxStaticText(m_pKeys[i], IDT_ANALOG_MAIN_X, wxT("X-axis"), wxPoint(l + 6, t + ALt), wxDefaultSize, 0, wxT("X-axis"));
		m_textMainY[i] = new wxStaticText(m_pKeys[i], IDT_ANALOG_MAIN_Y, wxT("Y-axis"), wxPoint(l + 6, t + ALt + 36), wxDefaultSize, 0, wxT("Y-axis"));
		
		// D-Pad
		int DPt = 255; int DPw = DPt + 14; int DPb = DPw + 2; // Set offset
		m_JoyDpadUp[i] = new wxTextCtrl(m_pKeys[i], ID_DPAD_UP, wxT("0"), wxPoint(l + 6, t + DPw), wxSize(59, t + 19), wxTE_READONLY | wxTE_CENTRE, wxDefaultValidator, wxT("0"));
		m_JoyDpadUp[i]->Enable(false);
		m_JoyDpadDown[i] = new wxTextCtrl(m_pKeys[i], ID_DPAD_DOWN, wxT("0"), wxPoint(l + 6, t + DPw + 36*1), wxSize(59, t + 19), wxTE_READONLY | wxTE_CENTRE, wxDefaultValidator, wxT("0"));
		m_JoyDpadDown[i]->Enable(false);
		m_JoyDpadLeft[i] = new wxTextCtrl(m_pKeys[i], ID_DPAD_LEFT, wxT("0"), wxPoint(l + 6, t + DPw + 36*2), wxSize(59, t + 19), wxTE_READONLY | wxTE_CENTRE, wxDefaultValidator, wxT("0"));
		m_JoyDpadLeft[i]->Enable(false);
		m_JoyDpadRight[i] = new wxTextCtrl(m_pKeys[i], ID_DPAD_RIGHT, wxT("0"), wxPoint(l + 6, t + DPw + 36*3), wxSize(59, t + 19), wxTE_READONLY | wxTE_CENTRE, wxDefaultValidator, wxT("0"));
		m_JoyDpadRight[i]->Enable(false);
		m_bJoyDpadUp[i] = new wxButton(m_pKeys[i], IDB_DPAD_UP, wxEmptyString, wxPoint(l + 70, t + DPb + 36*0), wxSize(21, 14), 0, wxDefaultValidator, wxEmptyString);
		m_bJoyDpadDown[i] = new wxButton(m_pKeys[i], IDB_DPAD_DOWN, wxEmptyString, wxPoint(l + 70, t + DPb + 36*1), wxSize(21, 14), 0, wxDefaultValidator, wxEmptyString);
		m_bJoyDpadLeft[i] = new wxButton(m_pKeys[i], IDB_DPAD_LEFT, wxEmptyString, wxPoint(l + 70, t + DPb + 36*2), wxSize(21, 14), 0, wxDefaultValidator, wxEmptyString);
		m_bJoyDpadRight[i] = new wxButton(m_pKeys[i], IDB_DPAD_RIGHT, wxEmptyString, wxPoint(l + 70, t + DPb + 36*3), wxSize(21, 14), 0, wxDefaultValidator, wxEmptyString);
		m_textDpadUp[i] = new wxStaticText(m_pKeys[i], IDT_DPAD_UP, wxT("Up"), wxPoint(l + 6, t + DPt + 36*0), wxDefaultSize, 0, wxT("Up"));
		m_textDpadDown[i] = new wxStaticText(m_pKeys[i], IDT_DPAD_DOWN, wxT("Down"), wxPoint(l + 6, t + DPt + 36*1), wxDefaultSize, 0, wxT("Down"));
		m_textDpadLeft[i] = new wxStaticText(m_pKeys[i], IDT_DPAD_LEFT, wxT("Left"), wxPoint(l + 6, t + DPt + 36*2), wxDefaultSize, 0, wxT("Left"));
		m_textDpadRight[i] = new wxStaticText(m_pKeys[i], IDT_DPAD_RIGHT, wxT("Right"), wxPoint(l + 6, t + DPt + 36*3), wxDefaultSize, 0, wxT("Right"));
		
		// Right side buttons
		m_JoyShoulderR[i] = new wxTextCtrl(m_pKeys[i], ID_SHOULDER_R, wxT("0"), wxPoint(l + 552, t + 106), wxSize(59, 19), wxTE_READONLY | wxTE_CENTRE, wxDefaultValidator, wxT("0"));
		m_JoyShoulderR[i]->Enable(false);
		m_JoyButtonA[i] = new wxTextCtrl(m_pKeys[i], ID_BUTTON_A, wxT("0"), wxPoint(l + 552, t + 280), wxSize(59, 19), wxTE_READONLY | wxTE_CENTRE, wxDefaultValidator, wxT("0"));
		m_JoyButtonA[i]->Enable(false);
		m_JoyButtonB[i] = new wxTextCtrl(m_pKeys[i], ID_BUTTON_B, wxT("0"), wxPoint(l + 552, t + 80), wxSize(59, 19), wxTE_READONLY | wxTE_CENTRE, wxDefaultValidator, wxT("0"));
		m_JoyButtonB[i]->Enable(false);
		m_JoyButtonX[i] = new wxTextCtrl(m_pKeys[i], ID_BUTTON_X, wxT("0"), wxPoint(l + 552, t + 242), wxSize(59, 19), wxTE_READONLY | wxTE_CENTRE, wxDefaultValidator, wxT("0"));
		m_JoyButtonX[i]->Enable(false);
		m_JoyButtonY[i] = new wxTextCtrl(m_pKeys[i], ID_BUTTON_Y, wxT("0"), wxPoint(l + 552, t + 171), wxSize(59, 19), wxTE_READONLY | wxTE_CENTRE, wxDefaultValidator, wxT("0"));
		m_JoyButtonY[i]->Enable(false);
		m_JoyButtonZ[i] = new wxTextCtrl(m_pKeys[i], ID_BUTTON_Z, wxT("0"), wxPoint(l + 552, t + 145), wxSize(59, 19), wxTE_READONLY | wxTE_CENTRE, wxDefaultValidator, wxT("0"));
		m_JoyButtonZ[i]->Enable(false);
		m_bJoyShoulderR[i] = new wxButton(m_pKeys[i], IDB_SHOULDER_R, wxEmptyString, wxPoint(l + 526, t + 108), wxSize(21, 14), 0, wxDefaultValidator, wxEmptyString);
		m_bJoyButtonA[i] = new wxButton(m_pKeys[i], IDB_BUTTON_A, wxEmptyString, wxPoint(l + 526, t + 282), wxSize(21, 14), 0, wxDefaultValidator, wxEmptyString);
		m_bJoyButtonB[i] = new wxButton(m_pKeys[i], IDB_BUTTON_B, wxEmptyString, wxPoint(l + 526, t + 82), wxSize(21, 14), 0, wxDefaultValidator, wxEmptyString);
		m_bJoyButtonX[i] = new wxButton(m_pKeys[i], IDB_BUTTON_X, wxEmptyString, wxPoint(l + 526, t + 244), wxSize(21, 14), 0, wxDefaultValidator, wxEmptyString);
		m_bJoyButtonY[i] = new wxButton(m_pKeys[i], IDB_BUTTON_Y, wxEmptyString, wxPoint(l + 526, t + 173), wxSize(21, 14), 0, wxDefaultValidator, wxEmptyString);
		m_bJoyButtonZ[i] = new wxButton(m_pKeys[i], IDB_BUTTON_Z, wxEmptyString, wxPoint(l + 526, t + 147), wxSize(21, 14), 0, wxDefaultValidator, wxEmptyString);

		// C-buttons
		m_JoyAnalogSubX[i] = new wxTextCtrl(m_pKeys[i], ID_ANALOG_SUB_X, wxT("0"), wxPoint(l + 552, t + 336), wxSize(59, 19), wxTE_READONLY | wxTE_CENTRE, wxDefaultValidator, wxT("0"));
		m_JoyAnalogSubX[i]->Enable(false);
		m_JoyAnalogSubY[i] = new wxTextCtrl(m_pKeys[i], ID_ANALOG_SUB_Y, wxT("0"), wxPoint(l + 552, t + 373), wxSize(59, 19), wxTE_READONLY | wxTE_CENTRE, wxDefaultValidator, wxT("0"));
		m_JoyAnalogSubY[i]->Enable(false);
		m_bJoyAnalogSubX[i] = new wxButton(m_pKeys[i], IDB_ANALOG_SUB_X, wxEmptyString, wxPoint(l + 526, t + 338), wxSize(21, 14), 0, wxDefaultValidator, wxEmptyString);
		m_bJoyAnalogSubY[i] = new wxButton(m_pKeys[i], IDB_ANALOG_SUB_Y, wxEmptyString, wxPoint(l + 526, t + 375), wxSize(21, 14), 0, wxDefaultValidator, wxEmptyString);
		m_textSubX[i] = new wxStaticText(m_pKeys[i], IDT_ANALOG_SUB_X, wxT("X-axis"), wxPoint(l + 552, t + 321), wxDefaultSize, 0, wxT("X-axis"));
		m_textSubY[i] = new wxStaticText(m_pKeys[i], IDT_ANALOG_SUB_Y, wxT("Y-axis"), wxPoint(l + 552, t + 358), wxDefaultSize, 0, wxT("Y-axis"));
		
		// Start button
		m_bJoyButtonStart[i] = new wxButton(m_pKeys[i], IDB_BUTTONSTART, wxEmptyString, wxPoint(l + 284, t + 365), wxSize(21, 14), 0, wxDefaultValidator, wxEmptyString);
		m_JoyButtonStart[i] = new wxTextCtrl(m_pKeys[i], ID_BUTTONSTART, wxT("0"), wxPoint(l + 220, t + 363), wxSize(59, 19), wxTE_READONLY | wxTE_CENTRE, wxDefaultValidator, wxT("0"));
		m_JoyButtonStart[i]->Enable(false);
		
		// Website text
		#ifdef _WIN32
		m_textWebsite[i] = new wxStaticText(m_pKeys[i], IDT_WEBSITE, wxT("www.multigesture.net"), wxPoint(l + 400, t + 380), wxDefaultSize, 0, wxT("www.multigesture.net"));
		#else
		m_textWebsite[i] = new wxStaticText(m_Controller[i], IDT_WEBSITE, wxT("www.multigesture.net"), wxPoint(l + 480, t + 418), wxDefaultSize, 0, wxT("www.multigesture.net"));
		#endif


		// --------------------------------------------------------------------
		// Populate Controller sizer
		// -----------------------------
		// Groups
		#ifdef _WIN32
		m_Joyname[i] = new wxComboBox(m_Controller[i], IDC_JOYNAME, wxEmptyString, wxDefaultPosition, wxSize(476, 21), arrayStringFor_Joyname, 0, wxDefaultValidator, wxT("m_Joyname"));
		m_Joyattach[i] = new wxCheckBox(m_Controller[i], IDC_JOYATTACH, wxT("Controller attached"), wxDefaultPosition, wxSize(109, 25), 0, wxDefaultValidator, wxT("Controller attached"));
		#else
		m_Joyname[i] = new wxComboBox(m_Controller[i], IDC_JOYNAME, wxEmptyString, wxDefaultPosition, wxSize(450, 25), arrayStringFor_Joyname, 0, wxDefaultValidator, wxT("m_Joyname"));
		m_Joyattach[i] = new wxCheckBox(m_Controller[i], IDC_JOYATTACH, wxT("Controller attached"), wxDefaultPosition, wxSize(140, 25), 0, wxDefaultValidator, wxT("Controller attached"));
		#endif

		m_gJoyname[i] = new wxStaticBoxSizer (wxHORIZONTAL, m_Controller[i], wxT("Controller:"));
		m_gJoyname[i]->Add(m_Joyname[i], 0, (wxLEFT | wxRIGHT), 5);
		m_gJoyname[i]->Add(m_Joyattach[i], 0, (wxRIGHT | wxLEFT | wxBOTTOM), 1);

		// --------------------------------------------------------------------
		// Populate settings sizer
		// -----------------------------
		
		// Extra settings members
		m_gGBExtrasettings[i] = new wxGridBagSizer(0, 0);
		m_JoyButtonHalfpress[i] = new wxTextCtrl(m_Controller[i], ID_BUTTONHALFPRESS, wxT("0"), wxDefaultPosition, wxSize(59, 19), wxTE_READONLY | wxTE_CENTRE, wxDefaultValidator, wxT("0"));
		m_JoyButtonHalfpress[i]->Enable(false);
		m_bJoyButtonHalfpress[i] = new wxButton(m_Controller[i], IDB_BUTTONHALFPRESS, wxEmptyString, wxPoint(231, 426), wxSize(21, 14), 0, wxDefaultValidator, wxEmptyString);
		#ifdef _WIN32
		m_Deadzone[i] = new wxComboBox(m_Controller[i], IDC_DEADZONE, wxEmptyString, wxDefaultPosition, wxSize(59, 21), arrayStringFor_Deadzone, 0, wxDefaultValidator, wxT("m_Deadzone"));
		m_textDeadzone[i] = new wxStaticText(m_Controller[i], IDT_DEADZONE, wxT("Deadzone"), wxDefaultPosition, wxDefaultSize, 0, wxT("Deadzone"));		
		m_textHalfpress[i] = new wxStaticText(m_Controller[i], IDT_BUTTONHALFPRESS, wxT("Half press"), wxDefaultPosition, wxDefaultSize, 0, wxT("Half press"));
		#else
		m_Deadzone[i] = new wxComboBox(m_Controller[i], IDC_DEADZONE, wxEmptyString, wxPoint(167, 398), wxSize(80, 25), arrayStringFor_Deadzone, 0, wxDefaultValidator, wxT("m_Deadzone"));
		m_textDeadzone[i] = new wxStaticText(m_Controller[i], IDT_DEADZONE, wxT("Deadzone"), wxPoint(105, 404), wxDefaultSize, 0, wxT("Deadzone"));		
		m_textHalfpress[i] = new wxStaticText(m_Controller[i], IDT_BUTTONHALFPRESS, wxT("Half press"), wxPoint(105, 428), wxDefaultSize, 0, wxT("Half press"));
		#endif

		// Populate extra settings
		m_gExtrasettings[i] = new wxStaticBoxSizer( wxVERTICAL, m_Controller[i], wxT("Extra settings"));
		m_gGBExtrasettings[i]->Add(m_textDeadzone[i], wxGBPosition(0, 0), wxGBSpan(1, 1), (wxRIGHT | wxTOP), 2);
		m_gGBExtrasettings[i]->Add(m_Deadzone[i], wxGBPosition(0, 1), wxGBSpan(1, 1), (wxBOTTOM), 2);
		m_gGBExtrasettings[i]->Add(m_textHalfpress[i], wxGBPosition(1, 0), wxGBSpan(1, 1), (wxRIGHT | wxTOP), 2);
		m_gGBExtrasettings[i]->Add(m_JoyButtonHalfpress[i], wxGBPosition(1, 1), wxGBSpan(1, 1), wxALL, 0);
		m_gGBExtrasettings[i]->Add(m_bJoyButtonHalfpress[i], wxGBPosition(1, 2), wxGBSpan(1, 1), (wxLEFT | wxTOP), 2);
		m_gExtrasettings[i]->Add(m_gGBExtrasettings[i], 0, wxEXPAND | wxALL, 3);

		// Populate controller typ
		m_gControllertype[i] = new wxStaticBoxSizer( wxVERTICAL, m_Controller[i], wxT("Controller type"));
		#ifdef _WIN32
		m_Controltype[i] = new wxComboBox(m_Controller[i], IDC_CONTROLTYPE, wxEmptyString, wxDefaultPosition, wxSize(131, 21), arrayStringFor_Controltype, 0, wxDefaultValidator, wxT("m_Controltype"));
		#else
		m_Controltype[i] = new wxComboBox(m_Controller[i], IDC_CONTROLTYPE, wxEmptyString, wxDefaultPosition, wxSize(150, 25), arrayStringFor_Controltype, 0, wxDefaultValidator, wxT("m_Controltype"));
		#endif
		m_gControllertype[i]->Add(m_Controltype[i], 0, wxEXPAND | wxALL, 3);

		// Populate input status
		/*
		m_gStatusIn[i] = new wxStaticBoxSizer( wxVERTICAL, m_Controller[i], wxT("In"));
		CreateAdvancedControls(i);
		m_gStatusIn[i]->Add(m_pInStatus[i], 0, wxALL, 0);
		*/
		
		// Populate settings
		m_sSettings[i] = new wxBoxSizer ( wxHORIZONTAL );
		m_sSettings[i]->Add(m_gExtrasettings[i], 0, wxEXPAND | wxALL, 0);
		m_sSettings[i]->Add(m_gControllertype[i], 0, wxEXPAND | wxLEFT, 5);
		//m_sSettings[i]->Add(m_gStatusIn[i], 0, wxLEFT, 5);
		

		// --------------------------------------------------------------------
		// Populate main sizer
		// -----------------------------
		m_sMain[i] = new wxBoxSizer(wxVERTICAL);
		m_sMain[i]->Add(m_gJoyname[i], 0, wxEXPAND | (wxALL), 5);
		m_sMain[i]->Add(m_sKeys[i], 1, wxEXPAND | (wxLEFT | wxRIGHT), 5);
		m_sMain[i]->Add(m_sSettings[i], 0, wxEXPAND | (wxALL), 5);
		m_Controller[i]->SetSizer(m_sMain[i]); // Set the main sizer


		// Disable when running
		if(emulator_running)
		{
			m_Joyname[i]->Enable(false);
			m_Joyattach[i]->Enable(false);
			m_Controltype[i]->Enable(false);			
		}
		
		// Set dialog items
		SetControllerAll(i);
	} // end of loop

	
	// --------------------------------------------------------------------
	// Populate buttons sizer.
	// -----------------------------
	wxBoxSizer * m_sButtons = new wxBoxSizer(wxHORIZONTAL);
	m_sButtons->Add(m_About, 0, (wxBOTTOM), 0);
	m_sButtons->AddStretchSpacer(1);
	m_sButtons->Add(m_OK, 0, wxALIGN_RIGHT | (wxBOTTOM), 0);
	m_sButtons->Add(m_Cancel, 0, wxALIGN_RIGHT | (wxLEFT), 5);


	// --------------------------------------------------------------------
	// Populate master sizer.
	// -----------------------------
	wxBoxSizer * m_MainSizer = new wxBoxSizer(wxVERTICAL);
	m_MainSizer->Add(m_Notebook, 0, wxEXPAND | wxALL, 5);
	m_MainSizer->Add(m_sButtons, 1, wxEXPAND | ( wxLEFT | wxRIGHT | wxBOTTOM), 5);
	this->SetSizer(m_MainSizer);

	// Set window size
	SetClientSize(m_MainSizer->GetMinSize().GetWidth(), m_MainSizer->GetMinSize().GetHeight());
	Center();
}


// Close window
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
void ConfigBox::OnClose(wxCloseEvent& /*event*/)
{
	EndModal(0);
}


// Call about dialog
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
void ConfigBox::AboutClick(wxCommandEvent& event)
{
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

	tmp << joysticks[controller].buttons[CTL_L_SHOULDER]; m_JoyShoulderL[controller]->SetValue(tmp); tmp.clear();
	tmp << joysticks[controller].buttons[CTL_R_SHOULDER]; m_JoyShoulderR[controller]->SetValue(tmp); tmp.clear();

	tmp << joysticks[controller].buttons[CTL_A_BUTTON]; m_JoyButtonA[controller]->SetValue(tmp); tmp.clear();
	tmp << joysticks[controller].buttons[CTL_B_BUTTON]; m_JoyButtonB[controller]->SetValue(tmp); tmp.clear();
	tmp << joysticks[controller].buttons[CTL_X_BUTTON]; m_JoyButtonX[controller]->SetValue(tmp); tmp.clear();
	tmp << joysticks[controller].buttons[CTL_Y_BUTTON]; m_JoyButtonY[controller]->SetValue(tmp); tmp.clear();
	tmp << joysticks[controller].buttons[CTL_Z_TRIGGER]; m_JoyButtonZ[controller]->SetValue(tmp); tmp.clear();

	tmp << joysticks[controller].buttons[CTL_START]; m_JoyButtonStart[controller]->SetValue(tmp); tmp.clear();
	tmp << joysticks[controller].halfpress; m_JoyButtonHalfpress[controller]->SetValue(tmp); tmp.clear();

	tmp << joysticks[controller].axis[CTL_MAIN_X]; m_JoyAnalogMainX[controller]->SetValue(tmp); tmp.clear();
	tmp << joysticks[controller].axis[CTL_MAIN_Y]; m_JoyAnalogMainY[controller]->SetValue(tmp); tmp.clear();
	tmp << joysticks[controller].axis[CTL_SUB_X]; m_JoyAnalogSubX[controller]->SetValue(tmp); tmp.clear();
	tmp << joysticks[controller].axis[CTL_SUB_Y]; m_JoyAnalogSubY[controller]->SetValue(tmp); tmp.clear();

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

// Get dialog items. Collect button identification.
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
void ConfigBox::GetControllerAll(int controller)
{
	wxString tmp;
	long value;

	joysticks[controller].ID = m_Joyname[controller]->GetSelection();

	m_JoyShoulderL[controller]->GetValue().ToLong(&value); joysticks[controller].buttons[CTL_L_SHOULDER] = value; tmp.clear();
	m_JoyShoulderR[controller]->GetValue().ToLong(&value); joysticks[controller].buttons[CTL_R_SHOULDER] = value; tmp.clear();

	m_JoyButtonA[controller]->GetValue().ToLong(&value); joysticks[controller].buttons[CTL_A_BUTTON] = value; tmp.clear();
	m_JoyButtonB[controller]->GetValue().ToLong(&value); joysticks[controller].buttons[CTL_B_BUTTON] = value; tmp.clear();
	m_JoyButtonX[controller]->GetValue().ToLong(&value); joysticks[controller].buttons[CTL_X_BUTTON] = value; tmp.clear();
	m_JoyButtonY[controller]->GetValue().ToLong(&value); joysticks[controller].buttons[CTL_Y_BUTTON] = value; tmp.clear();
	m_JoyButtonZ[controller]->GetValue().ToLong(&value); joysticks[controller].buttons[CTL_Z_TRIGGER] = value; tmp.clear();
	m_JoyButtonStart[controller]->GetValue().ToLong(&value); joysticks[controller].buttons[CTL_START] = value; tmp.clear();

	m_JoyButtonHalfpress[controller]->GetValue().ToLong(&value); joysticks[controller].halfpress = value; tmp.clear();

	if(joysticks[controller].controllertype == CTL_TYPE_JOYSTICK)
	{
		m_JoyDpadUp[controller]->GetValue().ToLong(&value); joysticks[controller].dpad = value; tmp.clear();
	}
	else
	{
		m_JoyDpadUp[controller]->GetValue().ToLong(&value); joysticks[controller].dpad2[CTL_D_PAD_UP] = value; tmp.clear();
		m_JoyDpadDown[controller]->GetValue().ToLong(&value); joysticks[controller].dpad2[CTL_D_PAD_DOWN] = value; tmp.clear();
		m_JoyDpadLeft[controller]->GetValue().ToLong(&value); joysticks[controller].dpad2[CTL_D_PAD_LEFT] = value; tmp.clear();
		m_JoyDpadRight[controller]->GetValue().ToLong(&value); joysticks[controller].dpad2[CTL_D_PAD_RIGHT] = value; tmp.clear();		
	}

	m_JoyAnalogMainX[controller]->GetValue().ToLong(&value); joysticks[controller].axis[CTL_MAIN_X] = value; tmp.clear();
	m_JoyAnalogMainY[controller]->GetValue().ToLong(&value); joysticks[controller].axis[CTL_MAIN_Y] = value; tmp.clear();
	m_JoyAnalogSubX[controller]->GetValue().ToLong(&value); joysticks[controller].axis[CTL_SUB_X] = value; tmp.clear();
	m_JoyAnalogSubY[controller]->GetValue().ToLong(&value); joysticks[controller].axis[CTL_SUB_Y] = value; tmp.clear();

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

	for(int i=0; i<4 ;i++) UpdateVisibleItems(i);	
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


// Wait for button press
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
void ConfigBox::GetButtons(wxCommandEvent& event)
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
		for(int b = 0; b < buttons; b++)
		{			
			if(SDL_JoystickGetButton(joy, b))
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

	sprintf(format, "%d", succeed ? pressed : -1);
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

	sprintf(format, "%d", succeed ? pressed : -1);
	SetButtonText(ID, format);

	if(SDL_JoystickOpened(joysticks[controller].ID))
		SDL_JoystickClose(joy);
}

// Wait for Analog
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
void ConfigBox::GetAxis(wxCommandEvent& event)
{
	int ID = event.GetId();
	int controller = notebookpage;

	SDL_Joystick *joy;
	joy=SDL_JoystickOpen(joysticks[controller].ID);

	char format[128];
	int axes = SDL_JoystickNumAxes(joy);
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
		for(int b = 0; b < axes; b++)
		{		
			value = SDL_JoystickGetAxis(joy, b);
			if(value < -10000 || value > 10000)
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

	sprintf(format, "%d", succeed ? pressed : -1);
	SetButtonText(ID, format);

	if(SDL_JoystickOpened(joysticks[controller].ID))
		SDL_JoystickClose(joy);
}
