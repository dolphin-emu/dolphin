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

#include "wiimote_hid.h"
#include "main.h"
#include "ConfigPadDlg.h"
#include "Config.h"
#include "EmuMain.h" // for WiiMoteEmu class
#if defined(HAVE_X11) && HAVE_X11
	#include "X11InputBase.h"
#endif
// Change Joystick
/* Function: When changing the joystick we save and load the settings and
   update the PadMapping and PadState array. PadState[].joy is the gamepad
   handle that is used to access the pad throughout the plugin. Joyinfo[].joy
   is only used the first time the pads are checked. */
void WiimotePadConfigDialog::DoChangeJoystick()
{
	WARN_LOG(WIIMOTE, "---- DoChangeJoystick ----");

	// Before changing the pad we save potential changes to the current pad
	DoSave(true);
	
	// Load the settings for the new Id
	g_Config.Load(true);
	// Update the GUI
	UpdateGUI(Page);
}

void WiimotePadConfigDialog::DoChangeDeadZone(bool Left)
{
	if(Left)
	{
		float Rad = (float)WiiMoteEmu::PadMapping[Page].DeadZoneL * ((float)BoxW / 100.0) * 0.5;	
		m_bmpDeadZoneLeftIn[Page]->SetBitmap(CreateBitmapClear());
		m_bmpDeadZoneLeftIn[Page]->SetSize(0, 0);
		m_bmpDeadZoneLeftIn[Page]->SetBitmap(CreateBitmapDeadZone((int)Rad));
		m_bmpDeadZoneLeftIn[Page]->SetPosition(wxPoint(BoxW / 2 - (int)Rad, BoxH / 2 - (int)Rad));
		m_bmpDeadZoneLeftIn[Page]->Refresh();	
	}
	else
	{
		float Rad = (float)WiiMoteEmu::PadMapping[Page].DeadZoneR * ((float)BoxW / 100.0) * 0.5;	
		m_bmpDeadZoneRightIn[Page]->SetBitmap(CreateBitmapClear());
		m_bmpDeadZoneRightIn[Page]->SetSize(0, 0);
		m_bmpDeadZoneRightIn[Page]->SetBitmap(CreateBitmapDeadZone((int)Rad));
		m_bmpDeadZoneRightIn[Page]->SetPosition(wxPoint(BoxW / 2 - (int)Rad, BoxH / 2 - (int)Rad));
		m_bmpDeadZoneRightIn[Page]->Refresh();	
	}
}


// Change settings

// Set the button text for all four Wiimotes
void WiimotePadConfigDialog::SetButtonTextAll(int id, char text[128])
{
	for (int i = 0; i < MAX_WIIMOTES; i++)
	{
		if (WiiMoteEmu::IDToName(WiiMoteEmu::PadMapping[i].ID) == WiiMoteEmu::IDToName(WiiMoteEmu::PadMapping[Page].ID))
		{
			SetButtonText(id, text, i);
			DEBUG_LOG(PAD, "Updated button text for slot %i", i);
		}
	};
}
void WiimotePadConfigDialog::SaveButtonMappingAll(int Slot)
{
	for (int i = 0; i < MAX_WIIMOTES; i++)
	{				
		if (WiiMoteEmu::IDToName(WiiMoteEmu::PadMapping[i].ID) == WiiMoteEmu::IDToName(WiiMoteEmu::PadMapping[Slot].ID))
			SaveButtonMapping(i, false, Slot);
	}
}

// Set dialog items from saved values
void WiimotePadConfigDialog::UpdateGUIButtonMapping(int controller)
{	
	NOTICE_LOG(WIIMOTE, "Load ButtonMapping | controller:%i", controller);

	// Temporary storage
	wxString tmp;

	// Update selected gamepad
	m_Joyname[controller]->SetSelection(WiiMoteEmu::PadMapping[controller].ID);

	// Update the enabled checkbox
	//m_Joyattach[controller]->SetValue(PadMapping[controller].enabled == 1 ? true : false);

	tmp << WiiMoteEmu::PadMapping[controller].Axis.Lx; m_AnalogLeftX[controller]->SetValue(tmp); tmp.clear();
	tmp << WiiMoteEmu::PadMapping[controller].Axis.Ly; m_AnalogLeftY[controller]->SetValue(tmp); tmp.clear();
	tmp << WiiMoteEmu::PadMapping[controller].Axis.Rx; m_AnalogRightX[controller]->SetValue(tmp); tmp.clear();
	tmp << WiiMoteEmu::PadMapping[controller].Axis.Ry; m_AnalogRightY[controller]->SetValue(tmp); tmp.clear();

	tmp << WiiMoteEmu::PadMapping[controller].Axis.Tl; m_AnalogTriggerL[controller]->SetValue(tmp); tmp.clear();
	tmp << WiiMoteEmu::PadMapping[controller].Axis.Tr; m_AnalogTriggerR[controller]->SetValue(tmp); tmp.clear();
	
	// Update the deadzone and controller type controls
	m_TriggerType[controller]->SetSelection(WiiMoteEmu::PadMapping[controller].triggertype);
	m_ComboDeadZoneLeft[controller]->SetSelection(WiiMoteEmu::PadMapping[controller].DeadZoneL);
	m_ComboDeadZoneRight[controller]->SetSelection(WiiMoteEmu::PadMapping[controller].DeadZoneR);
	m_ComboDiagonal[controller]->SetValue(wxString::FromAscii(WiiMoteEmu::PadMapping[controller].SDiagonal.c_str()));
	m_CheckC2S[controller]->SetValue(WiiMoteEmu::PadMapping[controller].bCircle2Square);
	m_TiltInvertRoll[controller]->SetValue(WiiMoteEmu::PadMapping[controller].bRollInvert);
	m_TiltInvertPitch[controller]->SetValue(WiiMoteEmu::PadMapping[controller].bPitchInvert);

	// Wiimote
#ifdef _WIN32
	for (int x = 0; x < WM_CONTROLS; x++)
			m_Button_Wiimote[x][controller]->SetLabel(wxString::FromAscii(
			InputCommon::VKToString(WiiMoteEmu::PadMapping[controller].Wm.keyForControls[x]).c_str()));
	if(g_Config.iExtensionConnected == EXT_NUNCHUCK)
	{
		for (int x = 0; x < NC_CONTROLS; x++)
			m_Button_NunChuck[x][controller]->SetLabel(wxString::FromAscii(
			InputCommon::VKToString(WiiMoteEmu::PadMapping[controller].Nc.keyForControls[x]).c_str()));
	}
	else if(g_Config.iExtensionConnected == EXT_CLASSIC_CONTROLLER)
	{
		for (int x = 0; x < CC_CONTROLS; x++)
			m_Button_Classic[x][controller]->SetLabel(wxString::FromAscii(
			InputCommon::VKToString(WiiMoteEmu::PadMapping[controller].Cc.keyForControls[x]).c_str()));
	}
	else if(g_Config.iExtensionConnected == EXT_GUITARHERO3_CONTROLLER)
	{
		for (int x = 0; x < GH3_CONTROLS; x++)
			m_Button_GH3[x][controller]->SetLabel(wxString::FromAscii(
			InputCommon::VKToString(WiiMoteEmu::PadMapping[controller].GH3c.keyForControls[x]).c_str()));
	}
#elif defined(HAVE_X11) && HAVE_X11
	char keyStr[10] = {0};
	for (int x = 0; x < WM_CONTROLS; x++)
	{
		InputCommon::XKeyToString(WiiMoteEmu::PadMapping[controller].Wm.keyForControls[x], keyStr);
		m_Button_Wiimote[x][controller]->SetLabel(wxString::FromAscii(keyStr));
	}
	if(g_Config.iExtensionConnected == EXT_NUNCHUCK)
	{
		for (int x = 0; x < NC_CONTROLS; x++)
		{
			InputCommon::XKeyToString(WiiMoteEmu::PadMapping[controller].Nc.keyForControls[x], keyStr);
			m_Button_NunChuck[x][controller]->SetLabel(wxString::FromAscii(keyStr));
		}

	}
	else if(g_Config.iExtensionConnected == EXT_CLASSIC_CONTROLLER)
	{
		for (int x = 0; x < CC_CONTROLS; x++)
		{
			InputCommon::XKeyToString(WiiMoteEmu::PadMapping[controller].Cc.keyForControls[x], keyStr);
			m_Button_Classic[x][controller]->SetLabel(wxString::FromAscii(keyStr));
		}
	}
	else if(g_Config.iExtensionConnected == EXT_GUITARHERO3_CONTROLLER)
	{
		for (int x = 0; x < GH3_CONTROLS; x++)
		{
			InputCommon::XKeyToString(WiiMoteEmu::PadMapping[controller].GH3c.keyForControls[x], keyStr);
			m_Button_GH3[x][controller]->SetLabel(wxString::FromAscii(keyStr));
		}
	}
#endif
	//DEBUG_LOG(WIIMOTE, "m_bWmA[%i] = %i = %s", controller, WiiMoteEmu::PadMapping[controller].Wm.keyForControls[0], InputCommon::VKToString(WiiMoteEmu::PadMapping[controller].Wm.keyForControls[0]).c_str());
}

/* Populate the PadMapping array with the dialog items settings (for example
   selected joystick, enabled or disabled status and so on) */

void WiimotePadConfigDialog::SaveButtonMapping(int controller, bool DontChangeId, int FromSlot)
{
	// Log
	NOTICE_LOG(WIIMOTE, "SaveButtonMapping | controller:%i FromSlot:%i", controller, FromSlot);

	// Temporary storage
	wxString tmp;
	long value;

	// Save from or to the same or different slots
	if (FromSlot == -1) FromSlot = controller;
	
	// TEMPORARY
	// There is	only one slot at the moment	
	if (FromSlot > MAX_WIIMOTES-1) FromSlot = MAX_WIIMOTES-1;

	// Replace "" with "-1" in the GUI controls
	ToBlank(false);

	/* Set physical device Id. GetSelection() should never be -1 here so we don't check that it's zero or higher. If it's possible that it can be
	   -1 that's a bug that should be fixed. Because the m_Joyname[] combo box should always show <No Gamepad Detected>, or a gamepad name, not a
	   a blank selection. */
	if (!DontChangeId)
	{
		WiiMoteEmu::PadMapping[controller].ID = WiiMoteEmu::joyinfo.at(m_Joyname[FromSlot]->GetSelection()).ID;
		WiiMoteEmu::PadMapping[controller].Name = m_Joyname[FromSlot]->GetValue().mb_str();
	}
	// Set enabled or disable status 
	if (FromSlot == controller)
		WiiMoteEmu::PadMapping[controller].enabled = true; //m_Joyattach[FromSlot]->GetValue(); // Only enable one
	// Set other settings
	//WiiMoteEmu::PadMapping[controller].controllertype = m_ControlType[FromSlot]->GetSelection();
	WiiMoteEmu::PadMapping[controller].triggertype = m_TriggerType[FromSlot]->GetSelection();
	WiiMoteEmu::PadMapping[controller].DeadZoneL = m_ComboDeadZoneLeft[FromSlot]->GetSelection();
	WiiMoteEmu::PadMapping[controller].DeadZoneR = m_ComboDeadZoneRight[FromSlot]->GetSelection();
	WiiMoteEmu::PadMapping[controller].SDiagonal = m_ComboDiagonal[FromSlot]->GetLabel().mb_str();
	WiiMoteEmu::PadMapping[controller].bCircle2Square = m_CheckC2S[FromSlot]->IsChecked();	
	WiiMoteEmu::PadMapping[controller].bRollInvert = m_TiltInvertRoll[FromSlot]->IsChecked();	
	WiiMoteEmu::PadMapping[controller].bPitchInvert = m_TiltInvertPitch[FromSlot]->IsChecked();	

	// The analog buttons
	m_AnalogLeftX[FromSlot]->GetValue().ToLong(&value); WiiMoteEmu::PadMapping[controller].Axis.Lx = value; tmp.clear();
	m_AnalogLeftY[FromSlot]->GetValue().ToLong(&value); WiiMoteEmu::PadMapping[controller].Axis.Ly = value; tmp.clear();
	m_AnalogRightX[FromSlot]->GetValue().ToLong(&value); WiiMoteEmu::PadMapping[controller].Axis.Rx = value; tmp.clear();
	m_AnalogRightY[FromSlot]->GetValue().ToLong(&value); WiiMoteEmu::PadMapping[controller].Axis.Ry = value; tmp.clear();

	// The shoulder buttons
	m_AnalogTriggerL[FromSlot]->GetValue().ToLong(&value); WiiMoteEmu::PadMapping[controller].Axis.Tl = value;
	m_AnalogTriggerR[FromSlot]->GetValue().ToLong(&value); WiiMoteEmu::PadMapping[controller].Axis.Tr = value;			

	//DEBUG_LOG(WIIMOTE, "WiiMoteEmu::PadMapping[%i].ID = %i, m_Joyname[%i]->GetSelection() = %i",
	//	controller, WiiMoteEmu::PadMapping[controller].ID, FromSlot, m_Joyname[FromSlot]->GetSelection());

	// Replace "-1" with "" 
	ToBlank();
}

// Save keyboard key mapping
void WiimotePadConfigDialog::SaveKeyboardMapping(int Controller, int Id, int Key)
{
	if (IDB_WM_A <= Id && Id <= IDB_WM_PITCH_R)
	{
		WiiMoteEmu::PadMapping[Controller].Wm.keyForControls[Id - IDB_WM_A] = Key;
	}
	else if (IDB_NC_Z <= Id && Id <= IDB_NC_SHAKE)
	{
		WiiMoteEmu::PadMapping[Controller].Nc.keyForControls[Id - IDB_NC_Z] = Key;
	}
	else if (IDB_CC_A <= Id && Id <= IDB_CC_RD)
	{
		WiiMoteEmu::PadMapping[Controller].Cc.keyForControls[Id - IDB_CC_A] = Key;
	}
	else if (IDB_GH3_GREEN <= Id && Id <= IDB_GH3_STRUM_DOWN)
	{
		WiiMoteEmu::PadMapping[Controller].GH3c.keyForControls[Id - IDB_GH3_GREEN] = Key;
	}
	//DEBUG_LOG(WIIMOTE, "WiiMoteEmu::PadMapping[%i].Wm.A = %i", Controller, WiiMoteEmu::PadMapping[Controller].Wm.A);
}

// Replace the harder to understand -1 with "" for the sake of user friendliness
void WiimotePadConfigDialog::ToBlank(bool _ToBlank)
{
	if (!ControlsCreated) return;

	for (int j = 0; j < 1; j++)
	{
		if(_ToBlank)
		{
			for(int i = IDB_ANALOG_LEFT_X; i <= IDB_TRIGGER_R; i++)
#if ! defined _WIN32 && ! wxCHECK_VERSION(2, 9, 0)
				if(GetButtonText(i, j).ToAscii() == "-1")
					SetButtonText(i, (char *)"", j);
#else
			if(GetButtonText(i, j) == wxT("-1")) SetButtonText(i, "", j);
#endif
		}
		else
		{
			for(int i = IDB_ANALOG_LEFT_X; i <= IDB_TRIGGER_R; i++)
				if(GetButtonText(i, j).IsEmpty())
					SetButtonText(i, (char *)"-1", j);
		}
	}	
}


// Update the textbox for the buttons
void WiimotePadConfigDialog::SetButtonText(int id, char text[128], int _Page)
{
	// Set controller value
	int controller;	
	if (_Page == -1) controller = Page; else controller = _Page;

	if (IDB_WM_A <= id && id <= IDB_WM_PITCH_R)
		m_Button_Wiimote[id - IDB_WM_A][controller]->SetLabel(wxString::FromAscii(text)); 
	else if (IDB_NC_Z <= id && id <= IDB_NC_SHAKE)
		m_Button_NunChuck[id - IDB_NC_Z][controller]->SetLabel(wxString::FromAscii(text)); 
	else if (IDB_CC_A <= id && id <= IDB_CC_RD)
		m_Button_Classic[id - IDB_CC_A][controller]->SetLabel(wxString::FromAscii(text)); 
	else if (IDB_GH3_GREEN <= id && id <= IDB_GH3_STRUM_DOWN)
		m_Button_GH3[id - IDB_GH3_GREEN][controller]->SetLabel(wxString::FromAscii(text)); 
	else switch(id)
	{
		case IDB_ANALOG_LEFT_X: m_AnalogLeftX[controller]->SetValue(wxString::FromAscii(text)); break;
		case IDB_ANALOG_LEFT_Y: m_AnalogLeftY[controller]->SetValue(wxString::FromAscii(text)); break;
		case IDB_ANALOG_RIGHT_X: m_AnalogRightX[controller]->SetValue(wxString::FromAscii(text)); break;
		case IDB_ANALOG_RIGHT_Y: m_AnalogRightY[controller]->SetValue(wxString::FromAscii(text)); break;

		case IDB_TRIGGER_L: m_AnalogTriggerL[controller]->SetValue(wxString::FromAscii(text)); break;
		case IDB_TRIGGER_R: m_AnalogTriggerR[controller]->SetValue(wxString::FromAscii(text)); break;
		default: break;
	}
	//DEBUG_LOG(WIIMOTE, "SetButtonText: %s", text);
}

// Get the text in the textbox for the buttons
wxString WiimotePadConfigDialog::GetButtonText(int id, int _Page)
{
	//DEBUG_LOG(WIIMOTE, "GetButtonText: %i", id);

	// Set controller value
	int controller;	
	if (_Page == -1) controller = Page; else controller = _Page;

	switch(id)
	{
		// Analog Stick
		case IDB_ANALOG_LEFT_X: return m_AnalogLeftX[controller]->GetValue();
		case IDB_ANALOG_LEFT_Y: return m_AnalogLeftY[controller]->GetValue();
		case IDB_ANALOG_RIGHT_X: return m_AnalogRightX[controller]->GetValue();
		case IDB_ANALOG_RIGHT_Y: return m_AnalogRightY[controller]->GetValue();

		// Shoulder Buttons
		case IDB_TRIGGER_L: return m_AnalogTriggerL[controller]->GetValue();
		case IDB_TRIGGER_R: return m_AnalogTriggerR[controller]->GetValue();
		
		default: return wxString();
	}
}


// Configure button mapping

// Wait for button press

/* Loop or timer: There are basically two ways to do this. With a while() or
   for() loop, or with a timer. The downside with the while() or for() loop is
   that there is no way to stop it if the user should select to configure
   another button while we are still in an old loop. What will happen then is
   that we start another parallel loop (at least in Windows) that blocks the
   old loop. And our only option to wait for the old loop to finish is with a
   new loop, and that will block the old loop for as long as it's going
   on. Therefore a timer is easier to control. */
void WiimotePadConfigDialog::GetButtons(wxCommandEvent& event)
{
	DoGetButtons(event.GetId());
}

void WiimotePadConfigDialog::DoGetButtons(int _GetId)
{
	DEBUG_LOG(WIIMOTE, "DoGetButtons");

	bool _LiveUpdates = LiveUpdates;
	LiveUpdates = false;

	// Collect the starting values

	// Get the current controller	
	int Controller = Page;
	int PadID = WiiMoteEmu::PadMapping[Controller].ID;

	// Get the controller and trigger type
	int TriggerType = WiiMoteEmu::PadMapping[Controller].triggertype;

	// Collect the accepted buttons for this slot
	bool LeftRight = (_GetId == IDB_TRIGGER_L || _GetId == IDB_TRIGGER_R);

	bool Axis = (_GetId >= IDB_ANALOG_LEFT_X && _GetId <= IDB_TRIGGER_R)
		// Don't allow SDL axis input for the shoulder buttons if XInput is selected
		&& !(TriggerType == InputCommon::CTL_TRIGGER_XINPUT && (_GetId == IDB_TRIGGER_L || _GetId == IDB_TRIGGER_R) );
	
	bool XInput = (TriggerType == InputCommon::CTL_TRIGGER_XINPUT);

	bool Button = false; // No digital buttons allowed

	bool Hat = false; // No hats allowed

	bool NoTriggerFilter = g_Config.bNoTriggerFilter;

	// Values used in this function
	char format[128];
	int Seconds = 4; // Seconds to wait for
	int TimesPerSecond = 60; // How often to run the check

	// Values returned from InputCommon::GetButton()
	int value; // Axis value
	int type; // Button type
	int pressed = 0;
	bool Succeed = false;
	bool Stop = false; // Stop the timer

	//DEBUG_LOG(WIIMOTE, "Before (%i) Id:%i %i  IsRunning:%i",
	//	GetButtonWaitingTimer, GetButtonWaitingID, _GetId, m_ButtonMappingTimer->IsRunning());

	// If the Id has changed or the timer is not running we should start one
	if( GetButtonWaitingID != _GetId || !m_ButtonMappingTimer->IsRunning() )
	{
		if(m_ButtonMappingTimer->IsRunning())
		{
			m_ButtonMappingTimer->Stop();
			GetButtonWaitingTimer = 0;

			// Update the old textbox
			SetButtonText(GetButtonWaitingID, (char *)"");
		}

		 // Save the button Id
		GetButtonWaitingID = _GetId;

		// Reset the key in case we happen to have an old one
		g_Pressed = 0;

		// Update the text box
		sprintf(format, "[%d]", Seconds);
		SetButtonText(_GetId, format);

		// Start the timer
		#if wxUSE_TIMER
			m_ButtonMappingTimer->Start( floor((double)(1000 / TimesPerSecond)) );
		#endif
		DEBUG_LOG(WIIMOTE, "Timer Started for Pad:%i _GetId:%i"
			" Allowed input is Axis:%i LeftRight:%i XInput:%i Button:%i Hat:%i\n",
			WiiMoteEmu::PadMapping[Controller].ID, _GetId,
			Axis, LeftRight, XInput, Button, Hat);
	}

	// Check for buttons

	// If there is a timer but we should not create a new one
	else
	{
		InputCommon::GetButton(
			WiiMoteEmu::PadState[Page].joy, PadID,
			g_Pressed, value, type, pressed, Succeed, Stop,
			LeftRight, Axis, XInput, Button, Hat, NoTriggerFilter);
	}

	// Process results

	// Count each time
	GetButtonWaitingTimer++;

	// This is run every second
	if(GetButtonWaitingTimer % TimesPerSecond == 0)
	{
		// Current time
		int TmpTime = Seconds - (GetButtonWaitingTimer / TimesPerSecond);

		// Update text
		sprintf(format, "[%d]", TmpTime);
		SetButtonText(_GetId, format);

		/* Debug 
		DEBUG_LOG(WIIMOTE, "Keyboard: %i", g_Pressed);*/
	}

	// Time's up
	if( (GetButtonWaitingTimer / TimesPerSecond) >= Seconds )
	{
		Stop = true;
		// Leave a blank mapping
		SetButtonTextAll(_GetId, (char *)"-1");
	}

	// If we got a button
	if (Succeed)
	{
		Stop = true;
		// Write the number of the pressed button to the text box
		sprintf(format, "%d", pressed);
		SetButtonTextAll(_GetId, format);
	}

	// Stop the timer
	if (Stop)
	{
		m_ButtonMappingTimer->Stop();
		GetButtonWaitingTimer = 0;
		LiveUpdates = _LiveUpdates;

		/* Update the button mapping for all slots that use this device. (It
		   doesn't make sense to have several slots controlled by the same
		   device, but several DirectInput instances of different but identical
		   devices may possible have the same id, I don't know. So we have to
		   do this. The user may also have selected the same device for several
		   disabled slots. */
		SaveButtonMappingAll(Controller);

		DEBUG_LOG(WIIMOTE, "Timer Stopped for Pad:%i _GetId:%i",
			WiiMoteEmu::PadMapping[Controller].ID, _GetId);
	}

	// If we got a bad button
	if(g_Pressed == -1)
	{
		// Update text
		SetButtonTextAll(_GetId, (char *)"-1");
		
		// Notify the user
		wxMessageBox(wxString::Format(
			wxT("You selected a key with a to low key code (%i), please")
			wxT(" select another key with a higher key code."), pressed)
			, wxT("Notice"), wxICON_INFORMATION);		
	}
	
	// Debugging
	/*
	DEBUG_LOG(WIIMOTE, "Change: %i %i %i %i  '%s' '%s' '%s' '%s'",
		WiiMoteEmu::PadMapping[0].halfpress, WiiMoteEmu::PadMapping[1].halfpress, WiiMoteEmu::PadMapping[2].halfpress, WiiMoteEmu::PadMapping[3].halfpress,
		m_JoyButtonHalfpress[0]->GetValue().c_str(), m_JoyButtonHalfpress[1]->GetValue().c_str(), m_JoyButtonHalfpress[2]->GetValue().c_str(), m_JoyButtonHalfpress[3]->GetValue().c_str()
		);*/
}



// Show current input status
// Convert the 0x8000 range values to BoxW and BoxH for the plot
void WiimotePadConfigDialog::Convert2Box(int &x)
{
	// Border adjustment
	int BoxW_ = BoxW - 2; int BoxH_ = BoxH - 2;

	// Convert values
	x = (BoxW_ / 2) + (x * BoxW_ / (32767 * 2));
}

// Update the input status boxes
void WiimotePadConfigDialog::PadGetStatus()
{
	//DEBUG_LOG(WIIMOTE, "PadGetStatus");

	/* Return if it's not detected. The ID should never be less than zero here,
	   it can only be that because of a manual ini file change, but we make
	   that check anway. */
	if (WiiMoteEmu::PadMapping[Page].ID < 0 || WiiMoteEmu::PadMapping[Page].ID >= SDL_NumJoysticks())
	{
		m_TStatusLeftIn[Page]->SetLabel(wxT("Not connected"));
		m_TStatusLeftOut[Page]->SetLabel(wxT("Not connected"));
		m_TStatusRightIn[Page]->SetLabel(wxT("Not connected"));
		m_TStatusRightOut[Page]->SetLabel(wxT("Not connected"));
		m_TriggerStatusLx[Page]->SetLabel(wxT("0"));
		m_TriggerStatusRx[Page]->SetLabel(wxT("0"));
		return;
	}

	// Return if it's not enabled
	if (!WiiMoteEmu::PadMapping[Page].enabled)
	{
		m_TStatusLeftIn[Page]->SetLabel(wxT("Not enabled"));
		m_TStatusLeftOut[Page]->SetLabel(wxT("Not enabled"));
		m_TStatusRightIn[Page]->SetLabel(wxT("Not enabled"));
		m_TStatusRightOut[Page]->SetLabel(wxT("Not enabled"));
		m_TriggerStatusLx[Page]->SetLabel(wxT("0"));
		m_TriggerStatusRx[Page]->SetLabel(wxT("0"));
		return;
	}

	// Get physical device status
	int ID = WiiMoteEmu::PadMapping[Page].ID;
	int TriggerType = WiiMoteEmu::PadMapping[Page].triggertype;

	// Check that Dolphin is in focus, otherwise don't update the pad status
	//if (IsFocus())
		WiiMoteEmu::GetJoyState(WiiMoteEmu::PadState[Page], WiiMoteEmu::PadMapping[Page], Page);

	// Analog stick
	// Set Deadzones perhaps out of function
	//int deadzone = (int)(((float)(128.00/100.00)) * (float)(PadMapping[_numPAD].deadzone+1));
	//int deadzone2 = (int)(((float)(-128.00/100.00)) * (float)(PadMapping[_numPAD].deadzone+1));

	// Get original values
	int main_x = WiiMoteEmu::PadState[Page].Axis.Lx;
	int main_y = WiiMoteEmu::PadState[Page].Axis.Ly;
    int right_x = WiiMoteEmu::PadState[Page].Axis.Rx;
	int right_y = WiiMoteEmu::PadState[Page].Axis.Ry;

	// Get adjusted values
	int main_x_after = main_x, main_y_after = main_y;
	int right_x_after = right_x, right_y_after = right_y;
	// Produce square
	if(WiiMoteEmu::PadMapping[Page].bCircle2Square)
	{
		InputCommon::Square2Circle(main_x_after, main_y_after, Page, WiiMoteEmu::PadMapping[Page].SDiagonal, true);
	}	
	// Check dead zone
	float DeadZoneLeft = (float)WiiMoteEmu::PadMapping[Page].DeadZoneL / 100.0;
	float DeadZoneRight = (float)WiiMoteEmu::PadMapping[Page].DeadZoneR / 100.0;
	if (InputCommon::IsDeadZone(DeadZoneLeft, main_x_after, main_y_after))
	{
		main_x_after = 0;
		main_y_after = 0;
	}
	if (InputCommon::IsDeadZone(DeadZoneRight, right_x_after, right_y_after))
	{
		right_x_after = 0;
		right_y_after = 0;
	}

	// Show the adjusted angles in the status box
	// Change 0x8000 to 180
	/*
	float x8000 = 0x8000;
	main_x_after = main_x_after * (180 / x8000);
	main_y_after = main_y_after * (180 / x8000);
	float f_main_x_after = (float)main_x_after;
	float f_main_y_after = (float)main_y_after;
	WiiMoteEmu::AdjustAngles(f_main_x_after, f_main_y_after);
	//WiiMoteEmu::AdjustAngles(f_main_x_after, f_main_y_after, true);
	main_x_after = (int)f_main_x_after;
	main_y_after = (int)f_main_y_after;
	// Change back 180 to 0x8000 
	main_x_after = main_x_after * (x8000 / 180);
	main_y_after = main_y_after * (x8000 / 180);
	*/

	// Convert the values to fractions
	float f_x = main_x / 32767.0;
	float f_y = main_y / 32767.0;
	float f_x_aft = main_x_after / 32767.0;
	float f_y_aft = main_y_after / 32767.0;

	float f_Rx = right_x / 32767.0;
	float f_Ry = right_y / 32767.0;
	float f_Rx_aft = right_x_after / 32767.0;
	float f_Ry_aft = right_y_after / 32767.0;

	m_TStatusLeftIn[Page]->SetLabel(wxString::Format(
		wxT("x:%1.2f y:%1.2f"), f_x, f_y	
		));
	m_TStatusLeftOut[Page]->SetLabel(wxString::Format(
		wxT("x:%1.2f y:%1.2f"), f_x_aft, f_y_aft
		));
	m_TStatusRightIn[Page]->SetLabel(wxString::Format(
		wxT("x:%1.2f y:%1.2f"), f_Rx, f_Ry
		));
	m_TStatusRightOut[Page]->SetLabel(wxString::Format(
		wxT("x:%1.2f y:%1.2f"), f_Rx_aft, f_Ry_aft
		));

	// Adjust the values for the plot
	Convert2Box(main_x); Convert2Box(main_y);
	Convert2Box(right_x); Convert2Box(right_y);

	Convert2Box(main_x_after); Convert2Box(main_y_after);
	Convert2Box(right_x_after); Convert2Box(right_y_after);

	// Adjust the dot
	m_bmpDotLeftIn[Page]->SetPosition(wxPoint(main_x, main_y));
	m_bmpDotLeftOut[Page]->SetPosition(wxPoint(main_x_after, main_y_after));
	m_bmpDotRightIn[Page]->SetPosition(wxPoint(right_x, right_y));
	m_bmpDotRightOut[Page]->SetPosition(wxPoint(right_x_after, right_y_after));


	// Triggers

	// Get the selected keys
	long Left, Right;
	m_AnalogTriggerL[Page]->GetValue().ToLong(&Left);
	m_AnalogTriggerR[Page]->GetValue().ToLong(&Right);

	// Get the trigger values
	int TriggerLeft = WiiMoteEmu::PadState[Page].Axis.Tl;
	int TriggerRight = WiiMoteEmu::PadState[Page].Axis.Tr;

	// Convert the triggers values
	if (WiiMoteEmu::PadMapping[Page].triggertype == InputCommon::CTL_TRIGGER_SDL)
	{
		TriggerLeft = InputCommon::Pad_Convert(TriggerLeft);
		TriggerRight = InputCommon::Pad_Convert(TriggerRight);
	}

	m_TriggerStatusLx[Page]->SetLabel(wxString::Format(
		wxT("%03i"), TriggerLeft));
	m_TriggerStatusRx[Page]->SetLabel(wxString::Format(
		wxT("%03i"), TriggerRight));
}

// Slow timer, once a second
void WiimotePadConfigDialog::Update(wxTimerEvent& WXUNUSED(event))
{
	if (!LiveUpdates) return;
	
	// Don't run this the first time
	int OldNumDIDevices;
	if (WiiMoteEmu::NumDIDevices == -1)
		OldNumDIDevices = InputCommon::SearchDIDevices();
	else
	// Search for connected devices and update dialog
		OldNumDIDevices = WiiMoteEmu::NumDIDevices;	
	WiiMoteEmu::NumDIDevices = InputCommon::SearchDIDevices();	

	// Update if a pad has been connected/disconnected. Todo: Add a better check that also takes into consideration the pad id
	// and other things to ensure nothing has changed
	//DEBUG_LOG(WIIMOTE, "Found %i devices", WiiMoteEmu::NumDIDevices);
	//DEBUG_LOG(WIIMOTE, "Update: %i %i", OldNumPads, WiiMoteEmu::NumPads);
	if (OldNumDIDevices != WiiMoteEmu::NumDIDevices)
	{
		WiiMoteEmu::LocalSearchDevicesReset(WiiMoteEmu::joyinfo, WiiMoteEmu::NumPads);
		WiimotePadConfigDialog::UpdateDeviceList();
	}
}

// Populate the advanced tab
void WiimotePadConfigDialog::UpdatePad(wxTimerEvent& WXUNUSED(event))
{
	if (!WiiMoteEmu::IsPolling()) return;

	//DEBUG_LOG(WIIMOTE, "UpdatePad");
	
	// Show the current status
	#ifdef SHOW_PAD_STATUS
		m_pStatusBar->SetLabel(wxString::FromAscii(InputCommon::DoShowStatus(
		Page, m_Joyname[Page]->GetSelection(),
		WiiMoteEmu::PadMapping, WiiMoteEmu::PadState, WiiMoteEmu::joyinfo
		).c_str()));
	#endif

	//LogMsg("Abc%s\n", ShowStatus(notebookpage).c_str());

	PadGetStatus();
}

