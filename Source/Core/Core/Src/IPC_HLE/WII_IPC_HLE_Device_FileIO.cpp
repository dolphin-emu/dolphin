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

#include "WII_IPC_HLE_Device_FileIO.h"


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
	Memory::Write_U32(0, _CommandAddress+4);
	return true;
}

std::string HLE_IPC_BuildFilename(const char* _pFilename)
{
	std::string Filename("WII");
	if (_pFilename[1] == '0')
		Filename += std::string("/title");     // this looks and feel like an hack...
	Filename += std::string (_pFilename);

	return Filename;
}

bool 
CWII_IPC_HLE_Device_FileIO::Open(u32 _CommandAddress, u32 _Mode)  
{ 
	u32 ReturnValue = 0;

	LOG(WII_IPC_FILEIO, "FileIO: Open (Device=%s)", GetDeviceName().c_str());	

    m_Filename = (HLE_IPC_BuildFilename(GetDeviceName().c_str()));

	switch(_Mode)
	{
	case 0x01:	m_pFileHandle = fopen(m_Filename.c_str(), "rb"); break;
	case 0x02:	m_pFileHandle = fopen(m_Filename.c_str(), "wb"); break;
	case 0x03:	m_pFileHandle = fopen(m_Filename.c_str(), "r+b"); break;
	default: PanicAlert("CWII_IPC_HLE_Device_FileIO: unknown open mode"); break;
	}

    if (m_pFileHandle != NULL)
    {
        fseek(m_pFileHandle, 0, SEEK_END);
        m_FileLength = (u32)ftell(m_pFileHandle);
        rewind(m_pFileHandle);

		ReturnValue = GetDeviceID();
    }
    else
    {
		ReturnValue = -106;
    }

    Memory::Write_U32(ReturnValue, _CommandAddress+4);
    return true; 
}

bool 
CWII_IPC_HLE_Device_FileIO::Seek(u32 _CommandAddress) 
{
	u32 ReturnValue = 0;
	u32 SeekPosition = Memory::Read_U32(_CommandAddress +0xC);
	u32 Mode = Memory::Read_U32(_CommandAddress +0x10);  

	LOG(WII_IPC_FILEIO, "FileIO: Seek Pos: %i, Mode: %i(Device=%s)", SeekPosition, Mode, GetDeviceName().c_str());	

	switch(Mode)
	{
	case 0:
		fseek(m_pFileHandle, SeekPosition, SEEK_SET);
		ReturnValue = 0;
		break;

	case 1: // cur
	case 2: // end
	default:
		PanicAlert("CWII_IPC_HLE_Device_FileIO unsupported seek mode");
		break;
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
        fread(Memory::GetPointer(Address), Size, 1, m_pFileHandle);
        ReturnValue = Size;
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
			u32 Position = ftell(m_pFileHandle);

            u32 BufferOut = Memory::Read_U32(_CommandAddress + 0x18);
            LOG(WII_IPC_FILEIO, "FileIO: ISFS_IOCTL_GETFILESTATS");
            LOG(WII_IPC_FILEIO, "Length: %i   Seek: %i", m_FileLength, Position);

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
