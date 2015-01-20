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

CEXIAgp::CEXIAgp(int index) :
	m_slot(index)
{
	// Create the ROM
	m_pHashArray = (u8*)AllocateMemoryPages(HASH_SIZE);
	m_rom_size = 0;

	LoadRom();

	m_address = 0;
	m_rom_hash_loaded = false;
}

CEXIAgp::~CEXIAgp()
{
	m_pROM = nullptr;
	m_pHashArray = nullptr;
}

void CEXIAgp::DoHash(u8* data, u32 size)
{
	for (u32 it = 0; it < size; it++)
	{
		m_hash = m_hash ^ data[it];
		m_hash = m_pHashArray[m_hash];
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
	INFO_LOG(EXPANSIONINTERFACE, "Loaded gba rom: %s card: %d", gbapath.c_str(), m_slot);
	LoadFileToEEPROM(gbapath + ".sav");
	INFO_LOG(EXPANSIONINTERFACE, "Loaded gba sav: %s card: %d", gbapath.c_str(), m_slot);
}

void CEXIAgp::LoadFileToROM(std::string filename)
{
	File::IOFile pStream(filename, "rb");
	if (pStream)
	{
		u64 filesize = pStream.GetSize();
		m_rom_size = filesize & 0xFFFFFFFF;
		m_rom_mask = (m_rom_size - 1);

		m_pROM = (u8*)AllocateMemoryPages(m_rom_size);

		pStream.ReadBytes(m_pROM, filesize);
	}
	else
	{
		// dummy rom data
		m_pROM = (u8*)AllocateMemoryPages(0x2000);
	}
}

void CEXIAgp::LoadFileToEEPROM(std::string filename)
{
	File::IOFile pStream(filename, "rb");
	if (pStream)
	{
		u64 filesize = pStream.GetSize();
		m_eeprom_size = filesize & 0xFFFFFFFF;
		m_eeprom_mask = (m_eeprom_size - 1);

		m_pEEPROM = (u8*)AllocateMemoryPages(m_eeprom_size);

		pStream.ReadBytes(m_pEEPROM, filesize);
	}
}

void CEXIAgp::LoadHash()
{
	if (!m_rom_hash_loaded && m_rom_size > 0)
	{
		for (int i = 0; i < 0x100; i++)
		{
			m_pHashArray[i] = Memory::ReadUnchecked_U8(0x0017e908 + i);
		}
		m_rom_hash_loaded = true;
	}
}

u32 CEXIAgp::ImmRead(u32 _uSize)
{
	// We don't really care about _uSize
	(void)_uSize;
	u32 uData = 0;
	u8 RomVal1, RomVal2, RomVal3, RomVal4;

	switch (m_currrent_cmd)
	{
	case 0xAE000000:
		uData = 0x5AAA5517; // 17 is precalculated hash
		m_currrent_cmd = 0;
		break;
	case 0xAE010000:
		uData = (m_return_pos == 0) ? 0x01020304 : 0xF0020304; // F0 is precalculated hash, 020304 is left over
		if (m_return_pos == 1)
			m_currrent_cmd = 0;
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
			RomVal1 = m_pROM[m_rw_offset++];
			RomVal2 = m_pROM[m_rw_offset++];
			LoadHash();
		}
		DoHash(&RomVal2, 1);
		DoHash(&RomVal1, 1);
		uData = (RomVal2 << 24) | (RomVal1 << 16) | (m_hash << 8);
		m_currrent_cmd = 0;
		break;
	case 0xAE030000:
		if (_uSize == 1)
		{
			uData = 0xFF000000;
			m_currrent_cmd = 0;
		}
		else
		{
			RomVal1 = m_pROM[m_rw_offset++];
			RomVal2 = m_pROM[m_rw_offset++];
			RomVal3 = m_pROM[m_rw_offset++];
			RomVal4 = m_pROM[m_rw_offset++];
			LoadHash();
			DoHash(&RomVal2, 1);
			DoHash(&RomVal1, 1);
			DoHash(&RomVal4, 1);
			DoHash(&RomVal3, 1);
			uData = (RomVal2 << 24) | (RomVal1 << 16) | (RomVal4 << 8) | (RomVal3);
		}
		break;
	case 0xAE0B0000:
		RomVal1 = m_eeprom_pos < 4 ? 0xA : (((u64*)m_pEEPROM)[(m_eeprom_cmd >> 1) & 0x3F] >> (m_eeprom_pos - 4)) & 0x1;
		RomVal2 = 0;
		DoHash(&RomVal2, 1);
		DoHash(&RomVal1, 1);
		uData = (RomVal2 << 24) | (RomVal1 << 16) | (m_hash << 8);
		m_eeprom_pos++;
		m_currrent_cmd = 0;
		break;
	case 0xAE0C0000:
		uData = m_hash << 24;
		m_currrent_cmd = 0;
		break;
	default:
		uData = 0x0;
		m_currrent_cmd = 0;
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
	switch (m_currrent_cmd)
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
				((u64*)(m_pEEPROM))[(m_eeprom_cmd >> 1) & 0x3F] = m_eeprom_data;
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
		m_currrent_cmd = _uData;
		m_return_pos = 0;
		m_hash = 0xFF;
		HashCmd = (_uData & 0x00FF0000) >> 16;
		DoHash(&HashCmd, 1);
		break;
	}
}

void CEXIAgp::DoState(PointerWrap &p)
{
	p.Do(m_position);
	p.Do(m_address);
	p.Do(m_rw_offset);
	p.Do(m_hash);
}
