// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>

#include "Common/ChunkFile.h"
#include "Common/CommonPaths.h"
#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/NandPaths.h"
#include "Common/StringUtil.h"

#include "Core/Core.h"
#include "Core/IPC_HLE/WII_IPC_HLE.h"
#include "Core/IPC_HLE/WII_IPC_HLE_Device_FileIO.h"
#include "Core/IPC_HLE/WII_IPC_HLE_Device_fs.h"


static Common::replace_v replacements;

static std::map <std::string, std::weak_ptr<File::IOFile>> openFiles;

// This is used by several of the FileIO and /dev/fs functions
std::string HLE_IPC_BuildFilename(std::string path_wii)
{
	std::string path_full = File::GetUserPath(D_SESSION_WIIROOT_IDX);

	// Replaces chars that FAT32 can't support with strings defined in /sys/replace
	for (auto& replacement : replacements)
	{
		for (size_t j = 0; (j = path_wii.find(replacement.first, j)) != path_wii.npos; ++j)
			path_wii.replace(j, 1, replacement.second);
	}

	path_full += path_wii;

	return path_full;
}

void HLE_IPC_CreateVirtualFATFilesystem()
{
	const int cdbSize = 0x01400000;
	const std::string cdbPath = Common::GetTitleDataPath(TITLEID_SYSMENU, Common::FROM_SESSION_ROOT) + "cdb.vff";
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
	: IWII_IPC_HLE_Device(_DeviceID, _rDeviceName, false) // not a real hardware
	, m_Mode(0)
	, m_SeekPos(0)
	, m_file()
{
	Common::ReadReplacements(replacements);
}

CWII_IPC_HLE_Device_FileIO::~CWII_IPC_HLE_Device_FileIO()
{
}

IPCCommandResult CWII_IPC_HLE_Device_FileIO::Close(u32 _CommandAddress, bool _bForce)
{
	INFO_LOG(WII_IPC_FILEIO, "FileIO: Close %s (DeviceID=%08x)", m_Name.c_str(), m_DeviceID);
	m_Mode = 0;

	// Let go of our pointer to the file, it will automatically close if we are the last handle accessing it.
	m_file.reset();

	// Close always return 0 for success
	if (_CommandAddress && !_bForce)
		Memory::Write_U32(0, _CommandAddress + 4);
	m_Active = false;
	return IPC_DEFAULT_REPLY;
}

IPCCommandResult CWII_IPC_HLE_Device_FileIO::Open(u32 _CommandAddress, u32 _Mode)
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

	m_filepath = HLE_IPC_BuildFilename(m_Name);

	// The file must exist before we can open it
	// It should be created by ISFS_CreateFile, not here
	if (File::Exists(m_filepath) && !File::IsDirectory(m_filepath))
	{
		INFO_LOG(WII_IPC_FILEIO, "FileIO: Open %s (%s == %08X)", m_Name.c_str(), Modes[_Mode], _Mode);
		OpenFile();
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
	return IPC_DEFAULT_REPLY;
}

// This isn't theadsafe, but it's only called from the CPU thread.
void CWII_IPC_HLE_Device_FileIO::OpenFile()
{
	// On the wii, all file operations are strongly ordered.
	// If a game opens the same file twice (or 8 times, looking at you PokePark Wii)
	// and writes to one file handle, it will be able to immediately read the written
	// data from the other handle.
	// On 'real' operating systems, there are various buffers and caches meaning
	// applications doing such naughty things will not get expected results.

	// So we fix this by catching any attempts to open the same file twice and
	// only opening one file. Accesses to a single file handle are ordered.
	//
	// Hall of Shame:
	//    - PokePark Wii (gets stuck on the loading screen of Pikachu falling)
	//    - PokePark 2 (Also gets stuck while loading)
	//    - Wii System Menu (Can't access the system settings, gets stuck on blank screen)
	//    - The Beatles: Rock Band (saving doesn't work)

	// Check if the file has already been opened.
	auto search = openFiles.find(m_Name);
	if (search != openFiles.end())
	{
		m_file = search->second.lock(); // Lock a shared pointer to use.
	}
	else
	{
		std::string path = m_Name;
		// This code will be called when all references to the shared pointer below have been removed.
		auto deleter = [path](File::IOFile* ptr)
		{
			delete ptr; // IOFile's deconstructor closes the file.
			openFiles.erase(path); // erase the weak pointer from the list of open files.
		};

		// All files are opened read/write. Actual access rights will be controlled per handle by the read/write functions below
		m_file = std::shared_ptr<File::IOFile>(new File::IOFile(m_filepath, "r+b"), deleter); // Use the custom deleter from above.

		// Store a weak pointer to our newly opened file in the cache.
		openFiles[path] = std::weak_ptr<File::IOFile>(m_file);
	}
}

IPCCommandResult CWII_IPC_HLE_Device_FileIO::Seek(u32 _CommandAddress)
{
	u32 ReturnValue = FS_RESULT_FATAL;
	const s32 SeekPosition = Memory::Read_U32(_CommandAddress + 0xC);
	const s32 Mode = Memory::Read_U32(_CommandAddress + 0x10);

	if (m_file->IsOpen())
	{
		ReturnValue = FS_RESULT_FATAL;

		const s32 fileSize = (s32) m_file->GetSize();
		INFO_LOG(WII_IPC_FILEIO, "FileIO: Seek Pos: 0x%08x, Mode: %i (%s, Length=0x%08x)", SeekPosition, Mode, m_Name.c_str(), fileSize);

		switch (Mode)
		{
			case WII_SEEK_SET:
			{
				if ((SeekPosition >=0) && (SeekPosition <= fileSize))
				{
					m_SeekPos = SeekPosition;
					ReturnValue = m_SeekPos;
				}
				break;
			}

			case WII_SEEK_CUR:
			{
				s32 wantedPos = SeekPosition+m_SeekPos;
				if (wantedPos >=0 && wantedPos <= fileSize)
				{
					m_SeekPos = wantedPos;
					ReturnValue = m_SeekPos;
				}
				break;
			}

			case WII_SEEK_END:
			{
				s32 wantedPos = SeekPosition+fileSize;
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
	}
	else
	{
		ReturnValue = FS_FILE_NOT_EXIST;
	}
	Memory::Write_U32(ReturnValue, _CommandAddress + 0x4);

	return IPC_DEFAULT_REPLY;
}

IPCCommandResult CWII_IPC_HLE_Device_FileIO::Read(u32 _CommandAddress)
{
	u32 ReturnValue = FS_EACCESS;
	const u32 Address = Memory::Read_U32(_CommandAddress + 0xC); // Read to this memory address
	const u32 Size    = Memory::Read_U32(_CommandAddress + 0x10);


	if (m_file->IsOpen())
	{
		if (m_Mode == ISFS_OPEN_WRITE)
		{
			WARN_LOG(WII_IPC_FILEIO, "FileIO: Attempted to read 0x%x bytes to 0x%08x on a write-only file %s", Size, Address, m_Name.c_str());
		}
		else
		{
			INFO_LOG(WII_IPC_FILEIO, "FileIO: Read 0x%x bytes to 0x%08x from %s", Size, Address, m_Name.c_str());
			m_file->Seek(m_SeekPos, SEEK_SET); // File might be opened twice, need to seek before we read
			ReturnValue = (u32)fread(Memory::GetPointer(Address), 1, Size, m_file->GetHandle());
			if (ReturnValue != Size && ferror(m_file->GetHandle()))
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
	return IPC_DEFAULT_REPLY;
}

IPCCommandResult CWII_IPC_HLE_Device_FileIO::Write(u32 _CommandAddress)
{
	u32 ReturnValue = FS_EACCESS;
	const u32 Address = Memory::Read_U32(_CommandAddress + 0xC); // Write data from this memory address
	const u32 Size    = Memory::Read_U32(_CommandAddress + 0x10);

	if (m_file->IsOpen())
	{
		if (m_Mode == ISFS_OPEN_READ)
		{
			WARN_LOG(WII_IPC_FILEIO, "FileIO: Attempted to write 0x%x bytes from 0x%08x to a read-only file %s", Size, Address, m_Name.c_str());
		}
		else
		{
			INFO_LOG(WII_IPC_FILEIO, "FileIO: Write 0x%04x bytes from 0x%08x to %s", Size, Address, m_Name.c_str());
			m_file->Seek(m_SeekPos, SEEK_SET); // File might be opened twice, need to seek before we write
			if (m_file->WriteBytes(Memory::GetPointer(Address), Size))
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
	return IPC_DEFAULT_REPLY;
}

IPCCommandResult CWII_IPC_HLE_Device_FileIO::IOCtl(u32 _CommandAddress)
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
			if (m_file->IsOpen())
			{
				u32 m_FileLength = (u32)m_file->GetSize();

				const u32 BufferOut = Memory::Read_U32(_CommandAddress + 0x18);
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

	return IPC_DEFAULT_REPLY;
}

void CWII_IPC_HLE_Device_FileIO::DoState(PointerWrap &p)
{
	DoStateShared(p);

	p.Do(m_Mode);
	p.Do(m_SeekPos);

	m_filepath = HLE_IPC_BuildFilename(m_Name);

	if (p.GetMode() == PointerWrap::MODE_READ)
	{
		OpenFile();
	}
}
