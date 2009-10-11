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

#include "Timer.h"

#include "EXI_DeviceIPL.h"
#include "../Core.h"
#include "../ConfigManager.h"
#include "MemoryUtil.h"
#include "FileUtil.h"

// english
SRAM sram_dump = {{
	0x04, 0x6B, 0xFB, 0x91, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0x40,
	0x05, 0x00, 0x00, 0x2C, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xD2, 0x2B, 0x29, 0xD5, 0xC7, 0xAA, 0x12, 0xCB,
	0x21, 0x27, 0xD1, 0x53, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x86, 0x00, 0xFF, 0x4A, 0x00, 0x00, 0x00, 0x00
}};

// german
SRAM sram_dump_german = {{ 
	0x1F, 0x66, 0xE0, 0x96, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x04, 0xEA, 0x19, 0x40,
	0x00, 0x00, 0x01, 0x3C, 0x12, 0xD5, 0xEA, 0xD3,
	0x00, 0xFA, 0x2D, 0x33, 0x13, 0x41, 0x26, 0x03,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x84, 0xFF, 0x00, 0x00, 0x00, 0x00
}};


// We should provide an option to choose from the above, or figure out the checksum (the algo in yagcd seems wrong)
// so that people can change default language.

static const char iplverPAL[0x100] = "(C) 1999-2001 Nintendo.  All rights reserved."
									 "(C) 1999 ArtX Inc.  All rights reserved."
									 "PAL  Revision 1.0  ";

static const char iplverNTSC[0x100] = "(C) 1999-2001 Nintendo.  All rights reserved."
									 "(C) 1999 ArtX Inc.  All rights reserved.";

// bootrom descrambler reversed by segher
// Copyright 2008 Segher Boessenkool <segher@kernel.crashing.org>
void CEXIIPL::Descrambler(u8* data, u32 size)
{
	u8 acc = 0;
	u8 nacc = 0;

	u16 t = 0x2953;
	u16 u = 0xd9c2;
	u16 v = 0x3ff1;

	u8 x = 1;

	for (u32 it = 0; it < size;)
	{
		int t0 = t & 1;
		int t1 = (t >> 1) & 1;
		int u0 = u & 1;
		int u1 = (u >> 1) & 1;
		int v0 = v & 1;

		x ^= t1 ^ v0;
		x ^= (u0 | u1);
		x ^= (t0 ^ u1 ^ v0) & (t0 ^ u0);

		if (t0 == u0)
		{
			v >>= 1;
			if (v0)
				v ^= 0xb3d0;
		}

		if (t0 == 0)
		{
			u >>= 1;
			if (u0)
				u ^= 0xfb10;
		}

		t >>= 1;
		if (t0)
			t ^= 0xa740;

		nacc++;
		acc = 2*acc + x;
		if (nacc == 8)
		{
			data[it++] ^= acc;
			nacc = 0;
		}
	}
}

CEXIIPL::CEXIIPL() :
	m_uPosition(0),
	m_uAddress(0),
	m_uRWOffset(0),
	m_count(0),
	m_FontsLoaded(false)
{
	// Determine region
	m_bNTSC = SConfig::GetInstance().m_LocalCoreStartupParameter.bNTSC;

	// Create the IPL
	m_pIPL = (u8*)AllocateMemoryPages(ROM_SIZE);
	
	if (SConfig::GetInstance().m_LocalCoreStartupParameter.bHLE_BS2)
	{
		// Copy header
		memcpy(m_pIPL, m_bNTSC ? iplverNTSC : iplverPAL, sizeof(m_bNTSC ? iplverNTSC : iplverPAL));

		// Load fonts
		LoadFileToIPL((File::GetSysDirectory() + GC_SYS_DIR + DIR_SEP + FONT_SJIS), 0x1aff00);
		LoadFileToIPL((File::GetSysDirectory() + GC_SYS_DIR + DIR_SEP + FONT_ANSI), 0x1fcf00);
	}
	else
	{
		// Load whole ROM dump
		LoadFileToIPL(SConfig::GetInstance().m_LocalCoreStartupParameter.m_strBootROM, 0);
		// Descramble the encrypted section (contains BS1 and BS2)
		Descrambler(m_pIPL + 0x100, 0x1aff00);
		INFO_LOG(BOOT, "Loaded bootrom: %s", m_pIPL); // yay for null-terminated strings ;p
	}

	// Clear RTC
	memset(m_RTC, 0, sizeof(m_RTC));

	// SRAM
	FILE *file = fopen(SConfig::GetInstance().m_LocalCoreStartupParameter.m_strSRAM.c_str(), "rb");
    if (file != NULL)
    {
        fread(&m_SRAM, 1, 64, file);
        fclose(file);
    }
    else
    {
		m_SRAM = sram_dump;
    }
    // We Overwrite language selection here since it's possible on the GC to change the language as you please
	m_SRAM.syssram.lang = SConfig::GetInstance().m_LocalCoreStartupParameter.SelectedLanguage;

	WriteProtectMemory(m_pIPL, ROM_SIZE);
	m_uAddress = 0;		
}

CEXIIPL::~CEXIIPL()
{
	if (m_count > 0)
	{
		m_szBuffer[m_count] = 0x00;
	}

	if (m_pIPL != NULL)
	{
		FreeMemoryPages(m_pIPL, ROM_SIZE);
		m_pIPL = NULL;
	}

    // SRAM
    FILE *file = fopen(SConfig::GetInstance().m_LocalCoreStartupParameter.m_strSRAM.c_str(), "wb");
    if (file)
    {
        fwrite(&m_SRAM, 1, 64, file);
        fclose(file);
    }
}
void CEXIIPL::DoState(PointerWrap &p)
{
	p.DoArray(m_RTC, 4);
}

void CEXIIPL::LoadFileToIPL(std::string filename, u32 offset)
{
	FILE* pStream = fopen(filename.c_str(), "rb");
	if (pStream != NULL)
	{
		fseek(pStream, 0, SEEK_END);
		size_t filesize = (size_t)ftell(pStream);
		rewind(pStream);

		fread(m_pIPL + offset, 1, filesize, pStream);
		fclose(pStream);

		m_FontsLoaded = true;
	}
}

void CEXIIPL::SetCS(int _iCS)
{
	if (_iCS)
	{
		// cs transition to high
		m_uPosition = 0;
	}
}

bool CEXIIPL::IsPresent()
{
	return true;
}

void CEXIIPL::TransferByte(u8& _uByte)
{
	// The first 4 bytes must be the address
	// If we haven't read it, do it now
	if (m_uPosition < 4)
	{
		m_uAddress <<= 8;
		m_uAddress |= _uByte;
		m_uRWOffset = 0;
		_uByte = 0xFF;

		// Check if the command is complete
		if (m_uPosition == 3)
		{
			// Get the time ... 
            u32 GCTime = CEXIIPL::GetGCTime();
			u8* pGCTime = (u8*)&GCTime;
			for (int i=0; i<4; i++)
			{
				m_RTC[i] = pGCTime[i^3];
			}	

#if MAX_LOGLEVEL >= INFO_LEVEL
			
			if ((m_uAddress & 0xF0000000) == 0xb0000000) 
			{
				INFO_LOG(EXPANSIONINTERFACE, "EXI IPL-DEV: WII something");
			}
			else if ((m_uAddress & 0xF0000000) == 0x30000000) 
			{
				// wii stuff perhaps wii SRAM?
				INFO_LOG(EXPANSIONINTERFACE, "EXI IPL-DEV: WII something (perhaps SRAM?)");
			}
			else if ((m_uAddress & 0x60000000) == 0)
			{
				INFO_LOG(EXPANSIONINTERFACE, "EXI IPL-DEV: IPL access");
			}
			else if ((m_uAddress & 0x7FFFFF00) == 0x20000000)
			{
				INFO_LOG(EXPANSIONINTERFACE, "EXI IPL-DEV: RTC access");
			}
			else if ((m_uAddress & 0x7FFFFF00) == 0x20000100)
			{
				INFO_LOG(EXPANSIONINTERFACE, "EXI IPL-DEV: SRAM access");
			}
			else if ((m_uAddress & 0x7FFFFF00) == 0x20010000)
			{
				DEBUG_LOG(EXPANSIONINTERFACE,  "EXI IPL-DEV: UART");
			}
			else if ((m_uAddress & 0x7FFFFF00) == 0x20011300)
			{
				DEBUG_LOG(EXPANSIONINTERFACE,  "EXI IPL-DEV: UART Barnacle");
			}
			else if ((m_uAddress & 0x7FFFFF00) == 0x20010300)
			{
				DEBUG_LOG(EXPANSIONINTERFACE,  "EXI IPL-DEV: UART Other?");
			}
            else if (((m_uAddress & 0x7FFFFF00) == 0x21000000) ||
                    ((m_uAddress & 0x7FFFFF00) == 0x21000100) ||
                    ((m_uAddress & 0x7FFFFF00) == 0x21000800))
            {
                ERROR_LOG(EXPANSIONINTERFACE,  "EXI IPL-DEV: RTC flags (WII only) - not implemented");
            }
			else
			{
				//_dbg_assert_(EXPANSIONINTERFACE, 0);
				_dbg_assert_msg_(EXPANSIONINTERFACE, 0, "EXI IPL-DEV: illegal access address %08x", m_uAddress);
				ERROR_LOG(EXPANSIONINTERFACE, "EXI IPL-DEV: illegal address %08x", m_uAddress);
			}
#endif
		}
	} 
	else
	{
		// --- Encrypted ROM ---
		// atm we pre-decrypt the whole thing, see CEXIIPL ctor
		if ((m_uAddress & 0x60000000) == 0)
		{
			if ((m_uAddress & 0x80000000) == 0)
			{
				u32 position = ((m_uAddress >> 6) & ROM_MASK) + m_uRWOffset;

				// Technically we should apply descrambling here, if it's currently enabled.
				_uByte = m_pIPL[position];

				if ((position >= 0x001AFF00) && (position <= 0x001FF474) && !m_FontsLoaded)
				{
					PanicAlert("Error: Trying to access %s fonts but they are not loaded. Games may not show fonts correctly, or crash.",
						(position >= 0x001FCF00)?"ANSI":"SJIS");
					m_FontsLoaded = true; // Don't be a nag :p
				}
			}
		} 
		// --- Real Time Clock (RTC) ---
		else if ((m_uAddress & 0x7FFFFF00) == 0x20000000)
		{
			if (m_uAddress & 0x80000000)
				m_RTC[(m_uAddress & 0x03) + m_uRWOffset] = _uByte;
			else
				_uByte = m_RTC[(m_uAddress & 0x03) + m_uRWOffset];
		}
		// --- SRAM ---
		else if ((m_uAddress & 0x7FFFFF00) == 0x20000100)
		{
			if (m_uAddress & 0x80000000)
				m_SRAM.p_SRAM[(m_uAddress & 0x3F) + m_uRWOffset] = _uByte;
			else
				_uByte = m_SRAM.p_SRAM[(m_uAddress & 0x3F) + m_uRWOffset];
		}
		// --- UART ---
		else if ((m_uAddress & 0x7FFFFF00) == 0x20010000)
		{
			if (m_uAddress & 0x80000000)
			{
				m_szBuffer[m_count++] = _uByte;
				if ((m_count >= 256) || (_uByte == 0xD))
				{					
					m_szBuffer[m_count] = 0x00;
					INFO_LOG(OSREPORT, "%s", m_szBuffer);
					memset(m_szBuffer, 0, sizeof(m_szBuffer));
					m_count = 0;
				}
			}
			else
				_uByte = 0x01; // dunno
		}
		else if ((m_uAddress & 0x7FFFFF00) == 0x20011300)
		{
			INFO_LOG(OSREPORT, "UART Barnacle %x", _uByte);
		}
		else if ((m_uAddress & 0x7FFFFF00) == 0x20010300)
		{
			INFO_LOG(OSREPORT, "UART? %x", _uByte);
			_uByte = 0xff;
		}
        else if (((m_uAddress & 0x7FFFFF00) == 0x21000000) ||
                ((m_uAddress & 0x7FFFFF00) == 0x21000100) ||
                ((m_uAddress & 0x7FFFFF00) == 0x21000800))
        {
            // WII only RTC flags... afaik just the wii menu initialize it
// 			if (m_uAddress & 0x80000000)
// 				m_SRAM.p_SRAM[(m_uAddress & 0x3F) + m_uRWOffset] = _uByte;
// 			else
// 				_uByte = m_SRAM.p_SRAM[(m_uAddress & 0x3F) + m_uRWOffset];
        }
		m_uRWOffset++;
	}
	m_uPosition++;
}

u32 CEXIIPL::GetGCTime()
{
    const u32 cJanuary2000 = 0x386D42C0;  // Seconds between 1.1.1970 and 1.1.2000
	const u32 cWiiBias     = 0x0F111566;  // Seconds between 1.1.2000 and 5.1.2008 (Wii epoch)

	// (mb2): I think we can get rid of the IPL bias.
	// I know, it's another hack so I let the previous code for a while.
#if 0
    // Get SRAM bias
    u32 Bias;

    for (int i=0; i<4; i++)
    {
        ((u8*)&Bias)[i] = sram_dump[0xc + (i^3)];
    }

    // Get the time ...
    u64 ltime = Common::Timer::GetTimeSinceJan1970();
    return ((u32)ltime - cJanuary2000 - Bias);
#else
	u64 ltime = Common::Timer::GetLocalTimeSinceJan1970();
	if (Core::GetStartupParameter().bWii)
		return ((u32)ltime - cJanuary2000 - cWiiBias);
	else
		return ((u32)ltime - cJanuary2000);
#endif
}

