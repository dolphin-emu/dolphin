// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Common/ChunkFile.h"
#include "Common/CommonPaths.h"
#include "Common/CommonTypes.h"
#include "Common/FileSearch.h"
#include "Common/FileUtil.h"
#include "Common/NandPaths.h"
#include "Common/StringUtil.h"

#include "Core/VolumeHandler.h"
#include "Core/HW/SystemTimers.h"
#include "Core/IPC_HLE/WII_IPC_HLE_Device_FileIO.h"
#include "Core/IPC_HLE/WII_IPC_HLE_Device_fs.h"

#define MAX_NAME  12

static Common::replace_v replacements;


CWII_IPC_HLE_Device_fs::CWII_IPC_HLE_Device_fs(u32 _DeviceID, const std::string& _rDeviceName)
	: IWII_IPC_HLE_Device(_DeviceID, _rDeviceName)
{
	Common::ReadReplacements(replacements);
}

CWII_IPC_HLE_Device_fs::~CWII_IPC_HLE_Device_fs()
{}

bool CWII_IPC_HLE_Device_fs::Open(u32 _CommandAddress, u32 _Mode)
{
	// clear tmp folder
	{
		std::string Path = File::GetUserPath(D_WIIUSER_IDX) + "tmp";
		File::DeleteDirRecursively(Path);
		File::CreateDir(Path);
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
	for (const File::FSTEntry& entry : parentEntry.children)
	{
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
	for (u32 i = 0; i < CommandBuffer.NumberPayloadBuffer; i++)
	{
		Memory::Memset(CommandBuffer.PayloadBuffer[i].m_Address, 0,
			CommandBuffer.PayloadBuffer[i].m_Size);
	}

	switch (CommandBuffer.Parameter)
	{
	case IOCTLV_READ_DIR:
		{
			// the Wii uses this function to define the type (dir or file)
			std::string DirName(HLE_IPC_BuildFilename(Memory::GetString(
				CommandBuffer.InBuffer[0].m_Address, CommandBuffer.InBuffer[0].m_Size)));

			INFO_LOG(WII_IPC_FILEIO, "FS: IOCTL_READ_DIR %s", DirName.c_str());

			if (!File::Exists(DirName))
			{
				WARN_LOG(WII_IPC_FILEIO, "FS: Search not found: %s", DirName.c_str());
				ReturnValue = FS_FILE_NOT_EXIST;
				break;
			}
			else if (!File::IsDirectory(DirName))
			{
				// It's not a directory, so error.
				// Games don't usually seem to care WHICH error they get, as long as it's <
				// Well the system menu CARES!
				WARN_LOG(WII_IPC_FILEIO, "\tNot a directory - return FS_RESULT_FATAL");
				ReturnValue = FS_RESULT_FATAL;
				break;
			}

			// make a file search
			CFileSearch::XStringVector Directories;
			Directories.push_back(DirName);

			CFileSearch::XStringVector Extensions;
			Extensions.push_back("*.*");

			CFileSearch FileSearch(Extensions, Directories);

			// it is one
			if ((CommandBuffer.InBuffer.size() == 1) && (CommandBuffer.PayloadBuffer.size() == 1))
			{
				size_t numFile = FileSearch.GetFileNames().size();
				INFO_LOG(WII_IPC_FILEIO, "\t%lu files found", (unsigned long)numFile);

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

					std::string name, ext;
					SplitPath(FileSearch.GetFileNames()[i], nullptr, &name, &ext);
					std::string FileName = name + ext;

					// Decode entities of invalid file system characters so that
					// games (such as HP:HBP) will be able to find what they expect.
					for (const Common::replace_t& r : replacements)
					{
						for (size_t j = 0; (j = FileName.find(r.second, j)) != FileName.npos; ++j)
							FileName.replace(j, r.second.length(), 1, r.first);
					}

					strcpy(pFilename, FileName.c_str());
					pFilename += FileName.length();
					*pFilename++ = 0x00;  // termination
					numFiles++;

					INFO_LOG(WII_IPC_FILEIO, "\tFound: %s", FileName.c_str());
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
			std::string relativepath = Memory::GetString(CommandBuffer.InBuffer[0].m_Address, CommandBuffer.InBuffer[0].m_Size);
			std::string path(HLE_IPC_BuildFilename(relativepath));
			u32 fsBlocks = 0;
			u32 iNodes = 0;

			INFO_LOG(WII_IPC_FILEIO, "IOCTL_GETUSAGE %s", path.c_str());
			if (File::IsDirectory(path))
			{
				// LPFaint99: After I found that setting the number of inodes to the number of children + 1 for the directory itself
				// I decided to compare with sneek which has the following 2 special cases which are
				// Copyright (C) 2009-2011  crediar http://code.google.com/p/sneek/
				if ((relativepath.compare(0, 16, "/title/00010001") == 0 ) ||
				    (relativepath.compare(0, 16, "/title/00010005") == 0 ))
				{
					fsBlocks = 23; // size is size/0x4000
					iNodes = 42; // empty folders return a FileCount of 1
				}
				else
				{
					File::FSTEntry parentDir;
					// add one for the folder itself, allows some games to create their save files
					// R8XE52 (Jurassic: The Hunted), STEETR (Tetris Party Deluxe) now create their saves with this change
					iNodes = 1 + File::ScanDirectoryTree(path, parentDir);

					u64 totalSize = ComputeTotalFileSize(parentDir); // "Real" size, to be converted to nand blocks

					fsBlocks = (u32)(totalSize / (16 * 1024));  // one bock is 16kb
				}
				ReturnValue = FS_RESULT_OK;

				INFO_LOG(WII_IPC_FILEIO, "FS: fsBlock: %i, iNodes: %i", fsBlocks, iNodes);
			}
			else
			{
				fsBlocks = 0;
				iNodes = 0;
				ReturnValue = FS_RESULT_OK;
				WARN_LOG(WII_IPC_FILEIO, "FS: fsBlock failed, cannot find directory: %s", path.c_str());
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
	switch (_Parameter)
	{
	case IOCTL_GET_STATS:
		{
			if (_BufferOutSize < 0x1c)
				return -1017;

			WARN_LOG(WII_IPC_FILEIO, "FS: GET STATS - returning static values for now");

			NANDStat fs;

			//TODO: scrape the real amounts from somewhere...
			fs.BlockSize      = 0x4000;
			fs.FreeUserBlocks = 0x5DEC;
			fs.UsedUserBlocks = 0x1DD4;
			fs.FreeSysBlocks  = 0x10;
			fs.UsedSysBlocks  = 0x02F0;
			fs.Free_INodes    = 0x146B;
			fs.Used_Inodes    = 0x0394;

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
			std::string DirName(HLE_IPC_BuildFilename(Memory::GetString(Addr, 64))); Addr += 64;
			Addr += 9; // owner attribs, permission
			u8 Attribs = Memory::Read_U8(Addr);

			INFO_LOG(WII_IPC_FILEIO, "FS: CREATE_DIR %s, OwnerID %#x, GroupID %#x, Attributes %#x", DirName.c_str(), OwnerID, GroupID, Attribs);

			DirName += DIR_SEP;
			File::CreateFullPath(DirName);
			_dbg_assert_msg_(WII_IPC_FILEIO, File::IsDirectory(DirName), "FS: CREATE_DIR %s failed", DirName.c_str());

			return FS_RESULT_OK;
		}
		break;

	case IOCTL_SET_ATTR:
		{
			u32 Addr = _BufferIn;

			u32 OwnerID = Memory::Read_U32(Addr); Addr += 4;
			u16 GroupID = Memory::Read_U16(Addr); Addr += 2;
			std::string Filename = HLE_IPC_BuildFilename(Memory::GetString(_BufferIn, 64)); Addr += 64;
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
			u16 GroupID = 0x3031; // this is also known as makercd, 01 (0x3031) for nintendo and 08 (0x3038) for MH3 etc
			std::string Filename = HLE_IPC_BuildFilename(Memory::GetString(_BufferIn, 64));
			u8 OwnerPerm = 0x3;   // read/write
			u8 GroupPerm = 0x3;   // read/write
			u8 OtherPerm = 0x3;   // read/write
			u8 Attributes = 0x00; // no attributes
			if (File::IsDirectory(Filename))
			{
				INFO_LOG(WII_IPC_FILEIO, "FS: GET_ATTR Directory %s - all permission flags are set", Filename.c_str());
			}
			else
			{
				if (File::Exists(Filename))
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
				Memory::Write_U32(OwnerID, Addr);                                    Addr += 4;
				Memory::Write_U16(GroupID, Addr);                                    Addr += 2;
				memcpy(Memory::GetPointer(Addr), Memory::GetPointer(_BufferIn), 64); Addr += 64;
				Memory::Write_U8(OwnerPerm, Addr);                                   Addr += 1;
				Memory::Write_U8(GroupPerm, Addr);                                   Addr += 1;
				Memory::Write_U8(OtherPerm, Addr);                                   Addr += 1;
				Memory::Write_U8(Attributes, Addr);                                  Addr += 1;
			}

			return FS_RESULT_OK;
		}
		break;


	case IOCTL_DELETE_FILE:
		{
			_dbg_assert_(WII_IPC_FILEIO, _BufferOutSize == 0);
			int Offset = 0;

			std::string Filename = HLE_IPC_BuildFilename(Memory::GetString(_BufferIn+Offset, 64));
			Offset += 64;
			if (File::Delete(Filename))
			{
				INFO_LOG(WII_IPC_FILEIO, "FS: DeleteFile %s", Filename.c_str());
			}
			else if (File::DeleteDir(Filename))
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

			std::string Filename = HLE_IPC_BuildFilename(Memory::GetString(_BufferIn+Offset, 64));
			Offset += 64;

			std::string FilenameRename = HLE_IPC_BuildFilename(Memory::GetString(_BufferIn+Offset, 64));
			Offset += 64;

			// try to make the basis directory
			File::CreateFullPath(FilenameRename);

			// if there is already a file, delete it
			if (File::Exists(Filename) && File::Exists(FilenameRename))
			{
				File::Delete(FilenameRename);
			}

			// finally try to rename the file
			if (File::Rename(Filename, FilenameRename))
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
			std::string Filename(HLE_IPC_BuildFilename(Memory::GetString(Addr, 64))); Addr += 64;
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
			if (File::Exists(Filename))
			{
				WARN_LOG(WII_IPC_FILEIO, "\tresult = FS_RESULT_EXISTS");
				return FS_FILE_EXIST;
			}

			// create the file
			File::CreateFullPath(Filename);  // just to be sure
			bool Result = File::CreateEmptyFile(Filename);
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
	case IOCTL_SHUTDOWN:
		{
			INFO_LOG(WII_IPC_FILEIO, "Wii called Shutdown()");
			// TODO: stop emulation
		}
		break;
	default:
		ERROR_LOG(WII_IPC_FILEIO, "CWII_IPC_HLE_Device_fs::IOCtl: ni  0x%x", _Parameter);
		PanicAlert("CWII_IPC_HLE_Device_fs::IOCtl: ni  0x%x", _Parameter);
		break;
	}

	return FS_RESULT_FATAL;
}

int CWII_IPC_HLE_Device_fs::GetCmdDelay(u32)
{
	// ~1/1000th of a second is too short and causes hangs in Wii Party
	// Play it safe at 1/500th
	return SystemTimers::GetTicksPerSecond() / 500;
}

void CWII_IPC_HLE_Device_fs::DoState(PointerWrap& p)
{
	DoStateShared(p);

	// handle /tmp

	std::string Path = File::GetUserPath(D_WIIUSER_IDX) + "tmp";
	if (p.GetMode() == PointerWrap::MODE_READ)
	{
		File::DeleteDirRecursively(Path);
		File::CreateDir(Path);

		//now restore from the stream
		while (1)
		{
			char type = 0;
			p.Do(type);
			if (!type)
				break;
			std::string filename;
			p.Do(filename);
			std::string name = Path + DIR_SEP + filename;
			switch (type)
			{
			case 'd':
			{
				File::CreateDir(name);
				break;
			}
			case 'f':
			{
				u32 size = 0;
				p.Do(size);

				File::IOFile handle(name, "wb");
				char buf[65536];
				u32 count = size;
				while (count > 65536)
				{
					p.DoArray(&buf[0], 65536);
					handle.WriteArray(&buf[0], 65536);
					count -= 65536;
				}
				p.DoArray(&buf[0], count);
				handle.WriteArray(&buf[0], count);
				break;
			}
			}
		}
	}
	else
	{
		//recurse through tmp and save dirs and files

		File::FSTEntry parentEntry;
		File::ScanDirectoryTree(Path, parentEntry);
		std::deque<File::FSTEntry> todo;
		todo.insert(todo.end(), parentEntry.children.begin(),
			    parentEntry.children.end());

		while (!todo.empty())
		{
			File::FSTEntry &entry = todo.front();
			std::string name = entry.physicalName;
			name.erase(0,Path.length()+1);
			char type = entry.isDirectory?'d':'f';
			p.Do(type);
			p.Do(name);
			if (entry.isDirectory)
			{
				todo.insert(todo.end(), entry.children.begin(),
					    entry.children.end());
			}
			else
			{
				u32 size = (u32)entry.size;
				p.Do(size);

				File::IOFile handle(entry.physicalName, "rb");
				char buf[65536];
				u32 count = size;
				while (count > 65536)
				{
					handle.ReadArray(&buf[0], 65536);
					p.DoArray(&buf[0], 65536);
					count -= 65536;
				}
				handle.ReadArray(&buf[0], count);
				p.DoArray(&buf[0], count);
			}
			todo.pop_front();
		}

		char type = 0;
		p.Do(type);
	}
}
