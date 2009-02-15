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


//////////////////////////////////////////////////////////////////////////////////////////
// Includes
// ¯¯¯¯¯¯¯¯¯¯¯¯¯
//#include "Common.h" // for u16
#include "CommonTypes.h" // for u16
#include "IniFile.h"
#include "Timer.h"

#include "wiimote_real.h" // Local
#include "wiimote_hid.h"
#include "main.h"
#include "ConfigDlg.h"
#include "Config.h"
#include "EmuMain.h" // for LoadRecordedMovements()
#include "EmuSubroutines.h" // for WmRequestStatus
#include "EmuDefinitions.h" // for joyinfo
//////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////////
// Change Joystick
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
/* Function: When changing the joystick we save and load the settings and update the PadMapping
   and PadState array. PadState[].joy is the gamepad handle that is used to access the pad throughout
   the plugin. Joyinfo[].joy is only used the first time the pads are checked. */
void ConfigDialog::DoChangeJoystick()
{
	// Close the current pad, unless it's used by another slot
	//if (PadMapping[notebookpage].enabled) PadClose(notebookpage);

	// Before changing the pad we save potential changes to the current pad
	DoSave(true);
	
	// Load the settings for the new Id
	g_Config.Load(true);
	UpdateGUI(Page); // Update the GUI

	// Open the new pad
	if (WiiMoteEmu::PadMapping[Page].enabled) PadOpen(Page);
}
void ConfigDialog::PadOpen(int Open) // Open for slot 1, 2, 3 or 4
{
	// Check that we got a good pad
	if (!WiiMoteEmu::joyinfo.at(WiiMoteEmu::PadMapping[Open].ID).Good)
	{
		Console::Print("A bad pad was selected\n");
		WiiMoteEmu::PadState[Open].joy = NULL;
		return;
	}

	Console::Print("Update the Slot %i handle to Id %i\n", Page, WiiMoteEmu::PadMapping[Open].ID);
	WiiMoteEmu::PadState[Open].joy = SDL_JoystickOpen(WiiMoteEmu::PadMapping[Open].ID);
}
void ConfigDialog::PadClose(int Close) // Close for slot 1, 2, 3 or 4
{
	if (SDL_JoystickOpened(WiiMoteEmu::PadMapping[Close].ID)) SDL_JoystickClose(WiiMoteEmu::PadState[Close].joy);
	WiiMoteEmu::PadState[Close].joy = NULL;
}

void ConfigDialog::DoChangeDeadZone(bool Left)
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
//////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////////
// Change settings
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯

// Set the button text for all four Wiimotes
void ConfigDialog::SetButtonTextAll(int id, char text[128])
{
	for (int i = 0; i < 1; i++)
	{
		// Safety check to avoid crash
		if(WiiMoteEmu::joyinfo.size() > WiiMoteEmu::PadMapping[i].ID)
			if (WiiMoteEmu::joyinfo[WiiMoteEmu::PadMapping[i].ID].Name == WiiMoteEmu::joyinfo[WiiMoteEmu::PadMapping[Page].ID].Name)
				SetButtonText(id, text, i);
	};
}


void ConfigDialog::SaveButtonMappingAll(int Slot)
{
	//Console::Print("SaveButtonMappingAll()\n");

	for (int i = 0; i < 4; i++)
	{
		// This can occur when no gamepad is detected
		if(WiiMoteEmu::joyinfo.size() > WiiMoteEmu::PadMapping[i].ID)
			if (WiiMoteEmu::joyinfo[WiiMoteEmu::PadMapping[i].ID].Name == WiiMoteEmu::joyinfo[WiiMoteEmu::PadMapping[Slot].ID].Name)
				SaveButtonMapping(i, false, Slot);
	}
}

// Set dialog items from saved values
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
void ConfigDialog::UpdateGUIButtonMapping(int controller)
{	
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
	m_bWmA[controller]->SetLabel(InputCommon::VKToString(WiiMoteEmu::PadMapping[controller].Wm.A));
	m_bWmB[controller]->SetLabel(InputCommon::VKToString(WiiMoteEmu::PadMapping[controller].Wm.B)); 
	m_bWm1[controller]->SetLabel(InputCommon::VKToString(WiiMoteEmu::PadMapping[controller].Wm.One));
	m_bWm2[controller]->SetLabel(InputCommon::VKToString(WiiMoteEmu::PadMapping[controller].Wm.Two)); 
	m_bWmP[controller]->SetLabel(InputCommon::VKToString(WiiMoteEmu::PadMapping[controller].Wm.P));
	m_bWmM[controller]->SetLabel(InputCommon::VKToString(WiiMoteEmu::PadMapping[controller].Wm.M)); 
	m_bWmH[controller]->SetLabel(InputCommon::VKToString(WiiMoteEmu::PadMapping[controller].Wm.H));
	m_bWmL[controller]->SetLabel(InputCommon::VKToString(WiiMoteEmu::PadMapping[controller].Wm.L)); 
	m_bWmR[controller]->SetLabel(InputCommon::VKToString(WiiMoteEmu::PadMapping[controller].Wm.R));
	m_bWmU[controller]->SetLabel(InputCommon::VKToString(WiiMoteEmu::PadMapping[controller].Wm.U)); 
	m_bWmD[controller]->SetLabel(InputCommon::VKToString(WiiMoteEmu::PadMapping[controller].Wm.D));
	m_bWmShake[controller]->SetLabel(InputCommon::VKToString(WiiMoteEmu::PadMapping[controller].Wm.Shake));

	// Nunchuck
	m_bNcZ[controller]->SetLabel(InputCommon::VKToString(WiiMoteEmu::PadMapping[controller].Nc.Z));
	m_bNcC[controller]->SetLabel(InputCommon::VKToString(WiiMoteEmu::PadMapping[controller].Nc.C));
	m_bNcL[controller]->SetLabel(InputCommon::VKToString(WiiMoteEmu::PadMapping[controller].Nc.L));
	m_bNcR[controller]->SetLabel(InputCommon::VKToString(WiiMoteEmu::PadMapping[controller].Nc.R));
	m_bNcU[controller]->SetLabel(InputCommon::VKToString(WiiMoteEmu::PadMapping[controller].Nc.U));
	m_bNcD[controller]->SetLabel(InputCommon::VKToString(WiiMoteEmu::PadMapping[controller].Nc.D));
	m_bNcShake[controller]->SetLabel(InputCommon::VKToString(WiiMoteEmu::PadMapping[controller].Nc.Shake));

	//Console::Print("m_bWmA[%i] = %i = %s\n", controller, WiiMoteEmu::PadMapping[controller].Wm.A, InputCommon::VKToString(WiiMoteEmu::PadMapping[controller].Wm.A).c_str());
}

/* Populate the PadMapping array with the dialog items settings (for example
   selected joystick, enabled or disabled status and so on) */
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
void ConfigDialog::SaveButtonMapping(int controller, bool DontChangeId, int FromSlot)
{
	// Temporary storage
	wxString tmp;
	long value;

	// Save from or to the same or different slots
	if (FromSlot == -1) FromSlot = controller;

	// Replace "" with "-1" in the GUI controls
	ToBlank(false);

	// Set physical device Id
	if(!DontChangeId) WiiMoteEmu::PadMapping[controller].ID = m_Joyname[FromSlot]->GetSelection();
	// Set enabled or disable status 
	if(FromSlot == controller) WiiMoteEmu::PadMapping[controller].enabled = true; //m_Joyattach[FromSlot]->GetValue(); // Only enable one
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

	//Console::Print("WiiMoteEmu::PadMapping[%i].deadzone = %i, m_ComboDeadZone[%i]->GetSelection() = %i\n",
	//	controller, WiiMoteEmu::PadMapping[controller].deadzone, FromSlot, m_ComboDeadZone[FromSlot]->GetSelection());

	// Replace "-1" with "" 
	ToBlank();
}

// Save keyboard key mapping
void ConfigDialog::SaveKeyboardMapping(int Controller, int Id, int Key)
{
	switch(Id)
	{
	// Wiimote
	case IDB_WM_A: WiiMoteEmu::PadMapping[Controller].Wm.A = Key; break;
	case IDB_WM_B: WiiMoteEmu::PadMapping[Controller].Wm.B = Key; break;
	case IDB_WM_1: WiiMoteEmu::PadMapping[Controller].Wm.One = Key; break;
	case IDB_WM_2: WiiMoteEmu::PadMapping[Controller].Wm.Two = Key; break;
	case IDB_WM_P: WiiMoteEmu::PadMapping[Controller].Wm.P = Key; break;
	case IDB_WM_M: WiiMoteEmu::PadMapping[Controller].Wm.M = Key; break;
	case IDB_WM_H: WiiMoteEmu::PadMapping[Controller].Wm.H = Key; break;
	case IDB_WM_L: WiiMoteEmu::PadMapping[Controller].Wm.L = Key; break;
	case IDB_WM_R: WiiMoteEmu::PadMapping[Controller].Wm.R = Key; break;
	case IDB_WM_U: WiiMoteEmu::PadMapping[Controller].Wm.U = Key; break;
	case IDB_WM_D: WiiMoteEmu::PadMapping[Controller].Wm.D = Key; break;
	case IDB_WM_SHAKE: WiiMoteEmu::PadMapping[Controller].Wm.Shake = Key; break;

	// Nunchuck
	case IDB_NC_Z: WiiMoteEmu::PadMapping[Controller].Nc.Z = Key; break;
	case IDB_NC_C: WiiMoteEmu::PadMapping[Controller].Nc.C = Key; break;
	case IDB_NC_L: WiiMoteEmu::PadMapping[Controller].Nc.L = Key; break;
	case IDB_NC_R: WiiMoteEmu::PadMapping[Controller].Nc.R = Key; break;
	case IDB_NC_U: WiiMoteEmu::PadMapping[Controller].Nc.U = Key; break;
	case IDB_NC_D: WiiMoteEmu::PadMapping[Controller].Nc.D = Key; break;
	case IDB_NC_SHAKE: WiiMoteEmu::PadMapping[Controller].Nc.Shake = Key; break;
	}

	//Console::Print("WiiMoteEmu::PadMapping[%i].Wm.A = %i", Controller, WiiMoteEmu::PadMapping[Controller].Wm.A);
}

// Replace the harder to understand -1 with "" for the sake of user friendliness
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
void ConfigDialog::ToBlank(bool ToBlank)
{
	if (!ControlsCreated) return;

	for (int j = 0; j < 4; j++)
	{
		if(ToBlank)
		{
			for(int i = IDB_ANALOG_LEFT_X; i <= IDB_TRIGGER_R; i++)
				#ifndef _WIN32
					if(GetButtonText(i, j).ToAscii() == "-1") SetButtonText(i, "", j);
				#else
					if(GetButtonText(i, j) == "-1") SetButtonText(i, "", j);
				#endif
		}
		else
		{
			for(int i = IDB_ANALOG_LEFT_X; i <= IDB_TRIGGER_R; i++)
				if(GetButtonText(i, j).IsEmpty()) SetButtonText(i, "-1", j);
		}
	}	
}
//////////////////////////////////////




// Update the textbox for the buttons
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
void ConfigDialog::SetButtonText(int id, char text[128], int _Page)
{
	// Set controller value
	int controller;	
	if (_Page == -1) controller = Page; else controller = _Page;

	switch(id)
	{
		case IDB_ANALOG_LEFT_X: m_AnalogLeftX[controller]->SetValue(wxString::FromAscii(text)); break;
		case IDB_ANALOG_LEFT_Y: m_AnalogLeftY[controller]->SetValue(wxString::FromAscii(text)); break;
		case IDB_ANALOG_RIGHT_X: m_AnalogRightX[controller]->SetValue(wxString::FromAscii(text)); break;
		case IDB_ANALOG_RIGHT_Y: m_AnalogRightY[controller]->SetValue(wxString::FromAscii(text)); break;

		case IDB_TRIGGER_L: m_AnalogTriggerL[controller]->SetValue(wxString::FromAscii(text)); break;
		case IDB_TRIGGER_R: m_AnalogTriggerR[controller]->SetValue(wxString::FromAscii(text)); break;

		// Wiimote
		case IDB_WM_A: m_bWmA[controller]->SetLabel(wxString::FromAscii(text)); break;
		case IDB_WM_B: m_bWmB[controller]->SetLabel(wxString::FromAscii(text)); break;
		case IDB_WM_1: m_bWm1[controller]->SetLabel(wxString::FromAscii(text)); break;
		case IDB_WM_2: m_bWm2[controller]->SetLabel(wxString::FromAscii(text)); break;
		case IDB_WM_P: m_bWmP[controller]->SetLabel(wxString::FromAscii(text)); break;
		case IDB_WM_M: m_bWmM[controller]->SetLabel(wxString::FromAscii(text)); break;
		case IDB_WM_H: m_bWmH[controller]->SetLabel(wxString::FromAscii(text)); break;
		case IDB_WM_L: m_bWmL[controller]->SetLabel(wxString::FromAscii(text)); break;
		case IDB_WM_R: m_bWmR[controller]->SetLabel(wxString::FromAscii(text)); break;
		case IDB_WM_U: m_bWmU[controller]->SetLabel(wxString::FromAscii(text)); break;
		case IDB_WM_D: m_bWmD[controller]->SetLabel(wxString::FromAscii(text)); break;
		case IDB_WM_SHAKE: m_bWmShake[controller]->SetLabel(wxString::FromAscii(text)); break;

		// Nunchuck
		case IDB_NC_Z: m_bNcZ[controller]->SetLabel(wxString::FromAscii(text)); break;
		case IDB_NC_C: m_bNcC[controller]->SetLabel(wxString::FromAscii(text)); break;
		case IDB_NC_L: m_bNcL[controller]->SetLabel(wxString::FromAscii(text)); break;
		case IDB_NC_R: m_bNcR[controller]->SetLabel(wxString::FromAscii(text)); break;
		case IDB_NC_U: m_bNcU[controller]->SetLabel(wxString::FromAscii(text)); break;
		case IDB_NC_D: m_bNcD[controller]->SetLabel(wxString::FromAscii(text)); break;
		case IDB_NC_SHAKE: m_bNcShake[controller]->SetLabel(wxString::FromAscii(text)); break;

		default: break;
	}
	//Console::Print("SetButtonText: %s\n", text);
}

// Get the text in the textbox for the buttons
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
wxString ConfigDialog::GetButtonText(int id, int _Page)
{
	//Console::Print("GetButtonText: %i\n", id);

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
void ConfigDialog::GetButtons(wxCommandEvent& event)
{
	DoGetButtons(event.GetId());
}

void ConfigDialog::DoGetButtons(int GetId)
{
	// =============================================
	// Collect the starting values
	// ----------------

	// Get the current controller	
	int Controller = Page;
	int PadID = WiiMoteEmu::PadMapping[Controller].ID;

	// Get the controller and trigger type
	int TriggerType = WiiMoteEmu::PadMapping[Controller].triggertype;

	// Collect the accepted buttons for this slot
	bool LeftRight = (GetId == IDB_TRIGGER_L || GetId == IDB_TRIGGER_R);

	bool Axis = (GetId >= IDB_ANALOG_LEFT_X && GetId <= IDB_TRIGGER_R)
		// Don't allow SDL axis input for the shoulder buttons if XInput is selected
		&& !(TriggerType == InputCommon::CTL_TRIGGER_XINPUT && (GetId == IDB_TRIGGER_L || GetId == IDB_TRIGGER_R) );
	
	bool XInput = (TriggerType == InputCommon::CTL_TRIGGER_XINPUT);

	bool Button = false; // No digital buttons allowed

	bool Hat = false; // No hats allowed

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
		Console::Print("Timer Started for Pad:%i GetId:%i\n"
			"Allowed input is Axis:%i LeftRight:%i XInput:%i Button:%i Hat:%i\n",
			WiiMoteEmu::PadMapping[Controller].ID, GetId,
			Axis, LeftRight, XInput, Button, Hat);
	}

	// ===============================================
	// Check for buttons
	// ----------------

	// If there is a timer but we should not create a new one
	else
	{
		InputCommon::GetButton(
			WiiMoteEmu::joyinfo[PadID].joy, PadID, WiiMoteEmu::joyinfo[PadID].NumButtons, WiiMoteEmu::joyinfo[PadID].NumAxes, WiiMoteEmu::joyinfo[PadID].NumHats, 
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

		/* Debug 
		Console::Print("Keyboard: %i\n", g_Pressed);*/
	}

	// Time's up
	if( (GetButtonWaitingTimer / TimesPerSecond) >= Seconds )
	{
		Stop = true;
		// Leave a blank mapping
		SetButtonTextAll(GetId, "-1");
	}

	// If we got a button
	if(Succeed)
	{
		Stop = true;		
		// Write the number of the pressed button to the text box
		sprintf(format, "%d", pressed);
		SetButtonTextAll(GetId, format);
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
		SaveButtonMappingAll(Controller);

		Console::Print("Timer Stopped for Pad:%i GetId:%i\n",
			WiiMoteEmu::PadMapping[Controller].ID, GetId);
	}

	// If we got a bad button
	if(g_Pressed == -1)
	{
		// Update text
		SetButtonTextAll(GetId, "-1");
		
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
		WiiMoteEmu::PadMapping[0].halfpress, WiiMoteEmu::PadMapping[1].halfpress, WiiMoteEmu::PadMapping[2].halfpress, WiiMoteEmu::PadMapping[3].halfpress,
		m_JoyButtonHalfpress[0]->GetValue().c_str(), m_JoyButtonHalfpress[1]->GetValue().c_str(), m_JoyButtonHalfpress[2]->GetValue().c_str(), m_JoyButtonHalfpress[3]->GetValue().c_str()
		);*/
}
/////////////////////////////////////////////////////////// Configure button mapping



//////////////////////////////////////////////////////////////////////////////////////////
// Show current input status
// ¯¯¯¯¯¯¯¯¯¯

// Convert the 0x8000 range values to BoxW and BoxH for the plot
void ConfigDialog::Convert2Box(int &x)
{
	// Border adjustment
	int BoxW_ = BoxW - 2; int BoxH_ = BoxH - 2;

	// Convert values
	x = (BoxW_ / 2) + (x * BoxW_ / (32767 * 2));
}

// Update the input status boxes
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯
void ConfigDialog::PadGetStatus()
{
	//Console::Print("SDL_WasInit: %i\n", SDL_WasInit(0));

	// Return if it's not detected
	if(WiiMoteEmu::PadMapping[Page].ID >= SDL_NumJoysticks())
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
	int PhysicalDevice = WiiMoteEmu::PadMapping[Page].ID;
	int TriggerType = WiiMoteEmu::PadMapping[Page].triggertype;
	
	// Check that Dolphin is in focus, otherwise don't update the pad status
	//if (IsFocus())
		WiiMoteEmu::GetJoyState(WiiMoteEmu::PadState[Page], WiiMoteEmu::PadMapping[Page], Page, WiiMoteEmu::joyinfo[WiiMoteEmu::PadMapping[Page].ID].NumButtons);


	//////////////////////////////////////
	// Analog stick
	// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
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
		std::vector<int> main_xy = InputCommon::Square2Circle(main_x, main_y, Page, WiiMoteEmu::PadMapping[Page].SDiagonal, true);
		main_x_after = main_xy.at(0);
		main_y_after = main_xy.at(1);
		//main_x = main_xy.at(0);
		//main_y = main_xy.at(1);
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

	// -------------------------------------------
	// Show the adjusted angles in the status box
	// --------------
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
	// ---------------------

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
	///////////////////// Analog stick


	//////////////////////////////////////
	// Triggers
	// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
	int TriggerValue = 0;

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
	///////////////////// Triggers
}

// Populate the advanced tab
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
void ConfigDialog::UpdatePad(wxTimerEvent& WXUNUSED(event))
{
	// Show the current status
	/*
	#ifdef SHOW_PAD_STATUS
		m_pStatusBar->SetLabel(wxString::Format(
			"%s", ShowStatus(notebookpage).c_str()
			));
	#endif*/

	//LogMsg("Abc%s\n", ShowStatus(notebookpage).c_str());

	PadGetStatus();
}
/////////////////////////////////////////////////////