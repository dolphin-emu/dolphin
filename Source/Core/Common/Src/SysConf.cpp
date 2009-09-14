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

#include "FileUtil.h"
#include "SysConf.h"

SysConf::SysConf()
: m_IsValid(false)
{
	if (LoadFromFile(WII_SYSCONF_FILE))
		m_IsValid = true;
}

SysConf::~SysConf()
{
	if (!m_IsValid)
		return;

	Save();

	for (size_t i = 0; i < m_Entries.size() - 1; i++)
	{
		delete [] m_Entries.at(i).data;
		m_Entries.at(i).data = NULL;
	}
}

bool SysConf::LoadFromFile(const char *filename)
{
	FILE* f = fopen(filename, "rb");

	if (f == NULL)
		return false;

	// Basic check
	if (File::GetSize(filename) != SYSCONF_SIZE)
	{
		PanicAlert("Your SYSCONF file is the wrong size - should be 0x%04x", SYSCONF_SIZE);
		return false;
	}

	// Fill in infos
	if (fread(&m_Header.version, sizeof(m_Header.version), 1, f) != 1) return false;
	if (fread(&m_Header.numEntries, sizeof(m_Header.numEntries), 1, f) != 1) return false;
	m_Header.numEntries = Common::swap16(m_Header.numEntries) + 1;

	for (u16 index = 0; index < m_Header.numEntries; index++)
	{
		SSysConfEntry tmpEntry;
		if (fread(&tmpEntry.offset, sizeof(tmpEntry.offset), 1, f) != 1) return false;
		tmpEntry.offset = Common::swap16(tmpEntry.offset);
		m_Entries.push_back(tmpEntry);
	}

	// Last offset is an invalid entry. We ignore it throughout this class
	for (size_t i = 0; i < m_Entries.size() - 1; i++)
	{
		SSysConfEntry& curEntry = m_Entries.at(i);
		if (fseek(f, curEntry.offset, SEEK_SET) != 0) return false;

		u8 description = 0;
		if (fread(&description, sizeof(description), 1, f) != 1) return false;
		// Data type
		curEntry.type = (SysconfType)((description & 0xe0) >> 5);
		// Length of name in bytes - 1
		curEntry.nameLength = (description & 0x1f) + 1;
		// Name
		if (fread(&curEntry.name, curEntry.nameLength, 1, f) != 1) return false;
		curEntry.name[curEntry.nameLength] = '\0';
		// Get length of data
		curEntry.dataLength = 0;
		switch (curEntry.type)
		{
		case Type_BigArray:
			if (fread(&curEntry.dataLength, 2, 1, f) != 1) return false;
			curEntry.dataLength = Common::swap16(curEntry.dataLength);
			break;
		case Type_SmallArray:
			if (fread(&curEntry.dataLength, 1, 1, f) != 1) return false;
			break;
		case Type_Byte:
		case Type_Bool:
			curEntry.dataLength = 1;
			break;
		case Type_Short:
			curEntry.dataLength = 2;
			break;
		case Type_Long:
			curEntry.dataLength = 4;
			break;
		default:
			PanicAlert("Unknown entry type %i in SYSCONF (%s@%x)!",
				curEntry.type, curEntry.name, curEntry.offset);
			return false;
		}
		// Fill in the actual data
		if (curEntry.dataLength)
		{
			curEntry.data = new u8[curEntry.dataLength];
			if (fread(curEntry.data, curEntry.dataLength, 1, f) != 1) return false;
		}
	}

	// OK!, done!
	m_Filename = filename;
	fclose(f);
	return true;
}

bool SysConf::SaveToFile(const char *filename)
{
	FILE *f = fopen(filename, "r+b");

	if (f == NULL)
		return false;

	for (size_t i = 0; i < m_Entries.size() - 1; i++)
	{
		// Seek to after the name of this entry
		if (fseek(f, m_Entries.at(i).offset + m_Entries.at(i).nameLength + 1, SEEK_SET) != 0) return false;
		// We may have to write array length value...
		if (m_Entries.at(i).type == Type_BigArray)
		{
			u16 tmpDataLength = Common::swap16(m_Entries.at(i).dataLength);
			if (fwrite(&tmpDataLength, 2, 1, f) != 1) return false;
		}
		else if (m_Entries.at(i).type == Type_SmallArray)
		{
			if (fwrite(&m_Entries.at(i).dataLength, 1, 1, f) != 1) return false;
		}
		// Now write the actual data
		if (fwrite(m_Entries.at(i).data, m_Entries.at(i).dataLength, 1, f) != 1) return false;
	}

	fclose(f);
	return true;
}

bool SysConf::Save()
{
	return SaveToFile(m_Filename.c_str());
}
