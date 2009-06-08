// Copyright (C) 2003-2009 Dolphin Project.

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

#include "Common.h"

#include "WII_IPC_HLE_Device_DI.h"

#include "../HW/CPU.h"
#include "../HW/Memmap.h"
#include "../Core.h"

#include "../VolumeHandler.h"

#include "VolumeCreator.h"
#include "Filesystem.h"


CWII_IPC_HLE_Device_di::CWII_IPC_HLE_Device_di(u32 _DeviceID, const std::string& _rDeviceName )
    : IWII_IPC_HLE_Device(_DeviceID, _rDeviceName)
    , m_pFileSystem(NULL)
	, m_ErrorStatus(0)
{
	if (VolumeHandler::IsValid())
        m_pFileSystem = DiscIO::CreateFileSystem(VolumeHandler::GetVolume());
}

CWII_IPC_HLE_Device_di::~CWII_IPC_HLE_Device_di()
{
	if (m_pFileSystem)
	{
		delete m_pFileSystem;
		m_pFileSystem = NULL;
	}
}

bool CWII_IPC_HLE_Device_di::Open(u32 _CommandAddress, u32 _Mode)
{
    Memory::Write_U32(GetDeviceID(), _CommandAddress + 4);
    return true;
}

bool CWII_IPC_HLE_Device_di::Close(u32 _CommandAddress)
{
    Memory::Write_U32(0, _CommandAddress + 4);
    return true;
}

bool CWII_IPC_HLE_Device_di::IOCtl(u32 _CommandAddress) 
{
    INFO_LOG(WII_IPC_DVD,  "CWII_IPC_DVD_Device_di::IOCtl");

	u32 BufferIn		= Memory::Read_U32(_CommandAddress + 0x10);
	u32 BufferInSize	= Memory::Read_U32(_CommandAddress + 0x14);
    u32 BufferOut		= Memory::Read_U32(_CommandAddress + 0x18);
    u32 BufferOutSize	= Memory::Read_U32(_CommandAddress + 0x1C);
	u32 Command			= Memory::Read_U32(BufferIn) >> 24;

    DEBUG_LOG(WII_IPC_DVD, "%s - Command(0x%08x) BufferIn(0x%08x, 0x%x) BufferOut(0x%08x, 0x%x)",
		GetDeviceName().c_str(), Command, BufferIn, BufferInSize, BufferOut, BufferOutSize);

	if (Command == 0x7a)
		DumpCommands(_CommandAddress, 8, LogTypes::WII_IPC_DVD, LogTypes::LINFO);

	u32 ReturnValue = ExecuteCommand(BufferIn, BufferInSize, BufferOut, BufferOutSize);	
    Memory::Write_U32(ReturnValue, _CommandAddress + 0x4);

    return true;
}

bool CWII_IPC_HLE_Device_di::IOCtlV(u32 _CommandAddress) 
{  
	INFO_LOG(WII_IPC_DVD,  "CWII_IPC_DVD_Device_di::IOCtlV");

	SIOCtlVBuffer CommandBuffer(_CommandAddress);

	// Prepare the out buffer(s) with zeros as a safety precaution
	// to avoid returning bad values
	for(u32 i = 0; i < CommandBuffer.NumberPayloadBuffer; i++)
	{
		Memory::Memset(CommandBuffer.PayloadBuffer[i].m_Address, 0,
			CommandBuffer.PayloadBuffer[i].m_Size);
	}

	u32 ReturnValue = 0;
	switch (CommandBuffer.Parameter)
	{
	// DVDLowOpenPartition
	case 0x8b:
		{
			_dbg_assert_msg_(WII_IPC_DVD, CommandBuffer.InBuffer[1].m_Address == 0, "DVDLowOpenPartition with ticket");
			_dbg_assert_msg_(WII_IPC_DVD, CommandBuffer.InBuffer[2].m_Address == 0, "DVDLowOpenPartition with cert chain");

			u8 partition = Memory::Read_U32(CommandBuffer.m_Address + 4);
			INFO_LOG(WII_IPC_DVD, "DVD IOCtlV: DVDLowOpenPartition %i", partition);

			bool readOK = false;

			u64 TMDOffset = 0;
			readOK |= VolumeHandler::GetTMDOffset(partition, TMDOffset);

			// Read TMD to the buffer
			readOK |= VolumeHandler::RAWReadToPtr(Memory::GetPointer(CommandBuffer.PayloadBuffer[0].m_Address),
				TMDOffset, CommandBuffer.PayloadBuffer[0].m_Size);

			// Second outbuffer is error, we can ignore it

			ReturnValue = readOK ? 1 : 0;
		}
		break;

	default:
		ERROR_LOG(WII_IPC_DVD, "DVD IOCtlV: %i", CommandBuffer.Parameter);
		_dbg_assert_msg_(WII_IPC_DVD, 0, "DVD IOCtlV: %i", CommandBuffer.Parameter);
		break;
	}

	Memory::Write_U32(ReturnValue, _CommandAddress + 4);
    return true;
}

u32 CWII_IPC_HLE_Device_di::ExecuteCommand(u32 _BufferIn, u32 _BufferInSize, u32 _BufferOut, u32 _BufferOutSize)
{    
    u32 Command = Memory::Read_U32(_BufferIn) >> 24;

	// TATSUNOKO VS CAPCOM: Gets here with _BufferOut == 0!!!
	if (_BufferOut != 0)
	{
		// Set out buffer to zeroes as a safety precaution to avoid answering
		// nonsense values
		Memory::Memset(_BufferOut, 0, _BufferOutSize);
	}

	// Initializing a filesystem if it was just loaded
	if(!m_pFileSystem && VolumeHandler::IsValid())
		m_pFileSystem = DiscIO::CreateFileSystem(VolumeHandler::GetVolume());

	// De-initializing a filesystem if the volume was unmounted
	if(m_pFileSystem && !VolumeHandler::IsValid()) {
		delete m_pFileSystem;
		m_pFileSystem = NULL;
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
// 			buffer[0] = 0x01; // rev
// 			buffer[1] = 0x02;
// 			buffer[2] = 0x03; // dev code
// 			buffer[3] = 0x04;
// 			buffer[4] = 0x20; // firmware date
// 			buffer[5] = 0x08;
// 			buffer[6] = 0x08;
// 			buffer[7] = 0x29;

			buffer[4] = 0x20; // firmware date
			buffer[5] = 0x02;
			buffer[6] = 0x04;
			buffer[7] = 0x02;
			buffer[8] = 0x61; // version

			DEBUG_LOG(WII_IPC_DVD, "%s executes DVDLowInquiry (Buffer 0x%08x, 0x%x)",
				GetDeviceName().c_str(), _BufferOut, _BufferOutSize);

            return 1;
		}
		break;

	// DVDLowReadDiskID
	case 0x70:
		{
			VolumeHandler::RAWReadToPtr(Memory::GetPointer(_BufferOut), 0, _BufferOutSize);

			DEBUG_LOG(WII_IPC_DVD, "DVDLowReadDiskID %s",
				ArrayToString(Memory::GetPointer(_BufferOut), _BufferOutSize, 0, _BufferOutSize).c_str());

			return 1;
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
				INFO_LOG(WII_IPC_DVD, "    DVDLowRead: %s (0x%x) - (DVDAddr: 0x%x, Size: 0x%x)",
					pFilename, m_pFileSystem->GetFileSize(pFilename), DVDAddress, Size);
			}
			else
			{
				INFO_LOG(WII_IPC_DVD, "    DVDLowRead: file unkw - (DVDAddr: 0x%x, Size: 0x%x)",
					DVDAddress, Size);
			}

			if (Size > _BufferOutSize)
			{
				PanicAlert("Detected attempt to read more data from the DVD than fit inside the out buffer. Clamp.");
				Size = _BufferOutSize;
			}

			if (!VolumeHandler::ReadToPtr(Memory::GetPointer(_BufferOut), DVDAddress, Size))
			{
				PanicAlert("Cant read from DVD_Plugin - DVD-Interface: Fatal Error");
			}

			return 1;
		}
		break;

	// DVDLowWaitForCoverClose
    case 0x79:
        {
			INFO_LOG(WII_IPC_DVD, "%s executes DVDLowWaitForCoverClose (Buffer 0x%08x, 0x%x)",
				GetDeviceName().c_str(), _BufferOut, _BufferOutSize);
			return 4;
        }
        break;

	// DVDLowPrepareCoverRegister
    // DVDLowGetCoverReg - Called by "Legend of Spyro", MP3 and Wii System Menu
    case 0x7a:
        {   
			u8* buffer = Memory::GetPointer(_BufferOut);
			
			// DVD Cover Register States:
			// 0x00: Unknown state (keeps checking for DVD)
			// 0x01: No Disc inside
			// 0xFE: Reads Disc

			// We notify the application that we ejected a disc by
			// replacing the Change Disc menu button with "Eject" which 
			// in turn makes volume handler invalid for at least one
			// ioctl and then the user has enough time to load another
			// disc.
			// TODO: Make ejection mechanism recognized. (Currently eject 
			//       only works if DVDLowReset is called)
			
			buffer[0] = buffer[1] = buffer[2] = buffer[3] = 0x00;

			if(m_pFileSystem)
				buffer[0] = buffer[1] = buffer[2] = buffer[3] = 0xFE;

			INFO_LOG(WII_IPC_DVD, "%s executes DVDLowGetCoverReg (Buffer 0x%08x, 0x%x) %s",
				GetDeviceName().c_str(), _BufferOut, _BufferOutSize, 
				ArrayToString(buffer, _BufferOutSize, 0, _BufferOutSize).c_str());


			/* 
			   Hack for Legend of Spyro. Switching the 4th byte between 0 and 1 gets
			   through this check. The out buffer address remains the same all the
			   time so we don't have to bother making a global function.
			   
			   TODO: Make this compatible with MP3 			
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

	// DVDLowNotifyReset
	case 0x7e:
		PanicAlert("DVDLowNotifyReset");
		break;

	// DVDLowReadDvdPhysical
	case 0x80:
		PanicAlert("DVDLowReadDvdPhysical");
		break;

	// DVDLowReadDvdCopyright
	case 0x81:
		PanicAlert("DVDLowReadDvdCopyright");
		break;

	// DVDLowReadDvdDiscKey
	case 0x82:
		PanicAlert("DVDLowReadDvdDiscKey");
		break;

	// DVDLowClearCoverInterrupt
	case 0x86:
		{
			INFO_LOG(WII_IPC_DVD, "%s executes DVDLowClearCoverInterrupt (Buffer 0x%08x, 0x%x)",
				GetDeviceName().c_str(), _BufferOut, _BufferOutSize);
			return 1;
		}
		break;

	// DVDLowGetCoverStatus
	case 0x88:
		{
			INFO_LOG(WII_IPC_DVD, "%s executes DVDLowGetCoverStatus (Buffer 0x%08x, 0x%x)",
				GetDeviceName().c_str(), _BufferOut, _BufferOutSize);
			return 1;
		}
		break;

	// DVDLowReset
	case 0x8a:
		{
			INFO_LOG(WII_IPC_DVD, "%s executes DVDLowReset (Buffer 0x%08x, 0x%x)",
				GetDeviceName().c_str(), _BufferOut, _BufferOutSize);
			return 1;
		}
		break;

	// DVDLowOpenPartition
	case 0x8b:
		PanicAlert("DVDLowOpenPartition");
		return 1;
		break;

	// DVDLowClosePartition
	case 0x8c:
		DEBUG_LOG(WII_IPC_DVD, "DVDLowClosePartition");
		return 1;
		break;

	// DVDLowUnencryptedRead 
	case 0x8d:
		{
			if (_BufferOut == 0)
			{
				PanicAlert("DVDLowRead : _BufferOut == 0");
				return 0;
			}

			u32 Size = Memory::Read_U32(_BufferIn + 0x04);

			// We must make sure it is in a valid area! (#001 check)
			// * 0x00000000 - 0x00014000 (limit of older IOS versions)
			// * 0x460a0000 - 0x460a0008
			// * 0x7ed40000 - 0x7ed40008
			u32 DVDAddress32 = Memory::Read_U32(_BufferIn + 0x08);
			if (!( (DVDAddress32 > 0x00000000 && DVDAddress32 < 0x00014000)
				|| (((DVDAddress32 + Size) > 0x00000000) && (DVDAddress32 + Size) < 0x00014000)
				|| (DVDAddress32 > 0x460a0000 && DVDAddress32 < 0x460a0008)
				|| (((DVDAddress32 + Size) > 0x460a0000) && (DVDAddress32 + Size) < 0x460a0008)
				|| (DVDAddress32 > 0x7ed40000 && DVDAddress32 < 0x7ed40008)
				|| (((DVDAddress32 + Size) > 0x7ed40000) && (DVDAddress32 + Size) < 0x7ed40008)
				))
			{
				INFO_LOG(WII_IPC_DVD, "DVDLowUnencryptedRead: trying to read out of bounds @ %x", DVDAddress32);
				m_ErrorStatus = 0x52100; // Logical block address out of range
				// Should cause software to call DVDLowRequestError
				return 2;
			}

			u64 DVDAddress = (u64)(DVDAddress32 << 2);

			INFO_LOG(WII_IPC_DVD, "DVDLowUnencryptedRead: DVDAddr: 0x%08x, Size: 0x%x", DVDAddress, Size);

			if (Size > _BufferOutSize)
			{
				PanicAlert("Detected attempt to read more data from the DVD than fit inside the out buffer. Clamp.");
				Size = _BufferOutSize;
			}

			if (!VolumeHandler::RAWReadToPtr(Memory::GetPointer(_BufferOut), DVDAddress, Size))
			{
				PanicAlert("Cant read from DVD_Plugin - DVD-Interface: Fatal Error");
			}

			return 1;
		}
		break;

	// DVDLowEnableDvdVideo
	case 0x8e:
		PanicAlert("DVDLowEnableDvdVideo");
		break;

	// DVDLowReportKey
	case 0xa4:
		INFO_LOG(WII_IPC_DVD, "DVDLowReportKey");
		// Does not work on commercial discs
		m_ErrorStatus = 0x052000; // Invalid command operation code
		return 2;
		break;
    
	// DVDLowSeek
    case 0xab:
		PanicAlert("DVDLowSeek");
        break;

// Apparently Dx commands have never been seen in dolphin? *shrug*
	// DVDLowReadDvd
	case 0xd0:
		PanicAlert("DVDLowReadDvd");
		break;

	// DVDLowReadDvdConfig
	case 0xd1:
		PanicAlert("DVDLowReadDvdConfig");
		break;

	// DVDLowStopLaser
	case 0xd2:
		PanicAlert("DVDLowStopLaser");
		break;

	// DVDLowOffset
	case 0xd9:
		PanicAlert("DVDLowOffset");
		break;

	// DVDLowReadDiskBca
	case 0xda:
		PanicAlert("DVDLowReadDiskBca");
		break;

	// DVDLowRequestDiscStatus
	case 0xdb:
		PanicAlert("DVDLowRequestDiscStatus");
		break;

	// DVDLowRequestRetryNumber
	case 0xdc:
		PanicAlert("DVDLowRequestRetryNumber");
		break;

	// DVDLowSetMaximumRotation
	case 0xdd:
		PanicAlert("DVDLowSetMaximumRotation");
		break;

	// DVDLowSerMeasControl
	case 0xdf:
		PanicAlert("DVDLowSerMeasControl");
		break;

	// DVDLowRequestError
	case 0xe0:
		// Identical to the error codes found in yagcd section 5.7.3.5.1 (so far)
		WARN_LOG(WII_IPC_DVD, "DVDLowRequestError status = 0x%08x", m_ErrorStatus);
		Memory::Write_U32(m_ErrorStatus, _BufferOut);
		break;

// Ex commands are immediate and respond with 4 bytes
	// DVDLowStopMotor
	case 0xe3:
		{
			u32 eject = Memory::Read_U32(_BufferIn + 0x04);			

			INFO_LOG(WII_IPC_DVD, "%s executes DVDLowStopMotor eject = %s",
				GetDeviceName().c_str(), eject ? "true" : "false");

			if (eject)
			{
				// TODO: eject the disc
			}
		}
		break;

	// DVDLowAudioBufferConfig
	case 0xe4:
		PanicAlert("DVDLowAudioBufferConfig");
		break;

	default:
		ERROR_LOG(WII_IPC_DVD, "%s executes unknown cmd 0x%08x (Buffer 0x%08x, 0x%x)",
			GetDeviceName().c_str(), Command, _BufferOut, _BufferOutSize);

        PanicAlert("%s executes unknown cmd 0x%08x", GetDeviceName().c_str(), Command);
		break;
	}

    // i dunno but prolly 1 is okay all the time :)
    return 1;
}
