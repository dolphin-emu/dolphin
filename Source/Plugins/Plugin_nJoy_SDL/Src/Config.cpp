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


////////////////////////
// Include
// ¯¯¯¯¯¯¯¯¯
#include "nJoy.h"



// Enable output log
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
void DEBUG_INIT()
{
	if (pFile)
		return;

	#ifdef _WIN32
	char dateStr [9]; 
	_strdate( dateStr);
	char timeStr [9]; 
	_strtime( timeStr );
	#endif

	pFile = fopen ("nJoy-debug.txt","wt");
	fprintf(pFile, "nJoy v"INPUT_VERSION" Debug\n");
	#ifdef _WIN32
	fprintf(pFile, "Date: %s\nTime: %s\n", dateStr, timeStr);
	#endif
	fprintf(pFile, "¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯\n");
}

// Disable output log
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
void DEBUG_QUIT()
{
	if (!pFile)
		return;

	#ifdef _WIN32
	char timeStr [9]; 
	_strtime(timeStr);

	fprintf(pFile, "_______________\n");
	fprintf(pFile, "Time: %s", timeStr);
	#endif
	fclose(pFile);
}

// Save settings to file
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
void SaveConfig()
{
	IniFile file;
	file.Load("nJoy.ini");

	for (int i = 0; i < 4; i++)
	{
		char SectionName[32];
		sprintf(SectionName, "PAD%i", i+1);

		file.Set(SectionName, "l_shoulder", joysticks[i].buttons[CTL_L_SHOULDER]);
		file.Set(SectionName, "r_shoulder", joysticks[i].buttons[CTL_R_SHOULDER]);
		file.Set(SectionName, "a_button", joysticks[i].buttons[CTL_A_BUTTON]);
		file.Set(SectionName, "b_button", joysticks[i].buttons[CTL_B_BUTTON]);
		file.Set(SectionName, "x_button", joysticks[i].buttons[CTL_X_BUTTON]);
		file.Set(SectionName, "y_button", joysticks[i].buttons[CTL_Y_BUTTON]);
		file.Set(SectionName, "z_trigger", joysticks[i].buttons[CTL_Z_TRIGGER]);
		file.Set(SectionName, "start_button", joysticks[i].buttons[CTL_START]);
		file.Set(SectionName, "dpad", joysticks[i].dpad);	
		file.Set(SectionName, "dpad_up", joysticks[i].dpad2[CTL_D_PAD_UP]);
		file.Set(SectionName, "dpad_down", joysticks[i].dpad2[CTL_D_PAD_DOWN]);
		file.Set(SectionName, "dpad_left", joysticks[i].dpad2[CTL_D_PAD_LEFT]);
		file.Set(SectionName, "dpad_right", joysticks[i].dpad2[CTL_D_PAD_RIGHT]);
		file.Set(SectionName, "main_x", joysticks[i].axis[CTL_MAIN_X]);
		file.Set(SectionName, "main_y", joysticks[i].axis[CTL_MAIN_Y]);
		file.Set(SectionName, "sub_x", joysticks[i].axis[CTL_SUB_X]);
		file.Set(SectionName, "sub_y", joysticks[i].axis[CTL_SUB_Y]);
		file.Set(SectionName, "enabled", joysticks[i].enabled);
		file.Set(SectionName, "deadzone", joysticks[i].deadzone);
		file.Set(SectionName, "halfpress", joysticks[i].halfpress);
		file.Set(SectionName, "joy_id", joysticks[i].ID);
		file.Set(SectionName, "controllertype", joysticks[i].controllertype);
		file.Set(SectionName, "eventnum", joysticks[i].eventnum);
	}

	file.Save("nJoy.ini");
}

// Load settings from file
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
void LoadConfig()
{
	IniFile file;
	file.Load("nJoy.ini");

	for (int i = 0; i < 4; i++)
	{
		char SectionName[32];
		sprintf(SectionName, "PAD%i", i+1);

		file.Get(SectionName, "l_shoulder", &joysticks[i].buttons[CTL_L_SHOULDER], 4);
		file.Get(SectionName, "r_shoulder", &joysticks[i].buttons[CTL_R_SHOULDER], 5);
		file.Get(SectionName, "a_button", &joysticks[i].buttons[CTL_A_BUTTON], 0);
		file.Get(SectionName, "b_button", &joysticks[i].buttons[CTL_B_BUTTON], 1);	
		file.Get(SectionName, "x_button", &joysticks[i].buttons[CTL_X_BUTTON], 3);
		file.Get(SectionName, "y_button", &joysticks[i].buttons[CTL_Y_BUTTON], 2);	
		file.Get(SectionName, "z_trigger", &joysticks[i].buttons[CTL_Z_TRIGGER], 7);
		file.Get(SectionName, "start_button", &joysticks[i].buttons[CTL_START], 9);	
		file.Get(SectionName, "dpad", &joysticks[i].dpad, 0);	
		file.Get(SectionName, "dpad_up", &joysticks[i].dpad2[CTL_D_PAD_UP], 0);
		file.Get(SectionName, "dpad_down", &joysticks[i].dpad2[CTL_D_PAD_DOWN], 0);
		file.Get(SectionName, "dpad_left", &joysticks[i].dpad2[CTL_D_PAD_LEFT], 0);
		file.Get(SectionName, "dpad_right", &joysticks[i].dpad2[CTL_D_PAD_RIGHT], 0);
		file.Get(SectionName, "main_x", &joysticks[i].axis[CTL_MAIN_X], 0);	
		file.Get(SectionName, "main_y", &joysticks[i].axis[CTL_MAIN_Y], 1);	
		file.Get(SectionName, "sub_x", &joysticks[i].axis[CTL_SUB_X], 2);	
		file.Get(SectionName, "sub_y", &joysticks[i].axis[CTL_SUB_Y], 3);	
		file.Get(SectionName, "enabled", &joysticks[i].enabled, 1);	
		file.Get(SectionName, "deadzone", &joysticks[i].deadzone, 9);	
		file.Get(SectionName, "halfpress", &joysticks[i].halfpress, 6);	
		file.Get(SectionName, "joy_id", &joysticks[i].ID, 0);
		file.Get(SectionName, "controllertype", &joysticks[i].controllertype, 0);
		file.Get(SectionName, "eventnum", &joysticks[i].eventnum, 0);
	}
}

