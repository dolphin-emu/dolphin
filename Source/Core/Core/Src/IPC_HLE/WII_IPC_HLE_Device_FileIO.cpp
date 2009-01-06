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

#include "Common.h"
#include "FileUtil.h"

#include "WII_IPC_HLE_Device_FileIO.h"


// ===================================================
/* This is used by several of the FileIO and /dev/fs/ functions */
// ----------------
std::string HLE_IPC_BuildFilename(const char* _pFilename, int _size)
{
	char Buffer[128];
	memcpy(Buffer, _pFilename, _size);

	std::string Filename(FULL_WII_ROOT_DIR);
	if (Buffer[1] == '0')
		Filename += std::string("/title");     // this looks and feel like an hack...

	Filename += Buffer;

	return Filename;
}

CWII_IPC_HLE_Device_FileIO::CWII_IPC_HLE_Device_FileIO(u32 _DeviceID, const std::string& _rDeviceName ) 
    : IWII_IPC_HLE_Device(_DeviceID, _rDeviceName)
    , m_pFileHandle(NULL)
    , m_FileLength(0)
{
}

CWII_IPC_HLE_Device_FileIO::~CWII_IPC_HLE_Device_FileIO()
{
    if (m_pFileHandle != NULL)
    {
        fclose(m_pFileHandle);
        m_pFileHandle = NULL;
    }
}

bool 
CWII_IPC_HLE_Device_FileIO::Close(u32 _CommandAddress)
{
	u32 DeviceID = Memory::Read_U32(_CommandAddress + 8);
	LOG(WII_IPC_FILEIO, "FileIO: Close %s (DeviceID=%08x)", GetDeviceName().c_str(), DeviceID);	

	// Close always return 0 for success
	Memory::Write_U32(0, _CommandAddress + 4);
	
	return true;
}

bool 
CWII_IPC_HLE_Device_FileIO::Open(u32 _CommandAddress, u32 _Mode)  
{ 
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

	LOG(WII_IPC_FILEIO, "FileIO: Open %s (%s)", GetDeviceName().c_str(), Modes[_Mode]);	

	m_Filename = std::string(HLE_IPC_BuildFilename(GetDeviceName().c_str(), 64));

	if (File::Exists(m_Filename.c_str()))
	{
		switch(_Mode)
		{
		// Do "r+b" for all writing to avoid truncating the file
		case 0x01:	m_pFileHandle = fopen(m_Filename.c_str(), "rb"); break;
		case 0x02:	//m_pFileHandle = fopen(m_Filename.c_str(), "wb"); break;
		case 0x03:	m_pFileHandle = fopen(m_Filename.c_str(), "r+b"); break;
		default: PanicAlert("CWII_IPC_HLE_Device_FileIO: unknown open mode"); break;
		}
	}

	u32 ReturnValue = 0;
    if (m_pFileHandle != NULL)
    {
        m_FileLength = File::GetSize(m_Filename.c_str());
        ReturnValue = GetDeviceID();
    }
    else
    {
        LOG(WII_IPC_FILEIO, "    failed - File doesn't exist");
        ReturnValue = -106;
    }

    Memory::Write_U32(ReturnValue, _CommandAddress+4);
    return true;
}

bool 
CWII_IPC_HLE_Device_FileIO::Seek(u32 _CommandAddress) 
{
	u32 ReturnValue = 0;
	u32 SeekPosition = Memory::Read_U32(_CommandAddress + 0xC);
	u32 Mode = Memory::Read_U32(_CommandAddress +0x10);  

	/* Zelda - TP Fix: It doesn't make much sense but it works in Zelda - TP and
	   it's probably better than trying to read outside the file (it seeks to 0x6000 instead
	   of the correct 0x2000 for the second half of the file). Could this be right
	   or has it misunderstood the filesize somehow? My guess is that the filesize is
	   hardcoded in to the game, and it never checks the filesize, so I don't know.
	   Maybe it's wrong to return the seekposition when it's zero? Perhaps it wants
	   the filesize then? - No, that didn't work either, it seeks to 0x6000 even if I return
	   0x4000 from the first seek. */
	if (SeekPosition > m_FileLength && Mode == 0)
		SeekPosition = SeekPosition % m_FileLength;

	LOG(WII_IPC_FILEIO, "FileIO: Seek Pos: 0x%04x, Mode: %i (Device=%s)", SeekPosition, Mode, GetDeviceName().c_str());	

	// Set seek mode
	int seek_mode[3] = {SEEK_SET, SEEK_CUR, SEEK_END};

	if (Mode >= 0 && Mode <= 2) {
        if (fseek(m_pFileHandle, SeekPosition, seek_mode[Mode]) == 0) {
			// Seek always return the seek position for success
			// Not sure if it's right in all modes though.
		    ReturnValue = SeekPosition;
        } else {
            LOG(WII_IPC_FILEIO, "FILEIO: Seek failed");
        }
	} else {
		PanicAlert("CWII_IPC_HLE_Device_FileIO unsupported seek mode %i", Mode);
	}

    Memory::Write_U32(ReturnValue, _CommandAddress + 0x4);

    return true;
}

bool 
CWII_IPC_HLE_Device_FileIO::Read(u32 _CommandAddress) 
{    
    u32 ReturnValue = 0;
    u32 Address = Memory::Read_U32(_CommandAddress +0xC);
    u32 Size = Memory::Read_U32(_CommandAddress +0x10);

    if (m_pFileHandle != NULL)
    {
        size_t readItems = fread(Memory::GetPointer(Address), 1, Size, m_pFileHandle);
        ReturnValue = (u32)readItems;
        LOG(WII_IPC_FILEIO, "FileIO reads from %s (Addr=0x%08x Size=0x%x)", GetDeviceName().c_str(), Address, Size);	
    }
    else
    {
        LOG(WII_IPC_FILEIO, "FileIO failed to read from %s (Addr=0x%08x Size=0x%x) - file not open", GetDeviceName().c_str(), Address, Size);	
    }

    Memory::Write_U32(ReturnValue, _CommandAddress + 0x4);

    return true;
}

bool 
CWII_IPC_HLE_Device_FileIO::Write(u32 _CommandAddress) 
{        
	u32 ReturnValue = 0;
	u32 Address = Memory::Read_U32(_CommandAddress +0xC);
	u32 Size = Memory::Read_U32(_CommandAddress +0x10);

	LOG(WII_IPC_FILEIO, "FileIO: Write Addr: 0x%08x Size: %i (Device=%s)", Address, Size, GetDeviceName().c_str());	

	if (m_pFileHandle)
	{
		fwrite(Memory::GetPointer(Address), Size, 1, m_pFileHandle);

		// Write always return the written bytes for success
		ReturnValue = Size;
	}

    Memory::Write_U32(ReturnValue, _CommandAddress + 0x4);

    return true;
}

bool 
CWII_IPC_HLE_Device_FileIO::IOCtl(u32 _CommandAddress) 
{
    LOG(WII_IPC_FILEIO, "FileIO: IOCtl (Device=%s)", GetDeviceName().c_str());	
    DumpCommands(_CommandAddress);

    u32 Parameter =  Memory::Read_U32(_CommandAddress + 0xC);

    //    u32 BufferIn =  Memory::Read_U32(_CommandAddress + 0x10);
    //    u32 BufferInSize =  Memory::Read_U32(_CommandAddress + 0x14);
    //    u32 BufferOut = Memory::Read_U32(_CommandAddress + 0x18);
    //    u32 BufferOutSize = Memory::Read_U32(_CommandAddress + 0x1C);

    switch(Parameter)
    {
    case ISFS_IOCTL_GETFILESTATS:
        {
            u32 Position = (u32)ftell(m_pFileHandle);
            
            u32 BufferOut = Memory::Read_U32(_CommandAddress + 0x18);
            LOG(WII_IPC_FILEIO, "FileIO: ISFS_IOCTL_GETFILESTATS");
            LOG(WII_IPC_FILEIO, "    Length: %i   Seek: %i", m_FileLength, Position);

            Memory::Write_U32((u32)m_FileLength, BufferOut);
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

bool
CWII_IPC_HLE_Device_FileIO::ReturnFileHandle()
{
	if(m_pFileHandle == NULL)
		return false;
	else
		return true;
}
