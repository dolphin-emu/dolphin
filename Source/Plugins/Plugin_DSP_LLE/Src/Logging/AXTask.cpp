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


#include "../Globals.h"
#include "Common.h"


extern u32 m_addressPBs;


// =======================================================================================
// Get the parameter block location - Example SSBM: We get the addr 8049cf00, first we
// always get 0 and go to AXLIST_STUDIOADDR, then we end up at AXLIST_PBADDR.
// --------------
bool AXTask(u32& _uMail)
{
	u32 uAddress = _uMail;
	DebugLog("AXTask - ================================================================");
	DebugLog("AXTask - AXCommandList-Addr: 0x%08x", uAddress);

	bool bExecuteList = true;

	while (bExecuteList)
	{
		// ---------------------------------------------------------------------------------------
		// SSBM: We get the addr 8049cf00, first we always get 0
		u16 iCommand = Memory_Read_U16(uAddress);
		uAddress += 2;
		// ---------------------------------------------------------------------------------------

		switch (iCommand)
		{
			// ---------------------------------------------------------------------------------------
			// ?
		case 0: // AXLIST_STUDIOADDR: //00
			{
				uAddress += 4;
				DebugLog("AXLIST AXLIST_SBUFFER: %08x", uAddress);
			}
			break;
			// ---------------------------------------------------------------------------------------


			// ---------------------------------------------------------------------------------------
		case 2: // AXLIST_PBADDR: // 02
				{
					m_addressPBs = Memory_Read_U32(uAddress);
					uAddress += 4;
					DebugLog("AXLIST PB address: %08x", m_addressPBs);
					bExecuteList = false;
				}
				break;

			// ---------------------------------------------------------------------------------------
		case 7: // AXLIST_SBUFFER: // 7
			{
				// Hopefully this is where in main ram to write.
				uAddress += 4;
				DebugLog("AXLIST AXLIST_SBUFFER: %08x", uAddress);
			}
			break;


		
			default:
				{
					// ---------------------------------------------------------------------------------------
					// Stop the execution of this TaskList
					DebugLog("AXLIST default: %08x", uAddress);
					bExecuteList = false;
					// ---------------------------------------------------------------------------------------
				}
				break;
			} // end of switch
		}

	DebugLog("AXTask - done, send resume");
	DebugLog("AXTask - ================================================================");

	// now resume
	return true;
}
// =======================================================================================


