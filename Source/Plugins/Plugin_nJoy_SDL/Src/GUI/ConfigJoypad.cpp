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
#include "math.h" // System

#include "ConfigBox.h" // Local
#include "../nJoy.h"
#include "Images/controller.xpm"

extern CONTROLLER_INFO	*joyinfo;
//extern CONTROLLER_MAPPING joysticks[4];
extern bool emulator_running;
////////////////////////


// Set dialog items from saved values
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

/* Populate the CONTROLLER_MAPPING joysticks array with the dialog items settings, for example
   selected joystick, enabled or disabled status and so on */
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

	// Set enabled or disable status and other settings
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


// Change controller type
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
void ConfigBox::ChangeControllertype(wxCommandEvent& event)
{
	joysticks[0].controllertype = m_Controltype[0]->GetSelection();
	joysticks[1].controllertype = m_Controltype[1]->GetSelection();
	joysticks[2].controllertype = m_Controltype[2]->GetSelection();
	joysticks[3].controllertype = m_Controltype[3]->GetSelection();

	for(int i=0; i<4 ;i++) UpdateVisibleItems(i);	
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
	// Get the current controller	
	int controller = notebookpage;
	
	// Get the ID for the wxWidgets button that was pressed
	int ID = event.GetId();

	// DPAD type check	
	if(ID == IDB_DPAD_UP)
		if(joysticks[controller].controllertype == 0)
		{
			GetHats(ID);
			return;
		}

	/* Open a new joystick. Joysticks[controller].ID is the system ID of the physicaljoystick
	   that is mapped to controller, for example 0, 1, 2, 3 for the first four joysticks */
	SDL_Joystick *joy = SDL_JoystickOpen(joysticks[controller].ID);

	// Declare values
	char format[128];
	int buttons = SDL_JoystickNumButtons(joy); // Get number of buttons
	bool waiting = true;
	bool succeed = false;
	int pressed = 0;
	int counter1 = 0; // Waiting limits
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

		// Stop waiting for a button
		counter1++;
		if(counter1 == 100)
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

	// Write the number of the pressed button to the text box
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
		// Go through all axes and read out their values
		SDL_JoystickUpdate();
		for(int b = 0; b < axes; b++)
		{		
			value = SDL_JoystickGetAxis(joy, b);
			if(value < -10000 || value > 10000) // Avoid detecting small values
			{
				pressed = b;	
				waiting = false;
				succeed = true;
				break;
			}			
		}	

		// Stop waiting for a button
		counter1++;
		if(counter1 == 100)
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
	
	sprintf(format, "%d", succeed ? pressed : -1); // Update the status text box
	SetButtonText(ID, format);

	if(SDL_JoystickOpened(joysticks[controller].ID)) // Close the handle
		SDL_JoystickClose(joy);

	// Update the axises for the advanced settings status
	GetControllerAll(controller);
}
