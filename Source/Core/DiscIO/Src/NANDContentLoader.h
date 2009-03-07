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

#ifndef _NAND_CONTENT_LOADER_H
#define _NAND_CONTENT_LOADER_H

#include <string>
#include <vector>
#include "Common.h"
#include "Blob.h"

namespace DiscIO
{

struct SNANDContent
{
    u32 m_ContentID;	
    u16 m_Index;
    u16 m_Type;
    u32 m_Size;

    u8* m_pData;
};

class CNANDContentLoader
{
public:

    CNANDContentLoader(const std::string& _rName);

    virtual ~CNANDContentLoader();

    bool IsValid() const    { return m_Valid; }
    u64 GetTitleID() const  { return m_TitleID; }
    u32 GetBootIndex() const  { return m_BootIndex; }
    size_t GetContentSize() const { return m_Content.size(); }
    SNANDContent* GetContentByIndex(int _Index);

    static bool IsWiiWAD(const std::string& _rName);

private:

    bool m_Valid;
    u64 m_TitleID;
    u32 m_BootIndex;

    std::vector<SNANDContent> m_Content;

    bool CreateFromDirectory(const std::string& _rPath);
    bool CreateFromWAD(const std::string& _rName);

    bool ParseWAD(DiscIO::IBlobReader& _rReader);    

    void AESDecode(u8* _pKey, u8* _IV, u8* _pSrc, u32 _Size, u8* _pDest);

    u8* CreateWADEntry(DiscIO::IBlobReader& _rReader, u32 _Size, u64 _Offset);

    void GetKeyFromTicket(u8* pTicket, u8* pTicketKey);

    bool ParseTMD(u8* pDataApp, u32 pDataAppSize, u8* pTicket, u8* pTMD);
};

}

#endif

