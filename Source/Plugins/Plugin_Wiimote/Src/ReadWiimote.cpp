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

#include "wiimote_real.h" // Local
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
	if (IS_PRESSED(wm, WIIMOTE_BUTTON_B))		printf("B pressed\n");
	if (IS_PRESSED(wm, WIIMOTE_BUTTON_UP))		printf("UP pressed\n");
	if (IS_PRESSED(wm, WIIMOTE_BUTTON_DOWN))	printf("DOWN pressed\n");
	if (IS_PRESSED(wm, WIIMOTE_BUTTON_LEFT))	printf("LEFT pressed\n");
	if (IS_PRESSED(wm, WIIMOTE_BUTTON_RIGHT))	printf("RIGHT pressed\n");
	if (IS_PRESSED(wm, WIIMOTE_BUTTON_MINUS))	printf("MINUS pressed\n");
	if (IS_PRESSED(wm, WIIMOTE_BUTTON_PLUS))	printf("PLUS pressed\n");
	if (IS_PRESSED(wm, WIIMOTE_BUTTON_ONE))		printf("ONE pressed\n");
	//if (IS_PRESSED(wm, WIIMOTE_BUTTON_ONE))		g_Run = false;
	if (IS_PRESSED(wm, WIIMOTE_BUTTON_TWO))		printf("TWO pressed\n");
	if (IS_PRESSED(wm, WIIMOTE_BUTTON_HOME))	printf("HOME pressed\n");
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