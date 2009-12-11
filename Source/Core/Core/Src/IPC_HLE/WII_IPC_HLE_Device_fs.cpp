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

#include "WII_IPC_HLE_Device_fs.h"

#include "StringUtil.h"
#include "FileSearch.h"
#include "FileUtil.h"

#include "../VolumeHandler.h"

extern std::string HLE_IPC_BuildFilename(const char* _pFilename, int _size);

#define MAX_NAME				(12)


CWII_IPC_HLE_Device_fs::CWII_IPC_HLE_Device_fs(u32 _DeviceID, const std::string& _rDeviceName) 
	: IWII_IPC_HLE_Device(_DeviceID, _rDeviceName)
{}

CWII_IPC_HLE_Device_fs::~CWII_IPC_HLE_Device_fs()
{}

bool CWII_IPC_HLE_Device_fs::Open(u32 _CommandAddress, u32 _Mode)
{
	// clear tmp folder
	{
	    //std::string WiiTempFolder = File::GetUserDirectory() + FULL_WII_USER_DIR + std::string("tmp");
	    std::string WiiTempFolder = FULL_WII_USER_DIR + std::string("tmp");
	    File::DeleteDirRecursively(WiiTempFolder.c_str());
	    File::CreateDir(WiiTempFolder.c_str());
	}

	// create home directory
    if (VolumeHandler::IsValid())
	{
		char Path[260+1];
		u32 TitleID, GameID;
		VolumeHandler::RAWReadToPtr((u8*)&TitleID, 0x0F8001DC, 4);
		
		TitleID = Common::swap32(TitleID);
		GameID = VolumeHandler::Read32(0);

        _dbg_assert_(WII_IPC_FILEIO, GameID != 0);
		if (GameID == 0) GameID = 0xF00DBEEF;
		if (TitleID == 0) TitleID = 0x00010000;

		sprintf(Path, FULL_WII_USER_DIR "title/%08x/%08x/data/nocopy/", TitleID, GameID);

		File::CreateFullPath(Path);
	}

	Memory::Write_U32(GetDeviceID(), _CommandAddress+4);
	m_Active = true;
	return true;
}

bool CWII_IPC_HLE_Device_fs::Close(u32 _CommandAddress, bool _bForce)
{
	INFO_LOG(WII_IPC_FILEIO, "Close");
	if (!_bForce)
		Memory::Write_U32(0, _CommandAddress + 4);
	m_Active = false;
	return true;
}

// Get total filesize of contents of a directory (recursive)
// Only used for ES_GetUsage atm, could be useful elsewhere?
static u64 ComputeTotalFileSize(const File::FSTEntry& parentEntry)
{
	u64 sizeOfFiles = 0;
	const std::vector<File::FSTEntry>& children = parentEntry.children;
	for (std::vector<File::FSTEntry>::const_iterator it = children.begin(); it != children.end(); ++it)
	{
		const File::FSTEntry& entry = *it;
		if (entry.isDirectory)
			sizeOfFiles += ComputeTotalFileSize(entry);
		else
			sizeOfFiles += entry.size;
	}
	return sizeOfFiles;
}

bool CWII_IPC_HLE_Device_fs::IOCtlV(u32 _CommandAddress) 
{ 
	u32 ReturnValue = FS_RESULT_OK;
	SIOCtlVBuffer CommandBuffer(_CommandAddress);
	
	// Prepare the out buffer(s) with zeros as a safety precaution
	// to avoid returning bad values
	for(u32 i = 0; i < CommandBuffer.NumberPayloadBuffer; i++)
	{
		Memory::Memset(CommandBuffer.PayloadBuffer[i].m_Address, 0,
			CommandBuffer.PayloadBuffer[i].m_Size);
	}

	switch(CommandBuffer.Parameter)
	{
	case IOCTLV_READ_DIR:
		{
			// the wii uses this function to define the type (dir or file)
			std::string Filename(HLE_IPC_BuildFilename((const char*)Memory::GetPointer(
				CommandBuffer.InBuffer[0].m_Address), CommandBuffer.InBuffer[0].m_Size));

			INFO_LOG(WII_IPC_FILEIO, "FS: IOCTL_READ_DIR %s", Filename.c_str());

			if (!File::Exists(Filename.c_str()))
			{
				WARN_LOG(WII_IPC_FILEIO, "FS: Search not found: %s", Filename.c_str());
				ReturnValue = FS_DIRFILE_NOT_FOUND;
				break;
			}

			// AyuanX: what if we return "found one successfully" if it is a file?
			else if (!File::IsDirectory(Filename.c_str()))
			{
				// It's not a directory, so error.
				// Games don't usually seem to care WHICH error they get, as long as it's <0
				WARN_LOG(WII_IPC_FILEIO, "\tNot a directory - return FS_INVALID_ARGUMENT");
				ReturnValue = FS_INVALID_ARGUMENT;
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
				INFO_LOG(WII_IPC_FILEIO, "    %i Files found", numFile);

				Memory::Write_U32((u32)numFile, CommandBuffer.PayloadBuffer[0].m_Address);
			}
			else
			{
				u32 MaxEntries = Memory::Read_U32(CommandBuffer.InBuffer[0].m_Address);

				memset(Memory::GetPointer(CommandBuffer.PayloadBuffer[0].m_Address), 0, CommandBuffer.PayloadBuffer[0].m_Size);

				size_t numFiles = 0;
				char* pFilename = (char*)Memory::GetPointer((u32)(CommandBuffer.PayloadBuffer[0].m_Address));

				for (size_t i=0; i<FileSearch.GetFileNames().size(); i++)
				{
					if (i >= MaxEntries)
						break;

					std::string filename, ext;
					SplitPath(FileSearch.GetFileNames()[i], NULL, &filename, &ext);
					std::string CompleteFilename = filename + ext;

					strcpy(pFilename, CompleteFilename.c_str());
					pFilename += CompleteFilename.length();
					*pFilename++ = 0x00;  // termination
					numFiles++;

					INFO_LOG(WII_IPC_FILEIO, "    Found: %s", CompleteFilename.c_str());
				}

				Memory::Write_U32((u32)numFiles, CommandBuffer.PayloadBuffer[1].m_Address);
			}

			ReturnValue = FS_RESULT_OK;
		}
		break;

	case IOCTLV_GETUSAGE:
		{
			_dbg_assert_(WII_IPC_FILEIO, CommandBuffer.PayloadBuffer.size() == 2);
			_dbg_assert_(WII_IPC_FILEIO, CommandBuffer.PayloadBuffer[0].m_Size == 4);
			_dbg_assert_(WII_IPC_FILEIO, CommandBuffer.PayloadBuffer[1].m_Size == 4);

			// this command sucks because it asks of the number of used 
			// fsBlocks and inodes
			// It should be correct, but don't count on it...
			std::string path(HLE_IPC_BuildFilename((const char*)Memory::GetPointer(CommandBuffer.InBuffer[0].m_Address), CommandBuffer.InBuffer[0].m_Size));
			u32 fsBlocks = 0;
			u32 iNodes = 0;

			INFO_LOG(WII_IPC_FILEIO, "IOCTL_GETUSAGE %s", path.c_str());
			if (File::IsDirectory(path.c_str()))
			{
				File::FSTEntry parentDir;
				iNodes = File::ScanDirectoryTree(path.c_str(), parentDir);

				u64 totalSize = ComputeTotalFileSize(parentDir); // "Real" size, to be converted to nand blocks

				fsBlocks = (u32)(totalSize / (16 * 1024));  // one bock is 16kb

				ReturnValue = FS_RESULT_OK;

				INFO_LOG(WII_IPC_FILEIO, "FS: fsBlock: %i, iNodes: %i", fsBlocks, iNodes);
			}
			else
			{
				fsBlocks = 0;
				iNodes = 0;
				ReturnValue = FS_RESULT_OK;
				WARN_LOG(WII_IPC_FILEIO, "FS: fsBlock failed, cannot find directoy: %s", path.c_str());
			}

			Memory::Write_U32(fsBlocks, CommandBuffer.PayloadBuffer[0].m_Address);
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

bool CWII_IPC_HLE_Device_fs::IOCtl(u32 _CommandAddress) 
{ 
	//u32 DeviceID = Memory::Read_U32(_CommandAddress + 8);
	//LOG(WII_IPC_FILEIO, "FS: IOCtl (Device=%s, DeviceID=%08x)", GetDeviceName().c_str(), DeviceID);

	u32 Parameter =  Memory::Read_U32(_CommandAddress + 0xC);
	u32 BufferIn =  Memory::Read_U32(_CommandAddress + 0x10);
	u32 BufferInSize =  Memory::Read_U32(_CommandAddress + 0x14);
	u32 BufferOut = Memory::Read_U32(_CommandAddress + 0x18);
	u32 BufferOutSize = Memory::Read_U32(_CommandAddress + 0x1C);

	/* Prepare the out buffer(s) with zeroes as a safety precaution
	   to avoid returning bad values. */
	//LOG(WII_IPC_FILEIO, "Cleared %u bytes of the out buffer", _BufferOutSize);
	Memory::Memset(BufferOut, 0, BufferOutSize);

	u32 ReturnValue = ExecuteCommand(Parameter, BufferIn, BufferInSize, BufferOut, BufferOutSize);	
	Memory::Write_U32(ReturnValue, _CommandAddress + 4);

	return true; 
}

s32 CWII_IPC_HLE_Device_fs::ExecuteCommand(u32 _Parameter, u32 _BufferIn, u32 _BufferInSize, u32 _BufferOut, u32 _BufferOutSize)
{
	switch(_Parameter)
	{
	case IOCTL_GET_STATS:
		{
			if (_BufferOutSize < 0x1c)
				return -1017;

			WARN_LOG(WII_IPC_FILEIO, "FS: GET STATS - returning static values for now");

			NANDStat fs;

			//TODO: scrape the real amounts from somewhere...
			fs.BlockSize	= 0x4000;
			fs.FreeBlocks	= 0x5DEC;
			fs.UsedBlocks	= 0x1DD4;
			fs.unk3			= 0x10;
			fs.unk4			= 0x02F0;
			fs.Free_INodes	= 0x146B;
			fs.unk5			= 0x0394;

			*(NANDStat*)Memory::GetPointer(_BufferOut) = fs;

			return FS_RESULT_OK;
		}
		break;

	case IOCTL_CREATE_DIR:
		{
			_dbg_assert_(WII_IPC_FILEIO, _BufferOutSize == 0);
			u32 Addr = _BufferIn;

			u32 OwnerID = Memory::Read_U32(Addr); Addr += 4;
			u16 GroupID = Memory::Read_U16(Addr); Addr += 2;
			std::string DirName(HLE_IPC_BuildFilename((const char*)Memory::GetPointer(Addr), 64)); Addr += 64;
			Addr += 9; // owner attribs, permission
			u8 Attribs = Memory::Read_U8(Addr);

			INFO_LOG(WII_IPC_FILEIO, "FS: CREATE_DIR %s", DirName.c_str());

			DirName += DIR_SEP;
			File::CreateFullPath(DirName.c_str());
			_dbg_assert_msg_(WII_IPC_FILEIO, File::IsDirectory(DirName.c_str()), "FS: CREATE_DIR %s failed", DirName.c_str());

			return FS_RESULT_OK;
		}
		break;

	case IOCTL_SET_ATTR:
		{
			u32 Addr = _BufferIn;
		
			u32 OwnerID = Memory::Read_U32(Addr); Addr += 4;
			u16 GroupID = Memory::Read_U16(Addr); Addr += 2;
			std::string Filename = HLE_IPC_BuildFilename((const char*)Memory::GetPointer(_BufferIn), 64); Addr += 64;
			u8 OwnerPerm = Memory::Read_U8(Addr); Addr += 1;
			u8 GroupPerm = Memory::Read_U8(Addr); Addr += 1;
			u8 OtherPerm = Memory::Read_U8(Addr); Addr += 1;
			u8 Attributes = Memory::Read_U8(Addr); Addr += 1;

			INFO_LOG(WII_IPC_FILEIO, "FS: SetAttrib %s", Filename.c_str());
			DEBUG_LOG(WII_IPC_FILEIO, "    OwnerID: 0x%08x", OwnerID);
			DEBUG_LOG(WII_IPC_FILEIO, "    GroupID: 0x%04x", GroupID);
			DEBUG_LOG(WII_IPC_FILEIO, "    OwnerPerm: 0x%02x", OwnerPerm);
			DEBUG_LOG(WII_IPC_FILEIO, "    GroupPerm: 0x%02x", GroupPerm);
			DEBUG_LOG(WII_IPC_FILEIO, "    OtherPerm: 0x%02x", OtherPerm);
			DEBUG_LOG(WII_IPC_FILEIO, "    Attributes: 0x%02x", Attributes);

			return FS_RESULT_OK;
		}
		break;

	case IOCTL_GET_ATTR:
		{		
			_dbg_assert_msg_(WII_IPC_FILEIO, _BufferOutSize == 76,
				"    GET_ATTR needs an 76 bytes large output buffer but it is %i bytes large",
				_BufferOutSize);

			u32 OwnerID = 0;
			u16 GroupID = 0;
			std::string Filename = HLE_IPC_BuildFilename((const char*)Memory::GetPointer(_BufferIn), 64);
			u8 OwnerPerm = 0x3;		// read/write
			u8 GroupPerm = 0x3;		// read/write
			u8 OtherPerm = 0x3;		// read/write		
			u8 Attributes = 0x00;	// no attributes
			if (File::IsDirectory(Filename.c_str()))
			{
				INFO_LOG(WII_IPC_FILEIO, "FS: GET_ATTR Directory %s - all permission flags are set", Filename.c_str());
			}
			else
			{
				if (File::Exists(Filename.c_str()))
				{
					INFO_LOG(WII_IPC_FILEIO, "FS: GET_ATTR %s - all permission flags are set", Filename.c_str());
				}
				else
				{
					INFO_LOG(WII_IPC_FILEIO, "FS: GET_ATTR unknown %s", Filename.c_str());
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


	case IOCTL_DELETE_FILE:
		{
			_dbg_assert_(WII_IPC_FILEIO, _BufferOutSize == 0);
			int Offset = 0;

			std::string Filename = HLE_IPC_BuildFilename((const char*)Memory::GetPointer(_BufferIn+Offset), 64);
			Offset += 64;
			if (File::Delete(Filename.c_str()))
			{
				INFO_LOG(WII_IPC_FILEIO, "FS: DeleteFile %s", Filename.c_str());
			}
			else if (File::DeleteDir(Filename.c_str()))
			{
				INFO_LOG(WII_IPC_FILEIO, "FS: DeleteDir %s", Filename.c_str());
			}
			else
			{
				WARN_LOG(WII_IPC_FILEIO, "FS: DeleteFile %s - failed!!!", Filename.c_str());
			}

			return FS_RESULT_OK;
		}
		break;

	case IOCTL_RENAME_FILE:
		{
			_dbg_assert_(WII_IPC_FILEIO, _BufferOutSize == 0);
			int Offset = 0;

			std::string Filename = HLE_IPC_BuildFilename((const char*)Memory::GetPointer(_BufferIn+Offset), 64);
			Offset += 64;

			std::string FilenameRename = HLE_IPC_BuildFilename((const char*)Memory::GetPointer(_BufferIn+Offset), 64);
			Offset += 64;

			// try to make the basis directory
			File::CreateFullPath(FilenameRename.c_str());

			// if there is already a file, delete it
			if (File::Exists(FilenameRename.c_str()))
			{
				File::Delete(FilenameRename.c_str());
			}

			// finally try to rename the file
			if (File::Rename(Filename.c_str(), FilenameRename.c_str()))
			{
				INFO_LOG(WII_IPC_FILEIO, "FS: Rename %s to %s", Filename.c_str(), FilenameRename.c_str());
			}
			else
			{
				ERROR_LOG(WII_IPC_FILEIO, "FS: Rename %s to %s - failed", Filename.c_str(), FilenameRename.c_str());
				return FS_FILE_NOT_EXIST;
			}

			return FS_RESULT_OK;
		}
		break;

	case IOCTL_CREATE_FILE:
		{
			_dbg_assert_(WII_IPC_FILEIO, _BufferOutSize == 0);

			u32 Addr = _BufferIn;
			u32 OwnerID = Memory::Read_U32(Addr); Addr += 4;
			u16 GroupID = Memory::Read_U16(Addr); Addr += 2;
			std::string Filename(HLE_IPC_BuildFilename((const char*)Memory::GetPointer(Addr), 64)); Addr += 64;
			u8 OwnerPerm = Memory::Read_U8(Addr); Addr++;
			u8 GroupPerm = Memory::Read_U8(Addr); Addr++;
			u8 OtherPerm = Memory::Read_U8(Addr); Addr++;
			u8 Attributes = Memory::Read_U8(Addr); Addr++;

			INFO_LOG(WII_IPC_FILEIO, "FS: CreateFile %s", Filename.c_str());
			DEBUG_LOG(WII_IPC_FILEIO, "    OwnerID: 0x%08x", OwnerID);
			DEBUG_LOG(WII_IPC_FILEIO, "    GroupID: 0x%04x", GroupID);
			DEBUG_LOG(WII_IPC_FILEIO, "    OwnerPerm: 0x%02x", OwnerPerm);
			DEBUG_LOG(WII_IPC_FILEIO, "    GroupPerm: 0x%02x", GroupPerm);
			DEBUG_LOG(WII_IPC_FILEIO, "    OtherPerm: 0x%02x", OtherPerm);
			DEBUG_LOG(WII_IPC_FILEIO, "    Attributes: 0x%02x", Attributes);

			// check if the file already exist
			if (File::Exists(Filename.c_str()))
			{
				WARN_LOG(WII_IPC_FILEIO, "    result = FS_RESULT_EXISTS", Filename.c_str());
				return FS_FILE_EXIST;
			}

			// create the file
			File::CreateFullPath(Filename.c_str());  // just to be sure
			bool Result = File::CreateEmptyFile(Filename.c_str());
			if (!Result)
			{
				ERROR_LOG(WII_IPC_FILEIO, "CWII_IPC_HLE_Device_fs: couldn't create new file");
				PanicAlert("CWII_IPC_HLE_Device_fs: couldn't create new file");
				return FS_RESULT_FATAL;
			}

			INFO_LOG(WII_IPC_FILEIO, "\tresult = FS_RESULT_OK");
			return FS_RESULT_OK;
		}
		break;

	default:
		ERROR_LOG(WII_IPC_FILEIO, "CWII_IPC_HLE_Device_fs::IOCtl: ni  0x%x", _Parameter);
		PanicAlert("CWII_IPC_HLE_Device_fs::IOCtl: ni  0x%x", _Parameter);
		break;
	}

	return FS_RESULT_FATAL;
}
