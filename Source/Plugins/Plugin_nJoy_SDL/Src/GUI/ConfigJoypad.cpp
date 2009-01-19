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
	
	// Update the deadzone and controller type controls
	m_Controltype[controller]->SetSelection(joysticks[controller].controllertype);
	m_Deadzone[controller]->SetSelection(joysticks[controller].deadzone);

	UpdateGUI(controller);

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

/* Populate the joysticks array with the dialog items settings, for example
   selected joystick, enabled or disabled status and so on */
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
void ConfigBox::GetControllerAll(int controller)
{
	wxString tmp;
	long value;

	joysticks[controller].ID = m_Joyname[controller]->GetSelection();
	if(joyinfo[joysticks[controller].ID].NumHats > 0) joysticks[controller].controllertype = CTL_TYPE_JOYSTICK;

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


// Change controller type
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
void ConfigBox::ChangeControllertype(wxCommandEvent& event)
{
	joysticks[0].controllertype = m_Controltype[0]->GetSelection();
	joysticks[1].controllertype = m_Controltype[1]->GetSelection();
	joysticks[2].controllertype = m_Controltype[2]->GetSelection();
	joysticks[3].controllertype = m_Controltype[3]->GetSelection();

	for(int i=0; i<4 ;i++) UpdateGUI(i);	
}


void ConfigBox::SetButtonText(int id, char text[128])
{
	int controller = notebookpage;

	switch(id)
	{
		case IDB_SHOULDER_L:
			m_JoyShoulderL[controller]->SetValue(wxString::FromAscii(text)); break;

		case IDB_SHOULDER_R:
			m_JoyShoulderR[controller]->SetValue(wxString::FromAscii(text)); break;
		
		case IDB_BUTTON_A:
			m_JoyButtonA[controller]->SetValue(wxString::FromAscii(text)); break;
		
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
			m_JoyButtonY[controller]->SetValue(wxString::FromAscii(text)); break;

		case IDB_BUTTON_Z:
			m_JoyButtonZ[controller]->SetValue(wxString::FromAscii(text)); break;

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
			m_JoyDpadRight[controller]->SetValue(wxString::FromAscii(text)); break;

		case IDB_ANALOG_MAIN_X:
			m_JoyAnalogMainX[controller]->SetValue(wxString::FromAscii(text)); break;

		case IDB_ANALOG_MAIN_Y:
			m_JoyAnalogMainY[controller]->SetValue(wxString::FromAscii(text)); break;

		case IDB_ANALOG_SUB_X:
			m_JoyAnalogSubX[controller]->SetValue(wxString::FromAscii(text)); break;

		case IDB_ANALOG_SUB_Y:
			m_JoyAnalogSubY[controller]->SetValue(wxString::FromAscii(text)); break;

		default:
			break;
	}
}


//////////////////////////////////////////////////////////////////////////////////////////
// Condifigure button mapping
// ¯¯¯¯¯¯¯¯¯¯


// Avoid extreme axis values
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
/* Function: We have to avoid very big values to becuse some triggers are -0x8000 in the
   unpressed state (and then go from -0x8000 to 0x8000 as they are fully pressed) */
bool AvoidValues(int value)
{
	// Avoid detecting very small or very big (for triggers) values
	if(    (value > -0x1000 && value < 0x1000) // Small values
		|| (value < -0x7000 || value > 0x7000)) // Big values
		return true; // Avoid
	else
		return false; // Keep
}


// Wait for button press
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
void ConfigBox::GetButtons(wxCommandEvent& event)
{
	// Get the current controller	
	int controller = notebookpage;
	
	// Get the ID for the wxWidgets button that was pressed
	int ID = event.GetId();

	// Collect the accepted buttons for this slot
	bool Axis = (event.GetId() >= IDB_ANALOG_MAIN_X && event.GetId() <= IDB_SHOULDER_R);
	bool Button = (event.GetId() >= IDB_BUTTON_A && event.GetId() <= IDB_BUTTONSTART)
			   || (event.GetId() == IDB_SHOULDER_L || event.GetId() == IDB_SHOULDER_R);
	bool Hat = (event.GetId() >= IDB_DPAD_UP && event.GetId() <= IDB_DPAD_RIGHT);

	/* Open a new joystick. Joysticks[controller].ID is the system ID of the physical joystick
	   that is mapped to controller, for example 0, 1, 2, 3 for the first four joysticks */
	SDL_Joystick *joy = SDL_JoystickOpen(joysticks[controller].ID);

	 // Get the number of axes, hats and buttons
	int buttons = SDL_JoystickNumButtons(joy);
	int axes = SDL_JoystickNumAxes(joy);
	int hats = SDL_JoystickNumHats(joy);	

	// Declare values
	char format[128];
	int value; // Axis value
	bool waiting = true;
	bool succeed = false;
	int pressed = 0;
	int counter1 = 0; // Waiting limits
	int counter2 = 30; // Iterations to wait for

	// Update the text box
	sprintf(format, "[%d]", counter2);
	SetButtonText(ID, format);
	wxWindow::Update();	// Win only? doesnt seem to work in linux...

	while(waiting)
	{
		// Update the internal status
		SDL_JoystickUpdate();

		// For the triggers we accept both a digital or an analog button
		if(Axis)
		{
			for(int i = 0; i < axes; i++)
			{
				value = SDL_JoystickGetAxis(joy, i);

				if(AvoidValues(value)) continue; // Avoid values

				pressed = i;	
				waiting = false;
				succeed = true;
				break; // Stop this loop
			}
		}

		// Check for a hat
		if(Hat)
		{
			for(int i = 0; i < hats; i++)
			{	
				value = SDL_JoystickGetHat(joy, i);
				if(value)
				{
					pressed = value;	
					waiting = false;
					succeed = true;
					break;
				}			
			}
		}

		// Check for a button
		if(Button)
		{
			for(int i = 0; i < buttons; i++)
			{			
				if(SDL_JoystickGetButton(joy, i))
				{
					pressed = i;	
					waiting = false;
					succeed = true;
					break;
				}
			}
		}

		// Check for keyboard action
		if (g_Pressed)
		{
			// Todo: Add a separate keyboard vector to remove this restriction
			if(g_Pressed >= buttons)
			{
				pressed = g_Pressed;
				waiting = false;
				succeed = true;
				g_Pressed = 0;
				break;				
			}
			else
			{
				wxMessageBox(wxString::Format(wxT(
					"You selected a key with a to low key code (%i), please"
					" select another key with a higher key code."), g_Pressed)
					, wxT("Notice"), wxICON_INFORMATION);

				pressed = g_Pressed;
				waiting = false;
				succeed = false;
				g_Pressed = 0;
				break;			
			}
		}

		// Stop waiting for a button
		counter1++;
		if(counter1 == 25)
		{
			counter1 = 0;
			counter2--;
			
			sprintf(format, "[%d]", counter2);
			SetButtonText(ID, format);
			wxWindow::Update();	// win only? doesnt seem to work in linux...
			wxYieldIfNeeded(); // Let through the keyboard input event
			if(counter2 < 0) waiting = false;
		}

		// Sleep for 10 ms then poll for keys again
		SLEEP(10);
		
		// Debugging
		/*
		m_pStatusBar->SetLabel(wxString::Format(
			"ID: %i  %i",
			counter1, NumKeys
			));
			*/
	}

	// Write the number of the pressed button to the text box
	sprintf(format, "%d", succeed ? pressed : -1);
	SetButtonText(ID, format);

	// We don't need thisgamepad handle any more
	if(SDL_JoystickOpened(joysticks[controller].ID)) SDL_JoystickClose(joy);
}


// Wait for Analog
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
void ConfigBox::GetAxis(wxCommandEvent& event)
{
	int ID = event.GetId();
	int controller = notebookpage;

	SDL_Joystick *joy = SDL_JoystickOpen(joysticks[controller].ID);

	char format[128];
	int axes = SDL_JoystickNumAxes(joy); // Get number of axes
	bool waiting = true;
	bool succeed = false;
	int pressed = 0;
	Sint16 value;
	
	int counter1 = 0;
	int counter2 = 10;
	
	sprintf(format, "[%d]", counter2);
	SetButtonText(ID, format);
	wxWindow::Update();	// Win only? doesnt seem to work in linux...

	while(waiting)
	{		
		// Go through all axes and read out their values
		SDL_JoystickUpdate();
		for(int i = 0; i < axes; i++)
		{		
			value = SDL_JoystickGetAxis(joy, i);
			
			if(AvoidValues(value)) continue; // Avoid values

			pressed = i;	
			waiting = false;
			succeed = true;
			break; // Stop this loop
		}	

		// Stop waiting for a button
		counter1++;
		if(counter1 == 100)
		{
			counter1 = 0;
			counter2--;
			
			sprintf(format, "[%d]", counter2);
			SetButtonText(ID, format);
			wxWindow::Update();	// win only? doesnt seem to work in linux...
			wxYieldIfNeeded(); // Let through debugging events

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


// Wait for D-Pad
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯
void ConfigBox::GetHats(int ID)
{	
	// Get the active controller
	int controller = notebookpage;

	/* Open a new joystick. Joysticks[controller].ID is the system ID of the physical joystick
	   that is mapped to controller, for example 0, 1, 2, 3 for the first four joysticks */
	SDL_Joystick *joy = SDL_JoystickOpen(joysticks[controller].ID);

	char format[128];
	int hats = SDL_JoystickNumHats(joy); // Get the number of sticks
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

/////////////////////////////////////////////////////////// Configure button mapping