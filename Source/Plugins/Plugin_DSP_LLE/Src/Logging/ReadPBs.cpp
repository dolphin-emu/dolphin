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




// Turn on and off logging modes
// --------------
//#define LOG1  // writes selected parameters only and with more readable formatting
//#define LOG2 // writes all parameters

#include "Common.h"
#include "../Globals.h"
#include "CommonTypes.h" // Pluginspecs

#include "UCode_AXStructs.h" // For the AXParamBlock structure


u32 m_addressPBs = 0;
extern u32 gLastBlock;



#ifdef _WIN32

int m = 0;
int n = 0;
#ifdef LOG2
bool logall = true;
#else
bool logall = false;
#endif
int ReadOutPBs(AXParamBlock * _pPBs, int _num)
{
	int count = 0;
	u32 blockAddr = m_addressPBs;
	u32 OldblockAddr = blockAddr;
	u32 paraAddr = blockAddr;

	// reading and 'halfword' swap
	n++;
	//FIXME	if (n > 20 && logall) {Console::ClearScreen();}
	for (int i = 0; i < _num; i++)
	{
		// ---------------------------------------------------------------------------------------
		// Check if there is something here.
		const short * pSrc = (const short *)g_dspInitialize.pGetMemoryPointer(blockAddr);
		// -------------

		if (pSrc != NULL) // only read non-blank blocks
		{
			// ---------------------------------------------------------------------------------------
			// Create a shortcut that let us update struct members
			short * pDest = (short *) & _pPBs[i];
			
			if (n > 20 && logall) {DEBUG_LOG(DSPHLE, "%c%i:", 223, i);} // logging

			// --------------
			// Here we update the PB. We do it by going through all 192 / 2 = 96 u16 values
			for (size_t p = 0; p < sizeof(AXParamBlock) / 2; p++)
			{
				paraAddr += 2;
				
				if(pSrc != NULL)
				{
					if (pSrc[p] != 0 && n > 20 && logall)
					{
						DEBUG_LOG(DSPHLE, "%i %04x | ", p, Common::swap16(pSrc[p]));
					}	
				}

				pDest[p] = Common::swap16(pSrc[p]);

			}

			if(n > 20 && logall) {DEBUG_LOG(DSPHLE, "\n");} // logging
			// --------------
			// Here we update the block address to the starting point of the next PB
			blockAddr = (_pPBs[i].next_pb_hi << 16) | _pPBs[i].next_pb_lo;
			// --------------
			// save some values
			count++;
			gLastBlock = paraAddr;  // blockAddr
			// ============
		}
		else
		{
			break;
		}

	} // end of the big loop
	if (n > 20) {n = 0;} // for logging


	// return the number of readed PBs
	return count;
}
// =======================================================================================

#endif
