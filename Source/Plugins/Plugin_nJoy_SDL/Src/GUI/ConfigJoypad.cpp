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

extern bool g_EmulatorRunning;
////////////////////////


// Set dialog items from saved values
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
void ConfigBox::UpdateGUIButtonMapping(int controller)
{	
	// http://wiki.wxwidgets.org/Converting_everything_to_and_from_wxString
	wxString tmp;

	// Update selected gamepad
	m_Joyname[controller]->SetSelection(PadMapping[controller].ID);

	// Update the enabled checkbox
	m_Joyattach[controller]->SetValue(PadMapping[controller].enabled == 1 ? true : false);

	tmp << PadMapping[controller].buttons[InputCommon::CTL_L_SHOULDER]; m_JoyShoulderL[controller]->SetValue(tmp); tmp.clear();
	tmp << PadMapping[controller].buttons[InputCommon::CTL_R_SHOULDER]; m_JoyShoulderR[controller]->SetValue(tmp); tmp.clear();

	tmp << PadMapping[controller].buttons[InputCommon::CTL_A_BUTTON]; m_JoyButtonA[controller]->SetValue(tmp); tmp.clear();
	tmp << PadMapping[controller].buttons[InputCommon::CTL_B_BUTTON]; m_JoyButtonB[controller]->SetValue(tmp); tmp.clear();
	tmp << PadMapping[controller].buttons[InputCommon::CTL_X_BUTTON]; m_JoyButtonX[controller]->SetValue(tmp); tmp.clear();
	tmp << PadMapping[controller].buttons[InputCommon::CTL_Y_BUTTON]; m_JoyButtonY[controller]->SetValue(tmp); tmp.clear();
	tmp << PadMapping[controller].buttons[InputCommon::CTL_Z_TRIGGER]; m_JoyButtonZ[controller]->SetValue(tmp); tmp.clear();

	tmp << PadMapping[controller].buttons[InputCommon::CTL_START]; m_JoyButtonStart[controller]->SetValue(tmp); tmp.clear();
	tmp << PadMapping[controller].halfpress; m_JoyButtonHalfpress[controller]->SetValue(tmp); tmp.clear();

	tmp << PadMapping[controller].axis[InputCommon::CTL_MAIN_X]; m_JoyAnalogMainX[controller]->SetValue(tmp); tmp.clear();
	tmp << PadMapping[controller].axis[InputCommon::CTL_MAIN_Y]; m_JoyAnalogMainY[controller]->SetValue(tmp); tmp.clear();
	tmp << PadMapping[controller].axis[InputCommon::CTL_SUB_X]; m_JoyAnalogSubX[controller]->SetValue(tmp); tmp.clear();
	tmp << PadMapping[controller].axis[InputCommon::CTL_SUB_Y]; m_JoyAnalogSubY[controller]->SetValue(tmp); tmp.clear();
	
	// Update the deadzone and controller type controls
	m_ControlType[controller]->SetSelection(PadMapping[controller].controllertype);
	m_TriggerType[controller]->SetSelection(PadMapping[controller].triggertype);
	m_Deadzone[controller]->SetSelection(PadMapping[controller].deadzone);
	m_CoBDiagonal[controller]->SetValue(wxString::FromAscii(PadMapping[controller].SDiagonal.c_str()));
	m_CBS_to_C[controller]->SetValue(PadMapping[controller].bSquareToCircle);
	m_AdvancedMapFilter[controller]->SetValue(g_Config.bNoTriggerFilter);
#ifdef RERECORDING
	m_CheckRecording[controller]->SetValue(g_Config.bRecording);
	m_CheckPlayback[controller]->SetValue(g_Config.bPlayback);
#endif
	//LogMsg("m_TriggerType[%i] = %i\n", controller, PadMapping[controller].triggertype);

	// Update D-Pad
	if(PadMapping[controller].controllertype == InputCommon::CTL_DPAD_HAT)
	{
		tmp << PadMapping[controller].dpad; m_JoyDpadDown[controller]->SetValue(tmp); tmp.clear();		
	}
	else
	{
		tmp << PadMapping[controller].dpad2[InputCommon::CTL_D_PAD_UP]; m_JoyDpadUp[controller]->SetValue(tmp); tmp.clear();
		tmp << PadMapping[controller].dpad2[InputCommon::CTL_D_PAD_DOWN]; m_JoyDpadDown[controller]->SetValue(tmp); tmp.clear();
		tmp << PadMapping[controller].dpad2[InputCommon::CTL_D_PAD_LEFT]; m_JoyDpadLeft[controller]->SetValue(tmp); tmp.clear();
		tmp << PadMapping[controller].dpad2[InputCommon::CTL_D_PAD_RIGHT]; m_JoyDpadRight[controller]->SetValue(tmp); tmp.clear();
	}

	// Replace "-1" with "" in the GUI controls
	//if(ControlsCreated) ToBlank();
}

/* Populate the PadMapping array with the dialog items settings (for example
   selected joystick, enabled or disabled status and so on) */
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
void ConfigBox::SaveButtonMapping(int controller, bool DontChangeId, int FromSlot)
{
	// Temporary storage
	wxString tmp;
	long value;

	// Save from or to the same or different slots
	if (FromSlot == -1) FromSlot = controller;

	// Replace "" with "-1" in the GUI controls
	ToBlank(false);

	// Set enabled or disable status and other settings
	if(!DontChangeId) PadMapping[controller].ID = m_Joyname[FromSlot]->GetSelection();
	if(FromSlot == controller) PadMapping[controller].enabled = m_Joyattach[FromSlot]->GetValue(); // Only enable one
	PadMapping[controller].controllertype = m_ControlType[FromSlot]->GetSelection();
	PadMapping[controller].triggertype = m_TriggerType[FromSlot]->GetSelection();
	PadMapping[controller].deadzone = m_Deadzone[FromSlot]->GetSelection();
	PadMapping[controller].SDiagonal = m_CoBDiagonal[FromSlot]->GetLabel().mb_str();
	PadMapping[controller].bSquareToCircle = m_CBS_to_C[FromSlot]->IsChecked();

	// The analog buttons
	m_JoyAnalogMainX[FromSlot]->GetValue().ToLong(&value); PadMapping[controller].axis[InputCommon::CTL_MAIN_X] = value; tmp.clear();
	m_JoyAnalogMainY[FromSlot]->GetValue().ToLong(&value); PadMapping[controller].axis[InputCommon::CTL_MAIN_Y] = value; tmp.clear();
	m_JoyAnalogSubX[FromSlot]->GetValue().ToLong(&value); PadMapping[controller].axis[InputCommon::CTL_SUB_X] = value; tmp.clear();
	m_JoyAnalogSubY[FromSlot]->GetValue().ToLong(&value); PadMapping[controller].axis[InputCommon::CTL_SUB_Y] = value; tmp.clear();

	// The shoulder buttons
	m_JoyShoulderL[FromSlot]->GetValue().ToLong(&value); PadMapping[controller].buttons[InputCommon::CTL_L_SHOULDER] = value;
	m_JoyShoulderR[FromSlot]->GetValue().ToLong(&value); PadMapping[controller].buttons[InputCommon::CTL_R_SHOULDER] = value;			

	// The digital buttons
	m_JoyButtonA[FromSlot]->GetValue().ToLong(&value); PadMapping[controller].buttons[InputCommon::CTL_A_BUTTON] = value; tmp.clear();
	m_JoyButtonB[FromSlot]->GetValue().ToLong(&value); PadMapping[controller].buttons[InputCommon::CTL_B_BUTTON] = value; tmp.clear();
	m_JoyButtonX[FromSlot]->GetValue().ToLong(&value); PadMapping[controller].buttons[InputCommon::CTL_X_BUTTON] = value; tmp.clear();
	m_JoyButtonY[FromSlot]->GetValue().ToLong(&value); PadMapping[controller].buttons[InputCommon::CTL_Y_BUTTON] = value; tmp.clear();
	m_JoyButtonZ[FromSlot]->GetValue().ToLong(&value); PadMapping[controller].buttons[InputCommon::CTL_Z_TRIGGER] = value; tmp.clear();
	m_JoyButtonStart[FromSlot]->GetValue().ToLong(&value); PadMapping[controller].buttons[InputCommon::CTL_START] = value; tmp.clear();

	//LogMsg("PadMapping[%i].triggertype = %i, m_TriggerType[%i]->GetSelection() = %i\n",
	//	controller, PadMapping[controller].triggertype, FromSlot, m_TriggerType[FromSlot]->GetSelection());

	// The halfpress button
	m_JoyButtonHalfpress[FromSlot]->GetValue().ToLong(&value); PadMapping[controller].halfpress = value; tmp.clear();

	// The digital pad
	if(PadMapping[controller].controllertype == InputCommon::CTL_DPAD_HAT)
	{
		m_JoyDpadDown[FromSlot]->GetValue().ToLong(&value); PadMapping[controller].dpad = value; tmp.clear();
	}
	else
	{
		m_JoyDpadUp[FromSlot]->GetValue().ToLong(&value); PadMapping[controller].dpad2[InputCommon::CTL_D_PAD_UP] = value; tmp.clear();
		m_JoyDpadDown[FromSlot]->GetValue().ToLong(&value); PadMapping[controller].dpad2[InputCommon::CTL_D_PAD_DOWN] = value; tmp.clear();
		m_JoyDpadLeft[FromSlot]->GetValue().ToLong(&value); PadMapping[controller].dpad2[InputCommon::CTL_D_PAD_LEFT] = value; tmp.clear();
		m_JoyDpadRight[FromSlot]->GetValue().ToLong(&value); PadMapping[controller].dpad2[InputCommon::CTL_D_PAD_RIGHT] = value; tmp.clear();		
	}

	// Replace "-1" with "" 
	ToBlank();
}

// Update the textbox for the buttons
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
void ConfigBox::SetButtonText(int id, char text[128], int Page)
{
	// Set controller value
	int controller;	
	if (Page == -1) controller = notebookpage; else controller = Page;

	switch(id)
	{
		case IDB_DPAD_RIGHT: m_JoyDpadRight[controller]->SetValue(wxString::FromAscii(text)); break;
		case IDB_DPAD_UP: m_JoyDpadUp[controller]->SetValue(wxString::FromAscii(text)); break;
		case IDB_DPAD_DOWN: m_JoyDpadDown[controller]->SetValue(wxString::FromAscii(text)); break;
		case IDB_DPAD_LEFT: m_JoyDpadLeft[controller]->SetValue(wxString::FromAscii(text));	break;

		case IDB_ANALOG_MAIN_X: m_JoyAnalogMainX[controller]->SetValue(wxString::FromAscii(text)); break;
		case IDB_ANALOG_MAIN_Y: m_JoyAnalogMainY[controller]->SetValue(wxString::FromAscii(text)); break;
		case IDB_ANALOG_SUB_X: m_JoyAnalogSubX[controller]->SetValue(wxString::FromAscii(text)); break;
		case IDB_ANALOG_SUB_Y: m_JoyAnalogSubY[controller]->SetValue(wxString::FromAscii(text)); break;

		case IDB_SHOULDER_L: m_JoyShoulderL[controller]->SetValue(wxString::FromAscii(text)); break;
		case IDB_SHOULDER_R: m_JoyShoulderR[controller]->SetValue(wxString::FromAscii(text)); break;
		
		case IDB_BUTTON_A: m_JoyButtonA[controller]->SetValue(wxString::FromAscii(text)); break;		
		case IDB_BUTTON_B: m_JoyButtonB[controller]->SetValue(wxString::FromAscii(text)); break;
		case IDB_BUTTON_X: m_JoyButtonX[controller]->SetValue(wxString::FromAscii(text)); break;
		case IDB_BUTTON_Y: m_JoyButtonY[controller]->SetValue(wxString::FromAscii(text)); break;
		case IDB_BUTTON_Z: m_JoyButtonZ[controller]->SetValue(wxString::FromAscii(text)); break;
		case IDB_BUTTONSTART: m_JoyButtonStart[controller]->SetValue(wxString::FromAscii(text)); break;

		case IDB_BUTTONHALFPRESS: m_JoyButtonHalfpress[controller]->SetValue(wxString::FromAscii(text)); break;
		default: break;
	}
}

// Get the text in the textbox for the buttons
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
wxString ConfigBox::GetButtonText(int id, int Page)
{
	// Set controller value
	int controller;	
	if (Page == -1) controller = notebookpage; else controller = Page;

	switch(id)
	{
		// D-Pad
		case IDB_DPAD_RIGHT: return m_JoyDpadRight[controller]->GetValue();
		case IDB_DPAD_UP: return m_JoyDpadUp[controller]->GetValue();
		case IDB_DPAD_DOWN: return m_JoyDpadDown[controller]->GetValue();
		case IDB_DPAD_LEFT: return m_JoyDpadLeft[controller]->GetValue();

		// Analog Stick
		case IDB_ANALOG_MAIN_X: return m_JoyAnalogMainX[controller]->GetValue();
		case IDB_ANALOG_MAIN_Y: return m_JoyAnalogMainY[controller]->GetValue();
		case IDB_ANALOG_SUB_X: return m_JoyAnalogSubX[controller]->GetValue();
		case IDB_ANALOG_SUB_Y: return m_JoyAnalogSubY[controller]->GetValue();

		// Shoulder Buttons
		case IDB_SHOULDER_L: return m_JoyShoulderL[controller]->GetValue();
		case IDB_SHOULDER_R: return m_JoyShoulderR[controller]->GetValue();
		
		// Buttons
		case IDB_BUTTON_A: return m_JoyButtonA[controller]->GetValue();		
		case IDB_BUTTON_B: return m_JoyButtonB[controller]->GetValue();
		case IDB_BUTTON_X: return m_JoyButtonX[controller]->GetValue();
		case IDB_BUTTON_Y: return m_JoyButtonY[controller]->GetValue();
		case IDB_BUTTON_Z: return m_JoyButtonZ[controller]->GetValue();
		case IDB_BUTTONSTART: return m_JoyButtonStart[controller]->GetValue();

		case IDB_BUTTONHALFPRESS: return m_JoyButtonHalfpress[controller]->GetValue();
		default: return wxString();
	}
}


//////////////////////////////////////////////////////////////////////////////////////////
// Configure button mapping
// ¯¯¯¯¯¯¯¯¯¯


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
	int PadID = PadMapping[Controller].ID;

	// Create a shortcut for the pad handle
	SDL_Joystick *joy = PadState[Controller].joy;

	 // Get the number of axes, hats and buttons
	int Buttons = SDL_JoystickNumButtons(joy);
	int Axes = SDL_JoystickNumAxes(joy);
	int Hats = SDL_JoystickNumHats(joy);

	Console::Print("PadID: %i Axes: %i\n", PadID, joyinfo[PadID].NumAxes, joyinfo[PadID].joy);

	// Get the controller and trigger type
	int ControllerType = PadMapping[Controller].controllertype;
	int TriggerType = PadMapping[Controller].triggertype;

	// Collect the accepted buttons for this slot
	bool LeftRight = (GetId == IDB_SHOULDER_L || GetId == IDB_SHOULDER_R);

	bool Axis = (GetId >= IDB_ANALOG_MAIN_X && GetId <= IDB_SHOULDER_R)
		// Don't allow SDL input for the triggers when XInput is selected
		&& !(TriggerType == InputCommon::CTL_TRIGGER_XINPUT && (GetId == IDB_SHOULDER_L || GetId == IDB_SHOULDER_R) );
	
	bool XInput = (TriggerType == InputCommon::CTL_TRIGGER_XINPUT);

	bool Button = (GetId >= IDB_BUTTON_A && GetId <= IDB_BUTTONHALFPRESS) // All digital buttons
			   || (GetId == IDB_SHOULDER_L || GetId == IDB_SHOULDER_R) // both shoulder buttons
			   || (GetId >= IDB_DPAD_UP && GetId <= IDB_DPAD_RIGHT && ControllerType == InputCommon::CTL_DPAD_CUSTOM); // Or the custom hat mode

	bool Hat = (GetId >= IDB_DPAD_UP && GetId <= IDB_DPAD_RIGHT) // All DPads
			   && (PadMapping[Controller].controllertype == InputCommon::CTL_DPAD_HAT); // Not with the hat option defined

	bool NoTriggerFilter = g_Config.bNoTriggerFilter;

	// Values used in this function
	char format[128];
	int Seconds = 4; // Seconds to wait for
	int TimesPerSecond = 40; // How often to run the check

	// Values returned from InputCommon::GetButton()
	int value; // Axis value
	int type; // Button type
	int pressed = 0;
	bool Succeed = false;
	bool Stop = false; // Stop the timer
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

	// ===============================================
	// Check for buttons
	// ----------------

	// If there is a timer but we should not create a new one
	else
	{
		InputCommon::GetButton(
			joy, PadID, Buttons, Axes, Hats, 
			g_Pressed, value, type, pressed, Succeed, Stop,
			LeftRight, Axis, XInput, Button, Hat, NoTriggerFilter);
	}
	// ========================= Check for keys


	// ===============================================
	// Process results
	// ----------------

	// Count each time
	GetButtonWaitingTimer++;

	// This is run every second
	if(GetButtonWaitingTimer % TimesPerSecond == 0)
	{
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
		if(g_Config.bSaveByID) SetButtonTextAll(GetId, "-1"); else SetButtonText(GetId, "-1");
	}

	// If we got a button
	if(Succeed)
	{
		Stop = true;		
		// Write the number of the pressed button to the text box
		sprintf(format, "%d", pressed);
		if(g_Config.bSaveByID) SetButtonTextAll(GetId, format); else SetButtonText(GetId, format);
	}

	// Stop the timer
	if(Stop)
	{
		m_ButtonMappingTimer->Stop();
		GetButtonWaitingTimer = 0;

		/* Update the button mapping for all slots that use this device. (It doesn't make sense to have several slots
		   controlled by the same device, but several DirectInput instances of different but identical devices may possible
		   have the same id, I don't know. So we have to do this. The user may also have selected the same device for
		   several disabled slots. */
		if(g_Config.bSaveByID) SaveButtonMappingAll(Controller); else SaveButtonMapping(Controller);
	}

	// If we got a bad button
	if(g_Pressed == -1)
	{
		// Update text
		if(g_Config.bSaveByID) SetButtonTextAll(GetId, "-1"); else SetButtonText(GetId, "-1");
		
		// Notify the user
		wxMessageBox(wxString::Format(wxT(
			"You selected a key with a to low key code (%i), please"
			" select another key with a higher key code."), pressed)
			, wxT("Notice"), wxICON_INFORMATION);		
	}
	// ======================== Process results

	// Debugging
	/*
	Console::Print("Change: %i %i %i %i  '%s' '%s' '%s' '%s'\n",
		PadMapping[0].halfpress, PadMapping[1].halfpress, PadMapping[2].halfpress, PadMapping[3].halfpress,
		m_JoyButtonHalfpress[0]->GetValue().c_str(), m_JoyButtonHalfpress[1]->GetValue().c_str(), m_JoyButtonHalfpress[2]->GetValue().c_str(), m_JoyButtonHalfpress[3]->GetValue().c_str()
		);*/
}
/////////////////////////////////////////////////////////// Configure button mapping
