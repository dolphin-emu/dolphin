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

#include "../Globals.h"  // this is the precompiled header and must be the first ...

// ---------------------------------------------------------------------------------------
// Includes
// -------------
#include <iostream>
#include <vector>
#include <string> // so that we can test std::string == abc
#include <math.h> // for the pow() function
#ifdef _WIN32
	#include <windows.h>
#endif

#include "../GLUtil.h"
#if defined(HAVE_WX) && HAVE_WX
#include "../Debugger/Debugger.h" // for the CDebugger class
#include "../Debugger/PBView.h"
#include "Console.h" // open and close console, clear console window
#endif
#include "../Logging/Logging.h" // for global logging values



// ---------------------------------------------------------------------------------------
// Externals
// -------------
extern int nFiles;
float ratioFactor; // a global to get the ratio factor from MixAdd
int gPreset = 0;
u32 gLastBlock;
extern bool gSSBM;
extern bool gSSBMremedy1;
extern bool gSSBMremedy2;
extern bool gSequenced;
extern bool gReset;
bool gOnlyLooping = false;
extern int gSaveFile;

//extern int gleft, gright, gtop, gbottom; // from BPStructs.cpp


// ---------------------------------------------------------------------------------------
// Counters
// -------------
int j = 0;
int k = 0;
bool iupdonce = false;
std::vector<u16> viupd(15); // the length of the update frequency bar


// ---------------------------------------------------------------------------------------
// Classes
// -------------
#if defined(HAVE_WX) && HAVE_WX
extern CDebugger* m_frame;
#endif		


// =======================================================================================
// Write title
// --------------
std::string writeTitle(int a)
{
	std::string b;
	if(a == 0)
	{
		b = "lef rig top bot | wid hei\n";
	}
	return b;
}
// =======================================================================================



// =======================================================================================
// Write main message (presets)
// --------------
std::string writeMessage(int a, int i)
{
	char buf [1000] = "";
	std::string sbuf;
	// =======================================================================================
	// PRESETS
	// ---------------------------------------------------------------------------------------
	/*
	PRESET 0
	"lef rig top bot | xof yof\n";
	"000 000 000 000 | 000 00
	*/
	if(a == 0)
	{
	sprintf(buf,"%03i %03i %03i %03i | %03i %03i",
		0, OpenGL_GetWidth(), OpenGL_GetHeight(), 0, 
		OpenGL_GetXoff(), OpenGL_GetYoff());
	}

	sbuf = buf;
	return sbuf;
}


// =======================================================================================



// Logging
void Logging(int a)
{


	// =======================================================================================
	// Update parameter values
	// --------------
	// AXPB base


	// ==============


	// ---------------------------------------------------------------------------------------
	// Write to file
	// --------------
	for (int ii = 0; ii < nFiles; ii++)
	{		
		std::string sfbuff;
		sfbuff = sfbuff + writeMessage(ii, 0);
		#if defined(HAVE_WX) && HAVE_WX
			aprintf(ii, (char *)sfbuff.c_str());
		#endif
	}
	// --------------

	
	// =======================================================================================
	// Control how often the screen is updated, and then update the screen
	// --------------
	if(a == 0) j++;
	//if(l == pow((double)2,32)) l=0; // reset l
	//l++;
	if (m_frame->gUpdFreq > 0 && j > (30 / m_frame->gUpdFreq))
	{

			
		// =======================================================================================
		// Write header
		// --------------
		char buffer [1000] = "";
		std::string sbuff;
		sbuff = writeTitle(gPreset);		
		// ==============


		// hopefully this is false if we don't have a debugging window and so it doesn't cause a crash
		/* // nothing do do here yet
		if(m_frame)
		{
			m_frame->m_GPRListView->m_CachedRegs[1][0] = 0;
		}
		*/

		// add new line
		sbuff = sbuff + writeMessage(gPreset, 0); strcpy(buffer, "");
		sbuff = sbuff + "\n";


		// =======================================================================================
		// Write global values
		// ---------------
		/*
		sprintf(buffer, "\nThe parameter blocks span from %08x to %08x | distance %i %i\n", m_addressPBs, gLastBlock, (gLastBlock-m_addressPBs), (gLastBlock-m_addressPBs) / 192);
		sbuff = sbuff + buffer; strcpy(buffer, "");
		*/
		// ===============
			
			
		// =======================================================================================
		// Write settings
		// ---------------
		/*
		sprintf(buffer, "\nSettings: SSBM fix %i | SSBM rem1 %i | SSBM rem2 %i | Sequenced %i | Reset %i | Only looping %i | Save file %i\n",
			gSSBM, gSSBMremedy1, gSSBMremedy2, gSequenced, gReset, gOnlyLooping, gSaveFile);
		sbuff = sbuff + buffer; strcpy(buffer, "");
		*/
		// ===============


		// =======================================================================================
		// Show update frequency
		// ---------------
		sbuff = sbuff + "\n";
		if(!iupdonce)
		{
			/*
			for (int i = 0; i < 10; i++)
			{
				viupd.at(i) == 0;
			}
			*/
			viupd.at(0) = 1;
			viupd.at(1) = 1;
			viupd.at(2) = 1;
			iupdonce = true;
		}

		for (u32 i = 0; i < viupd.size(); i++) // 0, 1,..., 9
		{
			if (i < viupd.size()-1)
			{
				viupd.at(viupd.size()-i-1) = viupd.at(viupd.size()-i-2);		// move all forward	
			}
			else
			{
				viupd.at(0) = viupd.at(viupd.size()-1);
			}
			
			// Correction
			if (viupd.at(viupd.size()-3) == 1 && viupd.at(viupd.size()-2) == 1 && viupd.at(viupd.size()-1) == 1)
			{
				viupd.at(0) = 0;
			}
			if(viupd.at(0) == 0 && viupd.at(1) == 1 && viupd.at(2) == 1 && viupd.at(3) == 0)
			{
				viupd.at(0) = 1;
			}				
		}

		for (u32 i = 0; i < viupd.size(); i++)
		{
			if(viupd.at(i) == 0)
				sbuff = sbuff + " ";
			else
				sbuff = sbuff + ".";
		}
		// ================


		// =======================================================================================
		// Print
		// ----------------
		#if defined(HAVE_WX) && HAVE_WX
			ClearScreen();
		#endif

		__Log("%s", sbuff.c_str());
		sbuff.clear(); strcpy(buffer, "");
		// ================

		
		// New values are written so update - DISABLED - It flickered a lot, even worse than a
		// console window. So for now only the console windows is updated.
		/*
		if(m_frame)
		{
			m_frame->NotifyUpdate();
		}
		*/			

		k=0;
		j=0;

	} // end of if (j>20)	
	
} // end of function
