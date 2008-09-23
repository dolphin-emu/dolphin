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

#pragma once

#include "Volume.h"
#include "Common.h"
#include <vector>
#include <string>
#include <map>

//
// --- this volume type is used for reading files directly from the hard drive ---
//

namespace DiscIO
{

struct FSTEntry
{
	bool isDirectory;
	u32 size;						// file length or number of entries from children
	std::string physicalName;		// name on disk
	std::string virtualName;		// name in FST names table
	std::vector<FSTEntry> children;
};

class CVolumeDirectory
	: public IVolume
{
	public:

		CVolumeDirectory(const std::string& _rDirectory, bool _bIsWii);

		~CVolumeDirectory();

		static bool IsValidDirectory(const std::string& _rDirectory);

		bool Read(u64 _Offset, u64 _Length, u8* _pBuffer) const;

		std::string GetName() const;
		void SetName(std::string);

		std::string GetUniqueID() const;
		void SetUniqueID(std::string _ID);

		ECountry GetCountry() const;

		u64 GetSize() const;		

		void BuildFST();

	private:
		static std::string ExtractDirectoryName(const std::string& _rDirectory);

		void SetDiskTypeWii();
		void SetDiskTypeGC();

		// writing to read buffer
		void WriteToBuffer(u64 _SrcStartAddress, u64 _SrcLength, u8* _Src,
						   u64& _Address, u64& _Length, u8*& _pBuffer) const;

		void PadToAddress(u64 _StartAddress, u64& _Address, u64& _Length, u8*& _pBuffer) const;

		void Write32(u32 data, u32 offset, u8* buffer);

		// FST creation
		void WriteEntryData(u32& entryOffset, u8 type, u32 nameOffset, u64 dataOffset, u32 length);
		void WriteEntryName(u32& nameOffset, const std::string& name);
		void WriteEntry(const FSTEntry& entry, u32& fstOffset, u32& nameOffset, u64& dataOffset, u32 parentEntryNum);

		// returns number of entries found in _Directory
		u32 AddDirectoryEntries(const std::string& _Directory, FSTEntry& parentEntry);

		std::string m_rootDirectory;

		std::map<u64, std::string> m_virtualDisk;

		u32 m_totalNameSize;

		// gc has no shift, wii has 2 bit shift
		u32 m_addressShift;

		// first address on disk containing file data
		u64 m_dataStartAddress;

		u64 m_fstNameOffset;

		u64 m_fstSize;

		u8* m_FSTData;
		u8* m_diskHeader;
};
} // namespace

