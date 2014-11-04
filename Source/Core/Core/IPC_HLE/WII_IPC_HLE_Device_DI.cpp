// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <cinttypes>

#include "Common/CommonTypes.h"
#include "Common/Logging/LogManager.h"

#include "Core/Core.h"
#include "Core/VolumeHandler.h"
#include "Core/HW/CPU.h"
#include "Core/HW/DVDInterface.h"
#include "Core/HW/Memmap.h"
#include "Core/HW/SystemTimers.h"

#include "Core/IPC_HLE/WII_IPC_HLE.h"
#include "Core/IPC_HLE/WII_IPC_HLE_Device_DI.h"

#include "DiscIO/FileMonitor.h"
#include "DiscIO/Filesystem.h"
#include "DiscIO/VolumeCreator.h"

using namespace DVDInterface;


#define DI_COVER_REG_INITIALIZED  0 // Should be 4, but doesn't work correctly...
#define DI_COVER_REG_NO_DISC      1

CWII_IPC_HLE_Device_di::CWII_IPC_HLE_Device_di(u32 _DeviceID, const std::string& _rDeviceName )
	: IWII_IPC_HLE_Device(_DeviceID, _rDeviceName)
	, m_pFileSystem(nullptr)
	, m_ErrorStatus(0)
	, m_CoverStatus(DI_COVER_REG_NO_DISC)
{}

CWII_IPC_HLE_Device_di::~CWII_IPC_HLE_Device_di()
{
	if (m_pFileSystem)
	{
		delete m_pFileSystem;
		m_pFileSystem = nullptr;
	}
}

bool CWII_IPC_HLE_Device_di::Open(u32 _CommandAddress, u32 _Mode)
{
	if (VolumeHandler::IsValid())
	{
		m_pFileSystem = DiscIO::CreateFileSystem(VolumeHandler::GetVolume());
		m_CoverStatus |= DI_COVER_REG_INITIALIZED;
		m_CoverStatus &= ~DI_COVER_REG_NO_DISC;
	}
	Memory::Write_U32(GetDeviceID(), _CommandAddress + 4);
	m_Active = true;
	return true;
}

bool CWII_IPC_HLE_Device_di::Close(u32 _CommandAddress, bool _bForce)
{
	if (m_pFileSystem)
	{
		delete m_pFileSystem;
		m_pFileSystem = nullptr;
	}
	m_ErrorStatus = 0;
	if (!_bForce)
		Memory::Write_U32(0, _CommandAddress + 4);
	m_Active = false;
	return true;
}

bool CWII_IPC_HLE_Device_di::IOCtl(u32 _CommandAddress)
{
	u32 BufferIn      = Memory::Read_U32(_CommandAddress + 0x10);
	u32 BufferInSize  = Memory::Read_U32(_CommandAddress + 0x14);
	u32 BufferOut     = Memory::Read_U32(_CommandAddress + 0x18);
	u32 BufferOutSize = Memory::Read_U32(_CommandAddress + 0x1C);
	u32 Command       = Memory::Read_U32(BufferIn) >> 24;

	DEBUG_LOG(WII_IPC_DVD, "IOCtl Command(0x%08x) BufferIn(0x%08x, 0x%x) BufferOut(0x%08x, 0x%x)",
		Command, BufferIn, BufferInSize, BufferOut, BufferOutSize);

	u32 ReturnValue = ExecuteCommand(BufferIn, BufferInSize, BufferOut, BufferOutSize);
	Memory::Write_U32(ReturnValue, _CommandAddress + 0x4);

	return true;
}

bool CWII_IPC_HLE_Device_di::IOCtlV(u32 _CommandAddress)
{
	SIOCtlVBuffer CommandBuffer(_CommandAddress);

	// Prepare the out buffer(s) with zeros as a safety precaution
	// to avoid returning bad values
	for (u32 i = 0; i < CommandBuffer.NumberPayloadBuffer; i++)
	{
		Memory::Memset(CommandBuffer.PayloadBuffer[i].m_Address, 0,
			CommandBuffer.PayloadBuffer[i].m_Size);
	}

	u32 ReturnValue = 0;
	switch (CommandBuffer.Parameter)
	{
	case DVDLowOpenPartition:
		{
			_dbg_assert_msg_(WII_IPC_DVD, CommandBuffer.InBuffer[1].m_Address == 0, "DVDLowOpenPartition with ticket");
			_dbg_assert_msg_(WII_IPC_DVD, CommandBuffer.InBuffer[2].m_Address == 0, "DVDLowOpenPartition with cert chain");

			// Get TMD offset for requested partition...
			u64 const TMDOffset = ((u64)Memory::Read_U32(CommandBuffer.InBuffer[0].m_Address + 4) << 2 ) + 0x2c0;

			INFO_LOG(WII_IPC_DVD, "DVDLowOpenPartition: TMDOffset 0x%016" PRIx64, TMDOffset);

			static u32 const TMDsz = 0x208; //CommandBuffer.PayloadBuffer[0].m_Size;
			u8 pTMD[TMDsz];

			// Read TMD to the buffer
			VolumeHandler::RAWReadToPtr(pTMD, TMDOffset, TMDsz);

			Memory::CopyToEmu(CommandBuffer.PayloadBuffer[0].m_Address, pTMD, TMDsz);
			WII_IPC_HLE_Interface::ES_DIVerify(pTMD, TMDsz);

			ReturnValue = 1;
		}
		break;

	default:
		ERROR_LOG(WII_IPC_DVD, "IOCtlV: %i", CommandBuffer.Parameter);
		_dbg_assert_msg_(WII_IPC_DVD, 0, "IOCtlV: %i", CommandBuffer.Parameter);
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
	if (!m_pFileSystem && VolumeHandler::IsValid())
	{
		m_pFileSystem = DiscIO::CreateFileSystem(VolumeHandler::GetVolume());
		m_CoverStatus |= DI_COVER_REG_INITIALIZED;
		m_CoverStatus &= ~DI_COVER_REG_NO_DISC;
	}

	// De-initializing a filesystem if the volume was unmounted
	if (m_pFileSystem && !VolumeHandler::IsValid())
	{
		delete m_pFileSystem;
		m_pFileSystem = nullptr;
		m_CoverStatus |= DI_COVER_REG_NO_DISC;
	}

	switch (Command)
	{
	case DVDLowInquiry:
		{
			// (shuffle2) Taken from my Wii
			Memory::Write_U32(0x00000002, _BufferOut);
			Memory::Write_U32(0x20060526, _BufferOut + 4);
			// This was in the oubuf even though this cmd is only supposed to reply with 64bits
			// However, this and other tests strongly suggest that the buffer is static, and it's never - or rarely cleared.
			Memory::Write_U32(0x41000000, _BufferOut + 8);

			INFO_LOG(WII_IPC_DVD, "DVDLowInquiry (Buffer 0x%08x, 0x%x)",
				_BufferOut, _BufferOutSize);
		}
		break;

	case DVDLowReadDiskID:
		{
			VolumeHandler::RAWReadToPtr(Memory::GetPointer(_BufferOut), 0, _BufferOutSize);

			INFO_LOG(WII_IPC_DVD, "DVDLowReadDiskID %s",
				ArrayToString(Memory::GetPointer(_BufferOut), _BufferOutSize, _BufferOutSize).c_str());
		}
		break;

	case DVDLowRead:
		{
			if (_BufferOut == 0)
			{
				PanicAlert("DVDLowRead : _BufferOut == 0");
				return 0;
			}
			u32 Size = Memory::Read_U32(_BufferIn + 0x04);
			u64 DVDAddress = (u64)Memory::Read_U32(_BufferIn + 0x08) << 2;

			// Don't do anything if the log is unselected
			if (LogManager::GetInstance()->IsEnabled(LogTypes::FILEMON))
			{
				if (m_pFileSystem)
				{
					const std::string filename = m_pFileSystem->GetFileName(DVDAddress);

					INFO_LOG(WII_IPC_DVD, "DVDLowRead: %s (0x%" PRIx64 ") - (DVDAddr: 0x%" PRIx64 ", Size: 0x%x)",
						filename.c_str(), m_pFileSystem->GetFileSize(filename), DVDAddress, Size);
					FileMon::CheckFile(filename, (int)m_pFileSystem->GetFileSize(filename));
				}
				else
				{
					ERROR_LOG(WII_IPC_DVD, "Filesystem is invalid.");
				}
			}

			if (Size > _BufferOutSize)
			{
				PanicAlertT("Detected attempt to read more data from the DVD than fit inside the out buffer. Clamp.");
				Size = _BufferOutSize;
			}

			if (!VolumeHandler::ReadToPtr(Memory::GetPointer(_BufferOut), DVDAddress, Size))
			{
				PanicAlertT("DVDLowRead - Fatal Error: failed to read from volume");
			}
		}
		break;

	case DVDLowWaitForCoverClose:
		{
			INFO_LOG(WII_IPC_DVD, "DVDLowWaitForCoverClose (Buffer 0x%08x, 0x%x)",
				_BufferOut, _BufferOutSize);
			return 4; // ???
		}
		break;

	case DVDLowGetCoverReg:
		Memory::Write_U32(m_CoverStatus, _BufferOut);

		INFO_LOG(WII_IPC_DVD, "DVDLowGetCoverReg 0x%08x", Memory::Read_U32(_BufferOut));
		break;

	case DVDLowNotifyReset:
		PanicAlert("DVDLowNotifyReset");
		break;

	case DVDLowReadDvdPhysical:
		PanicAlert("DVDLowReadDvdPhysical");
		break;

	case DVDLowReadDvdCopyright:
		PanicAlert("DVDLowReadDvdCopyright");
		break;

	case DVDLowReadDvdDiscKey:
		PanicAlert("DVDLowReadDvdDiscKey");
		break;

	case DVDLowClearCoverInterrupt:
		// TODO: check (seems to work ok)
		INFO_LOG(WII_IPC_DVD, "DVDLowClearCoverInterrupt");
		ClearCoverInterrupt();
		break;

	case DVDLowGetCoverStatus:
		Memory::Write_U32(IsDiscInside() ? 2 : 1, _BufferOut);
		INFO_LOG(WII_IPC_DVD, "DVDLowGetCoverStatus: Disc %sInserted", IsDiscInside() ? "" : "Not ");
		break;

	case DVDLowReset:
		INFO_LOG(WII_IPC_DVD, "DVDLowReset");
		break;

	case DVDLowClosePartition:
		INFO_LOG(WII_IPC_DVD, "DVDLowClosePartition");
		break;

	case DVDLowUnencryptedRead:
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
			if (!((DVDAddress32 > 0x00000000 && DVDAddress32 < 0x00014000) ||
			      (((DVDAddress32 + Size) > 0x00000000) && (DVDAddress32 + Size) < 0x00014000) ||
			      (DVDAddress32 > 0x460a0000 && DVDAddress32 < 0x460a0008) ||
			      (((DVDAddress32 + Size) > 0x460a0000) && (DVDAddress32 + Size) < 0x460a0008) ||
			      (DVDAddress32 > 0x7ed40000 && DVDAddress32 < 0x7ed40008) ||
			      (((DVDAddress32 + Size) > 0x7ed40000) && (DVDAddress32 + Size) < 0x7ed40008)))
			{
				WARN_LOG(WII_IPC_DVD, "DVDLowUnencryptedRead: trying to read out of bounds @ %x", DVDAddress32);
				m_ErrorStatus = ERROR_READY | ERROR_BLOCK_OOB;
				// Should cause software to call DVDLowRequestError
				return 2;
			}

			u64 DVDAddress = (u64)DVDAddress32 << 2;

			INFO_LOG(WII_IPC_DVD, "DVDLowUnencryptedRead: DVDAddr: 0x%08" PRIx64 ", Size: 0x%x", DVDAddress, Size);

			if (Size > _BufferOutSize)
			{
				PanicAlertT("Detected attempt to read more data from the DVD than fit inside the out buffer. Clamp.");
				Size = _BufferOutSize;
			}
			if (!VolumeHandler::RAWReadToPtr(Memory::GetPointer(_BufferOut), DVDAddress, Size))
			{
				PanicAlertT("DVDLowUnencryptedRead - Fatal Error: failed to read from volume");
			}
		}
		break;

	case DVDLowEnableDvdVideo:
		ERROR_LOG(WII_IPC_DVD, "DVDLowEnableDvdVideo");
		break;

	case DVDLowReportKey:
		INFO_LOG(WII_IPC_DVD, "DVDLowReportKey");
		// Does not work on retail discs/drives
		// Retail games send this command to see if they are running on real retail hw
		m_ErrorStatus = ERROR_READY | ERROR_INV_CMD;
		return 2;
		break;

	case DVDLowSeek:
		{
			u64 DVDAddress = Memory::Read_U32(_BufferIn + 0x4) << 2;

			if (m_pFileSystem)
			{
				const std::string filename = m_pFileSystem->GetFileName(DVDAddress);

				INFO_LOG(WII_IPC_DVD, "DVDLowSeek: %s (0x%" PRIx64 ") - (DVDAddr: 0x%" PRIx64 ")",
					filename.c_str(), m_pFileSystem->GetFileSize(filename), DVDAddress);
			}
			else
			{
				ERROR_LOG(WII_IPC_DVD, "Filesystem is invalid.");
			}
		}
		break;

	case DVDLowReadDvd:
		ERROR_LOG(WII_IPC_DVD, "DVDLowReadDvd");
		break;

	case DVDLowReadDvdConfig:
		ERROR_LOG(WII_IPC_DVD, "DVDLowReadDvdConfig");
		break;

	case DVDLowStopLaser:
		ERROR_LOG(WII_IPC_DVD, "DVDLowStopLaser");
		break;

	case DVDLowOffset:
		ERROR_LOG(WII_IPC_DVD, "DVDLowOffset");
		break;

	case DVDLowReadDiskBca:
		WARN_LOG(WII_IPC_DVD, "DVDLowReadDiskBca");
		Memory::Write_U32(1, _BufferOut + 0x30);
		break;

	case DVDLowRequestDiscStatus:
		ERROR_LOG(WII_IPC_DVD, "DVDLowRequestDiscStatus");
		break;

	case DVDLowRequestRetryNumber:
		ERROR_LOG(WII_IPC_DVD, "DVDLowRequestRetryNumber");
		break;

	case DVDLowSetMaximumRotation:
		ERROR_LOG(WII_IPC_DVD, "DVDLowSetMaximumRotation");
		break;

	case DVDLowSerMeasControl:
		ERROR_LOG(WII_IPC_DVD, "DVDLowSerMeasControl");
		break;

	case DVDLowRequestError:
		// Identical to the error codes found in yagcd section 5.7.3.5.1 (so far)
		WARN_LOG(WII_IPC_DVD, "DVDLowRequestError status = 0x%08x", m_ErrorStatus);
		Memory::Write_U32(m_ErrorStatus, _BufferOut);
		// When does error status get reset?
		break;

// Ex commands are immediate and respond with 4 bytes
	case DVDLowStopMotor:
		{
			u32 eject = Memory::Read_U32(_BufferIn + 4);
			// Drive won't do anything till reset is issued. I think it replies like nothing is wrong though?
			u32 kill = Memory::Read_U32(_BufferIn + 8);

			INFO_LOG(WII_IPC_DVD, "DVDLowStopMotor %s %s",
				eject ? "eject" : "", kill ? "kill!" : "");

			if (eject)
			{
				SetLidOpen(true);
				SetDiscInside(false);
			}
		}
		break;

	case DVDLowAudioBufferConfig:
		/*
			For more information: http://www.crazynation.org/GC/GC_DD_TECH/GCTech.htm

			Upon Power up or reset , 2 commands must be issued for proper use of audio streaming:
			DVDReadDiskID A8000040,00000000,00000020
			DVDLowAudioBufferConfig E4xx00yy,00000000,00000020

			xx=byte 8 [0 or 1] from the disk header retrieved from DVDReadDiskID
			yy=0 (if xx=0) or 0xA (if xx=1)
		*/
		ERROR_LOG(WII_IPC_DVD, "DVDLowAudioBufferConfig");
		break;

	// New Super Mario Bros.Wii sends these cmds
	// but it seems we don't need to implement anything
	case 0x95:
	case 0x96:
		WARN_LOG(WII_IPC_DVD, "Unimplemented command 0x%08x (Buffer 0x%08x, 0x%x)",
			Command, _BufferOut, _BufferOutSize);
		break;

	default:
		ERROR_LOG(WII_IPC_DVD, "Unknown command 0x%08x (Buffer 0x%08x, 0x%x)",
			Command, _BufferOut, _BufferOutSize);

		PanicAlertT("Unknown command 0x%08x", Command);
		break;
	}

	// i dunno but prolly 1 is okay all the time :)
	return 1;
}

int CWII_IPC_HLE_Device_di::GetCmdDelay(u32 _CommandAddress)
{
	u32 BufferIn = Memory::Read_U32(_CommandAddress + 0x10);
	u32 Command  = Memory::Read_U32(BufferIn) >> 24;

	// Hacks below

	switch (Command)
	{
	case DVDLowRead:
	case DVDLowUnencryptedRead:
	{
		u32 const Size = Memory::Read_U32(BufferIn + 0x04);
		// Delay depends on size of read, that makes sense, right?
		// More than ~1150K "bytes / sec" hangs NSMBWii on boot.
		// Less than ~800K "bytes / sec" hangs DKCR randomly (ok, probably not true)
		return SystemTimers::GetTicksPerSecond() / 975000 * Size;
	}

	case DVDLowClearCoverInterrupt:
		// Less than ~1/155th of a second hangs Oregon Trail at "loading wheel".
		// More than ~1/140th of a second hangs Resident Evil Archives: Resident Evil Zero.
		return SystemTimers::GetTicksPerSecond() / 146;

	// case DVDLowAudioBufferConfig:
	// case DVDLowInquiry:
	// case DVDLowReadDiskID:
	// case DVDLowWaitForCoverClose:
	// case DVDLowGetCoverReg:
	// case DVDLowGetCoverStatus:
	// case DVDLowReset:
	// case DVDLowClosePartition:
	default:
		// random numbers here!
		// More than ~1/2000th of a second hangs DKCR with DSP HLE, maybe.
		return SystemTimers::GetTicksPerSecond() / 15000;
	}
}
