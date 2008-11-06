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

#include "WII_IPC_HLE_Device_fs.h"

#include "StringUtil.h"
#include "FileSearch.h"
#include "FileUtil.h"

#include "../VolumeHandler.h"

extern std::string HLE_IPC_BuildFilename(const char* _pFilename, int _size);

#define FS_RESULT_OK		(0)
#define FS_FILE_EXIST		(-105)
#define FS_FILE_NOT_EXIST	(-106)
#define FS_RESULT_FATAL		(-128)

#define MAX_NAME			(12)


CWII_IPC_HLE_Device_fs::CWII_IPC_HLE_Device_fs(u32 _DeviceID, const std::string& _rDeviceName) 
	: IWII_IPC_HLE_Device(_DeviceID, _rDeviceName)
{}

CWII_IPC_HLE_Device_fs::~CWII_IPC_HLE_Device_fs()
{}

bool CWII_IPC_HLE_Device_fs::Open(u32 _CommandAddress, u32 _Mode)
{
	// clear tmp folder
	{
		std::string WiiTempFolder("Wii/tmp");
		bool Result = File::DeleteDirRecursively(WiiTempFolder.c_str());
		if (Result == false)
		{
			PanicAlert("Cant delete Wii Temp folder");
		}
		File::CreateDir(WiiTempFolder.c_str());
	}

	// create home directory
	{
		u32 TitleID = VolumeHandler::Read32(0);
		if (TitleID == 0)
			TitleID = 0xF00DBEEF;

		char* pTitleID = (char*)&TitleID;

		char Path[260+1];
		sprintf(Path, "Wii/title/00010000/%02x%02x%02x%02x/data/nocopy/", (u8)pTitleID[3], (u8)pTitleID[2], (u8)pTitleID[1], (u8)pTitleID[0]);
	
		CreateDirectoryStruct(Path);
	}

	Memory::Write_U32(GetDeviceID(), _CommandAddress+4);
	return true;
}

bool CWII_IPC_HLE_Device_fs::IOCtl(u32 _CommandAddress) 
{ 
	LOG(WII_IPC_FILEIO, "FS: IOCtl (Device=%s)", GetDeviceName().c_str());	

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
			// the wii uses this function to define the type (dir or file)
			std::string Filename(HLE_IPC_BuildFilename((const char*)Memory::GetPointer(CommandBuffer.InBuffer[0].m_Address), CommandBuffer.InBuffer[0].m_Size));

			LOG(WII_IPC_FILEIO, "FS: IOCTL_READ_DIR %s", Filename.c_str());

			// check if this is really a directory
			if (!File::IsDirectory(Filename.c_str()))
			{
				LOG(WII_IPC_FILEIO, "    Not a directory - return -6 (dunno if this is a correct return value)", Filename.c_str());
				ReturnValue = -6;				
				break;
			}

			// make a file search
			CFileSearch::XStringVector Directories;
			Directories.push_back(Filename);

			CFileSearch::XStringVector Extensions;
			Extensions.push_back("*.*");

			CFileSearch FileSearch(Extensions, Directories);

			// it is one
			if ((CommandBuffer.InBuffer.size() == 1) && (CommandBuffer.PayloadBuffer.size() == 1))
			{
				size_t numFile = FileSearch.GetFileNames().size();
				LOG(WII_IPC_FILEIO, "    Files in directory: %i", numFile);

				Memory::Write_U32((u32)numFile, CommandBuffer.PayloadBuffer[0].m_Address);
			}
			else
			{
				

				memset(Memory::GetPointer(CommandBuffer.PayloadBuffer[0].m_Address), 0, CommandBuffer.PayloadBuffer[0].m_Size);

				size_t numFile = FileSearch.GetFileNames().size();
				for (size_t i=0; i<numFile; i++)
				{
					char* pDest = (char*)Memory::GetPointer((u32)(CommandBuffer.PayloadBuffer[0].m_Address + i * MAX_NAME));

					std::string filename, ext;
					SplitPath(FileSearch.GetFileNames()[i], NULL, &filename, &ext);
					

					memcpy(pDest, (filename + ext).c_str(), MAX_NAME);
					pDest[MAX_NAME-1] = 0x00;
					LOG(WII_IPC_FILEIO, "    %s", pDest);
				}

				Memory::Write_U32((u32)numFile, CommandBuffer.PayloadBuffer[1].m_Address);
			}

			ReturnValue = 0;
		}
		break;

	case IOCTL_GETUSAGE:
		{
			// check buffer sizes
			_dbg_assert_(WII_IPC_FILEIO, CommandBuffer.PayloadBuffer.size() == 2);
			_dbg_assert_(WII_IPC_FILEIO, CommandBuffer.PayloadBuffer[0].m_Size == 4);
			_dbg_assert_(WII_IPC_FILEIO, CommandBuffer.PayloadBuffer[1].m_Size == 4);

			// this command sucks because it asks of the number of used 
			// fsBlocks and inodes
			// we answer nothing is used, but if a program uses it to check
			// how much memory has been used we are doomed...
			std::string Filename(HLE_IPC_BuildFilename((const char*)Memory::GetPointer(CommandBuffer.InBuffer[0].m_Address), CommandBuffer.InBuffer[0].m_Size));
			u32 fsBlock = 0;
			u32 iNodes = 0;

			LOG(WII_IPC_FILEIO, "FS: IOCTL_GETUSAGE %s", Filename.c_str());
			if (File::IsDirectory(Filename.c_str()))
			{
				// make a file search
				CFileSearch::XStringVector Directories;
				Directories.push_back(Filename);

				CFileSearch::XStringVector Extensions;
				Extensions.push_back("*.*");

				CFileSearch FileSearch(Extensions, Directories);
			
				u64 overAllSize = 0;
				for (size_t i=0; i<FileSearch.GetFileNames().size(); i++)
				{
					overAllSize += File::GetSize(FileSearch.GetFileNames()[i].c_str());
				}

				fsBlock = (u32)(overAllSize / (16 * 1024));  // one bock is 16kb
				iNodes = (u32)(FileSearch.GetFileNames().size());

				ReturnValue = 0;

				LOG(WII_IPC_FILEIO, "    fsBlock: %i, iNodes: %i", fsBlock, iNodes);
			}
			else
			{
				fsBlock = 0;
				iNodes = 0;
				ReturnValue = 0;
				LOG(WII_IPC_FILEIO, "    error: not executed on a valid directoy: %s", Filename.c_str());
			}
			
			Memory::Write_U32(fsBlock, CommandBuffer.PayloadBuffer[0].m_Address);
			Memory::Write_U32(iNodes, CommandBuffer.PayloadBuffer[1].m_Address);
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
	case CREATE_DIR:
		{
			_dbg_assert_(WII_IPC_FILEIO, _BufferOutSize == 0);
			u32 Addr = _BufferIn;

			u32 OwnerID = Memory::Read_U32(Addr); Addr += 4;
			u16 GroupID = Memory::Read_U16(Addr); Addr += 2;
			std::string DirName(HLE_IPC_BuildFilename((const char*)Memory::GetPointer(Addr), 64));
			Addr += 64;
			Addr += 9; // owner attribs, permission
			u8 Attribs = Memory::Read_U8(Addr);

			LOG(WII_IPC_FILEIO, "FS: CREATE_DIR %s", DirName.c_str());

			DirName += "\\";
			CreateDirectoryStruct(DirName);
			_dbg_assert_msg_(WII_IPC_FILEIO, File::IsDirectory(DirName.c_str()), "FS: CREATE_DIR %s failed", DirName.c_str());

			return FS_RESULT_OK;
		}
		break;

	case GET_ATTR:
		{		
			_dbg_assert_msg_(WII_IPC_FILEIO, _BufferOutSize == 76, "    GET_ATTR needs an 76 bytes large output buffer but it is %i bytes large", _BufferOutSize);

			// first clear the whole output buffer
			memset(Memory::GetPointer(_BufferOut), 0, _BufferOutSize);

			u32 OwnerID = 0;
			u16 GroupID = 0;
			std::string Filename = HLE_IPC_BuildFilename((const char*)Memory::GetPointer(_BufferIn), 64);
			u8 OwnerPerm = 0;
			u8 GroupPerm = 0;
			u8 OtherPerm = 0;
			u8 Attributes = 0;

			if (File::IsDirectory(Filename.c_str()))
			{
				LOG(WII_IPC_FILEIO, "FS: GET_ATTR Directory %s - ni", Filename.c_str());
			}
			else
			{
				if (File::Exists(Filename.c_str()))
				{
					LOG(WII_IPC_FILEIO, "FS: GET_ATTR %s - ni", Filename.c_str());
				}
				else
				{
					LOG(WII_IPC_FILEIO, "FS: GET_ATTR unknown %s", Filename.c_str());
					return FS_FILE_NOT_EXIST;
				}
			}

			// write answer to buffer
			if (_BufferOutSize == 76)
			{
				u32 Addr = _BufferOut;
				Memory::Write_U32(OwnerID, Addr);										Addr += 4;
				Memory::Write_U16(GroupID, Addr);										Addr += 2;
				memcpy(Memory::GetPointer(Addr), Filename.c_str(), Filename.size());	Addr += 64;
				Memory::Write_U8(OwnerPerm, Addr);										Addr += 1;
				Memory::Write_U8(GroupPerm, Addr);										Addr += 1;
				Memory::Write_U8(OtherPerm, Addr);										Addr += 1;
				Memory::Write_U8(Attributes, Addr);										Addr += 1;
			}

			return FS_RESULT_OK;
		}
		break;


	case DELETE_FILE:
		{
			_dbg_assert_(WII_IPC_FILEIO, _BufferOutSize == 0);
			int Offset = 0;

			std::string Filename = HLE_IPC_BuildFilename((const char*)Memory::GetPointer(_BufferIn+Offset), 64);
			Offset += 64;
			if (File::Delete(Filename.c_str()))
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
			_dbg_assert_(WII_IPC_FILEIO, _BufferOutSize == 0);
			int Offset = 0;

			std::string Filename = HLE_IPC_BuildFilename((const char*)Memory::GetPointer(_BufferIn+Offset), 64);
			Offset += 64;

			std::string FilenameRename = HLE_IPC_BuildFilename((const char*)Memory::GetPointer(_BufferIn+Offset), 64);
			Offset += 64;

			// try to make the basis directory
			CreateDirectoryStruct(Filename);

			// if there is already a filedelete it
			if (File::Exists(FilenameRename.c_str()))
			{
				File::Delete(FilenameRename.c_str());
			}

			// finally try to rename the file
			if (File::Rename(Filename.c_str(), FilenameRename.c_str()))
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
			_dbg_assert_(WII_IPC_FILEIO, _BufferOutSize == 0);

			u32 Addr = _BufferIn;
			u32 OwnerID = Memory::Read_U32(Addr); Addr += 4;
			u16 GroupID = Memory::Read_U16(Addr); Addr += 2;
			std::string Filename(HLE_IPC_BuildFilename((const char*)Memory::GetPointer(Addr), 64));
			Addr += 64;

			u8 OwnerPerm = Memory::Read_U8(Addr); Addr++;
			u8 GroupPerm = Memory::Read_U8(Addr); Addr++;
			u8 OtherPerm = Memory::Read_U8(Addr); Addr++;
			u8 Attributes = Memory::Read_U8(Addr); Addr++;

			LOG(WII_IPC_FILEIO, "FS: CreateFile %s", Filename.c_str());
			LOG(WII_IPC_FILEIO, "    OwnerID: 0x08%x", OwnerID);
			LOG(WII_IPC_FILEIO, "    GroupID: 0x04%x", GroupID);
			LOG(WII_IPC_FILEIO, "    OwnerPerm: 0x02%x", OwnerPerm);
			LOG(WII_IPC_FILEIO, "    GroupPerm: 0x02%x", GroupPerm);
			LOG(WII_IPC_FILEIO, "    OtherPerm: 0x02%x", OtherPerm);
			LOG(WII_IPC_FILEIO, "    Attributes: 0x02%x", Attributes);

			// check if the file allready exist
			if (File::Exists(Filename.c_str()))
			{
				LOG(WII_IPC_FILEIO, "    result = FS_RESULT_EXISTS", Filename.c_str());
				return FS_FILE_EXIST;
			}

			// create the file
			// F|RES: i think that we dont need this - CreateDirectoryStruct(Filename);
			bool Result = File::CreateEmptyFile(Filename.c_str());
			if (!Result)
			{
				PanicAlert("CWII_IPC_HLE_Device_fs: couldn't create new file");
				return FS_RESULT_FATAL;
			}

			LOG(WII_IPC_FILEIO, "    result = FS_RESULT_OK", Filename.c_str());
			return FS_RESULT_OK;
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
		// find next sub path
		{
			size_t nextPosition = _rFullPath.find('/', Position);
			if (nextPosition == std::string::npos)
				nextPosition = _rFullPath.find('\\', Position);
			Position = nextPosition;

			if (Position == std::string::npos)
				break;

			Position++;
		}

		// create next sub path
		std::string SubPath = _rFullPath.substr(0, Position);
		if (!SubPath.empty())
		{
			if (!File::IsDirectory(SubPath.c_str()))
			{
				File::CreateDir(SubPath.c_str());
				LOG(WII_IPC_FILEIO, "    CreateSubDir %s", SubPath.c_str());
			}
		}

		// just a safty check...
		PanicCounter--;
		if (PanicCounter <= 0)
		{
			PanicAlert("CreateDirectoryStruct creates way to much dirs...");
			break;
		}
	}
}
