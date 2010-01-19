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

#ifndef __SYSCONF_MANAGER_h__
#define __SYSCONF_MANAGER_h__

#include <string>
#include <vector>

// This class is meant to edit the values in a given Wii SYSCONF file
// It currently does not add/remove/rearrange sections,
// instead only modifies exiting sections' data

#define SYSCONF_SIZE 0x4000

enum SysconfType
{
	Type_BigArray = 1,
	Type_SmallArray,
	Type_Byte,
	Type_Short,
	Type_Long,
	Type_Unknown,
	Type_Bool
};

struct SSysConfHeader
{
	char version[4];
	u16 numEntries;
};

struct SSysConfEntry
{
	u16 offset;
	SysconfType type;
	u8 nameLength;
	char name[32];
	u16 dataLength;
	u8* data;

	template<class T>
	T GetData() { return *(T*)data; }
};

class SysConf
{
private:
	SSysConfHeader m_Header;
	std::string m_Filename;
	std::vector<SSysConfEntry> m_Entries;
	bool m_IsValid;

public:
	SysConf();
	~SysConf();

	bool IsValid() { return m_IsValid; }

	template<class T>
	T GetData(const char* sectionName)
	{
		if (!m_IsValid)
		{
			PanicAlert("Trying to read from invalid SYSCONF");
			return 0;
		}

		size_t index = 0;
		for (; index < m_Entries.size() - 1; index++)
		{
			if (strcmp(m_Entries.at(index).name, sectionName) == 0)
				break;
		}
		if (index == m_Entries.size() - 1)
		{
			PanicAlert("Section %s not found in SYSCONF", sectionName);
			return 0;
		}

		return m_Entries.at(index).GetData<T>();
	}

	template<class T>
	bool SetData(const char* sectionName, T newValue)
	{
		if (!m_IsValid)
			return false;

		size_t index = 0;
		for (; index < m_Entries.size() - 1; index++)
		{
			if (strcmp(m_Entries.at(index).name, sectionName) == 0)
				break;
		}
		if (index == m_Entries.size() - 1)
		{
			PanicAlert("Section %s not found in SYSCONF", sectionName);
			return false;
		}

		*(T*)m_Entries.at(index).data = newValue;
		return true;
	}

	bool Save();
	bool SaveToFile(const char* filename);
	bool LoadFromFile(const char* filename);
};

#endif // __SYSCONF_MANAGER_h__
