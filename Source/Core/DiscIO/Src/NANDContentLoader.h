// Copyright (C) 2003-2009 Dolphin Project.

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
#include <map>

#include "Common.h"
#include "Blob.h"
#include "Volume.h"

namespace DiscIO
{

struct SNANDContent
{
    u32 m_ContentID;	
    u16 m_Index;
    u16 m_Type;
    u32 m_Size;
    u8 m_SHA1Hash[20];
    u8 m_Header[36]; //all of the above

    u8* m_pData;
};

// pure virtual interface so just the NANDContentManager can create these files only
class INANDContentLoader
{
public:

    INANDContentLoader() {}

    virtual ~INANDContentLoader()  {}

    virtual bool IsValid() const = 0;
    virtual u64 GetTitleID() const = 0;
    virtual u16 GetIosVersion() const = 0;
    virtual u32 GetBootIndex() const = 0;
    virtual size_t GetContentSize() const = 0;
    virtual const SNANDContent* GetContentByIndex(int _Index) const = 0;
    virtual const u8* GetTicketView() const = 0;
    virtual const u8* GetTmdHeader() const = 0;
    virtual const std::vector<SNANDContent>& GetContent() const = 0;    
    virtual const u16 GetTitleVersion() const = 0;
    virtual const u16 GetNumEntries() const = 0;
    virtual const DiscIO::IVolume::ECountry GetCountry() const = 0;

    enum
    {
        TICKET_VIEW_SIZE = 0x58,
        TMD_HEADER_SIZE = 0x1e4,
		CONTENT_HEADER_SIZE = 0x24
    };
};


// we open the NAND Content files to often... lets cache them
class CNANDContentManager
{
public:

    static CNANDContentManager& Access() { return m_Instance; }

    const INANDContentLoader& GetNANDLoader(const std::string& _rName);

private:

    CNANDContentManager() {};

    ~CNANDContentManager();

    static CNANDContentManager m_Instance;

    typedef std::map<std::string, INANDContentLoader*> CNANDContentMap;
    CNANDContentMap m_Map;

};

}

#endif

