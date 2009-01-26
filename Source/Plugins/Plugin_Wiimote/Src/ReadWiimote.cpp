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
// ¯¯¯¯¯¯¯¯¯¯
#include <iostream> // System
#include "pluginspecs_wiimote.h"

#include "wiiuse.h" // Externals

#include "ConsoleWindow.h" // Common
#include "StringUtil.h"

#include "wiimote_real.h" // Local
#include "main.h" // Local
////////////////////////////////////////

namespace WiiMoteReal
{

void handle_ctrl_status(struct wiimote_t* wm)
{
	printf("\n\n--- CONTROLLER STATUS [wiimote id %i] ---\n", wm->unid);

	printf("attachment:      %i\n", wm->exp.type);
	printf("speaker:         %i\n", WIIUSE_USING_SPEAKER(wm));
	printf("ir:              %i\n", WIIUSE_USING_IR(wm));
	printf("leds:            %i %i %i %i\n", WIIUSE_IS_LED_SET(wm, 1), WIIUSE_IS_LED_SET(wm, 2), WIIUSE_IS_LED_SET(wm, 3), WIIUSE_IS_LED_SET(wm, 4));
	printf("battery:         %f %%\n", wm->battery_level);
}

void handle_event(struct wiimote_t* wm)
{
	printf("\n\n--- EVENT [id %i] ---\n", wm->unid);

	/* if a button is pressed, report it */
	if (IS_PRESSED(wm, WIIMOTE_BUTTON_A))		Console::Print("A pressed\n");
	if (IS_PRESSED(wm, WIIMOTE_BUTTON_B))		Console::Print("B pressed\n");
	if (IS_PRESSED(wm, WIIMOTE_BUTTON_UP))		Console::Print("UP pressed\n");
	if (IS_PRESSED(wm, WIIMOTE_BUTTON_DOWN))	Console::Print("DOWN pressed\n");
	if (IS_PRESSED(wm, WIIMOTE_BUTTON_LEFT))	Console::Print("LEFT pressed\n");
	if (IS_PRESSED(wm, WIIMOTE_BUTTON_RIGHT))	Console::Print("RIGHT pressed\n");
	if (IS_PRESSED(wm, WIIMOTE_BUTTON_MINUS))	Console::Print("MINUS pressed\n");
	if (IS_PRESSED(wm, WIIMOTE_BUTTON_PLUS))	Console::Print("PLUS pressed\n");
	if (IS_PRESSED(wm, WIIMOTE_BUTTON_ONE))		Console::Print("ONE pressed\n");
	//if (IS_PRESSED(wm, WIIMOTE_BUTTON_ONE))		g_Run = false;
	if (IS_PRESSED(wm, WIIMOTE_BUTTON_TWO))		Console::Print("TWO pressed\n");
	if (IS_PRESSED(wm, WIIMOTE_BUTTON_HOME))	Console::Print("HOME pressed\n");

	/*
	 *	Pressing minus will tell the wiimote we are no longer interested in movement.
	 *	This is useful because it saves battery power.
	 */
	if (IS_JUST_PRESSED(wm, WIIMOTE_BUTTON_MINUS))
		wiiuse_motion_sensing(wm, 0);

	/*
	 *	Pressing plus will tell the wiimote we are interested in movement.
	 */
	if (IS_JUST_PRESSED(wm, WIIMOTE_BUTTON_PLUS))
		wiiuse_motion_sensing(wm, 1);

	/* If the accelerometer is turned on then print angles */
	if (WIIUSE_USING_ACC(wm))
	{
		std::string Tmp;
		Tmp += StringFromFormat("Roll: %2.1f  ", wm->orient.roll);
		Tmp += StringFromFormat("Pitch: %2.1f  ", wm->orient.pitch);
		Tmp += StringFromFormat("Battery: %1.2f\n", wm->battery_level);
		Tmp += StringFromFormat("G-Force x, y, z: %1.2f %1.2f %1.2f\n", wm->gforce.x, wm->gforce.y, wm->gforce.z);
		Tmp += StringFromFormat("Accel x, y, z: %03i %03i %03i\n\n", wm->accel.x, wm->accel.y, wm->accel.z);
		Console::Print("%s", Tmp.c_str());

		if(frame)
		{
			frame->m_GaugeBattery->SetValue((int)floor((wm->battery_level * 100) + 0.5));

			frame->m_GaugeRoll[0]->SetValue(wm->orient.roll + 180);
			frame->m_GaugeRoll[1]->SetValue(wm->orient.pitch + 180);

			frame->m_GaugeGForce[0]->SetValue((int)floor((wm->gforce.x * 100) + 300.5));
			frame->m_GaugeGForce[1]->SetValue((int)floor((wm->gforce.y * 100) + 300.5));
			frame->m_GaugeGForce[2]->SetValue((int)floor((wm->gforce.z * 100) + 300.5));

			frame->m_GaugeAccel[0]->SetValue(wm->accel.x);
			frame->m_GaugeAccel[1]->SetValue(wm->accel.y);
			frame->m_GaugeAccel[2]->SetValue(wm->accel.z);
		}
	}
}

void ReadWiimote()
{
	if (wiiuse_poll(g_WiiMotesFromWiiUse, MAX_WIIMOTES))
	{
		/*
		 *	This happens if something happened on any wiimote.
		 *	So go through each one and check if anything happened.
		 */
		int i = 0;
		for (; i < MAX_WIIMOTES; ++i)
		{
			switch (g_WiiMotesFromWiiUse[i]->event)
			{
				case WIIUSE_EVENT:
					/* a generic event occured */
					handle_event(g_WiiMotesFromWiiUse[i]);
					break;

				case WIIUSE_STATUS:
					/* a status event occured */
					handle_ctrl_status(g_WiiMotesFromWiiUse[i]);
					break;

				case WIIUSE_DISCONNECT:
				case WIIUSE_UNEXPECTED_DISCONNECT:
					/* the wiimote disconnected */
					//handle_disconnect(wiimotes[i]);
					break;

				case WIIUSE_READ_DATA:
					/*
					 *	Data we requested to read was returned.
					 *	Take a look at wiimotes[i]->read_req
					 *	for the data.
					 */
					break;

				case WIIUSE_NUNCHUK_INSERTED:
					/*
					 *	a nunchuk was inserted
					 *	This is a good place to set any nunchuk specific
					 *	threshold values.  By default they are the same
					 *	as the wiimote.
					 */
					 //wiiuse_set_nunchuk_orient_threshold((struct nunchuk_t*)&wiimotes[i]->exp.nunchuk, 90.0f);
					 //wiiuse_set_nunchuk_accel_threshold((struct nunchuk_t*)&wiimotes[i]->exp.nunchuk, 100);
					printf("Nunchuk inserted.\n");
					break;

				case WIIUSE_CLASSIC_CTRL_INSERTED:
					//printf("Classic controller inserted.\n");
					break;

				case WIIUSE_GUITAR_HERO_3_CTRL_INSERTED:
					/* some expansion was inserted */
					//handle_ctrl_status(wiimotes[i]);
					printf("Guitar Hero 3 controller inserted.\n");
					break;

				case WIIUSE_NUNCHUK_REMOVED:
				case WIIUSE_CLASSIC_CTRL_REMOVED:
				case WIIUSE_GUITAR_HERO_3_CTRL_REMOVED:
					/* some expansion was removed */
					//handle_ctrl_status(wiimotes[i]);
					printf("An expansion was removed.\n");
					break;

				default:
					break;
			}
		}
	}
}

}; // end of namespace