// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cinttypes>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "Common/CommonFuncs.h"
#include "Common/CommonPaths.h"
#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/SysConf.h"
#include "Common/Logging/Log.h"

SysConf::SysConf()
	: m_IsValid(false)
{
	UpdateLocation();
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
	m_Entries.clear();
}

bool SysConf::LoadFromFile(const std::string& filename)
{
	if (m_IsValid)
		Clear();
	m_IsValid = false;

	// Basic check
	if (!File::Exists(filename))
	{
		File::CreateFullPath(filename);
		GenerateSysConf();
		return true;
	}

	u64 size = File::GetSize(filename);
	if (size != SYSCONF_SIZE)
	{
		if (AskYesNoT("Your SYSCONF file is the wrong size.\nIt should be 0x%04x (but is 0x%04" PRIx64 ")\nDo you want to generate a new one?",
					SYSCONF_SIZE, size))
		{
			GenerateSysConf();
			return true;
		}
		else
		{
			return false;
		}
	}

	File::IOFile f(filename, "rb");
	if (f.IsOpen())
	{
		if (LoadFromFileInternal(f.ReleaseHandle()))
		{
			m_Filename = filename;
			m_IsValid = true;
			return true;
		}
	}

	return false;
}

bool SysConf::LoadFromFileInternal(FILE* fh)
{
	File::IOFile f(fh);
	// Fill in infos
	SSysConfHeader s_Header;
	f.ReadArray(s_Header.version, 4);
	f.ReadArray(&s_Header.numEntries, 1);
	s_Header.numEntries = Common::swap16(s_Header.numEntries) + 1;

	for (u16 index = 0; index < s_Header.numEntries; index++)
	{
		SSysConfEntry tmpEntry;
		f.ReadArray(&tmpEntry.offset, 1);
		tmpEntry.offset = Common::swap16(tmpEntry.offset);
		m_Entries.push_back(std::move(tmpEntry));
	}

	// Last offset is an invalid entry. We ignore it throughout this class
	for (auto i = m_Entries.begin(); i < m_Entries.end() - 1; ++i)
	{
		SSysConfEntry& curEntry = *i;
		f.Seek(curEntry.offset, SEEK_SET);

		u8 description = 0;
		f.ReadArray(&description, 1);
		// Data type
		curEntry.type = (SysconfType)((description & 0xe0) >> 5);
		// Length of name in bytes - 1
		curEntry.nameLength = (description & 0x1f) + 1;
		// Name
		f.ReadArray(curEntry.name, curEntry.nameLength);
		curEntry.name[curEntry.nameLength] = '\0';
		// Get length of data
		curEntry.data = nullptr;
		curEntry.dataLength = 0;
		switch (curEntry.type)
		{
		case Type_BigArray:
			f.ReadArray(&curEntry.dataLength, 1);
			curEntry.dataLength = Common::swap16(curEntry.dataLength);
			break;

		case Type_SmallArray:
			{
			u8 dlength = 0;
			f.ReadBytes(&dlength, 1);
			curEntry.dataLength = dlength;
			break;
			}

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
			break;
		}
		// Fill in the actual data
		if (curEntry.dataLength)
		{
			curEntry.data = std::make_unique<u8[]>(curEntry.dataLength);
			f.ReadArray(curEntry.data.get(), curEntry.dataLength);
		}
	}

	return f.IsGood();
}

// Returns the size of the item in file
static unsigned int create_item(SSysConfEntry& item, SysconfType type, const std::string& name,
		const int data_length, unsigned int offset)
{
	item.offset = offset;
	item.type = type;
	item.nameLength = (u8)(name.length());
	strncpy(item.name, name.c_str(), 32);
	item.dataLength = data_length;
	item.data = std::make_unique <u8[]>(data_length);
	memset(item.data.get(), 0, data_length);
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

	// version length + size of numEntries + 28 * size of offset
	unsigned int current_offset = 4 + 2 + 28 * 2;

	// BT.DINF
	current_offset += create_item(items[0], Type_BigArray, "BT.DINF", 0x460, current_offset);
	items[0].data[0] = 4;
	for (u8 i = 0; i < 4; ++i)
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
	memcpy(items[2].data.get(), console_nick, 14);

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
	items[7].data[0] = 0x6c; //(Switzerland) TODO should this default be changed?

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


	for (SSysConfEntry& item : items)
		m_Entries.push_back(std::move(item));

	File::CreateFullPath(m_FilenameDefault);
	File::IOFile g(m_FilenameDefault, "wb");

	// Write the header and item offsets
	g.WriteBytes(&s_Header.version, sizeof(s_Header.version));
	g.WriteBytes(&s_Header.numEntries, sizeof(u16));
	for (int i = 0; i != 27; ++i)
	{
		const u16 tmp_offset = Common::swap16(m_Entries[i].offset);
		g.WriteBytes(&tmp_offset, 2);
	}
	const u16 end_data_offset = Common::swap16(current_offset);
	g.WriteBytes(&end_data_offset, 2);

	// Write the items
	const u8 null_byte = 0;
	for (int i = 0; i != 27; ++i)
	{
		u8 description = (m_Entries[i].type << 5) | (m_Entries[i].nameLength - 1);
		g.WriteBytes(&description, sizeof(description));
		g.WriteBytes(&m_Entries[i].name, m_Entries[i].nameLength);
		switch (m_Entries[i].type)
		{
			case Type_BigArray:
				{
					const u16 tmpDataLength = Common::swap16(m_Entries[i].dataLength);
					g.WriteBytes(&tmpDataLength, 2);
					g.WriteBytes(m_Entries[i].data.get(), m_Entries[i].dataLength);
					g.WriteBytes(&null_byte, 1);
				}
				break;

			case Type_SmallArray:
				g.WriteBytes(&m_Entries[i].dataLength, 1);
				g.WriteBytes(m_Entries[i].data.get(), m_Entries[i].dataLength);
				g.WriteBytes(&null_byte, 1);
				break;

			default:
				g.WriteBytes(m_Entries[i].data.get(), m_Entries[i].dataLength);
				break;
		}
	}

	// Pad file to the correct size
	const u64 cur_size = g.GetSize();
	for (unsigned int i = 0; i != 16380 - cur_size; ++i)
		g.WriteBytes(&null_byte, 1);

	// Write the footer
	g.WriteBytes("SCed", 4);

	m_Filename = m_FilenameDefault;
	m_IsValid = true;
}

bool SysConf::SaveToFile(const std::string& filename)
{
	File::IOFile f(filename, "r+b");

	for (auto i = m_Entries.begin(); i < m_Entries.end() - 1; ++i)
	{
		// Seek to after the name of this entry
		f.Seek(i->offset + i->nameLength + 1, SEEK_SET);

		// We may have to write array length value...
		if (i->type == Type_BigArray)
		{
			const u16 tmpDataLength = Common::swap16(i->dataLength);
			f.WriteArray(&tmpDataLength, 1);
		}
		else if (i->type == Type_SmallArray)
		{
			const u8 len = (u8)(i->dataLength);
			f.WriteArray(&len, 1);
		}

		// Now write the actual data
		f.WriteBytes(i->data.get(), i->dataLength);
	}

	return f.IsGood();
}

bool SysConf::Save()
{
	if (!m_IsValid)
		return false;

	return SaveToFile(m_Filename);
}

void SysConf::UpdateLocation()
{
	// if the old Wii User dir had a sysconf file save any settings that have been changed to it
	if (m_IsValid)
		Save();

	// Clear the old filename and set the default filename to the new user path
	// So that it can be generated if the file does not exist in the new location
	m_Filename.clear();
	// Note: We don't use the dummy Wii root here (if in use) because this is
	// all tied up with the configuration code.  In the future this should
	// probably just be synced with the other settings.
	m_FilenameDefault = File::GetUserPath(D_WIIROOT_IDX) + DIR_SEP WII_SYSCONF_DIR DIR_SEP WII_SYSCONF;
	Reload();
}

bool SysConf::Reload()
{
	std::string& filename = m_Filename.empty() ? m_FilenameDefault : m_Filename;

	LoadFromFile(filename);
	return m_IsValid;
}
