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
#ifndef _WII_IPC_HLE_DEVICE_ES_H_
#define _WII_IPC_HLE_DEVICE_ES_H_

#include "WII_IPC_HLE_Device.h"
#include "../VolumeHandler.h"

// http://wiibrew.org/index.php?title=/dev/es

class CWII_IPC_HLE_Device_es : public IWII_IPC_HLE_Device
{
public:

    enum
    {
        IOCTL_ES_ADDTICKET				= 0x01,
        IOCTL_ES_ADDTITLESTART			= 0x02,
        IOCTL_ES_ADDCONTENTSTART		= 0x03,
        IOCTL_ES_ADDCONTENTDATA			= 0x04,
        IOCTL_ES_ADDCONTENTFINISH		= 0x05,
        IOCTL_ES_ADDTITLEFINISH			= 0x06,
        IOCTL_ES_LAUNCH					= 0x08,
        IOCTL_ES_OPENCONTENT			= 0x09,
        IOCTL_ES_READCONTENT			= 0x0A,
        IOCTL_ES_CLOSECONTENT			= 0x0B,
        IOCTL_ES_GETTITLECOUNT			= 0x0E,
        IOCTL_ES_GETTITLES				= 0x0F,
        IOCTL_ES_GETVIEWCNT				= 0x12,
        IOCTL_ES_GETVIEWS				= 0x13,
		IOCTL_ES_GETTMDVIEWCNT			= 0x14,
        IOCTL_ES_DIVERIFY				= 0x1C,
        IOCTL_ES_GETTITLEDIR			= 0x1D,
        IOCTL_ES_GETTITLEID				= 0x20,
        IOCTL_ES_SEEKCONTENT			= 0x23,
        IOCTL_ES_ADDTMD					= 0x2B,
        IOCTL_ES_ADDTITLECANCEL			= 0x2F,
        IOCTL_ES_GETSTOREDCONTENTCNT	= 0x32,
        IOCTL_ES_GETSTOREDCONTENTS		= 0x33,
        IOCTL_ES_GETSTOREDTMDSIZE		= 0x34,
        IOCTL_ES_GETSTOREDTMD			= 0x35,
        IOCTL_ES_GETSHAREDCONTENTCNT	= 0x36,
        IOCTL_ES_GETSHAREDCONTENTS		= 0x37,
    };


    CWII_IPC_HLE_Device_es(u32 _DeviceID, const std::string& _rDeviceName) :
      IWII_IPC_HLE_Device(_DeviceID, _rDeviceName)
      {}

      virtual ~CWII_IPC_HLE_Device_es()
      {}

      virtual bool Open(u32 _CommandAddress, u32 _Mode)
      {
          Memory::Write_U32(GetDeviceID(), _CommandAddress+4);

          return true;
      }

        virtual bool IOCtlV(u32 _CommandAddress) 
        {
            SIOCtlVBuffer Buffer(_CommandAddress);

			LOG(WII_IPC_ES, "%s (0x%x)", GetDeviceName().c_str(), Buffer.Parameter);

			/* Extended logs 
			//if(Buffer.Parameter == IOCTL_ES_GETTITLEDIR || Buffer.Parameter == IOCTL_ES_GETTITLEID ||
			//	Buffer.Parameter == IOCTL_ES_GETVIEWCNT || Buffer.Parameter == IOCTL_ES_GETTMDVIEWCNT)
			{	
				u32 OutBuffer = Memory::Read_U32(Buffer.PayloadBuffer[0].m_Address | 0x80000000);
				if(Buffer.NumberInBuffer > 0)
				{					
					u32 InBuffer = Memory::Read_U32(Buffer.InBuffer[0].m_Address | 0x80000000);
					LOG(WII_IPC_ES, "ES Parameter: 0x%x (In: %i, Out:%i) (In 0x%08x = 0x%08x %i) (Out 0x%08x = 0x%08x  %i)",
						Buffer.Parameter,
						Buffer.NumberInBuffer, Buffer.NumberPayloadBuffer,
						Buffer.InBuffer[0].m_Address, InBuffer, Buffer.InBuffer[0].m_Size,
						Buffer.PayloadBuffer[0].m_Address, OutBuffer, Buffer.PayloadBuffer[0].m_Size);
				}
				else
				{
				LOG(WII_IPC_ES, "ES Parameter: 0x%x (In: %i, Out:%i) (Out 0x%08x = 0x%08x  %i)",
					Buffer.Parameter,
					Buffer.NumberInBuffer, Buffer.NumberPayloadBuffer,
					Buffer.PayloadBuffer[0].m_Address, OutBuffer, Buffer.PayloadBuffer[0].m_Size);				
				}
				//DumpCommands(_CommandAddress, 8);
			}*/

            switch(Buffer.Parameter)
            {
            case IOCTL_ES_GETTITLEDIR: // ES_GetDataDir in DevKitPro
                {
                    u32 TitleID = VolumeHandler::Read32(0);
                    if (TitleID == 0)
                        TitleID = 0xF00DBEEF;

                    char* pTitleID = (char*)&TitleID;

                    char* Path = (char*)Memory::GetPointer(Buffer.PayloadBuffer[0].m_Address);
                    sprintf(Path, "/00010000/%02x%02x%02x%02x/data", (u8)pTitleID[3], (u8)pTitleID[2], (u8)pTitleID[1], (u8)pTitleID[0]);

                    LOG(WII_IPC_ES, "ES: IOCTL_ES_GETTITLEDIR: %s ", Path);
                }
                break;

            case IOCTL_ES_GETTITLEID: // ES_GetTitleID in DevKitPro
                {
                    u32 OutBuffer = Memory::Read_U32(Buffer.PayloadBuffer[0].m_Address);

                    u32 TitleID = VolumeHandler::Read32(0);
                    if (TitleID == 0)
                        TitleID = 0xF00DBEEF;

					// Write the Title ID to 0x00000000
                    Memory::Write_U32(TitleID, OutBuffer);
					//Memory::Write_U32(0x00000000, OutBuffer);

                    LOG(WII_IPC_ES, "ES: IOCTL_ES_GETTITLEID: 0x%x", TitleID);
                }
                break;

			// This and 0x14 are called by Mario Kart
            case IOCTL_ES_GETVIEWCNT: // (0x12) ES_GetNumTicketViews in DevKitPro
                {
					if(Buffer.NumberPayloadBuffer)
						u32 OutBuffer = Memory::Read_U32(Buffer.PayloadBuffer[0].m_Address);
					if(Buffer.NumberInBuffer)
						u32 InBuffer = Memory::Read_U32(Buffer.InBuffer[0].m_Address);

					// Should we write something here?
					//Memory::Write_U32(0, Buffer.PayloadBuffer[0].m_Address);	
                }
                break;

			case IOCTL_ES_GETTMDVIEWCNT: // (0x14) ES_GetTMDViewSize in DevKitPro
                {
					if(Buffer.NumberPayloadBuffer)
						u32 OutBuffer = Memory::Read_U32(Buffer.PayloadBuffer[0].m_Address);
					if(Buffer.NumberInBuffer)
						u32 InBuffer = Memory::Read_U32(Buffer.InBuffer[0].m_Address);

					// Should we write something here?
					//Memory::Write_U32(0, Buffer.PayloadBuffer[0].m_Address);
                }
                break;

            case 0x16: // Consumption
            case 0x1B: // ES_DiGetTicketView

                // Mario Galaxy
                break;

			case IOCTL_ES_GETTITLECOUNT:
				{
					u32 OutBuffer = Memory::Read_U32(Buffer.PayloadBuffer[0].m_Address);

					Memory::Write_U32(0, OutBuffer);

					LOG(WII_IPC_ES, "CWII_IPC_HLE_Device_es command:"
						"      IOCTL_ES_GETTITLECOUNT: 0x%x", OutBuffer);
				}
				break;

            default:

                _dbg_assert_msg_(WII_IPC_HLE, 0, "CWII_IPC_HLE_Device_es: 0x%x", Buffer.Parameter);

                DumpCommands(_CommandAddress, 8);
                LOG(WII_IPC_HLE, "CWII_IPC_HLE_Device_es command:"
                    "Parameter: 0x%08x", Buffer.Parameter);

                break;
            }

            // Write return value (0 means OK)
            Memory::Write_U32(0, _CommandAddress + 0x4);		

            return true;
      }
};


#endif

