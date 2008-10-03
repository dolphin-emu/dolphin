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
		static int last_valid_command = 0;
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


