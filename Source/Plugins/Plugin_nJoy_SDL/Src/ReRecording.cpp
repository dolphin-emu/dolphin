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
// File description
/* ¯¯¯¯¯¯¯¯¯

Rerecording options

////////////////////////*/

 
//////////////////////////////////////////////////////////////////////////////////////////
// Include
// ¯¯¯¯¯¯¯¯¯
#include "nJoy.h"
#include "FileUtil.h"
#include "ChunkFile.h"
/////////////////////////


#ifdef RERECORDING


namespace Recording
{


//////////////////////////////////////////////////////////////////////////////////////////
// Definitions
// ¯¯¯¯¯¯¯¯¯¯¯¯¯
// Pre defined maxium storage limit
#define RECORD_SIZE (1024 * 128)
SPADStatus RecordBuffer[RECORD_SIZE];
int count = 0;
////////////////////////////////



//////////////////////////////////////////////////////////////////////////////////////////
// Recording functions
// ¯¯¯¯¯¯¯¯¯¯¯¯¯
void RecordInput(const SPADStatus& _rPADStatus)
{
	if (count >= RECORD_SIZE) return;
	RecordBuffer[count++] = _rPADStatus;

	// Logging
	//u8 TmpData[sizeof(SPADStatus)];
	//memcpy(TmpData, &RecordBuffer[count - 1], sizeof(SPADStatus));
	//Console::Print("RecordInput(%i): %s\n", count, ArrayToString(TmpData, sizeof(SPADStatus), 0, 30).c_str());

	// Auto save every ten seconds
	if (count % (60 * 10) == 0) Save();
}

const SPADStatus& Play()
{
	// Logging
	//Console::Print("PlayRecord(%i)\n", count);
	if (count >= RECORD_SIZE)
	{
		// Todo: Make the recording size unlimited?
		//PanicAlert("The recording reached its end");
		return(RecordBuffer[0]);
	}
	return(RecordBuffer[count++]);
}

void Load()
{
	FILE* pStream = fopen("pad-record.bin", "rb");

	if (pStream != NULL)
	{
		fread(RecordBuffer, 1, RECORD_SIZE * sizeof(SPADStatus), pStream);
		fclose(pStream);
	}
	else
	{
		PanicAlert("SimplePad: Could not open pad-record.bin");
	}

	//Console::Print("LoadRecord()");
}

void Save()
{
	// Open the file in a way that clears all old data
	FILE* pStream = fopen("pad-record.bin", "wb");

	if (pStream != NULL)
	{
		fwrite(RecordBuffer, 1, RECORD_SIZE * sizeof(SPADStatus), pStream);
		fclose(pStream);
	}
	else
	{
		PanicAlert("NJoy: Could not save pad-record.bin");
	}
	//PanicAlert("SaveRecord()");
	//Console::Print("SaveRecord()");
}
////////////////////////////////



void Initialize()
{
	// -------------------------------------------
	// Rerecording
	// ----------------------
	#ifdef RERECORDING
	/* Check if we are starting the pad to record the input, and an old file exists. In that case ask
	   if we really want to start the recording and eventually overwrite the file */
	if (g_Config.bRecording && File::Exists("pad-record.bin"))
	{
		if (!AskYesNo("An old version of '%s' aleady exists in your Dolphin directory. You can"
			" now make a copy of it before you start a new recording and overwrite the file."
			" Select Yes to continue and overwrite the file. Select No to turn off the input"
			" recording and continue without recording anything.",
			"pad-record.bin"))
		{
			// Turn off recording and continue
			g_Config.bRecording = false;
		}
	}

	// Load recorded input if we are to play it back, otherwise begin with a blank recording
	if (g_Config.bPlayback) Recording::Load();
	#endif
	// ----------------------
}


void ShutDown()
{
	// Save recording
	if (g_Config.bRecording) Recording::Save();
	// Reset the counter
	count = 0;
}

void DoState(unsigned char **ptr, int mode)
{
	// Load or save the counter
	PointerWrap p(ptr, mode);
	p.Do(count);

	//Console::Print("count: %i\n", count);

	// Update the frame counter for the sake of the status bar
	if (mode == PointerWrap::MODE_READ)
	{
		#ifdef _WIN32
			// This only works when rendering to the main window, I think
			PostMessage(GetParent(g_PADInitialize->hWnd), WM_USER, INPUT_FRAME_COUNTER, count);
		#endif
	}
}


} // Recording


#endif // RERECORDING