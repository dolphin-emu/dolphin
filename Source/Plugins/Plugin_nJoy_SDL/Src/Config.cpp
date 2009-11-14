
// Project description
// -------------------
// Name: nJoy 
// Description: A Dolphin Compatible Input Plugin
//
// Author: Falcon4ever (nJoy@falcon4ever.com)
// Site: www.multigesture.net
// Copyright (C) 2003 Dolphin Project.
//

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

// Include
// ---------
#include "nJoy.h"
#include "Common.h"

Config g_Config;
#if defined(HAVE_WX) && HAVE_WX
extern PADConfigDialognJoy* m_ConfigFrame;
#endif



// Run when created
// -----------------
Config::Config()
{
	// Clear the memory
	//memset(this, 0, sizeof(Config));
}


// Enable output log
// -----------------
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
	fprintf(pFile, "nJoy Debug\n");
	#ifdef _WIN32
	fprintf(pFile, "Date: %s\nTime: %s\n", dateStr, timeStr);
	#endif
	fprintf(pFile, "---------------\n");
}

// Disable output log
// ------------------
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
// ---------------------
void Config::Save(int Slot)
{
	// If there are no good pads don't save
	if (NumGoodPads == 0) return;

	// Load ini file
	IniFile file;
	file.Load(FULL_CONFIG_DIR "nJoy.ini");

	// ==================================================================
	// Global settings
	// -----------------
	file.Set("General", "ShowAdvanced", g_Config.bShowAdvanced);
	file.Set("General", "SaveByID", g_Config.bSaveByID);
	file.Set("General", "CheckForFocus", g_Config.bCheckFocus);
	file.Set("General", "NoTriggerFilter", g_Config.bNoTriggerFilter);
	file.Set("General", "RumbleStrength", g_Config.RumbleStrength);
#ifdef RERECORDING
	file.Set("General", "Recording", g_Config.bRecording);
	file.Set("General", "Playback", g_Config.bPlayback);
#endif
	// ========================

	for (int i = 0; i < 4; i++)
	{
		// Should we save this slot?
		if (Slot != -1 && Slot != i) continue;

		// ==================================================================
		// Slot specific settings only
		// -----------------
		std::string SectionName = StringFromFormat("PAD%i", i+1);

		// Save the physical device ID
		file.Set(SectionName.c_str(), "joy_id", PadMapping[i].ID);
		// ===================
		
		// ==================================================================
		// Joypad or slot specific settings
		// -----------------
			// Current joypad device ID: PadMapping[i].ID
			// Current joypad name: joyinfo[PadMapping[i].ID].Name		
		if(g_Config.bSaveByID)
		{
			/* Save joypad specific settings. Check for "PadMapping[i].ID < SDL_NumJoysticks()" to
			   avoid reading a joyinfo that does't exist */
			if((u32)PadMapping[i].ID >= joyinfo.size()) continue;

			// Create a new section name after the joypad name
			SectionName = joyinfo[PadMapping[i].ID].Name;
		}

		file.Set(SectionName.c_str(), "l_shoulder", PadMapping[i].buttons[InputCommon::CTL_L_SHOULDER]);
		file.Set(SectionName.c_str(), "r_shoulder", PadMapping[i].buttons[InputCommon::CTL_R_SHOULDER]);
		file.Set(SectionName.c_str(), "a_button", PadMapping[i].buttons[InputCommon::CTL_A_BUTTON]);
		file.Set(SectionName.c_str(), "b_button", PadMapping[i].buttons[InputCommon::CTL_B_BUTTON]);
		file.Set(SectionName.c_str(), "x_button", PadMapping[i].buttons[InputCommon::CTL_X_BUTTON]);
		file.Set(SectionName.c_str(), "y_button", PadMapping[i].buttons[InputCommon::CTL_Y_BUTTON]);
		file.Set(SectionName.c_str(), "z_trigger", PadMapping[i].buttons[InputCommon::CTL_Z_TRIGGER]);
		file.Set(SectionName.c_str(), "start_button", PadMapping[i].buttons[InputCommon::CTL_START]);
		file.Set(SectionName.c_str(), "dpad", PadMapping[i].dpad);	
		file.Set(SectionName.c_str(), "dpad_up", PadMapping[i].dpad2[InputCommon::CTL_D_PAD_UP]);
		file.Set(SectionName.c_str(), "dpad_down", PadMapping[i].dpad2[InputCommon::CTL_D_PAD_DOWN]);
		file.Set(SectionName.c_str(), "dpad_left", PadMapping[i].dpad2[InputCommon::CTL_D_PAD_LEFT]);
		file.Set(SectionName.c_str(), "dpad_right", PadMapping[i].dpad2[InputCommon::CTL_D_PAD_RIGHT]);
		file.Set(SectionName.c_str(), "main_x", PadMapping[i].axis[InputCommon::CTL_MAIN_X]);
		file.Set(SectionName.c_str(), "main_y", PadMapping[i].axis[InputCommon::CTL_MAIN_Y]);
		file.Set(SectionName.c_str(), "sub_x", PadMapping[i].axis[InputCommon::CTL_SUB_X]);
		file.Set(SectionName.c_str(), "sub_y", PadMapping[i].axis[InputCommon::CTL_SUB_Y]);
		
		file.Set(SectionName.c_str(), "deadzone", PadMapping[i].deadzone);
		file.Set(SectionName.c_str(), "halfpress", PadMapping[i].halfpress);
		
		file.Set(SectionName.c_str(), "controllertype", PadMapping[i].controllertype);
		file.Set(SectionName.c_str(), "TriggerType", PadMapping[i].triggertype);
		file.Set(SectionName.c_str(), "eventnum", PadMapping[i].eventnum);
		file.Set(SectionName.c_str(), "use_rumble", PadMapping[i].rumble);

		file.Set(SectionName.c_str(), "Radius", PadMapping[i].SRadius);
		file.Set(SectionName.c_str(), "RadiusOnOff", PadMapping[i].bRadiusOnOff);	
		file.Set(SectionName.c_str(), "Diagonal", PadMapping[i].SDiagonal);
		file.Set(SectionName.c_str(), "SquareToCircle", PadMapping[i].bSquareToCircle);
		file.Set(SectionName.c_str(), "RadiusC", PadMapping[i].SRadiusC);
		file.Set(SectionName.c_str(), "RadiusOnOffC", PadMapping[i].bRadiusOnOffC);	
		file.Set(SectionName.c_str(), "DiagonalC", PadMapping[i].SDiagonalC);
		file.Set(SectionName.c_str(), "SquareToCircleC", PadMapping[i].bSquareToCircleC);
	}

	INFO_LOG(CONSOLE, "%i: Save: %i\n", 0, PadMapping[0].halfpress);

	file.Save(FULL_CONFIG_DIR "nJoy.ini");
}

// Load settings from file
// -----------------------
void Config::Load(bool ChangePad, bool ChangeSaveByID)
{
	// If there are no good pads don't load
	if (NumGoodPads == 0) return;

	// Load file
	IniFile file;
	file.Load(FULL_CONFIG_DIR "nJoy.ini");
	bool Tmp;

	// ==================================================================
	// Global settings
	// -----------------
	file.Get("General", "ShowAdvanced", &g_Config.bShowAdvanced, false);
	file.Get("General", "CheckForFocus", &g_Config.bCheckFocus, false);
	file.Get("General", "NoTriggerFilter", &g_Config.bNoTriggerFilter, false);
	file.Get("General", "RumbleStrength", &g_Config.RumbleStrength, 8);

	if(!ChangeSaveByID)
	{
		file.Get("General", "SaveByID", &Tmp, false); g_Config.bSaveByID = Tmp;
	}
	// =============

	for (int i = 0; i < 4; i++)
	{
		std::string SectionName = StringFromFormat("PAD%i", i+1);

		// Don't update this when we are loading settings from the PADConfigDialognJoy
		if (!ChangePad)
		{
			file.Get(SectionName.c_str(), "joy_id", &PadMapping[i].ID, 0);
		}

		// ==================================================================
		// Joypad or slot specific settings
		// -----------------
			// Current joypad device ID: PadMapping[i].ID
			// Current joypad name: joyinfo[PadMapping[i].ID].Name
		if(g_Config.bSaveByID)
		{
			// Prevent a crash from illegal access to joyinfo that will only have values for
			// the current amount of connected pads 
			if((u32)PadMapping[i].ID >= joyinfo.size()) continue;

			// Create a section name			
			SectionName = joyinfo[PadMapping[i].ID].Name;
		}

		file.Get(SectionName.c_str(), "l_shoulder", &PadMapping[i].buttons[InputCommon::CTL_L_SHOULDER], 4);
		file.Get(SectionName.c_str(), "r_shoulder", &PadMapping[i].buttons[InputCommon::CTL_R_SHOULDER], 5);
		file.Get(SectionName.c_str(), "a_button", &PadMapping[i].buttons[InputCommon::CTL_A_BUTTON], 0);
		file.Get(SectionName.c_str(), "b_button", &PadMapping[i].buttons[InputCommon::CTL_B_BUTTON], 1);	
		file.Get(SectionName.c_str(), "x_button", &PadMapping[i].buttons[InputCommon::CTL_X_BUTTON], 3);
		file.Get(SectionName.c_str(), "y_button", &PadMapping[i].buttons[InputCommon::CTL_Y_BUTTON], 2);	
		file.Get(SectionName.c_str(), "z_trigger", &PadMapping[i].buttons[InputCommon::CTL_Z_TRIGGER], 7);
		file.Get(SectionName.c_str(), "start_button", &PadMapping[i].buttons[InputCommon::CTL_START], 9);	
		file.Get(SectionName.c_str(), "dpad", &PadMapping[i].dpad, 0);	
		file.Get(SectionName.c_str(), "dpad_up", &PadMapping[i].dpad2[InputCommon::CTL_D_PAD_UP], 0);
		file.Get(SectionName.c_str(), "dpad_down", &PadMapping[i].dpad2[InputCommon::CTL_D_PAD_DOWN], 0);
		file.Get(SectionName.c_str(), "dpad_left", &PadMapping[i].dpad2[InputCommon::CTL_D_PAD_LEFT], 0);
		file.Get(SectionName.c_str(), "dpad_right", &PadMapping[i].dpad2[InputCommon::CTL_D_PAD_RIGHT], 0);
		file.Get(SectionName.c_str(), "main_x", &PadMapping[i].axis[InputCommon::CTL_MAIN_X], 0);	
		file.Get(SectionName.c_str(), "main_y", &PadMapping[i].axis[InputCommon::CTL_MAIN_Y], 1);	
		file.Get(SectionName.c_str(), "sub_x", &PadMapping[i].axis[InputCommon::CTL_SUB_X], 2);	
		file.Get(SectionName.c_str(), "sub_y", &PadMapping[i].axis[InputCommon::CTL_SUB_Y], 3);

		file.Get(SectionName.c_str(), "deadzone", &PadMapping[i].deadzone, 9);
		file.Get(SectionName.c_str(), "halfpress", &PadMapping[i].halfpress, -1);	
		file.Get(SectionName.c_str(), "controllertype", &PadMapping[i].controllertype, 0);
		file.Get(SectionName.c_str(), "TriggerType", &PadMapping[i].triggertype, 0);
		file.Get(SectionName.c_str(), "eventnum", &PadMapping[i].eventnum, 0);
		file.Get(SectionName.c_str(), "use_rumble", &PadMapping[i].rumble, false);

		file.Get(SectionName.c_str(), "Radius", &PadMapping[i].SRadius, "100%");
		file.Get(SectionName.c_str(), "RadiusOnOff", &Tmp, false); PadMapping[i].bRadiusOnOff = Tmp;
		file.Get(SectionName.c_str(), "Diagonal", &PadMapping[i].SDiagonal, "100%");
		file.Get(SectionName.c_str(), "SquareToCircle", &Tmp, false); PadMapping[i].bSquareToCircle = Tmp;
		file.Get(SectionName.c_str(), "RadiusC", &PadMapping[i].SRadiusC, "100%");
		file.Get(SectionName.c_str(), "RadiusOnOffC", &Tmp, false); PadMapping[i].bRadiusOnOffC = Tmp;
		file.Get(SectionName.c_str(), "DiagonalC", &PadMapping[i].SDiagonalC, "100%");
		file.Get(SectionName.c_str(), "SquareToCircleC", &Tmp, false); PadMapping[i].bSquareToCircleC = Tmp;
	}

	INFO_LOG(CONSOLE, "%i: Load: %i\n", 0, PadMapping[0].halfpress);
}

