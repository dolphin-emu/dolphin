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


//////////////////////////////////////////////////////////////////////////////////////////
// Include
// ¯¯¯¯¯¯¯¯¯¯
#include "Common.h"

#include "WII_IPC_HLE_Device_DI.h"

#include "../HW/CPU.h"
#include "../HW/Memmap.h"
#include "../Core.h"

#include "../VolumeHandler.h"

#include "VolumeCreator.h"
#include "Filesystem.h"
///////////////////////////////////


//////////////////////////////////////////////////
// Music mod
// ¯¯¯¯¯¯¯¯¯¯
#include "Setup.h" // Define MUSICMOD here
#ifdef MUSICMOD
#include "../../../../Externals/MusicMod/Main/Src/Main.h" 
#endif
///////////////////////


CWII_IPC_HLE_Device_di::CWII_IPC_HLE_Device_di(u32 _DeviceID, const std::string& _rDeviceName )
    : IWII_IPC_HLE_Device(_DeviceID, _rDeviceName)
    , m_pVolume(NULL)
    , m_pFileSystem(NULL)
{
	m_pVolume = VolumeHandler::GetVolume();
    if (m_pVolume)
        m_pFileSystem = DiscIO::CreateFileSystem(m_pVolume);
}

CWII_IPC_HLE_Device_di::~CWII_IPC_HLE_Device_di()
{
	if(m_pFileSystem) { delete m_pFileSystem; m_pFileSystem = NULL; }
	/* This caused the crash in VolumeHandler.cpp, setting m_pVolume = NULL; didn't help
	   it still makes VolumeHandler.cpp have a  non-NULL pointer with no content so that
	   delete crashes */
	//if(m_pVolume) { delete m_pVolume; m_pVolume = NULL; }
}

bool CWII_IPC_HLE_Device_di::Open(u32 _CommandAddress, u32 _Mode)
{
    Memory::Write_U32(GetDeviceID(), _CommandAddress+4);
    return true;
}

bool CWII_IPC_HLE_Device_di::Close(u32 _CommandAddress)
{
    Memory::Write_U32(0, _CommandAddress+4);
    return true;
}

bool CWII_IPC_HLE_Device_di::IOCtl(u32 _CommandAddress) 
{
	INFO_LOG(WII_IPC_DVD, "*******************************");
    INFO_LOG(WII_IPC_DVD,  "CWII_IPC_DVD_Device_di::IOCtl");
    INFO_LOG(WII_IPC_DVD, "*******************************");


	u32 BufferIn =  Memory::Read_U32(_CommandAddress + 0x10);
	u32 BufferInSize =  Memory::Read_U32(_CommandAddress + 0x14);
    u32 BufferOut = Memory::Read_U32(_CommandAddress + 0x18);
    u32 BufferOutSize = Memory::Read_U32(_CommandAddress + 0x1C);
	u32 Command = Memory::Read_U32(BufferIn) >> 24;

    DEBUG_LOG(WII_IPC_DVD, "%s - Command(0x%08x) BufferIn(0x%08x, 0x%x) BufferOut(0x%08x, 0x%x)", GetDeviceName().c_str(), Command, BufferIn, BufferInSize, BufferOut, BufferOutSize);

	u32 ReturnValue = ExecuteCommand(BufferIn, BufferInSize, BufferOut, BufferOutSize);	
    Memory::Write_U32(ReturnValue, _CommandAddress + 0x4);

    return true;
}

bool CWII_IPC_HLE_Device_di::IOCtlV(u32 _CommandAddress) 
{  
//	PanicAlert("CWII_IPC_HLE_Device_di::IOCtlV() unknown");
//	DumpCommands(_CommandAddress);
	u32 ReturnValue = 0;

	SIOCtlVBuffer CommandBuffer(_CommandAddress);

	switch (CommandBuffer.Parameter)
	{
	// DVDLowOpenPartition???
	case 0x8b:
		{
			INFO_LOG(WII_IPC_DVD, "DVD IOCtlV: DVDLowOpenPartition");
			_dbg_assert_msg_(WII_IPC_DVD, 0, "DVD IOCtlV: DVDLowOpenPartition");
		}
		break;
	default:
		INFO_LOG(WII_IPC_DVD, "DVD IOCtlV: %i", CommandBuffer.Parameter);
		_dbg_assert_msg_(WII_IPC_DVD, 0, "DVD: %i", CommandBuffer.Parameter);
		break;
	}

	Memory::Write_U32(ReturnValue, _CommandAddress+4);
    return true;
}

u32 CWII_IPC_HLE_Device_di::ExecuteCommand(u32 _BufferIn, u32 _BufferInSize, u32 _BufferOut, u32 _BufferOutSize)
{    
    u32 Command = Memory::Read_U32(_BufferIn) >> 24;

	/* Set out buffer to zeroes as a safety precaution to avoid answering
	   nonsense values */

	// TATSUNOKO VS CAPCOM: Gets here with _BufferOut == 0x0!!!
	if (_BufferOut != 0) {
		Memory::Memset(_BufferOut, 0, _BufferOutSize);
	}

	switch (Command)
	{
	// DVDLowInquiry
	case 0x12:
		{
			u8* buffer = Memory::GetPointer(_BufferOut);
			
			/* In theory this gives a game the option to use different read / write behaviors
			   depending on which hardware revision that is used, if there have been more than
			   one. But it's probably not used at all by any game, in any case it would be strange
			   if it refused a certain value here if it's possible that that would make it
			   incompatible with new DVD drives for example. From an actual Wii the code was
			   0x0000, 0x0002, 0x20060526, I tried it in Balls of Fury that gives a DVD error
			   message after the DVDLowInquiry, but that did't change anything, it must be
			   something else. */
			buffer[0] = 0x01; // rev
			buffer[1] = 0x02;
			buffer[2] = 0x03; // dev code
			buffer[3] = 0x04;
			buffer[4] = 0x20; // firmware date
			buffer[5] = 0x08;
			buffer[6] = 0x08;
			buffer[7] = 0x29;			

			DEBUG_LOG(WII_IPC_DVD, "%s executes DVDLowInquiry (Buffer 0x%08x, 0x%x)", GetDeviceName().c_str(), _BufferOut, _BufferOutSize);
			
            return 0x1;			
		}
		break;

	// DVDLowReadDiskID
	case 0x70:
		{
			// TODO - verify that this is correct
			VolumeHandler::ReadToPtr(Memory::GetPointer(_BufferOut), 0, _BufferOutSize);

			DEBUG_LOG(WII_IPC_DVD, "%s executes DVDLowReadDiskID (Buffer 0x%08x, 0x%x)", GetDeviceName().c_str(), _BufferOut, _BufferOutSize);
			
            return 0x1;			
		}
		break;
	
	// DVDLowRead 
    case 0x71:
        {
			if (_BufferOut == 0)
			{
				PanicAlert("DVDLowRead : _BufferOut == 0");
				return 0;
			}
            u32 Size = Memory::Read_U32(_BufferIn + 0x04);
            u64 DVDAddress = (u64)Memory::Read_U32(_BufferIn + 0x08) << 2;

            const char *pFilename = m_pFileSystem->GetFileName(DVDAddress);
            if (pFilename != NULL)
            {
                INFO_LOG(WII_IPC_DVD, "    DVDLowRead: %s (0x%x) - (DVDAddr: 0x%x, Size: 0x%x)", pFilename, m_pFileSystem->GetFileSize(pFilename), DVDAddress, Size);
            }
            else
            {
                INFO_LOG(WII_IPC_DVD, "    DVDLowRead: file unkw - (DVDAddr: 0x%x, Size: 0x%x)", GetDeviceName().c_str(), DVDAddress, Size);
            }

            if (Size > _BufferOutSize)
            {
                PanicAlert("Detected attempt to read more data from the DVD than fit inside the out buffer. Clamp.");
                Size = _BufferOutSize;
            }

            if (VolumeHandler::ReadToPtr(Memory::GetPointer(_BufferOut), DVDAddress, Size) != true)
            {
                PanicAlert("Cant read from DVD_Plugin - DVD-Interface: Fatal Error");
            }	

			//////////////////////////////////////////////////
			// Music mod
			// ¯¯¯¯¯¯¯¯¯¯
			#ifdef MUSICMOD
				std::string Tmp = pFilename;
				MusicMod::CheckFile(Tmp);
			#endif
			///////////////////////			

            return 0x1;			
        }
        break;

	// DVDLowWaitForCoverClose
    case 0x79:
        {   
            Memory::Memset(_BufferOut, 0, _BufferOutSize);
			INFO_LOG(WII_IPC_DVD, "%s executes DVDLowWaitForCoverClose (Buffer 0x%08x, 0x%x)", GetDeviceName().c_str(), _BufferOut, _BufferOutSize);
			return 4;
        }
        break;

    // DVDLowGetCoverReg - Called by "Legend of Spyro" and MP3
    case 0x7a:
        {   
			INFO_LOG(WII_IPC_DVD, "%s executes DVDLowGetCoverReg (Buffer 0x%08x, 0x%x)", GetDeviceName().c_str(), _BufferOut, _BufferOutSize);

			// Write zeroes to the out buffer just in case there is some nonsense data there
			Memory::Memset(_BufferOut, 0, _BufferOutSize);	

			// --------------------------------------------------------------------
			/* Hack for Legend of Spyro. Switching the 4th byte between 0 and 1 gets
			   through this check. The out buffer address remains the same all the
			   time so we don't have to bother making a global function.
			   
			   TODO: Make this compatible with MP3 */
			// -------------------------
			/*
			static u8 coverByte = 0;

			u8* buffer = Memory::GetPointer(_BufferOut);
			buffer[3] = coverByte;

			if(coverByte)
				coverByte = 0;
			else
				coverByte = 0x01;
			
			return 1;
			*/
        }
        break;

	// DVDLowClearCoverInterrupt
	case 0x86:
		{
			Memory::Memset(_BufferOut, 0, _BufferOutSize);
			INFO_LOG(WII_IPC_DVD, "%s executes DVDLowClearCoverInterrupt (Buffer 0x%08x, 0x%x)", GetDeviceName().c_str(), _BufferOut, _BufferOutSize);
		}
		break;

	// DVDLowGetCoverStatus
	case 0x88:
		{
			Memory::Memset(_BufferOut, 0, _BufferOutSize);
			INFO_LOG(WII_IPC_DVD, "%s executes DVDLowGetCoverStatus (Buffer 0x%08x, 0x%x)", GetDeviceName().c_str(), _BufferOut, _BufferOutSize);
			return 1;
		}
		break;

	// DVDLowReset
	case 0x8a:
		{
			Memory::Memset(_BufferOut, 0, _BufferOutSize);
			INFO_LOG(WII_IPC_DVD, "%s executes DVDLowReset (Buffer 0x%08x, 0x%x)", GetDeviceName().c_str(), _BufferOut, _BufferOutSize);
			return 1;
		}
		break;

	// DVDLowOpenPartition
	case 0x8b:
		PanicAlert("DVDLowOpenPartition", Command);
		break;

	case 0x8c:
		//PanicAlert("DVDLowClosePartition");
		break;

	// DVDLowUnencryptedRead 
	case 0x8d:
		//PanicAlert("DVDLowUnencryptedRead");
		{
			if (_BufferOut == 0)
			{
				PanicAlert("DVDLowRead : _BufferOut == 0");
				return 0;
			}
			u32 Size = Memory::Read_U32(_BufferIn + 0x04);
			u64 DVDAddress = (u64)Memory::Read_U32(_BufferIn + 0x08) << 2;

			if (Size > _BufferOutSize)
			{
				PanicAlert("Detected attempt to read more data from the DVD than fit inside the out buffer. Clamp.");
				Size = _BufferOutSize;
			}

			if (VolumeHandler::RAWReadToPtr(Memory::GetPointer(_BufferOut), DVDAddress, Size) != true)
			{
				PanicAlert("Cant read from DVD_Plugin - DVD-Interface: Fatal Error");
			}		
		}
		break;

    // DVDLowSeek
    case 0xab:
		//PanicAlert("DVDLowSeek");
        break;

	case 0xe0:
		PanicAlert("DVD unknown command 0xe0, so far only seen in Tatsunoko. Don't know what to do, things will probably go wrong.");
		break;

	// DVDLowStopMotor
	case 0xe3:
		{
			Memory::Memset(_BufferOut, 0, _BufferOutSize);
			u32 eject = Memory::Read_U32(_BufferIn + 0x04);			

			INFO_LOG(WII_IPC_DVD, "%s executes DVDLowStopMotor (Buffer 0x%08x, 0x%x)", GetDeviceName().c_str(), _BufferOut, _BufferOutSize);

			if(eject)
			{
				INFO_LOG(WII_IPC_DVD, "Eject disc", GetDeviceName().c_str(), _BufferOut, _BufferOutSize);
				// TODO: eject the disc
			}
		}
		break;

	default:
		ERROR_LOG(WII_IPC_DVD, "%s executes unknown cmd 0x%08x (Buffer 0x%08x, 0x%x)", GetDeviceName().c_str(), Command, _BufferOut, _BufferOutSize);

        PanicAlert("%s executes unknown cmd 0x%08x", GetDeviceName().c_str(), Command);
		break;
	}

    // i dunno but prolly 1 is okay all the time :)
    return 1;
}
