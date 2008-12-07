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



// =======================================================
// File description
// -------------
/*  Here we handle /dev/es requests. We have cases for these functions, the exact
	DevKitPro name is en parenthesis:

	0x20 GetTitleID (ES_GetTitleID) (Input: none, Output: 8 bytes)
	0x1d GetDataDir (ES_GetDataDir) (Input: 8 bytes, Output: 30 bytes)

	0x1b DiGetTicketView (Input: none, Output: 216 bytes) 
	0x16 GetConsumption (Input: 8 bytes, Output: 0 bytes, 4 bytes) // there are two output buffers

	0x12 GetNumTicketViews (ES_GetNumTicketViews) (Input: 8 bytes, Output: 4 bytes)
	0x14 GetTMDViewSize (ES_GetTMDViewSize) (Input: ?, Output: ?) // I don't get this anymore,
		it used to come after 0x12

	but only the first two are correctly supported. For the other four we ignore any potential
	input and only write zero to the out buffer. However, most games only use first two,
	but some Nintendo developed games use the other ones to:
	
	0x1b: Mario Galaxy, Mario Kart, SSBB
	0x16: Mario Galaxy, Mario Kart, SSBB
	0x12: Mario Kart
	0x14: Mario Kart: But only if we don't return a zeroed out buffer for the 0x12 question,
		and instead answer for example 1 will this question appear.
 
*/
// =============



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
		IOCTL_ES_GETCONSUMPTION			= 0x16,
		IOCTL_ES_DIGETTICKETVIEW		= 0x1b,
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

		virtual bool Close(u32 _CommandAddress)
		{
			LOG(WII_IPC_ES, "ES: Close");
			Memory::Write_U32(0, _CommandAddress + 4);
			return true;
		}

        virtual bool IOCtlV(u32 _CommandAddress) 
        {
            SIOCtlVBuffer Buffer(_CommandAddress);

			LOG(WII_IPC_ES, "%s (0x%x)", GetDeviceName().c_str(), Buffer.Parameter);

			// Prepare the out buffer(s) with zeroes as a safety precaution
			// to avoid returning bad values
			for(int i = 0; i < Buffer.NumberPayloadBuffer; i++)
			{
				Memory::Memset(Buffer.PayloadBuffer[i].m_Address, 0,
					Buffer.PayloadBuffer[i].m_Size);
			}

            switch(Buffer.Parameter)
            {
            case IOCTL_ES_GETTITLEDIR: // 0x1d
                {
					/* I changed reading the TitleID from disc to reading from the
					   InBuffer, if it fails it's because of some kind of memory error
					   that we would want to fix anyway */
					u32 TitleID = Memory::Read_U32(Buffer.InBuffer[0].m_Address);
                    if (TitleID == 0) TitleID = 0xF00DBEEF;

                    char* pTitleID = (char*)&TitleID;
                    char* Path = (char*)Memory::GetPointer(Buffer.PayloadBuffer[0].m_Address);
                    sprintf(Path, "/00010000/%08x/data", TitleID);

					LOG(WII_IPC_ES, "ES: IOCTL_ES_GETTITLEDIR: %s)", Path);
                }
                break;

            case IOCTL_ES_GETTITLEID: // 0x20
                {
                    u32 TitleID = VolumeHandler::Read32(0);
                    if (TitleID == 0) TitleID = 0xF00DBEEF;

					/* This seems to be the right address to write the Title ID to
					   because then it shows up in the InBuffer of IOCTL_ES_GETTITLEDIR
					   that is called right after this. I have not seen that this
					   have any effect by itself, it probably only matters as an input to
					   IOCTL_ES_GETTITLEDIR. This values is not stored in 0x3180 or anywhere
					   else as I have seen, it's just used as an input buffer in the following
					   IOCTL_ES_GETTITLEDIR call and then forgotten. */
                    Memory::Write_U32(TitleID, Buffer.PayloadBuffer[0].m_Address);
                    LOG(WII_IPC_ES, "ES: IOCTL_ES_GETTITLEID: 0x%x", TitleID);
                }
                break;

            case IOCTL_ES_GETVIEWCNT: // 0x12 (Input: 8 bytes, Output: 4 bytes)
                {
					if(Buffer.NumberPayloadBuffer)
						u32 OutBuffer = Memory::Read_U32(Buffer.PayloadBuffer[0].m_Address);
					if(Buffer.NumberInBuffer)
						u32 InBuffer = Memory::Read_U32(Buffer.InBuffer[0].m_Address);

					// Should we write something here?
					//Memory::Write_U32(0, Buffer.PayloadBuffer[0].m_Address);	
                }
                break;

			case IOCTL_ES_GETTMDVIEWCNT: // 0x14
                {
					if(Buffer.NumberPayloadBuffer)
						u32 OutBuffer = Memory::Read_U32(Buffer.PayloadBuffer[0].m_Address);
					if(Buffer.NumberInBuffer)
						u32 InBuffer = Memory::Read_U32(Buffer.InBuffer[0].m_Address);

					// Should we write something here?
					//Memory::Write_U32(0, Buffer.PayloadBuffer[0].m_Address);
                }
                break;

           case IOCTL_ES_GETCONSUMPTION: // (Input: 8 bytes, Output: 0 bytes, 4 bytes)
           case IOCTL_ES_DIGETTICKETVIEW: // (Input: none, Output: 216 bytes)
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

