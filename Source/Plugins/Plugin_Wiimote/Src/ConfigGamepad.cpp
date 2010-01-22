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
#include "EmuDefinitions.h"

#if defined(HAVE_X11) && HAVE_X11
	#include "X11InputBase.h"
#endif

// Replace the harder to understand -1 with "" for the sake of user friendliness
void WiimotePadConfigDialog::ToBlank(bool ToBlank, int Id)
{
	if (!m_ControlsCreated)
		return;

	if(ToBlank)
	{
		if (GetButtonText(Id) == wxString(wxT("-1")))
			SetButtonText(Id, wxString());
	}
	else
	{
		if (GetButtonText(Id).IsEmpty())
			SetButtonText(Id, wxString(wxT("-1")));
	}
}

void WiimotePadConfigDialog::DoChangeDeadZone()
{
	float Rad;

	Rad = (float)WiiMoteEmu::WiiMapping[m_Page].DeadZoneL * ((float)BoxW / 100.0) * 0.5;	
	m_bmpDeadZoneLeftIn[m_Page]->SetBitmap(CreateBitmapClear());
	m_bmpDeadZoneLeftIn[m_Page]->SetSize(0, 0);
	m_bmpDeadZoneLeftIn[m_Page]->SetBitmap(CreateBitmapDeadZone((int)Rad));
	m_bmpDeadZoneLeftIn[m_Page]->SetPosition(wxPoint(BoxW / 2 - (int)Rad, BoxH / 2 - (int)Rad));
	m_bmpDeadZoneLeftIn[m_Page]->Refresh();	

	Rad = (float)WiiMoteEmu::WiiMapping[m_Page].DeadZoneR * ((float)BoxW / 100.0) * 0.5;	
	m_bmpDeadZoneRightIn[m_Page]->SetBitmap(CreateBitmapClear());
	m_bmpDeadZoneRightIn[m_Page]->SetSize(0, 0);
	m_bmpDeadZoneRightIn[m_Page]->SetBitmap(CreateBitmapDeadZone((int)Rad));
	m_bmpDeadZoneRightIn[m_Page]->SetPosition(wxPoint(BoxW / 2 - (int)Rad, BoxH / 2 - (int)Rad));
	m_bmpDeadZoneRightIn[m_Page]->Refresh();
}

// Set dialog items from saved values


// Update the textbox for the buttons
void WiimotePadConfigDialog::SetButtonText(int id, const wxString &str)
{
	if (IDB_ANALOG_LEFT_X <= id && id <= IDB_TRIGGER_R)
		m_Button_Analog[id - IDB_ANALOG_LEFT_X][m_Page]->SetLabel(str);
	else if (IDB_WM_A <= id && id <= IDB_WM_SHAKE)
		m_Button_Wiimote[id - IDB_WM_A][m_Page]->SetLabel(str);
	else if (IDB_NC_Z <= id && id <= IDB_NC_SHAKE)
		m_Button_NunChuck[id - IDB_NC_Z][m_Page]->SetLabel(str);
	else if (IDB_CC_A <= id && id <= IDB_CC_RD)
		m_Button_Classic[id - IDB_CC_A][m_Page]->SetLabel(str);
	else if (IDB_GH3_GREEN <= id && id <= IDB_GH3_STRUM_DOWN)
		m_Button_GH3[id - IDB_GH3_GREEN][m_Page]->SetLabel(str);
}

// Get the text in the textbox for the buttons
wxString WiimotePadConfigDialog::GetButtonText(int id)
{
	if (IDB_ANALOG_LEFT_X <= id && id <= IDB_TRIGGER_R)
		return m_Button_Analog[id - IDB_ANALOG_LEFT_X][m_Page]->GetLabel();
	else if (IDB_WM_A <= id && id <= IDB_WM_SHAKE)
		return m_Button_Wiimote[id - IDB_WM_A][m_Page]->GetLabel(); 
	else if (IDB_NC_Z <= id && id <= IDB_NC_SHAKE)
		return m_Button_NunChuck[id - IDB_NC_Z][m_Page]->GetLabel(); 
	else if (IDB_CC_A <= id && id <= IDB_CC_RD)
		return m_Button_Classic[id - IDB_CC_A][m_Page]->GetLabel(); 
	else if (IDB_GH3_GREEN <= id && id <= IDB_GH3_STRUM_DOWN)
		return m_Button_GH3[id - IDB_GH3_GREEN][m_Page]->GetLabel();
		
	return wxString();
}

void WiimotePadConfigDialog::DoGetButtons(int _GetId)
{
	// Collect the starting values
	// Get the current controller	
	int PadID = WiiMoteEmu::WiiMapping[m_Page].ID;

	// Get the controller and trigger type
	int TriggerType = WiiMoteEmu::WiiMapping[m_Page].TriggerType;

	// Collect the accepted buttons for this slot
	bool LeftRight = (_GetId == IDB_TRIGGER_L || _GetId == IDB_TRIGGER_R);

	bool Axis = (_GetId >= IDB_ANALOG_LEFT_X && _GetId <= IDB_TRIGGER_R)
		// Don't allow SDL axis input for the shoulder buttons if XInput is selected
		&& !(TriggerType == InputCommon::CTL_TRIGGER_XINPUT && (_GetId == IDB_TRIGGER_L || _GetId == IDB_TRIGGER_R) );
	
	bool XInput = (TriggerType == InputCommon::CTL_TRIGGER_XINPUT);

	bool Button = (_GetId >= IDB_WM_A && _GetId <= IDB_GH3_STRUM_DOWN);

	bool Hat = (_GetId >= IDB_WM_A && _GetId <= IDB_GH3_STRUM_DOWN);

	bool NoTriggerFilter = false;

	// Values used in this function
	int Seconds = 4; // Seconds to wait for
	int TimesPerSecond = 40; // How often to run the check

	// Values returned from InputCommon::GetButton()
	int value; // Axis value or Hat value
	int type; // Button type

	int KeyPressed = 0;
	int pressed = 0;
	bool Succeed = false;
	bool Stop = false; // Stop the timer

	//DEBUG_LOG(WIIMOTE, "Before (%i) Id:%i %i  IsRunning:%i",
	//	GetButtonWaitingTimer, GetButtonWaitingID, _GetId, m_ButtonMappingTimer->IsRunning());

	// If the Id has changed or the timer is not running we should start one
	if( GetButtonWaitingID != _GetId || !m_ButtonMappingTimer->IsRunning() )
	{
		if(m_ButtonMappingTimer->IsRunning())
			m_ButtonMappingTimer->Stop();

		 // Save the button Id
		GetButtonWaitingID = _GetId;
		GetButtonWaitingTimer = 0;

		// Start the timer
		#if wxUSE_TIMER
		m_ButtonMappingTimer->Start(1000 / TimesPerSecond);
		#endif
		DEBUG_LOG(WIIMOTE, "Timer Started: Pad:%i _GetId:%i "
			"Allowed input is Axis:%i LeftRight:%i XInput:%i Button:%i Hat:%i",
			WiiMoteEmu::WiiMapping[m_Page].ID, _GetId,
			Axis, LeftRight, XInput, Button, Hat);
	}

	// Check for buttons
	// If there is a timer we should not create a new one
	else if (WiiMoteEmu::NumGoodPads > 0)
	{
		InputCommon::GetButton(
			WiiMoteEmu::WiiMapping[m_Page].joy, PadID, WiiMoteEmu::joyinfo[PadID].NumButtons, WiiMoteEmu::joyinfo[PadID].NumAxes, WiiMoteEmu::joyinfo[PadID].NumHats, 
			KeyPressed, value, type, pressed, Succeed, Stop,
			LeftRight, Axis, XInput, Button, Hat, NoTriggerFilter);
	}

	// Process results
	// Count each time
	GetButtonWaitingTimer++;

	// This is run every second
	if (GetButtonWaitingTimer % TimesPerSecond == 0)
	{
		// Current time
		int TmpTime = Seconds - (GetButtonWaitingTimer / TimesPerSecond);
		// Update text
		SetButtonText(_GetId, wxString::Format(wxT("[ %d ]"), TmpTime));
	}

	// Time's up
	if (GetButtonWaitingTimer / TimesPerSecond >= Seconds)
	{
		Stop = true;
		// Revert back to old label
		SetButtonText(_GetId, OldLabel);
	}

	// If we got a button
	if(Succeed)
	{
		Stop = true;
		// We need to assign hat special code
		if (type == InputCommon::CTL_HAT)
		{
			// Index of pressed starts from 0
			if (value & SDL_HAT_UP) pressed = 0x0100 + 0x0010 * pressed + SDL_HAT_UP;
			else if (value & SDL_HAT_DOWN) pressed = 0x0100 + 0x0010 * pressed + SDL_HAT_DOWN;
			else if (value & SDL_HAT_LEFT) pressed = 0x0100 + 0x0010 * pressed + SDL_HAT_LEFT;
			else if (value & SDL_HAT_RIGHT) pressed = 0x0100 + 0x0010 * pressed + SDL_HAT_RIGHT;
			else pressed = -1;
		}
		if (IDB_WM_A <= _GetId && _GetId <= IDB_GH3_STRUM_DOWN)
		{
			// Better make the pad button code far from virtual key code
			SaveButtonMapping(_GetId, 0x1000 + pressed);
			SetButtonText(_GetId, wxString::Format(wxT("PAD: %d"), pressed));
		}
		else if (IDB_ANALOG_LEFT_X <= _GetId && _GetId <= IDB_TRIGGER_R)
		{
			SaveButtonMapping(_GetId, pressed);
			SetButtonText(_GetId, wxString::Format(wxT("%d"), pressed));
		}
	}

	// Stop the timer
	if(Stop)
	{
		DEBUG_LOG(WIIMOTE, "Timer Stopped for Pad:%i _GetId:%i",
			WiiMoteEmu::WiiMapping[m_Page].ID, _GetId);

		m_ButtonMappingTimer->Stop();
		GetButtonWaitingTimer = 0;
		GetButtonWaitingID = 0;
		ClickedButton = NULL;
	}

	// If we got a bad button
	if(KeyPressed == -1)
	{
		// Update text
		SetButtonText(_GetId, wxString(wxT("PAD: -1")));
		// Notify the user
		wxMessageBox(wxString::Format(
			wxT("You selected a key with a to low key code (%i), please")
			wxT(" select another key with a higher key code."), pressed)
			, wxT("Notice"), wxICON_INFORMATION);		
	}
}

// Convert the 0x8000 range values to BoxW and BoxH for the plot
void WiimotePadConfigDialog::Convert2Box(int &x)
{
	// Border adjustment
	int BoxW_ = BoxW - 2; int BoxH_ = BoxH - 2;
	// Convert values
	x = (BoxW_ / 2) + (x * BoxW_ / (32767 * 2));
}

// Update the input status boxes
void WiimotePadConfigDialog::UpdatePadInfo(wxTimerEvent& WXUNUSED(event))
{
	//DEBUG_LOG(WIIMOTE, "SDL_WasInit: %i", SDL_WasInit(0));

	/* Return if it's not enabled or not detected. The ID should never be less than zero here,
	   it can only be that because of a manual ini file change, but we make that check anway. */
	if (WiiMoteEmu::WiiMapping[m_Page].ID < 0 || WiiMoteEmu::WiiMapping[m_Page].ID >= WiiMoteEmu::NumGoodPads)
	{
		m_tStatusLeftIn[m_Page]->SetLabel(wxT("Not connected"));
		m_tStatusLeftOut[m_Page]->SetLabel(wxT("Not connected"));
		m_tStatusRightIn[m_Page]->SetLabel(wxT("Not connected"));
		m_tStatusRightOut[m_Page]->SetLabel(wxT("Not connected"));
		m_TriggerStatusL[m_Page]->SetLabel(wxT("000"));
		m_TriggerStatusR[m_Page]->SetLabel(wxT("000"));
		return;
	}

	// Check that Dolphin is in focus, otherwise don't update the pad status
	//if (IsFocus())
		WiiMoteEmu::GetAxisState(WiiMoteEmu::WiiMapping[m_Page]);

	// Analog stick
	// Get original values
	int main_x = WiiMoteEmu::WiiMapping[m_Page].AxisState.Lx;
	int main_y = WiiMoteEmu::WiiMapping[m_Page].AxisState.Ly;
    int right_x = WiiMoteEmu::WiiMapping[m_Page].AxisState.Rx;
	int right_y = WiiMoteEmu::WiiMapping[m_Page].AxisState.Ry;

	// Get adjusted values
	int main_x_after = main_x, main_y_after = main_y;
	int right_x_after = right_x, right_y_after = right_y;

	// Produce square
	if(WiiMoteEmu::WiiMapping[m_Page].bCircle2Square)
		InputCommon::Square2Circle(main_x_after, main_y_after, WiiMoteEmu::WiiMapping[m_Page].Diagonal, true);

	// Check dead zone
	float DeadZoneLeft = (float)WiiMoteEmu::WiiMapping[m_Page].DeadZoneL / 100.0;
	float DeadZoneRight = (float)WiiMoteEmu::WiiMapping[m_Page].DeadZoneR / 100.0;
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

	int Lx = InputCommon::Pad_Convert(main_x); int Ly = InputCommon::Pad_Convert(main_y);
	int Rx = InputCommon::Pad_Convert(right_x); int Ry = InputCommon::Pad_Convert(right_y);
	int Lx_after = InputCommon::Pad_Convert(main_x_after); int Ly_after = InputCommon::Pad_Convert(main_y_after);
	int Rx_after = InputCommon::Pad_Convert(right_x_after); int Ry_after = InputCommon::Pad_Convert(right_y_after);

	m_tStatusLeftIn[m_Page]->SetLabel(wxString::Format(wxT("X:%03i   Y:%03i"), Lx, Ly));
	m_tStatusLeftOut[m_Page]->SetLabel(wxString::Format(wxT("X:%03i   Y:%03i"), Lx_after, Ly_after));
	m_tStatusRightIn[m_Page]->SetLabel(wxString::Format(wxT("X:%03i   Y:%03i"), Rx, Ry));
	m_tStatusRightOut[m_Page]->SetLabel(wxString::Format(wxT("X:%03i   Y:%03i"), Rx_after, Ry_after));

	// Adjust the values for the plot
	Convert2Box(main_x); Convert2Box(main_y);
	Convert2Box(right_x); Convert2Box(right_y);
	Convert2Box(main_x_after); Convert2Box(main_y_after);
	Convert2Box(right_x_after); Convert2Box(right_y_after);

	// Adjust the dot
	m_bmpDotLeftIn[m_Page]->SetPosition(wxPoint(main_x, main_y));
	m_bmpDotLeftOut[m_Page]->SetPosition(wxPoint(main_x_after, main_y_after));
	m_bmpDotRightIn[m_Page]->SetPosition(wxPoint(right_x, right_y));
	m_bmpDotRightOut[m_Page]->SetPosition(wxPoint(right_x_after, right_y_after));

	// Get the trigger values
	int TriggerLeft = WiiMoteEmu::WiiMapping[m_Page].AxisState.Tl;
	int TriggerRight = WiiMoteEmu::WiiMapping[m_Page].AxisState.Tr;

	// Convert the triggers values
	if (WiiMoteEmu::WiiMapping[m_Page].TriggerType == InputCommon::CTL_TRIGGER_SDL)
	{
		TriggerLeft = InputCommon::Pad_Convert(TriggerLeft);
		TriggerRight = InputCommon::Pad_Convert(TriggerRight);
	}

	m_TriggerStatusL[m_Page]->SetLabel(wxString::Format(wxT("%03i"), TriggerLeft));
	m_TriggerStatusR[m_Page]->SetLabel(wxString::Format(wxT("%03i"), TriggerRight));
}
