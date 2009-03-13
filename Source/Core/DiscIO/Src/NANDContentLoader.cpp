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

#include <algorithm>
#include <cctype> 
#include "AES/aes.h"
#include "MathUtil.h"
#include "FileUtil.h"
#include "Log.h"

namespace DiscIO
{


class CSharedContent
{   
public:
    
    static CSharedContent& AccessInstance() { return m_Instance; }

    std::string GetFilenameFromSHA1(u8* _pHash);

private:


    CSharedContent();

    virtual ~CSharedContent();

    struct SElement
    {
        u8 FileName[8];
        u8 SHA1Hash[20];
    };

    std::vector<SElement> m_Elements;
    static CSharedContent m_Instance;
};

CSharedContent CSharedContent::m_Instance;

CSharedContent::CSharedContent()
{
    char szFilename[1024];
    sprintf(szFilename, "%sshared1/content.map", FULL_WII_USER_DIR);
    if (File::Exists(szFilename))
    {
        FILE* pFile = fopen(szFilename, "rb");
        while(!feof(pFile))
        {
            SElement Element;
            if (fread(&Element, sizeof(SElement), 1, pFile) == 1)
            {
                m_Elements.push_back(Element);
            }
        }
    }
}

CSharedContent::~CSharedContent()
{}

std::string CSharedContent::GetFilenameFromSHA1(u8* _pHash)
{
    for (size_t i=0; i<m_Elements.size(); i++)    
    {
        if (memcmp(_pHash, m_Elements[i].SHA1Hash, 20) == 0)
        {
            char szFilename[1024];
            sprintf(szFilename,  "%sshared1/%c%c%c%c%c%c%c%c.app", FULL_WII_USER_DIR, 
                m_Elements[i].FileName[0], m_Elements[i].FileName[1], m_Elements[i].FileName[2], m_Elements[i].FileName[3],
                m_Elements[i].FileName[4], m_Elements[i].FileName[5], m_Elements[i].FileName[6], m_Elements[i].FileName[7]);
            return szFilename;
        }
    }
    return "unk";
}



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


// this classes must be created by the CNANDContentManager
class CNANDContentLoader : public INANDContentLoader
{
public:

    CNANDContentLoader(const std::string& _rName);

    virtual ~CNANDContentLoader();

    bool IsValid() const    { return m_Valid; }
    u64 GetTitleID() const  { return m_TitleID; }
    u32 GetBootIndex() const  { return m_BootIndex; }
    size_t GetContentSize() const { return m_Content.size(); }
    const SNANDContent* GetContentByIndex(int _Index) const;
    const u8* GetTicket() const { return m_TicketView; }

    const std::vector<SNANDContent>& GetContent() const { return m_Content; }

    const u16 GetTitleVersion() const {return m_TileVersion;}
    const u16 GetNumEntries() const {return m_numEntries;}

private:

    bool m_Valid;
    u64 m_TitleID;
    u32 m_BootIndex;
    u16 m_numEntries;
    u16 m_TileVersion;
    u8 m_TicketView[TICKET_VIEW_SIZE];

    std::vector<SNANDContent> m_Content;

    bool CreateFromDirectory(const std::string& _rPath);
    bool CreateFromWAD(const std::string& _rName);

    bool ParseWAD(DiscIO::IBlobReader& _rReader);    

    void AESDecode(u8* _pKey, u8* _IV, u8* _pSrc, u32 _Size, u8* _pDest);

    u8* CreateWADEntry(DiscIO::IBlobReader& _rReader, u32 _Size, u64 _Offset);

    void GetKeyFromTicket(u8* pTicket, u8* pTicketKey);

    bool ParseTMD(u8* pDataApp, u32 pDataAppSize, u8* pTicket, u8* pTMD);
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

const SNANDContent* CNANDContentLoader::GetContentByIndex(int _Index) const
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
    u8* pTMD = new u8[Size];
    fread(pTMD, Size, 1, pTMDFile);
    fclose(pTMDFile);

    memcpy(m_TicketView, pTMD + 0x180, TICKET_VIEW_SIZE);

    ////// 
    m_TileVersion = Common::swap16(pTMD + 0x01dc);
    m_numEntries = Common::swap16(pTMD + 0x01de);
    m_BootIndex = Common::swap16(pTMD + 0x01e0);
    m_TitleID = Common::swap64(pTMD + 0x018C);

    m_Content.resize(m_numEntries);

    for (u32 i = 0; i < m_numEntries; i++) 
    {
        SNANDContent& rContent = m_Content[i];

        rContent.m_ContentID = Common::swap32(pTMD + 0x01e4 + 0x24*i);
        rContent.m_Index = Common::swap16(pTMD + 0x01e8 + 0x24*i);
        rContent.m_Type = Common::swap16(pTMD + 0x01ea + 0x24*i);
        rContent.m_Size = (u32)Common::swap64(pTMD + 0x01ec + 0x24*i);
        memcpy(rContent.m_SHA1Hash, pTMD + 0x01f4 + 0x24*i, 20);

        rContent.m_pData = NULL;         
        char szFilename[1024];

        if (rContent.m_Type & 0x8000)  // shared app
        {
            std::string Filename = CSharedContent::AccessInstance().GetFilenameFromSHA1(rContent.m_SHA1Hash);
            strcpy(szFilename, Filename.c_str());
        }
        else
        {
            sprintf(szFilename, "%s/%08x.app", _rPath.c_str(), rContent.m_ContentID);
        }

        INFO_LOG(DISCIO, "NANDContentLoader: load %s", szFilename);

        FILE* pFile = fopen(szFilename, "rb");
        if (pFile != NULL)
        {
            u64 Size = File::GetSize(szFilename);
            rContent.m_pData = new u8[Size];

            _dbg_assert_msg_(BOOT, rContent.m_Size==Size, "TMDLoader: Filesize doesnt fit (%s %i)... prolly you have a bad dump", szFilename, i);

            fread(rContent.m_pData, Size, 1, pFile);
            fclose(pFile);
        } 
        else 
        {
			PanicAlert("NANDContentLoader: error opening %s", szFilename);
		}
    }

    return true;
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

///////////////////


CNANDContentManager CNANDContentManager::m_Instance;


CNANDContentManager::~CNANDContentManager()
{
    CNANDContentMap::iterator itr = m_Map.begin();
    while (itr != m_Map.end())
    {
        delete itr->second;
        itr++;
    }
    m_Map.clear();
}

const INANDContentLoader& CNANDContentManager::GetNANDLoader(const std::string& _rName)
{
    std::string KeyString(_rName);

    std::transform(KeyString.begin(), KeyString.end(), KeyString.begin(),
        (int(*)(int)) std::toupper);


    CNANDContentMap::iterator itr = m_Map.find(KeyString);
    if (itr != m_Map.end())
        return *itr->second;

    m_Map[KeyString] = new CNANDContentLoader(KeyString);
    return *m_Map[KeyString];
}


bool CNANDContentManager::IsWiiWAD(const std::string& _rName)
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





} // namespace end



