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

#include "stdafx.h"
#include "NANDContentLoader.h"

#include "AES/aes.h"
#include "MathUtil.h"
#include "FileUtil.h"
#include "Log.h"

namespace DiscIO
{

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




CNANDContentLoader::CNANDContentLoader(const std::string& _rName)
    : m_TitleID(-1)
    , m_BootIndex(-1)
    , m_Valid(false)
{
    if (File::IsDirectory(_rName.c_str()))
    {
        m_Valid = CreateFromDirectory(_rName);
    }
    else if (File::Exists(_rName.c_str()))
    {
        m_Valid = CreateFromWAD(_rName);
    }
    else
    {
//        _dbg_assert_msg_(BOOT, 0, "CNANDContentLoader loads neither folder nor file");
    }
}

CNANDContentLoader::~CNANDContentLoader()
{
    for (size_t i=0; i<m_Content.size(); i++)
    {
        delete [] m_Content[i].m_pData;
    }
    m_Content.clear();
}

SNANDContent* CNANDContentLoader::GetContentByIndex(int _Index)
{
    for (size_t i=0; i<m_Content.size(); i++)
    {
        if (m_Content[i].m_Index == _Index)
            return &m_Content[i];
    }
    return NULL;
}

bool CNANDContentLoader::CreateFromWAD(const std::string& _rName)
{
    DiscIO::IBlobReader* pReader = DiscIO::CreateBlobReader(_rName.c_str());
    if (pReader == NULL)
		return false;

    bool Result = ParseWAD(*pReader);
    delete pReader;
    return Result;
}

bool CNANDContentLoader::CreateFromDirectory(const std::string& _rPath)
{
    std::string TMDFileName(_rPath);
	TMDFileName += "/title.tmd";

    FILE* pTMDFile = fopen(TMDFileName.c_str(), "rb");
    if (pTMDFile == NULL) {
		ERROR_LOG(DISCIO, "CreateFromDirectory: error opening %s", 
				  TMDFileName.c_str());
        return false;
	}
    u64 Size = File::GetSize(TMDFileName.c_str());
    u8* pTMD = new u8[(u32)Size];
    fread(pTMD, (size_t)Size, 1, pTMDFile);
    fclose(pTMDFile);

    ////// 
    u32 numEntries = Common::swap16(pTMD + 0x01de);
    m_BootIndex = Common::swap16(pTMD + 0x01e0);
    m_TitleID = Common::swap64(pTMD + 0x018C);

    m_Content.resize(numEntries);

    for (u32 i = 0; i < numEntries; i++) 
    {
        SNANDContent& rContent = m_Content[i];

        rContent.m_ContentID = Common::swap32(pTMD + 0x01e4 + 0x24*i);
        rContent.m_Index = Common::swap16(pTMD + 0x01e8 + 0x24*i);
        rContent.m_Type = Common::swap16(pTMD + 0x01ea + 0x24*i);
        rContent.m_Size = (u32)Common::swap64(pTMD + 0x01ec + 0x24*i);
        rContent.m_pData = NULL; 

        char szFilename[1024];
        sprintf(szFilename, "%s/%08x.app", _rPath.c_str(), rContent.m_ContentID);

        FILE* pFile = fopen(szFilename, "rb");
        // i have seen TMDs which index to app which doesn't exist...
        if (pFile != NULL)
        {
            u64 Size = File::GetSize(szFilename);
            rContent.m_pData = new u8[(u32)Size];

            _dbg_assert_msg_(BOOT, rContent.m_Size==Size, "TMDLoader: Filesize doesnt fit (%s %i)", szFilename, i);

            fread(rContent.m_pData, (size_t)Size, 1, pFile);
            fclose(pFile);
        } else {
			ERROR_LOG(DISCIO, "CreateFromDirectory: error opening %s", 
					  szFilename);

		}
    }

    return true;
}


bool CNANDContentLoader::IsWiiWAD(const std::string& _rName)
{
    DiscIO::IBlobReader* pReader = DiscIO::CreateBlobReader(_rName.c_str());
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



void CNANDContentLoader::AESDecode(u8* _pKey, u8* _IV, u8* _pSrc, u32 _Size, u8* _pDest)
{
    AES_KEY AESKey;

    AES_set_decrypt_key(_pKey, 128, &AESKey);
    AES_cbc_encrypt(_pSrc, _pDest, _Size, &AESKey, _IV, AES_DECRYPT);
}

u8* CNANDContentLoader::CreateWADEntry(DiscIO::IBlobReader& _rReader, u32 _Size, u64 _Offset)
{
    if (_Size > 0)
    {
        u8* pTmpBuffer = new u8[_Size];
        _dbg_assert_msg_(BOOT, pTmpBuffer!=0, "WiiWAD: Cant allocate memory for WAD entry");

        if (!_rReader.Read(_Offset, _Size, pTmpBuffer))
        {
			ERROR_LOG(DISCIO, "WiiWAD: Could not read from file");
            PanicAlert("WiiWAD: Could not read from file");
        }
        return pTmpBuffer;
    }
	return NULL;
}

void CNANDContentLoader::GetKeyFromTicket(u8* pTicket, u8* pTicketKey)
{
	u8 CommonKey[16] = {0xeb,0xe4,0x2a,0x22,0x5e,0x85,0x93,0xe4,0x48,0xd9,0xc5,0x45,0x73,0x81,0xaa,0xf7};	
    u8 IV[16];
    memset(IV, 0, sizeof IV);
    memcpy(IV, pTicket + 0x01dc, 8);
    AESDecode(CommonKey, IV, pTicket + 0x01bf, 16, pTicketKey);
}

bool CNANDContentLoader::ParseTMD(u8* pDataApp, u32 pDataAppSize, u8* pTicket, u8* pTMD)
{
	u8 DecryptTitleKey[16];
	u8 IV[16];

	GetKeyFromTicket(pTicket, DecryptTitleKey);
	
	u32 numEntries = Common::swap16(pTMD + 0x01de);
	m_BootIndex = Common::swap16(pTMD + 0x01e0);
    m_TitleID = Common::swap64(pTMD + 0x018C);

	u8* p = pDataApp;

	m_Content.resize(numEntries);

	for (u32 i=0; i<numEntries; i++) 
	{
		SNANDContent& rContent = m_Content[i];
				
		rContent.m_ContentID = Common::swap32(pTMD + 0x01e4 + 0x24*i);
		rContent.m_Index = Common::swap16(pTMD + 0x01e8 + 0x24*i);
		rContent.m_Type = Common::swap16(pTMD + 0x01ea + 0x24*i);
        rContent.m_Size= (u32)Common::swap64(pTMD + 0x01ec + 0x24*i);

        u32 RoundedSize = ROUND_UP(rContent.m_Size, 0x40);
		rContent.m_pData = new u8[RoundedSize];
		
		memset(IV, 0, sizeof IV);
		memcpy(IV, pTMD + 0x01e8 + 0x24*i, 2);
		AESDecode(DecryptTitleKey, IV, p, RoundedSize, rContent.m_pData);

		p += RoundedSize;
	}

    return true;
}

bool CNANDContentLoader::ParseWAD(DiscIO::IBlobReader& _rReader)
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

} // namespace end



