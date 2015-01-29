// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Common/FileUtil.h"
#include "Common/MemoryUtil.h"
#include "Common/StdMakeUnique.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/HW/EXI_Device.h"
#include "Core/HW/EXI_DeviceAGP.h"
#include "Core/HW/Memmap.h"

CEXIAgp::CEXIAgp(int index)
{
	m_slot = index;

	// Create the ROM
	m_rom_size = 0;

	LoadRom();

	m_address = 0;
	m_rom_hash_loaded = false;
}

CEXIAgp::~CEXIAgp()
{
	std::string path;
	std::string filename;
	std::string ext;
	std::string gbapath;
	SplitPath(m_slot == 0 ? SConfig::GetInstance().m_strGbaCartA : SConfig::GetInstance().m_strGbaCartB, &path, &filename, &ext);
	gbapath = path + filename;

	SaveFileFromEEPROM(gbapath + ".sav");
}

void CEXIAgp::DoHash(u8* data, u32 size)
{
	if (!m_rom_hash_loaded)
		LoadHash();

	for (u32 it = 0; it < size; it++)
	{
		m_hash = m_hash ^ data[it];
		m_hash = m_hash_array[m_hash];
	}
}

void CEXIAgp::LoadRom()
{
	// Load whole ROM dump
	std::string path;
	std::string filename;
	std::string ext;
	std::string gbapath;
	SplitPath(m_slot == 0 ? SConfig::GetInstance().m_strGbaCartA : SConfig::GetInstance().m_strGbaCartB, &path, &filename, &ext);
	gbapath = path + filename;
	LoadFileToROM(gbapath + ext);
	INFO_LOG(EXPANSIONINTERFACE, "Loaded GBA rom: %s card: %d", gbapath.c_str(), m_slot);
	LoadFileToEEPROM(gbapath + ".sav");
	INFO_LOG(EXPANSIONINTERFACE, "Loaded GBA sav: %s card: %d", gbapath.c_str(), m_slot);
}

void CEXIAgp::LoadFileToROM(const std::string& filename)
{
	File::IOFile pStream(filename, "rb");
	if (pStream)
	{
		u64 filesize = pStream.GetSize();
		m_rom_size = filesize & 0xFFFFFFFF;
		m_rom_mask = (m_rom_size - 1);

		m_rom.resize(m_rom_size);

		pStream.ReadBytes(m_rom.data(), filesize);
	}
	else
	{
		// dummy rom data
		m_rom.resize(0x2000);
	}
}

void CEXIAgp::LoadFileToEEPROM(const std::string& filename)
{
	File::IOFile pStream(filename, "rb");
	if (pStream)
	{
		u64 filesize = pStream.GetSize();
		m_eeprom_size = filesize & 0xFFFFFFFF;
		m_eeprom_mask = (m_eeprom_size - 1);

		m_eeprom.resize(m_eeprom_size);

		pStream.ReadBytes(m_eeprom.data(), filesize);
	}
}

void CEXIAgp::SaveFileFromEEPROM(const std::string& filename)
{
	File::IOFile pStream(filename, "wb");
	if (pStream)
	{
		pStream.WriteBytes(m_eeprom.data(), m_eeprom_size);
	}
}

void CEXIAgp::LoadHash()
{
	if (!m_rom_hash_loaded && m_rom_size > 0)
	{
		u32 hash_addr = 0;

		// Find where the hash is in memory
		for (u32 h = 0; h < Memory::REALRAM_SIZE; h += 4)
		{
			if (Memory::ReadUnchecked_U32(h) == 0x005ebce2)
				hash_addr = h;
		}

		for (int i = 0; i < 0x100; i++)
		{
			// Load the ROM hash expected by the AGP
			m_hash_array[i] = Memory::ReadUnchecked_U8(hash_addr + i);
		}

		// Verify the hash
		if (m_hash_array[HASH_SIZE - 1] == 0x35)
		{
			m_rom_hash_loaded = true;
		}
	}
}

u32 CEXIAgp::ImmRead(u32 _uSize)
{
	// We don't really care about _uSize
	(void)_uSize;
	u32 uData = 0;
	u8 RomVal1, RomVal2, RomVal3, RomVal4;

	switch (m_current_cmd)
	{
	case 0xAE000000:
		uData = 0x5AAA5517; // 17 is precalculated hash
		m_current_cmd = 0;
		break;
	case 0xAE010000:
		uData = (m_return_pos == 0) ? 0x01020304 : 0xF0020304; // F0 is precalculated hash, 020304 is left over
		if (m_return_pos == 1)
			m_current_cmd = 0;
		else
			m_return_pos = 1;
		break;
	case 0xAE020000:
		if (m_rw_offset == 0x8000000)
		{
			RomVal1 = 0x0;
			RomVal2 = 0x1;
		}
		else
		{
			RomVal1 = m_rom[m_rw_offset++];
			RomVal2 = m_rom[m_rw_offset++];
		}
		DoHash(&RomVal2, 1);
		DoHash(&RomVal1, 1);
		uData = (RomVal2 << 24) | (RomVal1 << 16) | (m_hash << 8);
		m_current_cmd = 0;
		break;
	case 0xAE030000:
		if (_uSize == 1)
		{
			uData = 0xFF000000;
			m_current_cmd = 0;
		}
		else
		{
			RomVal1 = m_rom[m_rw_offset++];
			RomVal2 = m_rom[m_rw_offset++];
			RomVal3 = m_rom[m_rw_offset++];
			RomVal4 = m_rom[m_rw_offset++];
			DoHash(&RomVal2, 1);
			DoHash(&RomVal1, 1);
			DoHash(&RomVal4, 1);
			DoHash(&RomVal3, 1);
			uData = (RomVal2 << 24) | (RomVal1 << 16) | (RomVal4 << 8) | (RomVal3);
		}
		break;
	case 0xAE0B0000:
		RomVal1 = m_eeprom_pos < 4 ? 0xA : (((u64*)m_eeprom.data())[(m_eeprom_cmd >> 1) & 0x3F] >> (m_eeprom_pos - 4)) & 0x1;
		RomVal2 = 0;
		DoHash(&RomVal2, 1);
		DoHash(&RomVal1, 1);
		uData = (RomVal2 << 24) | (RomVal1 << 16) | (m_hash << 8);
		m_eeprom_pos++;
		m_current_cmd = 0;
		break;
	case 0xAE0C0000:
		uData = m_hash << 24;
		m_current_cmd = 0;
		break;
	default:
		uData = 0x0;
		m_current_cmd = 0;
		break;
	}
	INFO_LOG(EXPANSIONINTERFACE, "AGP read %x", uData);
	return uData;
}

void CEXIAgp::ImmWrite(u32 _uData, u32 _uSize)
{
	if ((_uSize == 1) && ((_uData & 0xFF000000) == 0))
		return;

	u8 HashCmd;
	u64 Mask;
	INFO_LOG(EXPANSIONINTERFACE, "AGP command %x", _uData);
	switch (m_current_cmd)
	{
	case 0xAE020000:
	case 0xAE030000:
		m_rw_offset = ((_uData & 0xFFFFFF00) >> 7) & m_rom_mask;
		m_return_pos = 0;
		HashCmd = (_uData & 0xFF000000) >> 24;
		DoHash(&HashCmd, 1);
		HashCmd = (_uData & 0x00FF0000) >> 16;
		DoHash(&HashCmd, 1);
		HashCmd = (_uData & 0x0000FF00) >> 8;
		DoHash(&HashCmd, 1);
		break;
	case 0xAE0C0000:
		if ((m_eeprom_pos < 0x8) || (m_eeprom_pos == ((m_eeprom_cmd & EE_READ) ? 0x8 : 0x48)))
		{
			Mask = (u64)(1 << (0x8-(m_eeprom_pos > 0x8 ? 0x8 : m_eeprom_pos)));
			if ((_uData >> 16) & 0x1)
				m_eeprom_cmd |= Mask;
			else
				m_eeprom_cmd &= ~Mask;
			if (m_eeprom_pos == 0x48)
				((u64*)(m_eeprom.data()))[(m_eeprom_cmd >> 1) & 0x3F] = m_eeprom_data;
		}
		else
		{
			Mask = (u64)(1 << (0x47 - m_eeprom_pos));
			if ((_uData >> 16) & 0x1)
				m_eeprom_data |= Mask;
			else
				m_eeprom_data &= ~Mask;
		}
		m_eeprom_pos++;
		m_return_pos = 0;
		HashCmd = (_uData & 0xFF000000) >> 24;
		DoHash(&HashCmd, 1);
		HashCmd = (_uData & 0x00FF0000) >> 16;
		DoHash(&HashCmd, 1);
		break;
	case 0xAE0B0000:
		break;
	case 0xAE000000:
	case 0xAE010000:
	case 0xAE090000:
	case 0xAE0A0000:
	default:
		m_eeprom_pos = 0;
		m_current_cmd = _uData;
		m_return_pos = 0;
		m_hash = 0xFF;
		HashCmd = (_uData & 0x00FF0000) >> 16;
		DoHash(&HashCmd, 1);
		break;
	}
}

void CEXIAgp::DoState(PointerWrap &p)
{
	p.Do(m_slot);
	p.Do(m_address);
	p.Do(m_current_cmd);
	p.Do(m_eeprom);
	p.Do(m_eeprom_cmd);
	p.Do(m_eeprom_data);
	p.Do(m_eeprom_mask);
	p.Do(m_eeprom_pos);
	p.Do(m_eeprom_size);
	p.Do(m_hash);
	p.Do(m_hash_array);
	p.Do(m_position);
	p.Do(m_return_pos);
	p.Do(m_rom);
	p.Do(m_rom_mask);
	p.Do(m_rom_size);
	p.Do(m_rw_offset);
}
