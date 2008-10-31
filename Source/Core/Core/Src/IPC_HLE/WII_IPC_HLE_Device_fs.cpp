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
#include "StringUtil.h"
#include "FileSearch.h"

#include "WII_IPC_HLE_Device_fs.h"

#include <direct.h>




extern std::string HLE_IPC_BuildFilename(const char* _pFilename);

#define FS_RESULT_OK		(0)
#define FS_FILE_EXIST		(-105)
#define FS_RESULT_FATAL		(-128)

CWII_IPC_HLE_Device_fs::CWII_IPC_HLE_Device_fs(u32 _DeviceID, const std::string& _rDeviceName) 
	: IWII_IPC_HLE_Device(_DeviceID, _rDeviceName)
{}

CWII_IPC_HLE_Device_fs::~CWII_IPC_HLE_Device_fs()
{}

bool CWII_IPC_HLE_Device_fs::Open(u32 _CommandAddress, u32 _Mode)
{
	CFileSearch::XStringVector Directories;
	Directories.push_back("Wii/tmp");

	CFileSearch::XStringVector Extensions;
	Extensions.push_back("*.*");

	CFileSearch FileSearch(Extensions, Directories);
	const CFileSearch::XStringVector& rFilenames = FileSearch.GetFileNames();
	for (u32 i = 0; i < rFilenames.size(); i++)
	{
		if (rFilenames[i].c_str()[0] != '.')
			remove(rFilenames[i].c_str());
	}

	Memory::Write_U32(GetDeviceID(), _CommandAddress+4);
	return true;
}

bool CWII_IPC_HLE_Device_fs::IOCtl(u32 _CommandAddress) 
{ 
	LOG(WII_IPC_FILEIO, "FileIO: IOCtl (Device=%s)", GetDeviceName().c_str());	

	u32 Parameter =  Memory::Read_U32(_CommandAddress + 0xC);
	u32 BufferIn =  Memory::Read_U32(_CommandAddress + 0x10);
	u32 BufferInSize =  Memory::Read_U32(_CommandAddress + 0x14);
	u32 BufferOut = Memory::Read_U32(_CommandAddress + 0x18);
	u32 BufferOutSize = Memory::Read_U32(_CommandAddress + 0x1C);

	u32 ReturnValue = ExecuteCommand(Parameter, BufferIn, BufferInSize, BufferOut, BufferOutSize);	
	Memory::Write_U32(ReturnValue, _CommandAddress+4);

	return true; 
}

bool CWII_IPC_HLE_Device_fs::IOCtlV(u32 _CommandAddress) 
{ 
	u32 ReturnValue = 0;

	SIOCtlVBuffer CommandBuffer(_CommandAddress);
	
	switch(CommandBuffer.Parameter)
	{
	case IOCTL_READ_DIR:
		{
			std::string Filename(HLE_IPC_BuildFilename((const char*)Memory::GetPointer(CommandBuffer.InBuffer[0].m_Address)));

			if ((CommandBuffer.InBuffer.size() == 1) && (CommandBuffer.PayloadBuffer.size() == 1))
			{
				LOG(WII_IPC_FILEIO, "FS: IOCTL_READ_DIR %s (dunno what i should return, number of FiLES?)", Filename.c_str());

				Memory::Write_U32(0, CommandBuffer.PayloadBuffer[0].m_Address);
			}
			else
			{
				PanicAlert("IOCTL_READ_DIR with a lot of parameters");
			}
		}
		break;

	case IOCTL_GETUSAGE:
		{
			// this command sucks because it asks of the number of used 
			// fsBlocks and inodes
			// we answer nothing is used, but if a program uses it to check
			// how much memory has been used we are doomed...
			std::string Filename(HLE_IPC_BuildFilename((const char*)Memory::GetPointer(CommandBuffer.InBuffer[0].m_Address)));

			LOG(WII_IPC_FILEIO, "FS: IOCTL_GETUSAGE %s", Filename.c_str());

			Memory::Write_U32(0, CommandBuffer.PayloadBuffer[0].m_Address);
			Memory::Write_U32(0, CommandBuffer.PayloadBuffer[1].m_Address);
		}
		break;


	default:
		PanicAlert("CWII_IPC_HLE_Device_fs::IOCtlV: %i", CommandBuffer.Parameter);
		break;
	}

	Memory::Write_U32(ReturnValue, _CommandAddress+4);
	
	return true; 
}


s32 CWII_IPC_HLE_Device_fs::ExecuteCommand(u32 _Parameter, u32 _BufferIn, u32 _BufferInSize, u32 _BufferOut, u32 _BufferOutSize)
{
	switch(_Parameter)
	{
	case DELETE_FILE:
		{
			int Offset = 0;

			std::string Filename = HLE_IPC_BuildFilename((const char*)Memory::GetPointer(_BufferIn+Offset));
			Offset += 64;
			if (remove(Filename.c_str()) == 0)
			{
				LOG(WII_IPC_FILEIO, "FS: DeleteFile %s", Filename.c_str());
			}
			else
			{
				LOG(WII_IPC_FILEIO, "FS: DeleteFile %s - failed!!!", Filename.c_str());
			}

			return FS_RESULT_OK;
		}
		break;

	case RENAME_FILE:
		{
			int Offset = 0;

			std::string Filename = HLE_IPC_BuildFilename((const char*)Memory::GetPointer(_BufferIn+Offset));
			Offset += 64;

			std::string FilenameRename = HLE_IPC_BuildFilename((const char*)Memory::GetPointer(_BufferIn+Offset));
			Offset += 64;

			if (rename(Filename.c_str(), FilenameRename.c_str()) == 0)
			{
				LOG(WII_IPC_FILEIO, "FS: Rename %s to %s", Filename.c_str(), FilenameRename.c_str());
			}
			else
			{
				LOG(WII_IPC_FILEIO, "FS: Rename %s to %s - failed", Filename.c_str(), FilenameRename.c_str());
				PanicAlert("CWII_IPC_HLE_Device_fs: rename %s to %s failed", Filename.c_str(), FilenameRename.c_str());
			}

			return FS_RESULT_OK;
		}
		break;

	case CREATE_FILE:
		{
			u32 Addr = _BufferIn;
			u32 OwnerID = Memory::Read_U32(Addr); Addr += 4;
			u16 GroupID = Memory::Read_U16(Addr); Addr += 2;
			std::string Filename(HLE_IPC_BuildFilename((const char*)Memory::GetPointer(Addr)));
			Addr += 64;
			Addr += 9; // unk memory;
			u8 Attribs = Memory::Read_U8(Addr);

			LOG(WII_IPC_FILEIO, "FS: CreateFile %s (attrib: 0x%02x)", Filename.c_str(), Attribs);

			FILE* pFileHandle = fopen(Filename.c_str(), "r+b");
			if (pFileHandle != NULL)
			{
				fclose(pFileHandle);
				pFileHandle = NULL;
				LOG(WII_IPC_FILEIO, "    result = FS_RESULT_EXISTS", Filename.c_str(), Attribs);

				return FS_FILE_EXIST;
			}
			else
			{	
				CreateDirectoryStruct(Filename);

				FILE* pFileHandle = fopen(Filename.c_str(), "w+b");
				if (!pFileHandle)
				{
					PanicAlert("CWII_IPC_HLE_Device_fs: couldn't create new file");
					return FS_RESULT_FATAL;
				}
				fclose(pFileHandle);
				LOG(WII_IPC_FILEIO, "    result = FS_RESULT_OK", Filename.c_str(), Attribs);

				return FS_RESULT_OK;
			}
		}
		break;

	default:
		PanicAlert("CWII_IPC_HLE_Device_fs::IOCtl: ni  0x%x", _Parameter);
		break;
	}

	return FS_RESULT_FATAL;
}

void CWII_IPC_HLE_Device_fs::CreateDirectoryStruct(const std::string& _rFullPath)
{
	int PanicCounter = 10;

	size_t Position = 0;
	while(true)
	{
		Position = _rFullPath.find('/', Position);
		if (Position == std::string::npos)
			break;

		Position++;

		std::string SubPath = _rFullPath.substr(0, Position);
		if (!SubPath.empty())
			_mkdir(SubPath.c_str());

		LOG(WII_IPC_FILEIO, "    CreateSubDir %s", SubPath.c_str());

		PanicCounter--;
		if (PanicCounter <= 0)
		{
			PanicAlert("CreateDirectoryStruct creates way to much dirs...");
			break;
		}
	}
}