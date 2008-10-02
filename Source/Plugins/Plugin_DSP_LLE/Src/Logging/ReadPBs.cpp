#ifdef _WIN32

// =======================================================================================
// Turn on and off logging modes
// --------------
//#define LOG1  // writes selected parameters only and with more readable formatting
//#define LOG2 // writes all parameters
// ==============



// =======================================================================================
// Includes
// --------------
#include "Common.h"
#include "../Globals.h"
#include "CommonTypes.h" // Pluginspecs

#include "UCode_AXStructs.h" // For the AXParamBlock structure
#include "Console.h" // For wprintf, ClearScreen
// ==============


// =======================================================================================
// TODO: make this automatic
// --------------
//u32 m_addressPBs = 0x804a1a60; // SSBM (PAL)
//u32 m_addressPBs = 0x802798c0; // Baten
//u32 m_addressPBs = 0x80576d20; // Symphonia
u32 m_addressPBs = 0x80671d00; // Paper Mario
// --------------
extern u32 gLastBlock;
// ==============



// =======================================================================================
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
	int myDifference;


	// reading and 'halfword' swap
	n++;
	if (n > 20 && logall) {ClearScreen();}
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
			
			if (n > 20 && logall) {wprintf("%c%i:", 223, i);} // logging

			// --------------
			// Here we update the PB. We do it by going through all 192 / 2 = 96 u16 values
			for (size_t p = 0; p < sizeof(AXParamBlock) / 2; p++)
			{
				paraAddr += 2;
				
				if(pSrc != NULL)
				{
					if (pSrc[p] != 0 && n > 20 && logall)
					{
						wprintf("%i %04x | ", p, Common::swap16(pSrc[p]));
					}	
				}

				pDest[p] = Common::swap16(pSrc[p]);

			}

			if(n > 20 && logall) {wprintf("\n");} // logging
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