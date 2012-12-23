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

// This is used by several of the FileIO and /dev/fs functions 
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

	// close the file handle if we get a reopen
	//m_pFileHandle.Close();
	
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
	if (File::Exists(m_Filename))
	{
		INFO_LOG(WII_IPC_FILEIO, "FileIO: Open %s (%s == %08X)", m_Name.c_str(), Modes[_Mode], _Mode);
		ReturnValue = m_DeviceID;
	}
	else
	{
		WARN_LOG(WII_IPC_FILEIO, "FileIO: Open (%s) failed - File doesn't exist %s", Modes[_Mode], m_Filename.c_str());
		ReturnValue = FS_FILE_NOT_EXIST;
	}

	if (_CommandAddress)
		Memory::Write_U32(ReturnValue, _CommandAddress+4);
	m_Active = true;
	return true;
}



bool CWII_IPC_HLE_Device_FileIO::OpenFile()
{
	switch (m_Mode)
	{
	case ISFS_OPEN_READ:
	{
		m_pFileHandle.Open(m_Filename, "rb");
		break;
	}
	case ISFS_OPEN_WRITE:
	{
		m_pFileHandle.Open(m_Filename, "r+b");
		break;
	}
	case ISFS_OPEN_RW:
	{
		m_pFileHandle.Open(m_Filename, "r+b");
		break;
	}
	default:
	{
		PanicAlertT("FileIO: Unknown open mode : 0x%02x", m_Mode);
		break;
	}
	}
	return m_pFileHandle.IsOpen();
}

void CWII_IPC_HLE_Device_FileIO::CloseFile()
{
	m_pFileHandle.Close();
}

bool CWII_IPC_HLE_Device_FileIO::Seek(u32 _CommandAddress) 
{
	u32 ReturnValue	= FS_RESULT_FATAL;
	const u32 SeekPosition = Memory::Read_U32(_CommandAddress + 0xC);
	const u32 Mode = Memory::Read_U32(_CommandAddress + 0x10);

	
	if (OpenFile())
	{
		ReturnValue = FS_RESULT_FATAL;

		const u64 fileSize = m_pFileHandle.GetSize();
		INFO_LOG(WII_IPC_FILEIO, "FileIO: Seek Pos: 0x%08x, Mode: %i (%s, Length=0x%08llx)", SeekPosition, Mode, m_Name.c_str(), fileSize);
		switch (Mode){
			case 0:
			{
				if (SeekPosition >=0 && SeekPosition <= fileSize)
				{
					m_SeekPos = SeekPosition;
					ReturnValue = m_SeekPos;
				}
				break;
			}
			case 1:
			{
				u32 wantedPos = SeekPosition+m_SeekPos;
				if (wantedPos >=0 && wantedPos <= fileSize)
				{
					m_SeekPos = wantedPos;
					ReturnValue = m_SeekPos;
				}
				break;
			}
			case 2:
			{
				u64 wantedPos = fileSize+m_SeekPos;
				if (wantedPos >=0 && wantedPos <= fileSize)
				{
					m_SeekPos = wantedPos;
					ReturnValue = m_SeekPos;
				}
				break;
			}
			default:
			{
				PanicAlert("CWII_IPC_HLE_Device_FileIO Unsupported seek mode %i", Mode);
				ReturnValue = FS_RESULT_FATAL;
				break;
			}
		}
		CloseFile();
	}
	else
	{
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


	if (OpenFile())
	{
		if (m_Mode == ISFS_OPEN_WRITE) 
		{
			WARN_LOG(WII_IPC_FILEIO, "FileIO: Attempted to read 0x%x bytes to 0x%08x on write-only file %s", Size, Address, m_Name.c_str());
		}
		else 
		{
			INFO_LOG(WII_IPC_FILEIO, "FileIO: Read 0x%x bytes to 0x%08x from %s", Size, Address, m_Name.c_str());
			m_pFileHandle.Seek(m_SeekPos, SEEK_SET);
			ReturnValue = (u32)fread(Memory::GetPointer(Address), 1, Size, m_pFileHandle.GetHandle());		
			if (ReturnValue != Size && ferror(m_pFileHandle.GetHandle()))
			{
				ReturnValue = FS_EACCESS;
			}
			else
			{
				m_SeekPos += Size;
			}
			
		}
		CloseFile();
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


	if (OpenFile())
	{
		if (m_Mode == ISFS_OPEN_READ) 
		{
			WARN_LOG(WII_IPC_FILEIO, "FileIO: Attempted to write 0x%x bytes from 0x%08x to read-only file %s", Size, Address, m_Name.c_str());
		}
		else
		{
			INFO_LOG(WII_IPC_FILEIO, "FileIO: Write 0x%04x bytes from 0x%08x to %s", Size, Address, m_Name.c_str());
			m_pFileHandle.Seek(m_SeekPos, SEEK_SET);
			if (m_pFileHandle.WriteBytes(Memory::GetPointer(Address), Size))
			{
				ReturnValue = Size;
				m_SeekPos += Size;
			}
		}
		CloseFile();
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
			if (OpenFile())
			{
				u32 m_FileLength = (u32)m_pFileHandle.GetSize();

				const u32 BufferOut = Memory::Read_U32(_CommandAddress + 0x18);
				INFO_LOG(WII_IPC_FILEIO, "FileIO: ISFS_IOCTL_GETFILESTATS");
				INFO_LOG(WII_IPC_FILEIO, "  File: %s, Length: %i, Pos: %i", m_Name.c_str(), m_FileLength, m_SeekPos);

				Memory::Write_U32(m_FileLength, BufferOut);
				Memory::Write_U32(m_SeekPos, BufferOut+4);
				CloseFile();
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

	bool have_file_handle = (m_pFileHandle != 0);
	s32 seek = (have_file_handle) ? (s32)m_pFileHandle.Tell() : 0;

	p.Do(have_file_handle);
	p.Do(m_Mode);
	p.Do(seek);
	p.Do(m_SeekPos);
	p.Do(m_Filename);

	if (p.GetMode() == PointerWrap::MODE_READ)
	{
		int mode = m_Mode;
		bool active = m_Active;
		if (have_file_handle)
		{
			Open(0, m_Mode);
			_dbg_assert_msg_(WII_IPC_HLE, m_pFileHandle, "bad filehandle");
		}
		else
			Close(0, true);
		m_Mode = mode;
		m_Active = active;
	}

	if (have_file_handle)
		m_pFileHandle.Seek(seek, SEEK_SET);
}
