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
#include <algorithm>
#include <fstream>

typedef std::pair<char, std::string> replace_t;
typedef std::vector<replace_t> replace_v;
static replace_v replacements;

static void CreateReplacementFile(std::string &filename)
{
	std::ofstream replace(filename.c_str());
	replace <<"\" __22__\n";
	replace << "* __2a__\n";
	//replace << "/ __2f__\n";
	replace << ": __3a__\n";
	replace << "< __3c__\n";
	replace << "> __3e__\n";
	replace << "? __3f__\n";
	//replace <<"\\ __5c__\n";
	replace << "| __7c__\n";
}

static void ReadReplacements()
{
	const std::string replace_fname = "/sys/replace";
	std::string filename(File::GetUserPath(D_WIIROOT_IDX));
	filename += replace_fname;

	if (!File::Exists(filename.c_str()))
		CreateReplacementFile(filename);

	std::ifstream f(filename.c_str());
	char letter;
	std::string replacement;

	while (f >> letter >> replacement && replacement.size())
		replacements.push_back(std::make_pair(letter, replacement));
}

// This is used by several of the FileIO and /dev/fs/ functions 
std::string HLE_IPC_BuildFilename(const char* _pFilename, int _size)
{
	std::string path_full = std::string(File::GetUserPath(D_WIIROOT_IDX));
	std::string path_wii(_pFilename);

	if (path_wii[1] == '0')
		path_full += std::string("/title"); // this looks and feel like a hack...

	// Replaces chars that FAT32 can't support with strings defined in /sys/replace
	for (replace_v::const_iterator i = replacements.begin(); i != replacements.end(); ++i)
	{
		for (size_t j = 0; (j = path_wii.find(i->first, j)) != path_wii.npos; ++j)
			path_wii.replace(j, 1, i->second);
	}

	path_full += path_wii;

	return path_full;
}

CWII_IPC_HLE_Device_FileIO::CWII_IPC_HLE_Device_FileIO(u32 _DeviceID, const std::string& _rDeviceName) 
    : IWII_IPC_HLE_Device(_DeviceID, _rDeviceName, false)	// not a real hardware
    , m_pFileHandle(NULL)
    , m_FileLength(0)
	, m_Mode(0)
	, m_Seek(0)
{
	ReadReplacements();
}

CWII_IPC_HLE_Device_FileIO::~CWII_IPC_HLE_Device_FileIO()
{
}

bool CWII_IPC_HLE_Device_FileIO::Close(u32 _CommandAddress, bool _bForce)
{
	INFO_LOG(WII_IPC_FILEIO, "FileIO: Close %s (DeviceID=%08x)", m_Name.c_str(), m_DeviceID);	

	if (m_pFileHandle != NULL)
	{
		fclose(m_pFileHandle);
		m_pFileHandle = NULL;
	}

	m_FileLength = 0;
	m_Mode = 0;
	m_Seek = 0;

	// Close always return 0 for success
	if (!_bForce)
		Memory::Write_U32(0, _CommandAddress + 4);
	m_Active = false;
	return true;
}

bool CWII_IPC_HLE_Device_FileIO::Open(u32 _CommandAddress, u32 _Mode)  
{ 
	m_Mode = _Mode;
	u32 ReturnValue = 0;

	// close the file handle if we get a reopen
	if (m_pFileHandle != NULL)
	{
		fclose(m_pFileHandle);
		m_pFileHandle = NULL;
	}
	
	const char Modes[][128] =
	{
		{ "Unk Mode" },
		{ "Read only" },
		{ "Write only" },
		{ "Read and Write" }
	};

	m_Filename = std::string(HLE_IPC_BuildFilename(m_Name.c_str(), 64));

	// The file must exist before we can open it
	// It should be created by ISFS_CreateFile, not here
	if(File::Exists(m_Filename.c_str()))
	{
		INFO_LOG(WII_IPC_FILEIO, "FileIO: Open %s (%s)", m_Name.c_str(), Modes[_Mode]);
		switch(_Mode)
		{
		case ISFS_OPEN_READ:	m_pFileHandle = fopen(m_Filename.c_str(), "rb"); break;
		case ISFS_OPEN_WRITE:	m_pFileHandle = fopen(m_Filename.c_str(), "r+b"); break;
			// MK Wii gets here corrupting its saves (truncating rksys.dat), however using rb+ mode works fine
			// TODO : figure it properly...
		case ISFS_OPEN_RW:		m_pFileHandle = fopen(m_Filename.c_str(), "r+b"); break;
		default: PanicAlert("FileIO: Unknown open mode : 0x%02x", _Mode); break;
		}
	}
	else
	{
		WARN_LOG(WII_IPC_FILEIO, "FileIO: Open failed - File doesn't exist %s", m_Filename.c_str());
		ReturnValue = FS_FILE_NOT_EXIST;
	}

	if (m_pFileHandle != NULL)
	{
		m_FileLength = (u32)File::GetSize(m_pFileHandle);
		ReturnValue = m_DeviceID;
	}
	else if (ReturnValue == 0)
	{
		ERROR_LOG(WII_IPC_FILEIO, " FileIO failed open: %s (%s) - I/O Error", m_Filename.c_str(), Modes[_Mode]);
		ReturnValue = FS_INVALID_ARGUMENT;
	}

	if (_CommandAddress)
		Memory::Write_U32(ReturnValue, _CommandAddress+4);
	m_Active = true;
	return true;
}

bool CWII_IPC_HLE_Device_FileIO::Seek(u32 _CommandAddress) 
{
	u32 ReturnValue		= FS_INVALID_ARGUMENT;
	s32 SeekPosition	= Memory::Read_U32(_CommandAddress + 0xC);
	s32 Mode			= Memory::Read_U32(_CommandAddress + 0x10);  

	INFO_LOG(WII_IPC_FILEIO, "FileIO: Seek Pos: 0x%08x, Mode: %i (%s, Length=0x%08x)", SeekPosition, Mode, m_Name.c_str(), File::GetSize(m_pFileHandle));

	/* TODO: Check if the new changes and the removed hack
	         "magically" fixes Zelda - Twilight Princess as well */

	// Set seek mode
	int seek_mode[3] = {SEEK_SET, SEEK_CUR, SEEK_END};

	if (Mode >= 0 && Mode <= 2)
	{
        if (fseeko(m_pFileHandle, SeekPosition, seek_mode[Mode]) == 0)
		{
		    ReturnValue = (u32)ftello(m_pFileHandle);
        }
		else
		{
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
    u32 ReturnValue = 0;
    u32 Address	= Memory::Read_U32(_CommandAddress + 0xC); // Read to this memory address
    u32 Size	= Memory::Read_U32(_CommandAddress + 0x10);

    if (m_pFileHandle != NULL)
    {
		INFO_LOG(WII_IPC_FILEIO, "FileIO: Read 0x%x bytes to 0x%08x from %s", Size, Address, m_Name.c_str());
        ReturnValue = (u32)fread(Memory::GetPointer(Address), 1, Size, m_pFileHandle);		
        if ((ReturnValue != Size) && ferror(m_pFileHandle)) ReturnValue = FS_EACCESS;
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
	u32 ReturnValue = 0;
	u32 Address	= Memory::Read_U32(_CommandAddress + 0xC); // Write data from this memory address
	u32 Size	= Memory::Read_U32(_CommandAddress + 0x10);

	INFO_LOG(WII_IPC_FILEIO, "FileIO: Write 0x%04x bytes from 0x%08x to %s", Size, Address, m_Name.c_str());

	if (m_pFileHandle)
	{
		size_t Result = fwrite(Memory::GetPointer(Address), Size, 1, m_pFileHandle);
#if MAX_LOGLEVEL >= DEBUG_LEVEL
		_dbg_assert_msg_(WII_IPC_FILEIO, Result == 1, "fwrite failed");   
#else
		(void)Result;
#endif
	        ReturnValue = (Result == 1) ? Size : FS_EACCESS;
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
    u32 Parameter =  Memory::Read_U32(_CommandAddress + 0xC);

    //u32 BufferIn		= Memory::Read_U32(_CommandAddress + 0x10);
    //u32 BufferInSize	= Memory::Read_U32(_CommandAddress + 0x14);
    //u32 BufferOut		= Memory::Read_U32(_CommandAddress + 0x18);
    //u32 BufferOutSize	= Memory::Read_U32(_CommandAddress + 0x1C);

    switch(Parameter)
    {
    case ISFS_IOCTL_GETFILESTATS:
        {
	    m_FileLength = File::GetSize(m_pFileHandle);
            u32 Position = (u32)ftello(m_pFileHandle);

            u32 BufferOut = Memory::Read_U32(_CommandAddress + 0x18);
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
    u32 ReturnValue = 0;  // no error
    Memory::Write_U32(ReturnValue, _CommandAddress + 0x4);

    return true;
}

void CWII_IPC_HLE_Device_FileIO::DoState(PointerWrap &p)
{
	if (p.GetMode() == PointerWrap::MODE_WRITE)
	{
		m_Seek = (m_pFileHandle) ? (s32)ftello(m_pFileHandle) : 0;
	}

	p.Do(m_Mode);
	p.Do(m_Seek);

	if (p.GetMode() == PointerWrap::MODE_READ)
	{
		if (m_Mode)
		{
			Open(0, m_Mode);
			if (m_pFileHandle)
				fseeko(m_pFileHandle, m_Seek, SEEK_SET);
		}
	}
}
