// Copyright (C) 2003 Dolphin Project.

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
#include "FileUtil.h"
#include "StringUtil.h"
#include "ChunkFile.h"

#include "WII_IPC_HLE_Device_fs.h"
#include "WII_IPC_HLE_Device_FileIO.h"
#include "NandPaths.h"
#include <algorithm>


static Common::replace_v replacements;

// This is used by several of the FileIO and /dev/fs/ functions 
std::string HLE_IPC_BuildFilename(const char* _pFilename, int _size)
{
	std::string path_full = File::GetUserPath(D_WIIROOT_IDX);
	std::string path_wii(_pFilename);

	if ((path_wii.length() > 0) && (path_wii[1] == '0'))
		path_full += std::string("/title"); // this looks and feel like a hack...

	// Replaces chars that FAT32 can't support with strings defined in /sys/replace
	for (Common::replace_v::const_iterator i = replacements.begin(); i != replacements.end(); ++i)
	{
		for (size_t j = 0; (j = path_wii.find(i->first, j)) != path_wii.npos; ++j)
			path_wii.replace(j, 1, i->second);
	}

	path_full += path_wii;

	return path_full;
}

void HLE_IPC_CreateVirtualFATFilesystem()
{
	const int cdbSize = 0x01400000;
	const std::string cdbPath = Common::GetTitleDataPath(TITLEID_SYSMENU) + "cdb.vff";
	if ((int)File::GetSize(cdbPath) < cdbSize)
	{
		// cdb.vff is a virtual Fat filesystem created on first launch of sysmenu
		// we create it here as it is faster ~3 minutes for me when sysmenu does it ~1 second created here
		const u8 cdbHDR[0x20] = {'V', 'F', 'F', 0x20, 0xfe, 0xff, 1, 0, 1, 0x40, 0, 0, 0, 0x20};
		const u8 cdbFAT[4] = {0xf0, 0xff, 0xff, 0xff};

		File::IOFile cdbFile(cdbPath, "wb");
		if (cdbFile)
		{
			cdbFile.WriteBytes(cdbHDR, 0x20);
			cdbFile.WriteBytes(cdbFAT, 0x4);
			cdbFile.Seek(0x14020, SEEK_SET);
			cdbFile.WriteBytes(cdbFAT, 0x4);
			// 20 MiB file
			cdbFile.Seek(cdbSize - 1, SEEK_SET);
			// write the final 0 to 0 file from the second FAT to 20 MiB
			cdbFile.WriteBytes(cdbHDR + 14, 1);
			if (!cdbFile.IsGood())
			{
				cdbFile.Close();
				File::Delete(cdbPath);
			}
			cdbFile.Flush();
			cdbFile.Close();
		}
	}
}

CWII_IPC_HLE_Device_FileIO::CWII_IPC_HLE_Device_FileIO(u32 _DeviceID, const std::string& _rDeviceName) 
	: IWII_IPC_HLE_Device(_DeviceID, _rDeviceName, false)	// not a real hardware
	, m_pFileHandle(NULL)
	, m_FileLength(0)
	, m_Mode(0)
{
	Common::ReadReplacements(replacements);
}

CWII_IPC_HLE_Device_FileIO::~CWII_IPC_HLE_Device_FileIO()
{
}

bool CWII_IPC_HLE_Device_FileIO::Close(u32 _CommandAddress, bool _bForce)
{
	INFO_LOG(WII_IPC_FILEIO, "FileIO: Close %s (DeviceID=%08x)", m_Name.c_str(), m_DeviceID);	

	m_pFileHandle.Close();

	m_FileLength = 0;
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

	// close the file handle if we get a reopen
	m_pFileHandle.Close();
	
	static const char* const Modes[] =
	{
		"Unk Mode",
		"Read only",
		"Write only",
		"Read and Write"
	};

	m_Filename = std::string(HLE_IPC_BuildFilename(m_Name.c_str(), 64));

	// The file must exist before we can open it
	// It should be created by ISFS_CreateFile, not here
	if(File::Exists(m_Filename))
	{
		INFO_LOG(WII_IPC_FILEIO, "FileIO: Open %s (%s)", m_Name.c_str(), Modes[_Mode]);
		switch(_Mode)
		{
		case ISFS_OPEN_READ:
			m_pFileHandle.Open(m_Filename, "rb");
			break;

		// "r+b" is technically wrong, but OPEN_WRITE should not truncate the file as "wb" does.
		case ISFS_OPEN_WRITE:
			m_pFileHandle.Open(m_Filename, "r+b");
			break;

		case ISFS_OPEN_RW:
			m_pFileHandle.Open(m_Filename, "r+b");
			break;

		default:
			PanicAlertT("FileIO: Unknown open mode : 0x%02x", _Mode);
			break;
		}
	}
	else
	{
		WARN_LOG(WII_IPC_FILEIO, "FileIO: Open (%s) failed - File doesn't exist %s", Modes[_Mode], m_Filename.c_str());
		ReturnValue = FS_FILE_NOT_EXIST;
	}

	if (m_pFileHandle != NULL)
	{
		m_FileLength = (u32)m_pFileHandle.GetSize();
		ReturnValue = m_DeviceID;
	}
	else if (ReturnValue == 0)
	{
		ERROR_LOG(WII_IPC_FILEIO, " FileIO failed open: %s (%s) - I/O Error", m_Filename.c_str(), Modes[_Mode]);
		ReturnValue = FS_RESULT_FATAL;
	}

	if (_CommandAddress)
		Memory::Write_U32(ReturnValue, _CommandAddress+4);
	m_Active = true;
	return true;
}

bool CWII_IPC_HLE_Device_FileIO::Seek(u32 _CommandAddress) 
{
	u32 ReturnValue		= FS_RESULT_FATAL;
	const s32 SeekPosition	= Memory::Read_U32(_CommandAddress + 0xC);
	const s32 Mode			= Memory::Read_U32(_CommandAddress + 0x10);  

	if (m_pFileHandle == NULL)
	{
		ERROR_LOG(WII_IPC_FILEIO, "FILEIO: Seek failed because of null file handle - %s", m_Name.c_str());
		ReturnValue = FS_FILE_NOT_EXIST;
	}
	else if (Mode >= 0 && Mode <= 2)
	{
		const u64 fileSize		= m_pFileHandle.GetSize();

		INFO_LOG(WII_IPC_FILEIO, "FileIO: Seek Pos: 0x%08x, Mode: %i (%s, Length=0x%08llx)", SeekPosition, Mode, m_Name.c_str(), fileSize);

		// Set seek mode
		int seek_mode[3] = {SEEK_SET, SEEK_CUR, SEEK_END};

		// POSIX allows seek past EOF, the Wii does not. 
		// TODO: Can we check this without tell'ing/seek'ing twice?
		const u64 curPos = m_pFileHandle.Tell();
		if (m_pFileHandle.Seek(SeekPosition, seek_mode[Mode]))
		{
			const u64 newPos = m_pFileHandle.Tell();
			if (newPos > fileSize) 
			{
				ERROR_LOG(WII_IPC_FILEIO, "FILEIO: Seek past EOF - %s", m_Name.c_str());
				m_pFileHandle.Seek(curPos, SEEK_SET);
			}
			else
				ReturnValue = (u32)newPos;
		}
		else
		{
			// Must clear the failbits, since subsequent seeks don't care about
			// past failure on Wii
			m_pFileHandle.Clear();
			ERROR_LOG(WII_IPC_FILEIO, "FILEIO: Seek failed - %s", m_Name.c_str());
		}
	}
	else
	{
		PanicAlert("CWII_IPC_HLE_Device_FileIO Unsupported seek mode %i", Mode);
	}

	Memory::Write_U32(ReturnValue, _CommandAddress + 0x4);

	return true;
}

bool CWII_IPC_HLE_Device_FileIO::Read(u32 _CommandAddress) 
{	
	u32 ReturnValue = FS_EACCESS;
	const u32 Address	= Memory::Read_U32(_CommandAddress + 0xC); // Read to this memory address
	const u32 Size	= Memory::Read_U32(_CommandAddress + 0x10);

	if (m_pFileHandle != NULL)
	{
		if (m_Mode == ISFS_OPEN_WRITE) 
		{
			WARN_LOG(WII_IPC_FILEIO, "FileIO: Attempted to read 0x%x bytes to 0x%08x on write-only file %s", Size, Address, m_Name.c_str());
		}
		else 
		{
			INFO_LOG(WII_IPC_FILEIO, "FileIO: Read 0x%x bytes to 0x%08x from %s", Size, Address, m_Name.c_str());

			ReturnValue = (u32)fread(Memory::GetPointer(Address), 1, Size, m_pFileHandle.GetHandle());		
			if (ReturnValue != Size && ferror(m_pFileHandle.GetHandle()))
				ReturnValue = FS_EACCESS;
		}
	}
	else
	{
		ERROR_LOG(WII_IPC_FILEIO, "FileIO: Failed to read from %s (Addr=0x%08x Size=0x%x) - file not open", m_Name.c_str(), Address, Size);	
	}

	Memory::Write_U32(ReturnValue, _CommandAddress + 0x4);
	return true;
}

bool CWII_IPC_HLE_Device_FileIO::Write(u32 _CommandAddress) 
{		
	u32 ReturnValue = FS_EACCESS;
	const u32 Address	= Memory::Read_U32(_CommandAddress + 0xC); // Write data from this memory address
	const u32 Size	= Memory::Read_U32(_CommandAddress + 0x10);

	INFO_LOG(WII_IPC_FILEIO, "FileIO: Write 0x%04x bytes from 0x%08x to %s", Size, Address, m_Name.c_str());

	if (m_pFileHandle)
	{
		if (m_Mode == ISFS_OPEN_READ) 
		{
			WARN_LOG(WII_IPC_FILEIO, "FileIO: Attempted to write 0x%x bytes from 0x%08x to read-only file %s", Size, Address, m_Name.c_str());
		}
		else
		{
			if (m_pFileHandle.WriteBytes(Memory::GetPointer(Address), Size))
				ReturnValue = Size;
		}
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

	//u32 BufferIn		= Memory::Read_U32(_CommandAddress + 0x10);
	//u32 BufferInSize	= Memory::Read_U32(_CommandAddress + 0x14);
	//u32 BufferOut		= Memory::Read_U32(_CommandAddress + 0x18);
	//u32 BufferOutSize	= Memory::Read_U32(_CommandAddress + 0x1C);

	switch(Parameter)
	{
	case ISFS_IOCTL_GETFILESTATS:
		{
			m_FileLength = (u32)m_pFileHandle.GetSize();
			const u32 Position = (u32)m_pFileHandle.Tell();

			const u32 BufferOut = Memory::Read_U32(_CommandAddress + 0x18);
			INFO_LOG(WII_IPC_FILEIO, "FileIO: ISFS_IOCTL_GETFILESTATS");
			INFO_LOG(WII_IPC_FILEIO, "  File: %s, Length: %i, Pos: %i", m_Name.c_str(), m_FileLength, Position);

			Memory::Write_U32(m_FileLength, BufferOut);
			Memory::Write_U32(Position, BufferOut+4);
		}
		break;

	default:
		{
			PanicAlert("CWII_IPC_HLE_Device_FileIO: Parameter %i", Parameter);
		}
		break;

	}

	// Return Value
	const u32 ReturnValue = 0;  // no error
	Memory::Write_U32(ReturnValue, _CommandAddress + 0x4);

	return true;
}

void CWII_IPC_HLE_Device_FileIO::DoState(PointerWrap &p)
{
	bool have_file_handle = m_pFileHandle;
	s32 seek = (have_file_handle) ? (s32)m_pFileHandle.Tell() : 0;

	p.Do(have_file_handle);
	p.Do(m_Mode);
	p.Do(seek);

	if (p.GetMode() == PointerWrap::MODE_READ)
	{
		if (have_file_handle)
		{
			// TODO: isn't it naive and error-prone to assume that the file hasn't changed since we created the savestate?
			Open(0, m_Mode);
			_dbg_assert_msg_(WII_IPC_HLE, m_pFileHandle, "bad filehandle");
		}
		else
			Close(0, true);
	}

	if (have_file_handle)
		m_pFileHandle.Seek(seek, SEEK_SET);
}
