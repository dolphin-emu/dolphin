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
#include "Common.h"

Config g_Config;

Config::Config()
{
	//memset(this, 0, sizeof(Config)); // Clear the memory

	bSaveByID.resize(4); // Set vector size
	bSquareToCircle.resize(4);
	SDiagonal.resize(4); 
}


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


/* Check for duplicate Joypad names. If we find a duplicate notify the user about it. */
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
int Config::CheckForDuplicateJoypads(bool OK)
{
	// Count the number of duplicate names
	int NumDuplicates = 0, Duplicate = 0;
	for(u32 i = 0; i < 4; i++)
	{
		for(u32 j = 0; j < 4; j++)
		{
			// Avoid potential crash
			if(joysticks[i].ID >= SDL_NumJoysticks() || joysticks[j].ID >= SDL_NumJoysticks()) continue;

			if (i == j) continue; // Don't compare to itself
			if (! memcmp(&joyinfo[joysticks[i].ID], &joyinfo[joysticks[j].ID], sizeof(joyinfo)))
			{
				// If one of them is not enabled, then there is no problem
				if(!joysticks[i].enabled || !joysticks[j].enabled) continue;

				// If oen of them don't save by ID, then there is no problem
				if(!g_Config.bSaveByID.at(i) || !g_Config.bSaveByID.at(j)) continue;

				//PanicAlert("%i  %i", i, j);
				NumDuplicates++;
				Duplicate = i;
			}
		}
	}
	
	/////////////////////////////////////////////////////////////////////////
	// Notify the user about the multiple devices
	// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
	if(NumDuplicates > 0)
	{
		std::string ExtendedText;
		std::string MainText =
				"You have selected SaveByID for several identical joypads with the name '%s', because nJoy"
				" has no way of separating between them the settings for the last one will now be saved."
				" This may not be the settings you have intended to save. It is therefore recommended"
				" that you either unselect SaveByID for all but one of the identical joypads"
				" or disable them entirely."
				" If you are aware of this issue and want to keep the same settings for the identical"
				" pads you can ignore this message.";
				
		if (OK) // We got here from the OK button
		{
			ExtendedText =
			"\n\n[Select 'OK' to return to the configuration window. Select 'Cancel' to ignore this"
			" message and close the configuration window and don't show this message again.]";
		}
		else
		{
			ExtendedText =
			"\n\n[Select 'Cancel' if you don't want to see this information again.]";
		}

		bool ret = PanicYesNo((MainText + ExtendedText).c_str(), joyinfo[joysticks[Duplicate].ID].Name);

		if (ret)
			g_Config.bSaveByIDNotice = false;

		return ret ? 4 : 16;
	}
	
	return -1;
}


// Save settings to file
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
void Config::Save(bool CheckedForDuplicates)
{
	// Load ini file
	IniFile file;
	file.Load("nJoy.ini");

	// Show potential warning
	if(!CheckedForDuplicates && g_Config.bSaveByIDNotice) CheckForDuplicateJoypads(false);

	file.Set("General", "ShowAdvanced", g_Config.bShowAdvanced);
	file.Set("General", "SaveByIDNotice", g_Config.bSaveByIDNotice);

	for (int i = 0; i < 4; i++)
	{
		std::string SectionName = StringFromFormat("PAD%i", i+1);
		file.Set(SectionName.c_str(), "enabled", joysticks[i].enabled);

		// Save the physical device ID
		file.Set(SectionName.c_str(), "joy_id", joysticks[i].ID);			
		file.Set(SectionName.c_str(), "SaveByID", g_Config.bSaveByID.at(i));

		/* Don't save anything more from the disabled joypads, if a joypad is enabled we can run
		   this again after any settings are changed for it */
		if(!joysticks[i].enabled) continue;
		
		//////////////////////////////////////
		// Save joypad specific settings
		// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
			// Current joypad device ID: joysticks[i].ID
			// Current joypad name: joyinfo[joysticks[i].ID].Name		
		if(g_Config.bSaveByID.at(i))
		{
			/* Save joypad specific settings. Check for "joysticks[i].ID < SDL_NumJoysticks()" to
			   avoid reading a joyinfo that does't exist */
			if(joysticks[i].ID >= SDL_NumJoysticks()) continue;
			
			//PanicAlert("%i", m_frame->m_Joyname[0]->GetSelection());
			//if(i == 0) PanicAlert("%i", joysticks[i].buttons[CTL_START]);
			//PanicAlert("%s", joyinfo[joysticks[i].ID].Name);

			// Create a new section name after the joypad name
			SectionName = joyinfo[joysticks[i].ID].Name;
		}

		file.Set(SectionName.c_str(), "l_shoulder", joysticks[i].buttons[CTL_L_SHOULDER]);
		file.Set(SectionName.c_str(), "r_shoulder", joysticks[i].buttons[CTL_R_SHOULDER]);
		file.Set(SectionName.c_str(), "a_button", joysticks[i].buttons[CTL_A_BUTTON]);
		file.Set(SectionName.c_str(), "b_button", joysticks[i].buttons[CTL_B_BUTTON]);
		file.Set(SectionName.c_str(), "x_button", joysticks[i].buttons[CTL_X_BUTTON]);
		file.Set(SectionName.c_str(), "y_button", joysticks[i].buttons[CTL_Y_BUTTON]);
		file.Set(SectionName.c_str(), "z_trigger", joysticks[i].buttons[CTL_Z_TRIGGER]);
		file.Set(SectionName.c_str(), "start_button", joysticks[i].buttons[CTL_START]);
		file.Set(SectionName.c_str(), "dpad", joysticks[i].dpad);	
		file.Set(SectionName.c_str(), "dpad_up", joysticks[i].dpad2[CTL_D_PAD_UP]);
		file.Set(SectionName.c_str(), "dpad_down", joysticks[i].dpad2[CTL_D_PAD_DOWN]);
		file.Set(SectionName.c_str(), "dpad_left", joysticks[i].dpad2[CTL_D_PAD_LEFT]);
		file.Set(SectionName.c_str(), "dpad_right", joysticks[i].dpad2[CTL_D_PAD_RIGHT]);
		file.Set(SectionName.c_str(), "main_x", joysticks[i].axis[CTL_MAIN_X]);
		file.Set(SectionName.c_str(), "main_y", joysticks[i].axis[CTL_MAIN_Y]);
		file.Set(SectionName.c_str(), "sub_x", joysticks[i].axis[CTL_SUB_X]);
		file.Set(SectionName.c_str(), "sub_y", joysticks[i].axis[CTL_SUB_Y]);
		
		file.Set(SectionName.c_str(), "deadzone", joysticks[i].deadzone);
		file.Set(SectionName.c_str(), "halfpress", joysticks[i].halfpress);
		
		file.Set(SectionName.c_str(), "controllertype", joysticks[i].controllertype);
		file.Set(SectionName.c_str(), "TriggerType", joysticks[i].triggertype);
		file.Set(SectionName.c_str(), "eventnum", joysticks[i].eventnum);

		file.Set(SectionName.c_str(), "Diagonal", g_Config.SDiagonal.at(i).c_str());
		file.Set(SectionName.c_str(), "SquareToCircle", g_Config.bSquareToCircle.at(i));
	}

	file.Save("nJoy.ini");
}

// Load settings from file
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
void Config::Load(bool config)
{
	IniFile file;
	file.Load("nJoy.ini");
	std::vector<std::string> Duplicates;

	file.Get("General", "ShowAdvanced", &g_Config.bShowAdvanced, false);
	file.Get("General", "SaveByIDNotice", &g_Config.bSaveByIDNotice, true);

	for (int i = 0; i < 4; i++)
	{
		std::string SectionName = StringFromFormat("PAD%i", i+1);

		// Don't update this when we are loading settings from the ConfigBox
		if(!config)
		{
			file.Get(SectionName.c_str(), "joy_id", &joysticks[i].ID, 0);
			file.Get(SectionName.c_str(), "enabled", &joysticks[i].enabled, 1);
		}

		bool Tmp;
		file.Get(SectionName.c_str(), "SaveByID", &Tmp, false);
		g_Config.bSaveByID.at(i) = Tmp;

		//////////////////////////////////////
		// Load joypad specific settings
		// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
			// Current joypad device ID: joysticks[i].ID
			// Current joypad name: joyinfo[joysticks[i].ID].Name
		if(g_Config.bSaveByID.at(i))
		{
			/* Prevent a crash from illegal access to joyinfo that will only have values for
			   the current amount of connected joysticks */
			if(joysticks[i].ID >= SDL_NumJoysticks()) continue;

			//PanicAlert("%i  %i",joysticks[i].ID, SDL_NumJoysticks());
			//PanicAlert("%s", joyinfo[joysticks[i].ID].Name);

			// Create a section name			
			SectionName = joyinfo[joysticks[i].ID].Name;
		}

		file.Get(SectionName.c_str(), "l_shoulder", &joysticks[i].buttons[CTL_L_SHOULDER], 4);
		file.Get(SectionName.c_str(), "r_shoulder", &joysticks[i].buttons[CTL_R_SHOULDER], 5);
		file.Get(SectionName.c_str(), "a_button", &joysticks[i].buttons[CTL_A_BUTTON], 0);
		file.Get(SectionName.c_str(), "b_button", &joysticks[i].buttons[CTL_B_BUTTON], 1);	
		file.Get(SectionName.c_str(), "x_button", &joysticks[i].buttons[CTL_X_BUTTON], 3);
		file.Get(SectionName.c_str(), "y_button", &joysticks[i].buttons[CTL_Y_BUTTON], 2);	
		file.Get(SectionName.c_str(), "z_trigger", &joysticks[i].buttons[CTL_Z_TRIGGER], 7);
		file.Get(SectionName.c_str(), "start_button", &joysticks[i].buttons[CTL_START], 9);	
		file.Get(SectionName.c_str(), "dpad", &joysticks[i].dpad, 0);	
		file.Get(SectionName.c_str(), "dpad_up", &joysticks[i].dpad2[CTL_D_PAD_UP], 0);
		file.Get(SectionName.c_str(), "dpad_down", &joysticks[i].dpad2[CTL_D_PAD_DOWN], 0);
		file.Get(SectionName.c_str(), "dpad_left", &joysticks[i].dpad2[CTL_D_PAD_LEFT], 0);
		file.Get(SectionName.c_str(), "dpad_right", &joysticks[i].dpad2[CTL_D_PAD_RIGHT], 0);
		file.Get(SectionName.c_str(), "main_x", &joysticks[i].axis[CTL_MAIN_X], 0);	
		file.Get(SectionName.c_str(), "main_y", &joysticks[i].axis[CTL_MAIN_Y], 1);	
		file.Get(SectionName.c_str(), "sub_x", &joysticks[i].axis[CTL_SUB_X], 2);	
		file.Get(SectionName.c_str(), "sub_y", &joysticks[i].axis[CTL_SUB_Y], 3);

		file.Get(SectionName.c_str(), "deadzone", &joysticks[i].deadzone, 9);
		file.Get(SectionName.c_str(), "halfpress", &joysticks[i].halfpress, -1);	
		file.Get(SectionName.c_str(), "controllertype", &joysticks[i].controllertype, 0);
		file.Get(SectionName.c_str(), "TriggerType", &joysticks[i].triggertype, 0);
		file.Get(SectionName.c_str(), "eventnum", &joysticks[i].eventnum, 0);

		file.Get(SectionName.c_str(), "Diagonal", &g_Config.SDiagonal.at(i), "100%");
		file.Get(SectionName.c_str(), "SquareToCircle", &Tmp, false); g_Config.bSquareToCircle.at(i) = Tmp;
	}
}

