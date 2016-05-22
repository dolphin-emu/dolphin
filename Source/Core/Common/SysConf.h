// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/MsgHandler.h"
#include "Common/Logging/Log.h"

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
	bool GetArrayData(u8* dest, u16 destSize)
	{
		if (dest && destSize >= dataLength)
		{
			memcpy(dest, data, dataLength);
			return true;
		}
		return false;
	}
	bool SetArrayData(u8* buffer, u16 bufferSize)
	{
		if (buffer)
		{
			memcpy(data, buffer, std::min<u16>(bufferSize, dataLength));
			return true;
		}
		return false;
	}
};

class SysConf
{
public:
	SysConf();
	~SysConf();

	bool IsValid() { return m_IsValid; }

	template<class T>
	T GetData(const char* sectionName)
	{
		if (!m_IsValid)
		{
			PanicAlertT("Trying to read from invalid SYSCONF");
			return 0;
		}

		std::vector<SSysConfEntry>::iterator index = m_Entries.begin();
		for (; index < m_Entries.end() - 1; ++index)
		{
			if (strcmp(index->name, sectionName) == 0)
				break;
		}
		if (index == m_Entries.end() - 1)
		{
			PanicAlertT("Section %s not found in SYSCONF", sectionName);
			return 0;
		}

		return index->GetData<T>();
	}

	bool GetArrayData(const char* sectionName, u8* dest, u16 destSize)
	{
		if (!m_IsValid)
		{
			PanicAlertT("Trying to read from invalid SYSCONF");
			return 0;
		}

		std::vector<SSysConfEntry>::iterator index = m_Entries.begin();
		for (; index < m_Entries.end() - 1; ++index)
		{
			if (strcmp(index->name, sectionName) == 0)
				break;
		}
		if (index == m_Entries.end() - 1)
		{
			PanicAlertT("Section %s not found in SYSCONF", sectionName);
			return 0;
		}

		return index->GetArrayData(dest, destSize);
	}

	bool SetArrayData(const char* sectionName, u8* buffer, u16 bufferSize)
	{
		if (!m_IsValid)
			return false;

		std::vector<SSysConfEntry>::iterator index = m_Entries.begin();
		for (; index < m_Entries.end() - 1; ++index)
		{
			if (strcmp(index->name, sectionName) == 0)
				break;
		}
		if (index == m_Entries.end() - 1)
		{
			PanicAlertT("Section %s not found in SYSCONF", sectionName);
			return false;
		}

		return index->SetArrayData(buffer, bufferSize);
	}

	template<class T>
	bool SetData(const char* sectionName, T newValue)
	{
		if (!m_IsValid)
			return false;

		std::vector<SSysConfEntry>::iterator index = m_Entries.begin();
		for (; index < m_Entries.end() - 1; ++index)
		{
			if (strcmp(index->name, sectionName) == 0)
				break;
		}
		if (index == m_Entries.end() - 1)
		{
			PanicAlertT("Section %s not found in SYSCONF", sectionName);
			return false;
		}

		*(T*)index->data = newValue;
		return true;
	}

	bool Save();
	bool SaveToFile(const std::string& filename);
	bool LoadFromFile(const std::string& filename);
	bool Reload();
	// This function is used when the NAND root is changed
	void UpdateLocation();

private:
	bool LoadFromFileInternal(FILE* fh);
	void GenerateSysConf();
	void Clear();

	std::string m_Filename;
	std::string m_FilenameDefault;
	std::vector<SSysConfEntry> m_Entries;
	bool m_IsValid;
};
