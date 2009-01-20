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
void ConfigBox::UpdateGUIKeys(int controller)
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
	m_ControlType[controller]->SetSelection(joysticks[controller].controllertype);
	m_TriggerType[controller]->SetSelection(joysticks[controller].triggertype);
	m_Deadzone[controller]->SetSelection(joysticks[controller].deadzone);

	UpdateGUI(controller);

	if(joysticks[controller].controllertype == CTL_DPAD_HAT)
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
void ConfigBox::SaveButtonMapping(int controller)
{
	// Temporary storage
	wxString tmp;
	long value;

	// The controller ID
	joysticks[controller].ID = m_Joyname[controller]->GetSelection();

	// The shoulder buttons
	m_JoyShoulderL[controller]->GetValue().ToLong(&value); joysticks[controller].buttons[CTL_L_SHOULDER] = value;
	m_JoyShoulderR[controller]->GetValue().ToLong(&value); joysticks[controller].buttons[CTL_R_SHOULDER] = value;			

	// The digital buttons
	m_JoyButtonA[controller]->GetValue().ToLong(&value); joysticks[controller].buttons[CTL_A_BUTTON] = value; tmp.clear();
	m_JoyButtonB[controller]->GetValue().ToLong(&value); joysticks[controller].buttons[CTL_B_BUTTON] = value; tmp.clear();
	m_JoyButtonX[controller]->GetValue().ToLong(&value); joysticks[controller].buttons[CTL_X_BUTTON] = value; tmp.clear();
	m_JoyButtonY[controller]->GetValue().ToLong(&value); joysticks[controller].buttons[CTL_Y_BUTTON] = value; tmp.clear();
	m_JoyButtonZ[controller]->GetValue().ToLong(&value); joysticks[controller].buttons[CTL_Z_TRIGGER] = value; tmp.clear();
	m_JoyButtonStart[controller]->GetValue().ToLong(&value); joysticks[controller].buttons[CTL_START] = value; tmp.clear();

	m_JoyButtonHalfpress[controller]->GetValue().ToLong(&value); joysticks[controller].halfpress = value; tmp.clear();

	// Digital pad type
	if(joysticks[controller].controllertype == CTL_DPAD_HAT)
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
	joysticks[controller].controllertype = m_ControlType[controller]->GetSelection();
	joysticks[controller].triggertype = m_TriggerType[controller]->GetSelection();
	joysticks[controller].deadzone = m_Deadzone[controller]->GetSelection();	
}


// Change controller type
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
// Called from: When the controller type is changed
void ConfigBox::ChangeControllertype(wxCommandEvent& event)
{
	SaveButtonMapping(notebookpage);
}


// Update the textbox for the buttons
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
void ConfigBox::SetButtonText(int id, char text[128])
{
	int controller = notebookpage;

	switch(id)
	{
		case IDB_DPAD_RIGHT:
			m_JoyDpadRight[controller]->SetValue(wxString::FromAscii(text)); break;
		case IDB_DPAD_UP:
			m_JoyDpadUp[controller]->SetValue(wxString::FromAscii(text)); break;
		case IDB_DPAD_DOWN:
			m_JoyDpadDown[controller]->SetValue(wxString::FromAscii(text)); break;
		case IDB_DPAD_LEFT:
			m_JoyDpadLeft[controller]->SetValue(wxString::FromAscii(text));	break;

		case IDB_ANALOG_MAIN_X:
			m_JoyAnalogMainX[controller]->SetValue(wxString::FromAscii(text)); break;
		case IDB_ANALOG_MAIN_Y:
			m_JoyAnalogMainY[controller]->SetValue(wxString::FromAscii(text)); break;
		case IDB_ANALOG_SUB_X:
			m_JoyAnalogSubX[controller]->SetValue(wxString::FromAscii(text)); break;
		case IDB_ANALOG_SUB_Y:
			m_JoyAnalogSubY[controller]->SetValue(wxString::FromAscii(text)); break;

		case IDB_SHOULDER_L:
			m_JoyShoulderL[controller]->SetValue(wxString::FromAscii(text)); break;
		case IDB_SHOULDER_R:
			m_JoyShoulderR[controller]->SetValue(wxString::FromAscii(text)); break;
		
		case IDB_BUTTON_A:
			m_JoyButtonA[controller]->SetValue(wxString::FromAscii(text)); break;		
		case IDB_BUTTON_B:
			m_JoyButtonB[controller]->SetValue(wxString::FromAscii(text)); break;
		case IDB_BUTTON_X:
			m_JoyButtonX[controller]->SetValue(wxString::FromAscii(text)); break;
		case IDB_BUTTON_Y:
			m_JoyButtonY[controller]->SetValue(wxString::FromAscii(text)); break;
		case IDB_BUTTON_Z:
			m_JoyButtonZ[controller]->SetValue(wxString::FromAscii(text)); break;
		case IDB_BUTTONSTART:
			m_JoyButtonStart[controller]->SetValue(wxString::FromAscii(text)); break;

		case IDB_BUTTONHALFPRESS:
			m_JoyButtonHalfpress[controller]->SetValue(wxString::FromAscii(text)); break;

		default:
			break;
	}
}


//////////////////////////////////////////////////////////////////////////////////////////
// Configure button mapping
// ¯¯¯¯¯¯¯¯¯¯


// Avoid extreme axis values
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
/* Function: We have to avoid very big values to becuse some triggers are -0x8000 in the
   unpressed state (and then go from -0x8000 to 0x8000 as they are fully pressed) */
bool AvoidValues(int value)
{
	// Avoid detecting very small or very big (for triggers) values
	if(    (value > -0x2000 && value < 0x2000) // Small values
		|| (value < -0x6000 || value > 0x6000)) // Big values
		return true; // Avoid
	else
		return false; // Keep
}


// Wait for button press
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
/* Loop or timer: There are basically two ways to do this. With a while() or for() loop, or with a
   timer. The downside with the while() or for() loop is that there is no way to stop it if the user
   should select to configure another button while we are still in an old loop. What will happen then
   is that we start another parallel loop (at least in Windows) that blocks the old loop. And our only
   option to wait for the old loop to finish is with a new loop, and that will block the old loop for as
   long as it's going on. Therefore a timer is easier to control. */
void ConfigBox::GetButtons(wxCommandEvent& event)
{
	DoGetButtons(event.GetId());
}
void ConfigBox::DoGetButtons(int GetId)
{
	// =============================================
	// Collect the starting values
	// ----------------

	// Get the current controller	
	int Controller = notebookpage;

	// Collect the accepted buttons for this slot
	bool LeftRight = (GetId == IDB_SHOULDER_L || GetId == IDB_SHOULDER_R);
	bool Axis = (GetId >= IDB_ANALOG_MAIN_X && GetId <= IDB_SHOULDER_R);
	bool Button = (GetId >= IDB_BUTTON_A && GetId <= IDB_BUTTONHALFPRESS)
			   || (GetId == IDB_SHOULDER_L || GetId == IDB_SHOULDER_R)
			   || (GetId >= IDB_DPAD_UP && GetId <= IDB_DPAD_RIGHT && joysticks[Controller].controllertype == CTL_DPAD_CUSTOM);
	bool Hat = (GetId >= IDB_DPAD_UP && GetId <= IDB_DPAD_RIGHT)
			   && (joysticks[Controller].controllertype == CTL_DPAD_HAT);

	/* Open a new joystick. Joysticks[controller].GetId is the system GetId of the physical joystick
	   that is mapped to controller, for example 0, 1, 2, 3 for the first four joysticks */
	SDL_Joystick *joy = SDL_JoystickOpen(joysticks[Controller].ID);

	 // Get the number of axes, hats and buttons
	int buttons = SDL_JoystickNumButtons(joy);
	int axes = SDL_JoystickNumAxes(joy);
	int hats = SDL_JoystickNumHats(joy);	

	// Declare values
	char format[128];
	int value; // Axis value
	int type; // Button type
	bool Succeed = false;
	bool Stop = false; // Stop the timer
	int pressed = 0;
	int Seconds = 4; // Seconds to wait for
	int TimesPerSecond = 40; // How often to run the check
	// =======================

	//Console::Print("Before (%i) Id:%i %i  IsRunning:%i\n",
	//	GetButtonWaitingTimer, GetButtonWaitingID, GetId, m_ButtonMappingTimer->IsRunning());

	// If the Id has changed or the timer is not running we should start one
	if( GetButtonWaitingID != GetId || !m_ButtonMappingTimer->IsRunning() )
	{
		if(m_ButtonMappingTimer->IsRunning())
		{
			m_ButtonMappingTimer->Stop();
			GetButtonWaitingTimer = 0;

			// Update the old textbox
			SetButtonText(GetButtonWaitingID, "");
		}

		 // Save the button Id
		GetButtonWaitingID = GetId;

		// Reset the key in case we happen to have an old one
		g_Pressed = 0;

		// Update the text box
		sprintf(format, "[%d]", Seconds);
		SetButtonText(GetId, format);

		// Start the timer
		#if wxUSE_TIMER
			m_ButtonMappingTimer->Start( floor((double)(1000 / TimesPerSecond)) );
		#endif
	}

	// If there is a timer but we should not create a new one
	else
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

				pressed = i + (LeftRight ? 1000 : 0); // Identify the analog triggers
				type = CTL_AXIS;
				Succeed = true;
			}
		}

		// Check for a hat
		if(Hat)
		{
			for(int i = 0; i < hats; i++)
			{	
				if(SDL_JoystickGetHat(joy, i))
				{
					pressed = i;
					type = CTL_HAT;
					Succeed = true;
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
					type = CTL_BUTTON;
					Succeed = true;
				}
			}
		}

		// Check for keyboard action
		if (g_Pressed && Button)
		{
			// Todo: Add a separate keyboard vector to remove this restriction
			if(g_Pressed >= buttons)
			{
				pressed = g_Pressed;
				type = CTL_BUTTON;
				Succeed = true;
				g_Pressed = 0;			
			}
			else
			{
				pressed = g_Pressed;
				g_Pressed = -1;
				Stop = true;
			}
		}

		// Count each time
		GetButtonWaitingTimer++;

		// This is run every second
		if(GetButtonWaitingTimer % TimesPerSecond == 0)
		{
			//Console::Print("Second\n\n");

			// Current time
			int TmpTime = Seconds - (GetButtonWaitingTimer / TimesPerSecond);

			// Update text
			sprintf(format, "[%d]", TmpTime);
			SetButtonText(GetId, format);
		}

		// Time's up
		if( (GetButtonWaitingTimer / TimesPerSecond) >= Seconds )
		{
			Stop = true;

			// Leave a blank mapping
			SetButtonText(GetId, "");	
		}

		// If we got a button
		if(Succeed)
		{
			Stop = true;
			
			// Write the number of the pressed button to the text box
			sprintf(format, "%d", pressed);
			SetButtonText(GetId, format);			
		}
	}

	// Stop the timer
	if(Stop)
	{
		m_ButtonMappingTimer->Stop();
		GetButtonWaitingTimer = 0;
	}

	// If we got a bad button
	if(g_Pressed == -1)
	{
		SetButtonText(GetId, ""); // Update text
		
		// Notify the user
		wxMessageBox(wxString::Format(wxT(
			"You selected a key with a to low key code (%i), please"
			" select another key with a higher key code."), pressed)
			, wxT("Notice"), wxICON_INFORMATION);		
	}

	// We don't need this gamepad handle any more
	if(SDL_JoystickOpened(joysticks[Controller].ID)) SDL_JoystickClose(joy);

	// Update the button mapping
	SaveButtonMapping(Controller);

	// Debugging
	//Console::Print("IsRunning: %i\n", m_ButtonMappingTimer->IsRunning());
}

/////////////////////////////////////////////////////////// Configure button mapping