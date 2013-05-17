// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Common.h"
#include "FileUtil.h"
#include "StringUtil.h"
#include "ChunkFile.h"

#include "WII_IPC_HLE_Device_fs.h"
#include "WII_IPC_HLE_Device_FileIO.h"
#include "NandPaths.h"
#include <algorithm>


static Common::replace_v replacements;

// This is used by several of the FileIO and /dev/fs functions
std::string HLE_IPC_BuildFilename(std::string path_wii, int _size)
{
	std::string path_full = File::GetUserPath(D_WIIROOT_IDX);

	// Replaces chars that FAT32 can't support with strings defined in /sys/replace
	for (auto i = replacements.begin(); i != replacements.end(); ++i)
	{
		for (size_t j = 0; (j = path_wii.find(i->first, j)) != path_wii.npos; ++j)
			path_wii.replace(j, 1, i->second);
	}

	path_full += path_wii;

	return path_full;
}

CWII_IPC_HLE_Device_FileIO::CWII_IPC_HLE_Device_FileIO(u32 _DeviceID, const std::string& _rDeviceName)
	: IWII_IPC_HLE_Device(_DeviceID, _rDeviceName, false)	// not a real hardware
	, m_Mode(0)
	, m_SeekPos(0)
{
	Common::ReadReplacements(replacements);
}

CWII_IPC_HLE_Device_FileIO::~CWII_IPC_HLE_Device_FileIO()
{
}

bool CWII_IPC_HLE_Device_FileIO::Close(u32 _CommandAddress, bool _bForce)
{
	INFO_LOG(WII_IPC_FILEIO, "FileIO: Close %s (DeviceID=%08x)", m_Name.c_str(), m_DeviceID);	
	m_Mode = 0;

	// Close always return 0 for success
	if (_CommandAddress && !_bForce)
		Memory::Write_U32(0, _CommandAddress + 4);
	m_Active = false;
	return true;
}

bool CWII_IPC_HLE_Device_FileIO::Open(u32 _CommandAddress, u32 _Mode)  
{
	m_Mode = _Mode;
	u32 ReturnValue = 0;
	
	static const char* const Modes[] =
	{
		"Unk Mode",
		"Read only",
		"Write only",
		"Read and Write"
	};
	
	m_filepath = HLE_IPC_BuildFilename(m_Name, 64);
	
	// The file must exist before we can open it
	// It should be created by ISFS_CreateFile, not here
	if (File::Exists(m_filepath))
	{
		INFO_LOG(WII_IPC_FILEIO, "FileIO: Open %s (%s == %08X)", m_Name.c_str(), Modes[_Mode], _Mode);
		ReturnValue = m_DeviceID;
	}
	else
	{
		WARN_LOG(WII_IPC_FILEIO, "FileIO: Open (%s) failed - File doesn't exist %s", Modes[_Mode], m_filepath.c_str());
		ReturnValue = FS_FILE_NOT_EXIST;
	}

	if (_CommandAddress)
		Memory::Write_U32(ReturnValue, _CommandAddress+4);
	m_Active = true;
	return true;
}

File::IOFile CWII_IPC_HLE_Device_FileIO::OpenFile()
{
	const char* open_mode = "";
	
	switch (m_Mode)
	{
	case ISFS_OPEN_READ:
		open_mode = "rb";
		break;
	
	case ISFS_OPEN_WRITE:
	case ISFS_OPEN_RW:
		open_mode = "r+b";
		break;
	
	default:
		PanicAlertT("FileIO: Unknown open mode : 0x%02x", m_Mode);
		break;
	}
	
	return File::IOFile(m_filepath, open_mode);
}

bool CWII_IPC_HLE_Device_FileIO::Seek(u32 _CommandAddress)
{
	u32 ReturnValue	= FS_RESULT_FATAL;
	const u32 SeekOffset = Memory::Read_U32(_CommandAddress + 0xC);
	const u32 Mode = Memory::Read_U32(_CommandAddress + 0x10);

	if (auto file = OpenFile())
	{
		ReturnValue = FS_RESULT_FATAL;

		const u64 fileSize = file.GetSize();
		INFO_LOG(WII_IPC_FILEIO, "FileIO: Seek Pos: 0x%08x, Mode: %i (%s, Length=0x%08llx)", SeekOffset, Mode, m_Name.c_str(), fileSize);
		u64 wantedPos = 0;
		switch (Mode)
		{
		case 0:
			wantedPos = SeekOffset;
			break;
			
		case 1:
			wantedPos = m_SeekPos + SeekOffset;
			break;
			
		case 2:
			wantedPos = fileSize + (s32)SeekOffset;
			break;
			
		default:
			PanicAlert("CWII_IPC_HLE_Device_FileIO Unsupported seek mode %i", Mode);
			ReturnValue = FS_RESULT_FATAL;
			break;
		}
		
		if (wantedPos <= fileSize)
		{
			m_SeekPos = wantedPos;
			ReturnValue = m_SeekPos;
		}
	}
	else
	{
		// TODO: This can't be right.
		ReturnValue = FS_FILE_NOT_EXIST;
	}
	Memory::Write_U32(ReturnValue, _CommandAddress + 0x4);

	return true;
}

bool CWII_IPC_HLE_Device_FileIO::Read(u32 _CommandAddress) 
{	
	u32 ReturnValue = FS_EACCESS;
	const u32 Address	= Memory::Read_U32(_CommandAddress + 0xC); // Read to this memory address
	const u32 Size	= Memory::Read_U32(_CommandAddress + 0x10);


	if (auto file = OpenFile())
	{
		if (m_Mode == ISFS_OPEN_WRITE)
		{
			WARN_LOG(WII_IPC_FILEIO, "FileIO: Attempted to read 0x%x bytes to 0x%08x on a write-only file %s", Size, Address, m_Name.c_str());
		}
		else
		{
			INFO_LOG(WII_IPC_FILEIO, "FileIO: Read 0x%x bytes to 0x%08x from %s", Size, Address, m_Name.c_str());
			file.Seek(m_SeekPos, SEEK_SET);
			ReturnValue = (u32)fread(Memory::GetPointer(Address), 1, Size, file.GetHandle());
			if (ReturnValue != Size && ferror(file.GetHandle()))
			{
				ReturnValue = FS_EACCESS;
			}
			else
			{
				m_SeekPos += Size;
			}
			
		}
	}
	else
	{
		ERROR_LOG(WII_IPC_FILEIO, "FileIO: Failed to read from %s (Addr=0x%08x Size=0x%x) - file could not be opened or does not exist", m_Name.c_str(), Address, Size);	
		ReturnValue = FS_FILE_NOT_EXIST;
	}

	Memory::Write_U32(ReturnValue, _CommandAddress + 0x4);
	return true;
}

bool CWII_IPC_HLE_Device_FileIO::Write(u32 _CommandAddress) 
{		
	u32 ReturnValue = FS_EACCESS;
	const u32 Address	= Memory::Read_U32(_CommandAddress + 0xC); // Write data from this memory address
	const u32 Size	= Memory::Read_U32(_CommandAddress + 0x10);


	if (auto file = OpenFile())
	{
		if (m_Mode == ISFS_OPEN_READ)
		{
			WARN_LOG(WII_IPC_FILEIO, "FileIO: Attempted to write 0x%x bytes from 0x%08x to a read-only file %s", Size, Address, m_Name.c_str());
		}
		else
		{
			INFO_LOG(WII_IPC_FILEIO, "FileIO: Write 0x%04x bytes from 0x%08x to %s", Size, Address, m_Name.c_str());
			file.Seek(m_SeekPos, SEEK_SET);
			if (file.WriteBytes(Memory::GetPointer(Address), Size))
			{
				ReturnValue = Size;
				m_SeekPos += Size;
			}
		}
	}
	else
	{
		ERROR_LOG(WII_IPC_FILEIO, "FileIO: Failed to read from %s (Addr=0x%08x Size=0x%x) - file could not be opened or does not exist", m_Name.c_str(), Address, Size);	
		ReturnValue = FS_FILE_NOT_EXIST;
	}

	Memory::Write_U32(ReturnValue, _CommandAddress + 0x4);
	return true;
}

bool CWII_IPC_HLE_Device_FileIO::IOCtl(u32 _CommandAddress) 
{
	INFO_LOG(WII_IPC_FILEIO, "FileIO: IOCtl (Device=%s)", m_Name.c_str());
#if defined(_DEBUG) || defined(DEBUGFAST)
	DumpCommands(_CommandAddress);
#endif
	const u32 Parameter =  Memory::Read_U32(_CommandAddress + 0xC);	
	u32 ReturnValue = 0;

	switch (Parameter)
	{
	case ISFS_IOCTL_GETFILESTATS:
		{
			if (auto file = OpenFile())
			{
				u32 m_FileLength = (u32)file.GetSize();

				const u32 BufferOut = Memory::Read_U32(_CommandAddress + 0x18);
				INFO_LOG(WII_IPC_FILEIO, "FileIO: ISFS_IOCTL_GETFILESTATS");
				INFO_LOG(WII_IPC_FILEIO, "  File: %s, Length: %i, Pos: %i", m_Name.c_str(), m_FileLength, m_SeekPos);

				Memory::Write_U32(m_FileLength, BufferOut);
				Memory::Write_U32(m_SeekPos, BufferOut+4);
			}
			else
			{
				ReturnValue = FS_FILE_NOT_EXIST;
			}
		}
		break;

	default:
		{
			PanicAlert("CWII_IPC_HLE_Device_FileIO: Parameter %i", Parameter);
		}
		break;

	}

	Memory::Write_U32(ReturnValue, _CommandAddress + 0x4);

	return true;
}

void CWII_IPC_HLE_Device_FileIO::DoState(PointerWrap &p)
{
	DoStateShared(p);

	p.Do(m_Mode);
	p.Do(m_SeekPos);
	
	m_filepath = HLE_IPC_BuildFilename(m_Name, 64);
}
