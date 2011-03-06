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
	m_FilenameDefault = File::GetUserPath(F_WIISYSCONF_IDX);
	m_IsValid = LoadFromFile(m_FilenameDefault.c_str());
}

SysConf::~SysConf()
{
	if (!m_IsValid)
		return;

	Save();
	Clear();
}

void SysConf::Clear()
{
	for (std::vector<SSysConfEntry>::const_iterator i = m_Entries.begin();
			i < m_Entries.end() - 1; i++)
		delete [] i->data;
	m_Entries.clear();
}

bool SysConf::LoadFromFile(const char *filename)
{
	// Basic check
	u64 size = File::GetSize(filename);
	if (size != SYSCONF_SIZE)
	{
		if (AskYesNoT("Your SYSCONF file is the wrong size.\nIt should be 0x%04x (but is 0x%04llx)\nDo you want to generate a new one?", 
					SYSCONF_SIZE, size))
		{
			GenerateSysConf();
			return true;
		}
		else
			return false;
	}
	FILE* f = fopen(filename, "rb");

	if (f == NULL)
		return false;
	bool result = LoadFromFileInternal(f);
	if (result)
		m_Filename = filename;
	fclose(f);
	return result;
}

bool SysConf::LoadFromFileInternal(FILE *f)
{
	// Fill in infos
	SSysConfHeader s_Header;
	if (fread(&s_Header.version, sizeof(s_Header.version), 1, f) != 1) return false;
	if (fread(&s_Header.numEntries, sizeof(s_Header.numEntries), 1, f) != 1) return false;
	s_Header.numEntries = Common::swap16(s_Header.numEntries) + 1;

	for (u16 index = 0; index < s_Header.numEntries; index++)
	{
		SSysConfEntry tmpEntry;
		if (fread(&tmpEntry.offset, sizeof(tmpEntry.offset), 1, f) != 1) return false;
		tmpEntry.offset = Common::swap16(tmpEntry.offset);
		m_Entries.push_back(tmpEntry);
	}

	// Last offset is an invalid entry. We ignore it throughout this class
	for (std::vector<SSysConfEntry>::iterator i = m_Entries.begin();
			i < m_Entries.end() - 1; i++)
	{
		SSysConfEntry& curEntry = *i;
		if (fseeko(f, curEntry.offset, SEEK_SET) != 0) return false;

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
			PanicAlertT("Unknown entry type %i in SYSCONF (%s@%x)!",
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

	return true;
}

// Returns the size of the item in file
unsigned int create_item(SSysConfEntry &item, SysconfType type, const std::string &name,
		const int data_length, unsigned int offset)
{
	item.offset = offset;
	item.type = type;
	item.nameLength = name.length();
	strncpy(item.name, name.c_str(), 32);
	item.dataLength = data_length;
	item.data = new u8[data_length];
	memset(item.data, 0, data_length);
	switch (type)
	{
		case Type_BigArray:
			// size of description + name length + size of dataLength + data length + null
			return 1 + item.nameLength + 2 + item.dataLength + 1;
		case Type_SmallArray:
			// size of description + name length + size of dataLength + data length + null
			return 1 + item.nameLength + 1 + item.dataLength + 1;
		case Type_Byte:
		case Type_Bool:
		case Type_Short:
		case Type_Long:
			// size of description + name length + data length
			return 1 + item.nameLength + item.dataLength;
		default:
			return 0;
	}
}

void SysConf::GenerateSysConf()
{
	SSysConfHeader s_Header;
	strncpy(s_Header.version, "SCv0", 4);
	s_Header.numEntries = Common::swap16(28 - 1);

	SSysConfEntry items[27];
	memset(items, 0, sizeof(SSysConfEntry) * 27);

	// version length + size of numEntries + 28 * size of offset
	unsigned int current_offset = 4 + 2 + 28 * 2;

	// BT.DINF
	current_offset += create_item(items[0], Type_BigArray, "BT.DINF", 0x460, current_offset);
	items[0].data[0] = 4;
	for (unsigned int i = 0; i < 4; ++i)
	{
		const u8 bt_addr[6] = {i, 0x00, 0x79, 0x19, 0x02, 0x11};
		memcpy(&items[0].data[1 + 70 * i], bt_addr, sizeof(bt_addr));
		memcpy(&items[0].data[7 + 70 * i], "Nintendo RVL-CNT-01", 19);
	}

	// BT.SENS
	current_offset += create_item(items[1], Type_Long, "BT.SENS", 4, current_offset);
	items[1].data[3] = 0x03;

	// IPL.NIK
	current_offset += create_item(items[2], Type_SmallArray, "IPL.NIK", 0x15, current_offset);
	const u8 console_nick[14] = {0, 'd', 0, 'o', 0, 'l', 0, 'p', 0, 'h', 0, 'i', 0, 'n'};
	memcpy(items[2].data, console_nick, 14);

	// IPL.AR
	current_offset += create_item(items[3], Type_Byte, "IPL.AR", 1, current_offset);
	items[3].data[0] = 0x01;

	// BT.BAR
	current_offset += create_item(items[4], Type_Byte, "BT.BAR", 1, current_offset);
	items[4].data[0] = 0x01;

	// IPL.SSV
	current_offset += create_item(items[5], Type_Byte, "IPL.SSV", 1, current_offset);

	// IPL.LNG
	current_offset += create_item(items[6], Type_Byte, "IPL.LNG", 1, current_offset);
	items[6].data[0] = 0x01;

	// IPL.SADR
	current_offset += create_item(items[7], Type_BigArray, "IPL.SADR", 0x1007, current_offset);
	items[7].data[0] = 0x6c;

	// IPL.CB
	current_offset += create_item(items[8], Type_Long, "IPL.CB", 4, current_offset);
	items[8].data[0] = 0x0f; items[8].data[1] = 0x11;
	items[8].data[2] = 0x14; items[8].data[3] = 0xa6;

	// BT.SPKV
	current_offset += create_item(items[9], Type_Byte, "BT.SPKV", 1, current_offset);
	items[9].data[0] = 0x58;

	// IPL.PC
	current_offset += create_item(items[10], Type_SmallArray, "IPL.PC", 0x49, current_offset);
	items[10].data[1] = 0x04; items[10].data[2] = 0x14;

	// NET.CTPC
	current_offset += create_item(items[11], Type_Long, "NET.CTPC", 4, current_offset);

	// WWW.RST
	current_offset += create_item(items[12], Type_Bool, "WWW.RST", 1, current_offset);

	// BT.CDIF
	current_offset += create_item(items[13], Type_BigArray, "BT.CDIF", 0x204, current_offset);

	// IPL.INC
	current_offset += create_item(items[14], Type_Long, "IPL.INC", 4, current_offset);
	items[14].data[3] = 0x08;

	// IPL.FRC
	current_offset += create_item(items[15], Type_Long, "IPL.FRC", 4, current_offset);
	items[15].data[3] = 0x28;

	// IPL.CD
	current_offset += create_item(items[16], Type_Bool, "IPL.CD", 1, current_offset);
	items[16].data[0] = 0x01;

	// IPL.CD2
	current_offset += create_item(items[17], Type_Bool, "IPL.CD2", 1, current_offset);
	items[17].data[0] = 0x01;

	// IPL.UPT
	current_offset += create_item(items[18], Type_Byte, "IPL.UPT", 1, current_offset);
	items[18].data[0] = 0x02;

	// IPL.PGS
	current_offset += create_item(items[19], Type_Byte, "IPL.PGS", 1, current_offset);

	// IPL.E60
	current_offset += create_item(items[20], Type_Byte, "IPL.E60", 1, current_offset);
	items[20].data[0] = 0x01;

	// IPL.DH
	current_offset += create_item(items[21], Type_Byte, "IPL.DH", 1, current_offset);

	// NET.WCFG
	current_offset += create_item(items[22], Type_Long, "NET.WCFG", 4, current_offset);
	items[22].data[3] = 0x01;

	// IPL.IDL
	current_offset += create_item(items[23], Type_SmallArray, "IPL.IDL", 1, current_offset);
	items[23].data[0] = 0x01;

	// IPL.EULA
	current_offset += create_item(items[24], Type_Bool, "IPL.EULA", 1, current_offset);
	items[24].data[0] = 0x01;

	// BT.MOT
	current_offset += create_item(items[25], Type_Byte, "BT.MOT", 1, current_offset);
	items[25].data[0] = 0x01;

	// MPLS.MOVIE
	current_offset += create_item(items[26], Type_Bool, "MPLS.MOVIE", 1, current_offset);
	items[26].data[0] = 0x01;


	for (int i = 0; i < 27; i++)
		m_Entries.push_back(items[i]);

	File::CreateFullPath(m_FilenameDefault);
	FILE *g = fopen(m_FilenameDefault.c_str(), "wb");

	// Write the header and item offsets
	fwrite(&s_Header.version, sizeof(s_Header.version), 1, g);
	fwrite(&s_Header.numEntries, sizeof(u16), 1, g);
	for (int i = 0; i < 27; ++i)
	{
		u16 tmp_offset = Common::swap16(items[i].offset);
		fwrite(&tmp_offset, 2, 1, g);
	}
	const u16 end_data_offset = Common::swap16(current_offset);
	fwrite(&end_data_offset, 2, 1, g);
	
	// Write the items
	const u8 null_byte = 0;
	for (int i = 0; i < 27; ++i)
	{
		u8 description = (items[i].type << 5) | (items[i].nameLength - 1);
		fwrite(&description, sizeof(description), 1, g);
		fwrite(&items[i].name, items[i].nameLength, 1, g);
		switch (items[i].type)
		{
			case Type_BigArray:
				{
					u16 tmpDataLength = Common::swap16(items[i].dataLength);
					fwrite(&tmpDataLength, 2, 1, g);
					fwrite(items[i].data, items[i].dataLength, 1, g);
					fwrite(&null_byte, 1, 1, g);
				}
				break;
			case Type_SmallArray:
				fwrite(&items[i].dataLength, 1, 1, g);
				fwrite(items[i].data, items[i].dataLength, 1, g);
				fwrite(&null_byte, 1, 1, g);
				break;
			default:
				fwrite(items[i].data, items[i].dataLength, 1, g);
				break;
		}
	}

	// Pad file to the correct size
	const u64 cur_size = File::GetSize(g); 
	for (unsigned int i = 0; i < 16380 - cur_size; ++i)
		fwrite(&null_byte, 1, 1, g);

	// Write the footer
	const char footer[5] = "SCed";
	fwrite(&footer, 4, 1, g);

	fclose(g);
	m_Filename = m_FilenameDefault;
}

bool SysConf::SaveToFile(const char *filename)
{
	FILE *f = fopen(filename, "r+b");

	if (f == NULL)
		return false;

	for (std::vector<SSysConfEntry>::iterator i = m_Entries.begin();
			i < m_Entries.end() - 1; i++)
	{
		// Seek to after the name of this entry
		if (fseeko(f, i->offset + i->nameLength + 1, SEEK_SET) != 0) return false;
		// We may have to write array length value...
		if (i->type == Type_BigArray)
		{
			u16 tmpDataLength = Common::swap16(i->dataLength);
			if (fwrite(&tmpDataLength, 2, 1, f) != 1) return false;
		}
		else if (i->type == Type_SmallArray)
		{
			if (fwrite(&i->dataLength, 1, 1, f) != 1) return false;
		}
		// Now write the actual data
		if (fwrite(i->data, i->dataLength, 1, f) != 1) return false;
	}

	fclose(f);
	return true;
}

bool SysConf::Save()
{
	if (!m_IsValid)
		return false;
	return SaveToFile(m_Filename.c_str());
}

bool SysConf::Reload()
{
	if (m_IsValid)
		Clear();

	std::string& filename = m_Filename.empty() ? m_FilenameDefault : m_Filename;

	m_IsValid = LoadFromFile(filename.c_str());
	return m_IsValid;
}
