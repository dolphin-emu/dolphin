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

#include "Boot.h"
#include "../PowerPC/PowerPC.h"
#include "../HLE/HLE.h"
#include "../HW/Memmap.h"
#include "../ConfigManager.h"
#include "Blob.h"
#include "MappedFile.h"
#include "Boot_DOL.h"
#include "Boot_WiiWAD.h"
#include "AES/aes.h"
#include "MathUtil.h"

class CBlobBigEndianReader
{
public:
	CBlobBigEndianReader(DiscIO::IBlobReader& _rReader) : m_rReader(_rReader) {}

	u32 Read32(u64 _Offset)
	{
		u32 Temp;
		m_rReader.Read(_Offset, 4, (u8*)&Temp);
		return(Common::swap32(Temp));
	}

private:
	DiscIO::IBlobReader& m_rReader;
};

std::vector<STileMetaContent> m_TileMetaContent;
u16 m_BootIndex = -1;

void AESDecode(u8* _pKey, u8* _IV, u8* _pSrc, u32 _Size, u8* _pDest)
{
    AES_KEY AESKey;

    AES_set_decrypt_key(_pKey, 128, &AESKey);
    AES_cbc_encrypt(_pSrc, _pDest, _Size, &AESKey, _IV, AES_DECRYPT);
}

u8* CreateWADEntry(DiscIO::IBlobReader& _rReader, u32 _Size, u64 _Offset)
{
    if (_Size > 0)
    {
        u8* pTmpBuffer = new u8[_Size];
        _dbg_assert_msg_(BOOT, pTmpBuffer!=0, "WiiWAD: Cant allocate memory for WAD entry");

        if (!_rReader.Read(_Offset, _Size, pTmpBuffer))
        {
            PanicAlert("WiiWAD: Could not read from file");
        }
        return pTmpBuffer;
    }
	return NULL;
}

void GetKeyFromTicket(u8* pTicket, u8* pTicketKey)
{
	u8 CommonKey[16];	
	FILE* pMasterKeyFile = fopen(WII_MASTERKEY_FILE, "rb");
    _dbg_assert_msg_(BOOT, pMasterKeyFile!=0x0, "WiiWAD: Cant open MasterKeyFile for WII");

	if (pMasterKeyFile)
	{
		fread(CommonKey, 16, 1, pMasterKeyFile);
		fclose(pMasterKeyFile);

        u8 IV[16];
        memset(IV, 0, sizeof IV);
        memcpy(IV, pTicket + 0x01dc, 8);
        AESDecode(CommonKey, IV, pTicket + 0x01bf, 16, pTicketKey);
	}
}

bool ParseTMD(u8* pDataApp, u32 pDataAppSize, u8* pTicket, u8* pTMD)
{
	u8 DecryptTitleKey[16];
	u8 IV[16];

	GetKeyFromTicket(pTicket, DecryptTitleKey);
	
	u32 numEntries = Common::swap16(pTMD + 0x01de);
	m_BootIndex = Common::swap16(pTMD + 0x01e0);
	u8* p = pDataApp;

	m_TileMetaContent.resize(numEntries);

	for (u32 i=0; i<numEntries; i++) 
	{
		STileMetaContent& rContent = m_TileMetaContent[i];
				
		rContent.m_ContentID = Common::swap32(pTMD + 0x01e4 + 0x24*i);
		rContent.m_Index = Common::swap16(pTMD + 0x01e8 + 0x24*i);
		rContent.m_Type = Common::swap16(pTMD + 0x01ea + 0x24*i);
        rContent.m_Size= (u32)ROUND_UP(Common::swap64(pTMD + 0x01ec + 0x24*i), 0x40);
		rContent.m_pData = new u8[rContent.m_Size];

		memset(IV, 0, sizeof IV);
		memcpy(IV, pTMD + 0x01e8 + 0x24*i, 2);
		AESDecode(DecryptTitleKey, IV, p, rContent.m_Size, rContent.m_pData);

		p += rContent.m_Size;
	}

    return true;
}

bool ParseWAD(DiscIO::IBlobReader& _rReader)
{
    CBlobBigEndianReader ReaderBig(_rReader);

    // get header size	
	u32 HeaderSize = ReaderBig.Read32(0);
    if (HeaderSize != 0x20) 
    {
        _dbg_assert_msg_(BOOT, (HeaderSize==0x20), "WiiWAD: Header size != 0x20");
        return false;
    }    

    // get header 
    u8 Header[0x20];
    _rReader.Read(0, HeaderSize, Header);
	u32 HeaderType = ReaderBig.Read32(0x4);
    if ((0x49730000 != HeaderType) && (0x69620000 != HeaderType))
        return false;

    u32 CertificateChainSize    = ReaderBig.Read32(0x8);
    u32 Reserved                = ReaderBig.Read32(0xC);
    u32 TicketSize              = ReaderBig.Read32(0x10);
    u32 TMDSize                 = ReaderBig.Read32(0x14);
    u32 DataAppSize             = ReaderBig.Read32(0x18);
    u32 FooterSize              = ReaderBig.Read32(0x1C);
    _dbg_assert_msg_(BOOT, Reserved==0x00, "WiiWAD: Reserved must be 0x00");

    u32 Offset = 0x40;
    u8* pCertificateChain   = CreateWADEntry(_rReader, CertificateChainSize, Offset);  Offset += ROUND_UP(CertificateChainSize, 0x40);
    u8* pTicket             = CreateWADEntry(_rReader, TicketSize, Offset);            Offset += ROUND_UP(TicketSize, 0x40);
    u8* pTMD                = CreateWADEntry(_rReader, TMDSize, Offset);               Offset += ROUND_UP(TMDSize, 0x40);
    u8* pDataApp            = CreateWADEntry(_rReader, DataAppSize, Offset);           Offset += ROUND_UP(DataAppSize, 0x40);
    u8* pFooter             = CreateWADEntry(_rReader, FooterSize, Offset);            Offset += ROUND_UP(FooterSize, 0x40);

    bool Result = ParseTMD(pDataApp, DataAppSize, pTicket, pTMD);

    return Result;
}

bool CBoot::IsWiiWAD(const char* _pFileName)
{
	DiscIO::IBlobReader* pReader = DiscIO::CreateBlobReader(_pFileName);
	if (pReader == NULL)
		return false;

	CBlobBigEndianReader Reader(*pReader);
	bool Result = false;

	// check for wii wad
	if (Reader.Read32(0x00) == 0x20)
	{
		u32 WADTYpe = Reader.Read32(0x04);
		switch(WADTYpe)
		{
		case 0x49730000:
		case 0x69620000:
			Result = true;
		}
	}

	delete pReader;

	return Result;
}


void SetupWiiMem()
{
    //
    // TODO: REDUDANT CODE SUX ....
    //

    Memory::Write_U32(0x5d1c9ea3, 0x00000018);		// Magic word it is a wii disc
    Memory::Write_U32(0x0D15EA5E, 0x00000020);		// Another magic word
    Memory::Write_U32(0x00000001, 0x00000024);		// Unknown
    Memory::Write_U32(0x01800000, 0x00000028);		// MEM1 size 24MB
    Memory::Write_U32(0x00000023, 0x0000002c);		// Production Board Model
    Memory::Write_U32(0x00000000, 0x00000030);		// Init
    Memory::Write_U32(0x817FEC60, 0x00000034);		// Init
    // 38, 3C should get start, size of FST through apploader
    Memory::Write_U32(0x38a00040, 0x00000060);		// Exception init
    Memory::Write_U32(0x8008f7b8, 0x000000e4);		// Thread Init
    Memory::Write_U32(0x01800000, 0x000000f0);		// "Simulated memory size" (debug mode?)
    Memory::Write_U32(0x8179b500, 0x000000f4);		// __start
    Memory::Write_U32(0x0e7be2c0, 0x000000f8);		// Bus speed
    Memory::Write_U32(0x2B73A840, 0x000000fc);		// CPU speed
    Memory::Write_U16(0x0000,     0x000030e6);		// Console type
    Memory::Write_U32(0x00000000, 0x000030c0);		// EXI
    Memory::Write_U32(0x00000000, 0x000030c4);		// EXI
    Memory::Write_U32(0x00000000, 0x000030dc);		// Time
    Memory::Write_U32(0x00000000, 0x000030d8);		// Time
    Memory::Write_U32(0x00000000, 0x000030f0);		// Apploader
    Memory::Write_U32(0x01800000, 0x00003100);		// BAT
    Memory::Write_U32(0x01800000, 0x00003104);		// BAT
    Memory::Write_U32(0x00000000, 0x0000310c);		// Init
    Memory::Write_U32(0x8179d500, 0x00003110);		// Init
    Memory::Write_U32(0x04000000, 0x00003118);		// Unknown
    Memory::Write_U32(0x04000000, 0x0000311c);		// BAT
    Memory::Write_U32(0x93400000, 0x00003120);		// BAT
    Memory::Write_U32(0x90000800, 0x00003124);		// Init - MEM2 low
    Memory::Write_U32(0x933e0000, 0x00003128);		// Init - MEM2 high
    Memory::Write_U32(0x933e0000, 0x00003130);		// IOS MEM2 low
    Memory::Write_U32(0x93400000, 0x00003134);		// IOS MEM2 high
    Memory::Write_U32(0x00000011, 0x00003138);		// Console type
    Memory::Write_U64(0x0009020400062507ULL, 0x00003140);	// IOS Version
    Memory::Write_U16(0x0113,     0x0000315e);		// Apploader
    Memory::Write_U32(0x0000FF16, 0x00003158);		// DDR ram vendor code

    Memory::Write_U8(0x80, 0x0000315c);				// OSInit
    Memory::Write_U8(0x00, 0x00000006);				// DVDInit
    Memory::Write_U8(0x00, 0x00000007);				// DVDInit
    Memory::Write_U16(0x0000, 0x000030e0);			// PADInit

    // Fake the VI Init of the BIOS 
    Memory::Write_U32(SConfig::GetInstance().m_LocalCoreStartupParameter.bNTSC ? 0 : 1, 0x000000CC);

    // Clear exception handler. Why? Don't we begin with only zeroes?
    for (int i = 0x3000; i <= 0x3038; i += 4)
    {
        Memory::Write_U32(0x00000000, 0x80000000 + i);
    }	

    /* This is some kind of consistency check that is compared to the 0x00
    values as the game boots. This location keep the 4 byte ID for as long
    as the game is running. The 6 byte ID at 0x00 is overwritten sometime
    after this check during booting. */


    //	VolumeHandler::ReadToPtr(Memory::GetPointer(0x3180), 0, 4);
    //	Memory::Write_U8(0x80, 0x00003184);
    // ================
}

bool CBoot::Boot_WiiWAD(const char* _pFilename)
{
    DiscIO::IBlobReader* pReader = DiscIO::CreateBlobReader(_pFilename);
    if (pReader == NULL)
        return false;

	bool Result = ParseWAD(*pReader);
    delete pReader;

    if (!Result)
        return false;

    SetupWiiMem();

	// DOL
	STileMetaContent& rContent = m_TileMetaContent[m_BootIndex];
	CDolLoader DolLoader(rContent.m_pData, rContent.m_Size);

	PC = DolLoader.GetEntryPoint() | 0x80000000;

    return true;
}

