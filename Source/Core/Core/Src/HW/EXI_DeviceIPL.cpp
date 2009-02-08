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

#include "Timer.h"

#include "EXI_DeviceIPL.h"
#include "../Core.h"
#include "../ConfigManager.h"

#include "MemoryUtil.h"

// english
const unsigned char sram_dump[64] = {
	0x04, 0x6B, 0xFB, 0x91, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0x40,
	0x05, 0x00, 0x00, 0x2C, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xD2, 0x2B, 0x29, 0xD5, 0xC7, 0xAA, 0x12, 0xCB,
	0x21, 0x27, 0xD1, 0x53, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x86, 0x00, 0xFF, 0x4A, 0x00, 0x00, 0x00, 0x00
};

// german
const unsigned char sram_dump_german[64] ={ 
	0x1F, 0x66, 0xE0, 0x96, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x04, 0xEA, 0x19, 0x40,
	0x00, 0x00, 0x01, 0x3C, 0x12, 0xD5, 0xEA, 0xD3,
	0x00, 0xFA, 0x2D, 0x33, 0x13, 0x41, 0x26, 0x03,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x84, 0xFF, 0x00, 0x00, 0x00, 0x00
};


// We should provide an option to choose from the above, or figure out the checksum (the algo in yagcd seems wrong)
// so that people can change default language.

static const char iplver[0x100] = "(C) 1999-2001 Nintendo.  All rights reserved."
								  "(C) 1999 ArtX Inc.  All rights reserved."
								  "PAL  Revision 1.0 ";

CEXIIPL::CEXIIPL() :
	m_uPosition(0),
	m_uAddress(0),
	m_uRWOffset(0),
	m_count(0)
{
	// Load the IPL
	m_pIPL = (u8*)AllocateMemoryPages(ROM_SIZE); 
	FILE* pStream = NULL;
	pStream = fopen(FONT_ANSI_FILE, "rb");
	if (pStream != NULL)
	{
		fseek(pStream, 0, SEEK_END);
		size_t filesize = (size_t)ftell(pStream);
		rewind(pStream);

		fread(m_pIPL + 0x001fcf00, 1, filesize, pStream);
		fclose(pStream);
	}
	else
	{
/*		This is a poor way to handle failure.  We should either not display this message unless fonts
		are actually accessed, or better yet, emulate them using a system font.  -bushing */
		// JP: But I want to see this error
		#ifndef __APPLE__
			PanicAlert("Error: failed to load '" FONT_ANSI_FILE "'. Fonts in a few games may not work, or"
				" crash the game.");
		#endif
	}

	pStream = fopen(FONT_SJIS_FILE, "rb");
	if (pStream != NULL)
	{
		fseek(pStream, 0, SEEK_END);
		size_t filesize = (size_t)ftell(pStream);
		rewind(pStream);

		fread(m_pIPL + 0x001aff00, 1, filesize, pStream);
		fclose(pStream);
	}
	else
	{
		// Heh, BIOS fonts don't really work in JAP games anyway ... we get bogus characters.
		// JP: But I want to see this error
		#ifndef __APPLE__
			PanicAlert("Error: failed to load '" FONT_SJIS_FILE "'. Fonts in a few Japanese games may"
				" not work or crash the game.");
		#endif
	}

	memcpy(m_pIPL, iplver, sizeof(iplver));

	// Clear RTC 
	memset(m_RTC, 0, sizeof(m_RTC));

	// SRAM
    pStream = fopen(SConfig::GetInstance().m_LocalCoreStartupParameter.m_strSRAM.c_str(), "rb");
    if (pStream != NULL)
    {
        fread(m_SRAM, 1, 64, pStream);
        fclose(pStream);
    }
    else
    {
        memcpy(m_SRAM, sram_dump, sizeof(m_SRAM));
    }
    // We Overwrite it here since it's possible on the GC to change the language as you please
    m_SRAM[0x12] = SConfig::GetInstance().m_LocalCoreStartupParameter.SelectedLanguage;

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
        fwrite(m_SRAM, 1, 64, file);
        fclose(file);
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

#ifdef LOGGING
			
			if ((m_uAddress & 0xF0000000) == 0xb0000000) 
			{
				LOG(EXPANSIONINTERFACE, "EXI IPL-DEV: WII something");
			}
			else if ((m_uAddress & 0xF0000000) == 0x30000000) 
			{
				// wii stuff perhaps wii SRAM?
				LOG(EXPANSIONINTERFACE, "EXI IPL-DEV: WII something (perhaps SRAM?)");
			}
			// debug only
			else if ((m_uAddress & 0x60000000) == 0)
			{
				LOGV(EXPANSIONINTERFACE, 2, "EXI IPL-DEV: IPL access");
			}
			else if ((m_uAddress & 0x7FFFFF00) == 0x20000000)
			{
				LOG(EXPANSIONINTERFACE, "EXI IPL-DEV: RTC access");
			}
			else if ((m_uAddress & 0x7FFFFF00) == 0x20000100)
			{
				LOG(EXPANSIONINTERFACE, "EXI IPL-DEV: SRAM access");
			}
			else if ((m_uAddress & 0x7FFFFF00) == 0x20010000)
			{
				LOGV(EXPANSIONINTERFACE, 3, "EXI IPL-DEV: UART");
			}
			else
			{
				_dbg_assert_(EXPANSIONINTERFACE, 0);
				LOG(EXPANSIONINTERFACE, "EXI IPL-DEV: illegal address %08x", m_uAddress);
			}
#endif
		}
	} 
	else
	{
		//
		// --- ROM ---
		//
		if ((m_uAddress & 0x60000000) == 0)
		{
			if ((m_uAddress & 0x80000000) == 0)
				_uByte = m_pIPL[((m_uAddress >> 6) & ROM_MASK) + m_uRWOffset];
		} 
		//
		// --- Real Time Clock (RTC) ---
		//
		else if ((m_uAddress & 0x7FFFFF00) == 0x20000000)
		{
			if (m_uAddress & 0x80000000)
				m_RTC[(m_uAddress & 0x03) + m_uRWOffset] = _uByte;
			else
				_uByte = m_RTC[(m_uAddress & 0x03) + m_uRWOffset];
		} 
		//
		// --- SRAM ---
		//
		else if ((m_uAddress & 0x7FFFFF00) == 0x20000100)
		{
			if (m_uAddress & 0x80000000)
				m_SRAM[(m_uAddress & 0x3F) + m_uRWOffset] = _uByte;
			else
				_uByte = m_SRAM[(m_uAddress & 0x3F) + m_uRWOffset];
		}
		//
		// --- UART ---
		//
		else if ((m_uAddress & 0x7FFFFF00) == 0x20010000)
		{
			if (m_uAddress & 0x80000000)
			{
				m_szBuffer[m_count++] = _uByte;
				if ((m_count >= 256) || (_uByte == 0xD))
				{					
					m_szBuffer[m_count] = 0x00;
					LOG(OSREPORT, "%s", m_szBuffer);
					memset(m_szBuffer, 0, sizeof(m_szBuffer));
					m_count = 0;
				}
			}
			else
				_uByte = 0x01; // dunno
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

