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
#include "stdafx.h"

#ifdef _WIN32
#include <io.h>
#include <windows.h>
#else
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#endif

#include "VolumeDirectory.h"
#include "FileBlob.h"

const u8 ENTRY_SIZE = 0x0c;
const u8 FILE_ENTRY = 0;
const u8 DIRECTORY_ENTRY = 1;
const u64 FST_ADDRESS = 0x440;
const u32 MAX_NAME_LENGTH = 0x3df;

namespace DiscIO
{


CVolumeDirectory::CVolumeDirectory(const std::string& _rDirectory, bool _bIsWii)
	: m_totalNameSize(0)
	, m_dataStartAddress(-1)
	, m_fstSize(0)
	, m_FSTData(NULL)
{
	m_rootDirectory = ExtractDirectoryName(_rDirectory);	

	// create the default disk header
	m_diskHeader = new u8[FST_ADDRESS];
	memset(m_diskHeader, 0, (size_t)FST_ADDRESS);
	SetUniqueID("RZDE01");	
	SetName("Default name");

	if(_bIsWii)
	{
		SetDiskTypeWii();
	}
	else
	{
		SetDiskTypeGC();
	}

	BuildFST();
}

CVolumeDirectory::~CVolumeDirectory()
{
	delete m_FSTData;
	m_FSTData = NULL;

	delete m_diskHeader;
	m_diskHeader = NULL;
}

bool CVolumeDirectory::IsValidDirectory(const std::string& _rDirectory)
{
	std::string directoryName = ExtractDirectoryName(_rDirectory);

#ifdef _WIN32
	WIN32_FIND_DATA ffd;
	HANDLE hFind = FindFirstFile(directoryName.c_str(), &ffd);

	if (hFind == INVALID_HANDLE_VALUE)
		return false;

	return true;
#else
	struct stat info;
	if (!stat(directoryName.c_str(), &info))
		return false;

	return S_ISDIR(info.st_mode);
#endif
}

bool CVolumeDirectory::Read(u64 _Offset, u64 _Length, u8* _pBuffer) const
{
	if(_Offset < FST_ADDRESS)
	{
		WriteToBuffer(0, FST_ADDRESS, m_diskHeader, _Offset, _Length, _pBuffer);
	}

	if(_Offset < m_dataStartAddress)
	{
		WriteToBuffer(FST_ADDRESS, m_fstSize, m_FSTData, _Offset, _Length, _pBuffer);		
	}
	
	if(m_virtualDisk.size() == 0)
		return true;

	// Determine which file the offset refers to
	std::map<u64, std::string>::const_iterator fileIter = m_virtualDisk.lower_bound(_Offset);
	if(fileIter->first > _Offset && fileIter != m_virtualDisk.begin())
		--fileIter;

	// zero fill to start of file data
	PadToAddress(fileIter->first, _Offset, _Length, _pBuffer);

	while(fileIter != m_virtualDisk.end() && _Length > 0)
	{	
		_dbg_assert_(DVDINTERFACE, fileIter->first <= _Offset);
		u64 fileOffset = _Offset - fileIter->first;

		PlainFileReader* reader = PlainFileReader::Create(fileIter->second.c_str());		
		if(reader == NULL)
			return false;

		u64 fileSize = reader->GetDataSize();
		
		if(fileOffset < fileSize)
		{
			u64 fileBytes = fileSize - fileOffset;
			if(_Length < fileBytes)
				fileBytes = _Length;

			if(!reader->Read(fileOffset, fileBytes, _pBuffer))
				return false;

			_Length -= fileBytes;
			_pBuffer += fileBytes;
			_Offset += fileBytes;
		}

		++fileIter;

		if(fileIter != m_virtualDisk.end())
		{
			_dbg_assert_(DVDINTERFACE, fileIter->first >= _Offset);
			PadToAddress(fileIter->first, _Offset, _Length, _pBuffer);
		}
	}
	
	return true;
}


std::string CVolumeDirectory::GetName() const
{
	_dbg_assert_(DVDINTERFACE, m_diskHeader);
	std::string name = (char*)(m_diskHeader + 0x20);
	return name;
}

void CVolumeDirectory::SetName(std::string _Name)
{
	_dbg_assert_(DVDINTERFACE, m_diskHeader);

	u32 length = _Name.length();
	if(length > MAX_NAME_LENGTH)
		length = MAX_NAME_LENGTH;

	memcpy(m_diskHeader + 0x20, _Name.c_str(), length);
	m_diskHeader[length + 0x20] = 0;
}

std::string CVolumeDirectory::GetUniqueID() const
{
	_dbg_assert_(DVDINTERFACE, m_diskHeader);
	
	char buffer[7];
	memcpy(buffer, m_diskHeader, 6);
	buffer[6] = 0;

	std::string id = buffer;
	return id;
}

void CVolumeDirectory::SetUniqueID(std::string _ID)
{
	_dbg_assert_(DVDINTERFACE, m_diskHeader);

	u32 length = _ID.length();
	if(length > 6)
		length = 6;

	memcpy(m_diskHeader, _ID.c_str(), length);
}

IVolume::ECountry CVolumeDirectory::GetCountry() const
{
	_dbg_assert_(DVDINTERFACE, m_diskHeader);
	
	u8 CountryCode = m_diskHeader[3];

	ECountry country = COUNTRY_UNKNOWN;

	switch (CountryCode)
	{
	    case 'S':
		    country = COUNTRY_EUROPE;
		    break; // PAL <- that is shitty :) zelda demo disc

	    case 'P':
		    country = COUNTRY_EUROPE;
		    break; // PAL

	    case 'D':
		    country = COUNTRY_EUROPE;
		    break; // PAL

	    case 'F':
		    country = COUNTRY_FRANCE;
		    break; // PAL

	    case 'X':
		    country = COUNTRY_EUROPE;
		    break; // XIII <- uses X but is PAL rip

	    case 'E':
		    country = COUNTRY_USA;
		    break; // USA

	    case 'J':
		    country = COUNTRY_JAP;
		    break; // JAP

	    case 'O':
		    country = COUNTRY_UNKNOWN;
		    break; // SDK

	    default:
		    country = COUNTRY_UNKNOWN;
		    break;
	}

	return(country);
}

u64 CVolumeDirectory::GetSize() const
{
	return 0;
}

static const char DIR_SEPARATOR =
#ifdef _WIN32
	'\\';
#else
	'/';
#endif

std::string CVolumeDirectory::ExtractDirectoryName(const std::string& _rDirectory)
{
	std::string directoryName = _rDirectory;

	size_t lastSep = directoryName.find_last_of(DIR_SEPARATOR);

	if(lastSep != directoryName.size() - 1)
	{
		// TODO: This assumes that file names will always have a dot in them
		//       and directory names never will; both assumptions are often
		//       right but in general wrong.
		size_t extensionStart = directoryName.find_last_of('.');
		if(extensionStart != std::string::npos && extensionStart > lastSep)
		{
			directoryName.resize(lastSep);
		}
	}
	else
	{
		directoryName.resize(lastSep);
	}

	return directoryName;
}

void CVolumeDirectory::SetDiskTypeWii()
{
	_dbg_assert_(DVDINTERFACE, m_diskHeader);

	m_diskHeader[0x18] = 0x5d;
	m_diskHeader[0x19] = 0x1c;
	m_diskHeader[0x1a] = 0x9e;
	m_diskHeader[0x1b] = 0xa3;	
	memset(m_diskHeader + 0x1c, 0, 4);

	m_addressShift = 2;
}

void CVolumeDirectory::SetDiskTypeGC()
{
	_dbg_assert_(DVDINTERFACE, m_diskHeader);

	memset(m_diskHeader + 0x18, 0, 4);
	m_diskHeader[0x1c] = 0xc2;
	m_diskHeader[0x1d] = 0x33;
	m_diskHeader[0x1e] = 0x9f;
	m_diskHeader[0x1f] = 0x3d;

	m_addressShift = 0;
}

void CVolumeDirectory::BuildFST()
{
	if(m_FSTData)
	{
		delete m_FSTData;
	}

	FSTEntry rootEntry;

	// read data from physical disk to rootEntry
	u32 totalEntries = AddDirectoryEntries(m_rootDirectory, rootEntry) + 1;

	m_fstNameOffset = totalEntries * ENTRY_SIZE;	// offset in FST nameTable
	m_fstSize = m_fstNameOffset + m_totalNameSize;
	m_FSTData = new u8[(u32)m_fstSize];

	// 4 byte aligned start of data on disk
	m_dataStartAddress = (FST_ADDRESS + m_fstSize + 3) & 0xfffffffc;
	u64 curDataAddress = m_dataStartAddress;

	u32 fstOffset = 0;		// offset within FST data
	u32 nameOffset = 0;		// offset within name table
	u32 rootOffset = 0;		// offset of root of FST

	// write root entry
	WriteEntryData(fstOffset, DIRECTORY_ENTRY, 0, 0, totalEntries);

	for(std::vector<FSTEntry>::iterator iter = rootEntry.children.begin(); iter != rootEntry.children.end(); ++iter)
	{
		WriteEntry(*iter, fstOffset, nameOffset, curDataAddress, rootOffset);
	}

	// overflow check
	_dbg_assert_(DVDINTERFACE, nameOffset == m_fstSize);

	// write FST size and location
	_dbg_assert_(DVDINTERFACE, m_diskHeader);
	Write32((u32)(FST_ADDRESS >> m_addressShift), 0x0424, m_diskHeader);
	Write32((u32)m_fstSize, 0x0428, m_diskHeader);
	Write32((u32)m_fstSize, 0x042c, m_diskHeader);
}

void CVolumeDirectory::WriteToBuffer(u64 _SrcStartAddress, u64 _SrcLength, u8* _Src,
									 u64& _Address, u64& _Length, u8*& _pBuffer) const
{
	_dbg_assert_(DVDINTERFACE, _Address >= _SrcStartAddress);

	u64 srcOffset = _Address - _SrcStartAddress;

	if(srcOffset < _SrcLength)
	{
		u64 srcBytes = _SrcLength - srcOffset;
		if(_Length < srcBytes)
			srcBytes = _Length;

		memcpy(_pBuffer, _Src + srcOffset, (size_t)srcBytes);

		_Length -= srcBytes;
		_pBuffer += srcBytes;
		_Address += srcBytes;
	}	
}

void CVolumeDirectory::PadToAddress(u64 _StartAddress, u64& _Address, u64& _Length, u8*& _pBuffer) const
{
	if(_StartAddress <= _Address)
		return;

	u64 padBytes = _StartAddress - _Address;
	if(padBytes > _Length)
		padBytes = _Length;

	if(_Length > 0)
	{
		memset(_pBuffer, 0, (size_t)padBytes);
		_Length -= padBytes;
		_pBuffer += padBytes;
		_Address += padBytes;
	}
}

void CVolumeDirectory::Write32(u32 data, u32 offset, u8* buffer)
{
	buffer[offset++] = (data >> 24);
	buffer[offset++] = (data >> 16) & 0xff;
	buffer[offset++] = (data >> 8) & 0xff;
	buffer[offset] = (data) & 0xff;
}

void CVolumeDirectory::WriteEntryData(u32& entryOffset, u8 type, u32 nameOffset, u64 dataOffset, u32 length)
{
	m_FSTData[entryOffset++] = type;

	m_FSTData[entryOffset++] = (nameOffset >> 16) & 0xff;
	m_FSTData[entryOffset++] = (nameOffset >> 8) & 0xff;
	m_FSTData[entryOffset++] = (nameOffset) & 0xff;

	Write32((u32)(dataOffset >> m_addressShift), entryOffset, m_FSTData);
	entryOffset += 4;

	Write32((u32)length, entryOffset, m_FSTData);
	entryOffset += 4;
}

void CVolumeDirectory::WriteEntryName(u32& nameOffset, const std::string& name)
{
	strncpy((char*)(m_FSTData + nameOffset + m_fstNameOffset), name.c_str(), name.length() + 1);
	
	nameOffset += (name.length() + 1);
}

void CVolumeDirectory::WriteEntry(const FSTEntry& entry, u32& fstOffset, u32& nameOffset, u64& dataOffset, u32 parentEntryNum)
{	
	if(entry.isDirectory)
	{
		u32 myOffset = fstOffset;
		u32 myEntryNum = myOffset / ENTRY_SIZE;		
		WriteEntryData(fstOffset, DIRECTORY_ENTRY, nameOffset, parentEntryNum, myEntryNum + entry.size + 1);
		WriteEntryName(nameOffset, entry.virtualName);

		for(std::vector<FSTEntry>::const_iterator iter = entry.children.begin(); iter != entry.children.end(); ++iter)
		{
			WriteEntry(*iter, fstOffset, nameOffset, dataOffset, myEntryNum);
		}
	}
	else
	{
		// put entry in FST
		WriteEntryData(fstOffset, FILE_ENTRY, nameOffset, dataOffset, entry.size);
		WriteEntryName(nameOffset, entry.virtualName);

		// write entry to virtual disk
		_dbg_assert_(DVDINTERFACE, m_virtualDisk.find(dataOffset) == m_virtualDisk.end());
		m_virtualDisk.insert(make_pair(dataOffset, entry.physicalName));

		// 4 byte aligned
		dataOffset = (dataOffset + entry.size + 3) & 0xfffffffc;
	}
}

#ifdef _WIN32
bool ReadFoundFile(const WIN32_FIND_DATA& ffd, CVolumeDirectory::FSTEntry& entry)	
{
	// ignore files starting with a .
	if(strncmp(ffd.cFileName, ".", 1) == 0)
		return false;

	entry.virtualName = ffd.cFileName;

	if(ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
	{
		entry.isDirectory = true;
	}
	else
	{
		entry.isDirectory = false;
		entry.size = ffd.nFileSizeLow;
	}

	return true;
}

u32 CVolumeDirectory::AddDirectoryEntries(const std::string& _Directory, FSTEntry& parentEntry)
{
	// Find the first file in the directory.
	WIN32_FIND_DATA ffd;
	std::string searchName = _Directory + "\\*";
	HANDLE hFind = FindFirstFile(searchName.c_str(), &ffd);

	u32 foundEntries = 0;

	if (hFind != INVALID_HANDLE_VALUE)
	{
		do
		{
			FSTEntry entry;

			if(ReadFoundFile(ffd, entry))
			{
				entry.physicalName = _Directory + "\\" + entry.virtualName;
				if(entry.isDirectory)
				{
					u32 childEntries = AddDirectoryEntries(entry.physicalName, entry);
					entry.size = childEntries;
					foundEntries += childEntries;
				}

				++foundEntries;

				parentEntry.children.push_back(entry);
				m_totalNameSize += entry.virtualName.length() + 1;
			}
		} while (FindNextFile(hFind, &ffd) != 0);
	}

	FindClose(hFind);

	return foundEntries;
}
#else
u32 CVolumeDirectory::AddDirectoryEntries(const std::string& _Directory, FSTEntry& parentEntry)
{
	// TODO - Insert linux stuff here
	return 0;
}
#endif

} // namespace
