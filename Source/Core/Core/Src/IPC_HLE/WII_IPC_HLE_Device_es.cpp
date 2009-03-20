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



// =======================================================
// File description
// -------------
/*  Here we handle /dev/es requests. We have cases for these functions, the exact
	DevKitPro/libogc name is in parenthesis:

	0x20 GetTitleID (ES_GetTitleID) (Input: none, Output: 8 bytes)
	0x1d GetDataDir (ES_GetDataDir) (Input: 8 bytes, Output: 30 bytes)

	0x1b DiGetTicketView (Input: none, Output: 216 bytes) 
	0x16 GetConsumption (Input: 8 bytes, Output: 0 bytes, 4 bytes) // there are two output buffers

	0x12 GetNumTicketViews (ES_GetNumTicketViews) (Input: 8 bytes, Output: 4 bytes)
	0x14 GetTMDViewSize (ES_GetTMDViewSize) (Input: ?, Output: ?) // I don't get this anymore,
		it used to come after 0x12

	but only the first two are correctly supported. For the other four we ignore any potential
	input and only write zero to the out buffer. However, most games only use first two,
	but some Nintendo developed games use the other ones to:
	
	0x1b: Mario Galaxy, Mario Kart, SSBB
	0x16: Mario Galaxy, Mario Kart, SSBB
	0x12: Mario Kart
	0x14: Mario Kart: But only if we don't return a zeroed out buffer for the 0x12 question,
		and instead answer for example 1 will this question appear.
 
*/
// =============

#include "WII_IPC_HLE_Device_es.h"

#include "../PowerPC/PowerPC.h"
#include "../VolumeHandler.h"
#include "FileUtil.h"



CWII_IPC_HLE_Device_es::CWII_IPC_HLE_Device_es(u32 _DeviceID, const std::string& _rDeviceName, const std::string& _rDefaultContentFile) 
    : IWII_IPC_HLE_Device(_DeviceID, _rDeviceName)
    , m_pContentLoader(NULL)
    , m_TitleID(-1)
    , AccessIdentID(0x6000000)
{
    m_pContentLoader = &DiscIO::CNANDContentManager::Access().GetNANDLoader(_rDefaultContentFile);

    // check for cd ...
    if (m_pContentLoader->IsValid())
    {
        m_TitleID = m_pContentLoader->GetTitleID();
    }
    else if (VolumeHandler::IsValid())
    {
        m_TitleID = ((u64)0x00010000 << 32) | VolumeHandler::Read32(0);                        
    }
    else
    {
        m_TitleID = ((u64)0x00010000 << 32) | 0xF00DBEEF;
    }   


    // scan for the title ids listed in TMDs within /title/
	m_TitleIDs.clear();
    m_TitleIDs.push_back(0x0000000100000002ULL);
    // m_TitleIDs.push_back(0x0001000248414741ULL); 
    // m_TitleIDs.push_back(0x0001000248414341ULL);
    // m_TitleIDs.push_back(0x0001000248414241ULL);
    // m_TitleIDs.push_back(0x0001000248414141ULL);
    
   // FindValidTitleIDs();


    INFO_LOG(WII_IPC_ES, "Set default title to %08x/%08x", m_TitleID>>32, m_TitleID);
}

CWII_IPC_HLE_Device_es::~CWII_IPC_HLE_Device_es()
{
    CTitleToContentMap::const_iterator itr = m_NANDContent.begin();
    while(itr != m_NANDContent.end())
    {
        if (itr->second)
            delete itr->second;
        itr++;
    }
    m_NANDContent.clear();
}

bool CWII_IPC_HLE_Device_es::Open(u32 _CommandAddress, u32 _Mode)
{
    Memory::Write_U32(GetDeviceID(), _CommandAddress+4);
    return true;
}

bool CWII_IPC_HLE_Device_es::Close(u32 _CommandAddress)
{
    INFO_LOG(WII_IPC_ES, "ES: Close");
    Memory::Write_U32(0, _CommandAddress + 4);
    return true;
}

bool CWII_IPC_HLE_Device_es::IOCtlV(u32 _CommandAddress) 
{
    SIOCtlVBuffer Buffer(_CommandAddress);

    INFO_LOG(WII_IPC_ES, "%s (0x%x)", GetDeviceName().c_str(), Buffer.Parameter);

    // Prepare the out buffer(s) with zeroes as a safety precaution
    // to avoid returning bad values
    for (u32 i = 0; i < Buffer.NumberPayloadBuffer; i++)
    {
        Memory::Memset(Buffer.PayloadBuffer[i].m_Address, 0,
            Buffer.PayloadBuffer[i].m_Size);
    }

    switch (Buffer.Parameter)
    {
    case IOCTL_ES_GETTITLECONTENTSCNT:
        {
            _dbg_assert_(WII_IPC_ES, Buffer.NumberInBuffer == 1);
            _dbg_assert_(WII_IPC_ES, Buffer.NumberPayloadBuffer == 1);    

            u64 TitleID = Memory::Read_U64(Buffer.InBuffer[0].m_Address);

            const DiscIO::INANDContentLoader& rNANDCOntent = AccessContentDevice(TitleID);
            _dbg_assert_(WII_IPC_ES, rNANDCOntent.IsValid());
        
            u32 NumberOfPrivateContent = 0;
            const std::vector<DiscIO::SNANDContent>& rContent = rNANDCOntent.GetContent();
            for (size_t i=0; i<rContent.size();i++)
            {
                if ((rContent[i].m_Type & 0x8000) == 0)
                {
                    NumberOfPrivateContent++;
                }
            }

            Memory::Write_U32(NumberOfPrivateContent, Buffer.PayloadBuffer[0].m_Address);	
            
            INFO_LOG(WII_IPC_ES, "ES: IOCTL_ES_GETTITLECONTENTSCNT: TitleID: %08x/%08x  content count %i  (just content in private directory)", 
                                (u32)(TitleID>>32), (u32)TitleID, rNANDCOntent.GetContentSize());

            Memory::Write_U32(0, _CommandAddress + 0x4);	
            return true;
        }
        break;

    case IOCTL_ES_GETTITLECONTENTS:
        {
            _dbg_assert_msg_(WII_IPC_ES, Buffer.NumberInBuffer == 2, "CWII_IPC_HLE_Device_es: IOCTL_ES_GETTITLECONTENTS no in buffer");
            _dbg_assert_msg_(WII_IPC_ES, Buffer.NumberPayloadBuffer == 1, "CWII_IPC_HLE_Device_es: IOCTL_ES_GETTITLECONTENTS no out buffer");

            u64 TitleID = Memory::Read_U64(Buffer.InBuffer[0].m_Address);
            u32 MaxCount = Memory::Read_U32(Buffer.InBuffer[1].m_Address);
            std::string TitleFilename = CreateTitleContentPath(TitleID);
            const DiscIO::INANDContentLoader& Loader = DiscIO::CNANDContentManager::Access().GetNANDLoader(TitleFilename);

            ERROR_LOG(WII_IPC_ES, "IOCTL_ES_GETTITLECONTENTS: title: %08x/%08x   MaxCount: %i (just content in private directory)", (u32)(TitleID >> 32), (u32)TitleID, MaxCount);

            _dbg_assert_(WII_IPC_ES, Loader.GetContentSize() == MaxCount);
            if (Loader.IsValid())
            {
                const std::vector<DiscIO::SNANDContent>& rContent = Loader.GetContent();
                for (size_t i=0; i<Loader.GetContentSize(); i++)
                {
                    if ((rContent[i].m_Type & 0x8000) == 0)
                    {
                        Memory::Write_U32(rContent[i].m_ContentID, Buffer.PayloadBuffer[0].m_Address+i*4);
                        ERROR_LOG(WII_IPC_ES, "IOCTL_ES_GETTITLECONTENTS: Index 0x%x", rContent[i].m_ContentID);
                    }
                }
            }
            Memory::Write_U32(0, _CommandAddress + 0x4);		
            ERROR_LOG(WII_IPC_ES, "IOCTL_ES_GETTITLECONTENTS: Count %i", Loader.GetContentSize());
            return true;  
        }
        break;


    case IOCTL_ES_OPENTITLECONTENT:
        {
            _dbg_assert_(WII_IPC_ES, Buffer.NumberInBuffer == 3);
            _dbg_assert_(WII_IPC_ES, Buffer.NumberPayloadBuffer == 0);
            
            u64 TitleID = Memory::Read_U64(Buffer.InBuffer[0].m_Address);
            u8* pTicket = Memory::GetPointer(Buffer.InBuffer[1].m_Address); 
            u32 Index = Memory::Read_U32(Buffer.InBuffer[2].m_Address);

            u32 CFD = AccessIdentID++;
            m_ContentAccessMap[CFD].m_Position = 0;
            m_ContentAccessMap[CFD].m_pContent = AccessContentDevice(TitleID).GetContentByIndex(Index);
            _dbg_assert_msg_(WII_IPC_ES, m_ContentAccessMap[CFD].m_pContent != NULL, "No Content for TitleID: %08x/%08x at Index %x", (u32)(TitleID>>32), (u32)TitleID, Index);

            Memory::Write_U32(CFD, _CommandAddress + 0x4);		

            INFO_LOG(WII_IPC_ES, "ES: IOCTL_ES_OPENTITLECONTENT: TitleID: %08x/%08x  Index %i -> got CFD %x", (u32)(TitleID>>32), (u32)TitleID, Index, CFD);
            return true;
        }
        break;

    case IOCTL_ES_OPENCONTENT:
        {
            _dbg_assert_(WII_IPC_ES, Buffer.NumberInBuffer == 1);
            _dbg_assert_(WII_IPC_ES, Buffer.NumberPayloadBuffer == 0);

            u32 CFD = AccessIdentID++;
            u32 Index = Memory::Read_U32(Buffer.InBuffer[0].m_Address);

            m_ContentAccessMap[CFD].m_Position = 0;
            m_ContentAccessMap[CFD].m_pContent = AccessContentDevice(m_TitleID).GetContentByIndex(Index);
            _dbg_assert_(WII_IPC_ES, m_ContentAccessMap[CFD].m_pContent != NULL);

            Memory::Write_U32(CFD, _CommandAddress + 0x4);		

            INFO_LOG(WII_IPC_ES, "ES: IOCTL_ES_OPENCONTENT: Index %i -> got CFD %x", Index, CFD);
            return true;
        }
        break;

    case IOCTL_ES_READCONTENT:
        {
            _dbg_assert_(WII_IPC_ES, Buffer.NumberInBuffer == 1);
            _dbg_assert_(WII_IPC_ES, Buffer.NumberPayloadBuffer == 1);

            u32 CFD = Memory::Read_U32(Buffer.InBuffer[0].m_Address);
            u32 Size = Buffer.PayloadBuffer[0].m_Size;
            u32 Addr = Buffer.PayloadBuffer[0].m_Address;

            _dbg_assert_(WII_IPC_ES, m_ContentAccessMap.find(CFD) != m_ContentAccessMap.end());
            SContentAccess& rContent = m_ContentAccessMap[CFD];

            _dbg_assert_(WII_IPC_ES, rContent.m_pContent->m_pData != NULL);

            u8* pSrc = &rContent.m_pContent->m_pData[rContent.m_Position];
            u8* pDest = Memory::GetPointer(Addr);

            if (rContent.m_Position + Size > rContent.m_pContent->m_Size) 
            {
                Size = rContent.m_pContent->m_Size-rContent.m_Position;
            }

            if (Size > 0)
            {
				if (pDest) {
					memcpy(pDest, pSrc, Size);
					rContent.m_Position += Size;
				} else {
					PanicAlert("IOCTL_ES_READCONTENT - bad destination");
				}
            }

            INFO_LOG(WII_IPC_ES, "ES: IOCTL_ES_READCONTENT: CFD %x, Addr 0x%x, Size %i -> stream pos %i (Index %i)", CFD, Addr, Size, rContent.m_Position, rContent.m_pContent->m_Index);

            Memory::Write_U32(Size, _CommandAddress + 0x4);		
            return true;
        }
        break;

    case IOCTL_ES_CLOSECONTENT:
        {
            _dbg_assert_(WII_IPC_ES, Buffer.NumberInBuffer == 1);
            _dbg_assert_(WII_IPC_ES, Buffer.NumberPayloadBuffer == 0);

            u32 CFD = Memory::Read_U32(Buffer.InBuffer[0].m_Address);

            CContentAccessMap::iterator itr = m_ContentAccessMap.find(CFD);
            m_ContentAccessMap.erase(itr);

            INFO_LOG(WII_IPC_ES, "ES: IOCTL_ES_CLOSECONTENT: CFD %x", CFD);

            Memory::Write_U32(0, _CommandAddress + 0x4);		
            return true;
        }
        break;

    case IOCTL_ES_SEEKCONTENT:
        {	
            _dbg_assert_(WII_IPC_ES, Buffer.NumberInBuffer == 3);
            _dbg_assert_(WII_IPC_ES, Buffer.NumberPayloadBuffer == 0);

            u32 CFD = Memory::Read_U32(Buffer.InBuffer[0].m_Address);
            u32 Addr = Memory::Read_U32(Buffer.InBuffer[1].m_Address);
            u32 Mode = Memory::Read_U32(Buffer.InBuffer[2].m_Address);

            _dbg_assert_(WII_IPC_ES, m_ContentAccessMap.find(CFD) != m_ContentAccessMap.end());
            SContentAccess& rContent = m_ContentAccessMap[CFD];

            switch (Mode)
            {
            case 0:  // SET
                rContent.m_Position = Addr;
                break;

            case 1:  // CUR
                rContent.m_Position += Addr;
                break;

            case 2:  // END
                rContent.m_Position = rContent.m_pContent->m_Size + Addr;
                break;
            }

            INFO_LOG(WII_IPC_ES, "ES: IOCTL_ES_SEEKCONTENT: CFD %x, Addr 0x%x, Mode %i -> Pos %i", CFD, Addr, Mode, rContent.m_Position);

            Memory::Write_U32(rContent.m_Position, _CommandAddress + 0x4);		
            return true;
        }
        break;

    case IOCTL_ES_GETTITLEDIR:
        {          
            _dbg_assert_(WII_IPC_ES, Buffer.NumberInBuffer == 1);
            _dbg_assert_(WII_IPC_ES, Buffer.NumberPayloadBuffer == 1);

            u64 TitleID = Memory::Read_U64(Buffer.InBuffer[0].m_Address);

            char* pTitleID = (char*)&TitleID;
            char* Path = (char*)Memory::GetPointer(Buffer.PayloadBuffer[0].m_Address);
            sprintf(Path, "/%08x/%08x/data", (u32)(TitleID >> 32), (u32)TitleID);

            INFO_LOG(WII_IPC_ES, "ES: IOCTL_ES_GETTITLEDIR: %s)", Path);
        }
        break;

    case IOCTL_ES_GETTITLEID:
        {
            _dbg_assert_(WII_IPC_ES, Buffer.NumberInBuffer == 0);
            _dbg_assert_msg_(WII_IPC_ES, Buffer.NumberPayloadBuffer == 1, "CWII_IPC_HLE_Device_es: IOCTL_ES_GETTITLEID no out buffer");

            Memory::Write_U64(m_TitleID, Buffer.PayloadBuffer[0].m_Address);
            INFO_LOG(WII_IPC_ES, "ES: IOCTL_ES_GETTITLEID: %08x/%08x", (u32)(m_TitleID>>32), (u32)m_TitleID);
        }
        break;

    case IOCTL_ES_SETUID:
        {
            _dbg_assert_msg_(WII_IPC_ES, Buffer.NumberInBuffer == 1, "CWII_IPC_HLE_Device_es: IOCTL_ES_GETTITLEID no in buffer");
            _dbg_assert_(WII_IPC_ES, Buffer.NumberPayloadBuffer == 0);

            u64 TitleID = Memory::Read_U64(Buffer.InBuffer[0].m_Address);
            INFO_LOG(WII_IPC_ES, "ES: IOCTL_ES_SETUID titleID: %08x/%08x", (u32)(TitleID>>32), (u32)TitleID );
        }
        break;

    case IOCTL_ES_GETTITLECNT:
        {
            _dbg_assert_msg_(WII_IPC_ES, Buffer.NumberInBuffer == 0, "IOCTL_ES_GETTITLECNT has an in buffer");
            _dbg_assert_msg_(WII_IPC_ES, Buffer.NumberPayloadBuffer == 1, "IOCTL_ES_GETTITLECNT has no out buffer");
            _dbg_assert_msg_(WII_IPC_ES, Buffer.PayloadBuffer[0].m_Size == 4, "IOCTL_ES_GETTITLECNT payload[0].size != 4");

            Memory::Write_U32((u32)m_TitleIDs.size(), Buffer.PayloadBuffer[0].m_Address);

            ERROR_LOG(WII_IPC_ES, "IOCTL_ES_GETTITLECNT: Number of Titles %i", m_TitleIDs.size());

            Memory::Write_U32(0, _CommandAddress + 0x4);		

            return true;
        }
        break;


    case IOCTL_ES_GETTITLES:
        {
            _dbg_assert_msg_(WII_IPC_ES, Buffer.NumberInBuffer == 1, "IOCTL_ES_GETTITLES has an in buffer");
            _dbg_assert_msg_(WII_IPC_ES, Buffer.NumberPayloadBuffer == 1, "IOCTL_ES_GETTITLES has no out buffer");

            u32 MaxCount = Memory::Read_U32(Buffer.InBuffer[0].m_Address);
            u32 Count = 0;
            for (int i = 0; i < (int)m_TitleIDs.size(); i++)
            {
                Memory::Write_U64(m_TitleIDs[i], Buffer.PayloadBuffer[0].m_Address + i*8);
                ERROR_LOG(WII_IPC_ES, "IOCTL_ES_GETTITLES: %08x/%08x", (u32)(m_TitleIDs[i] >> 32), (u32)m_TitleIDs[i]);
                Count++;
                if (Count >= MaxCount)
                    break;
            }

            ERROR_LOG(WII_IPC_ES, "IOCTL_ES_GETTITLES: Number of titles returned %i", Count);

            // really strange: if i return the number of returned titleIDs the WiiMenu_00 doesn't work anymore
            // and i get an error message ("The system files are corrupted. Please refer to the Wii Operation Manual...")
            // if i return 0 it works...
            Memory::Write_U32(0, _CommandAddress + 0x4);		
            return true;
        }
        break;


    case IOCTL_ES_GETVIEWCNT:
        {
            _dbg_assert_msg_(WII_IPC_ES, Buffer.NumberInBuffer == 1, "CWII_IPC_HLE_Device_es: IOCTL_ES_GETVIEWCNT no in buffer");
            _dbg_assert_msg_(WII_IPC_ES, Buffer.NumberPayloadBuffer == 1, "CWII_IPC_HLE_Device_es: IOCTL_ES_GETVIEWCNT no out buffer");

            u64 TitleID = Memory::Read_U64(Buffer.InBuffer[0].m_Address);

            std::string TicketFilename = CreateTicketFileName(TitleID);

            u32 ViewCount = 0;
            if (File::Exists(TicketFilename.c_str()))
            {
                const u32 SIZE_OF_ONE_TICKET = 676;

                u32 FileSize = (u32)(File::GetSize(TicketFilename.c_str()));
                _dbg_assert_msg_(WII_IPC_ES, (FileSize % SIZE_OF_ONE_TICKET) == 0, "CWII_IPC_HLE_Device_es: IOCTL_ES_GETVIEWCNT ticket file size seems to be wrong");

                ViewCount = FileSize / SIZE_OF_ONE_TICKET;
                _dbg_assert_msg_(WII_IPC_ES, (ViewCount>0) && (ViewCount<=4), "CWII_IPC_HLE_Device_es: IOCTL_ES_GETVIEWCNT ticket count seems to be wrong");
            }
            else
            {
                if (TitleID == 0x0000000100000002ull)
                {
                    PanicAlert("There must be a ticket for 00000001/00000002. Prolly your NAND dump is incomplete");
                }
                ViewCount = 0;                
            }            
            
            INFO_LOG(WII_IPC_ES, "ES: IOCTL_ES_GETVIEWCNT for titleID: %08x/%08x (View Count = %i)", (u32)(TitleID>>32), (u32)TitleID, ViewCount);

            Memory::Write_U32(ViewCount, Buffer.PayloadBuffer[0].m_Address);
            Memory::Write_U32(0, _CommandAddress + 0x4);		
            return true;
        }
        break;

    case IOCTL_ES_GETVIEWS:
        {
            _dbg_assert_msg_(WII_IPC_ES, Buffer.NumberInBuffer == 2, "IOCTL_ES_GETVIEWS no in buffer");
            _dbg_assert_msg_(WII_IPC_ES, Buffer.NumberPayloadBuffer == 1, "IOCTL_ES_GETVIEWS no out buffer");

            u64 TitleID = Memory::Read_U64(Buffer.InBuffer[0].m_Address);
            u32 Count = Memory::Read_U32(Buffer.InBuffer[1].m_Address);

            std::string TicketFilename = CreateTicketFileName(TitleID);
            if (File::Exists(TicketFilename.c_str()))
            {
                const u32 SIZE_OF_ONE_TICKET = 676;
                FILE* pFile = fopen(TicketFilename.c_str(), "rb");
                if (pFile)
                {
                    int View = 0;
                    u8 Ticket[SIZE_OF_ONE_TICKET];
                    while (fread(Ticket, SIZE_OF_ONE_TICKET, 1, pFile) == 1)
                    {
                        Memory::Write_U32(View, Buffer.PayloadBuffer[0].m_Address);
                        Memory::WriteBigEData(Ticket+0x1D0, Buffer.PayloadBuffer[0].m_Address+4, 212);
                        View++;
                    }
                    fclose(pFile);
                }
            }
            else
            {
                PanicAlert("IOCTL_ES_GETVIEWS: Try to get data from an unknown ticket: %08x/%08x", (u32)(TitleID >> 32), (u32)TitleID);
            }

            INFO_LOG(WII_IPC_ES, "ES: IOCTL_ES_GETVIEWS for titleID: %08x/%08x", (u32)(TitleID>>32), (u32)TitleID );

            Memory::Write_U32(0, _CommandAddress + 0x4);
            return true;
        }
        break;

    case IOCTL_ES_GETTMDVIEWCNT:
        {
            _dbg_assert_msg_(WII_IPC_ES, Buffer.NumberInBuffer == 1, "CWII_IPC_HLE_Device_es: IOCTL_ES_GETTMDVIEWCNT no in buffer");
            _dbg_assert_msg_(WII_IPC_ES, Buffer.NumberPayloadBuffer == 1, "CWII_IPC_HLE_Device_es: IOCTL_ES_GETTMDVIEWCNT no out buffer");

            u64 TitleID = Memory::Read_U64(Buffer.InBuffer[0].m_Address);
            std::string TitleFilename = CreateTitleContentPath(TitleID);
            const DiscIO::INANDContentLoader& Loader = DiscIO::CNANDContentManager::Access().GetNANDLoader(TitleFilename);

            _dbg_assert_(WII_IPC_ES, Loader.IsValid());
            u32 TMDViewCnt = 0;
            if (Loader.IsValid())
            {
                TMDViewCnt += DiscIO::INANDContentLoader::TICKET_VIEW_SIZE; 
                TMDViewCnt += 2; // title version
                TMDViewCnt += 2; // num entries
                TMDViewCnt += (u32)Loader.GetContentSize() * (4+2+2+8); // content id, index, type, size
            }
            Memory::Write_U32(TMDViewCnt, Buffer.PayloadBuffer[0].m_Address);

            Memory::Write_U32(0, _CommandAddress + 0x4);		

            INFO_LOG(WII_IPC_ES, "IOCTL_ES_GETTMDVIEWCNT: title: %08x/%08x (view size %i)", (u32)(TitleID >> 32), (u32)TitleID, TMDViewCnt);
            return true;            
        }
        break;

    case IOCTL_ES_GETTMDVIEWS:
        {
            _dbg_assert_msg_(WII_IPC_ES, Buffer.NumberInBuffer == 2, "CWII_IPC_HLE_Device_es: IOCTL_ES_GETTMDVIEWCNT no in buffer");
            _dbg_assert_msg_(WII_IPC_ES, Buffer.NumberPayloadBuffer == 1, "CWII_IPC_HLE_Device_es: IOCTL_ES_GETTMDVIEWCNT no out buffer");

            u64 TitleID = Memory::Read_U64(Buffer.InBuffer[0].m_Address);
            u32 MaxCount = Memory::Read_U32(Buffer.InBuffer[1].m_Address);
            std::string TitleFilename = CreateTitleContentPath(TitleID);
            const DiscIO::INANDContentLoader& Loader = DiscIO::CNANDContentManager::Access().GetNANDLoader(TitleFilename);            


            INFO_LOG(WII_IPC_ES, "IOCTL_ES_GETTMDVIEWCNT: title: %08x/%08x   buffersize: %i", (u32)(TitleID >> 32), (u32)TitleID, MaxCount);

            u32 Count = 0;
            u8* p = Memory::GetPointer(Buffer.PayloadBuffer[0].m_Address);
            if (Loader.IsValid())
            {
                u32 Address = Buffer.PayloadBuffer[0].m_Address;

                Memory::WriteBigEData(Loader.GetTicket(), Address, DiscIO::INANDContentLoader::TICKET_VIEW_SIZE);
                Address += DiscIO::INANDContentLoader::TICKET_VIEW_SIZE;
                
                Memory::Write_U16(Loader.GetTitleVersion(), Address); Address += 2;
                Memory::Write_U16(Loader.GetNumEntries(), Address); Address += 2;

                const std::vector<DiscIO::SNANDContent>& rContent = Loader.GetContent();
                for (size_t i=0; i<Loader.GetContentSize(); i++)
                {
                    Memory::Write_U32(rContent[i].m_ContentID,  Address); Address += 4;
                    Memory::Write_U16(rContent[i].m_Index,      Address); Address += 2;
                    Memory::Write_U16(rContent[i].m_Type,       Address); Address += 2;
                    Memory::Write_U64(rContent[i].m_Size,       Address); Address += 8;
                }

                _dbg_assert_(WII_IPC_ES, (Address-Buffer.PayloadBuffer[0].m_Address) == Buffer.PayloadBuffer[0].m_Size);
            }
            Memory::Write_U32(0, _CommandAddress + 0x4);	

            INFO_LOG(WII_IPC_ES, "IOCTL_ES_GETTMDVIEWS: title: %08x/%08x (buffer size: %i)", TitleID >> 32, TitleID, MaxCount);
            return true;            
        }
        break;


        // ===============================================================================================
        // unsupported functions 
        // ===============================================================================================

    case IOCTL_ES_LAUNCH:
        {
            _dbg_assert_(WII_IPC_ES, Buffer.NumberInBuffer == 2);

            u64 TitleID = Memory::Read_U64(Buffer.InBuffer[0].m_Address);

            u32 view =      Memory::Read_U32(Buffer.InBuffer[1].m_Address);
            u64 ticketid =  Memory::Read_U64(Buffer.InBuffer[1].m_Address+4);
            u32 devicetype =Memory::Read_U32(Buffer.InBuffer[1].m_Address+12);
            u64 titleid =   Memory::Read_U64(Buffer.InBuffer[1].m_Address+16);
            u16 access =    Memory::Read_U16(Buffer.InBuffer[1].m_Address+24);

            PanicAlert("IOCTL_ES_LAUNCH: src titleID %08x/%08x -> start %08x/%08x \n"
                "This means that dolphin tries to relaunch the WiiMenu or"
                "launches code from the an URL. Both wont work and dolphin will prolly hang...",
                (u32)(TitleID>>32), (u32)TitleID, (u32)(titleid>>32), (u32)titleid );

            Memory::Write_U32(0, _CommandAddress + 0x4);

            ERROR_LOG(WII_IPC_ES, "IOCTL_ES_LAUNCH %016llx %08x %016llx %08x %016llx %04x", TitleID,view,ticketid,devicetype,titleid,access);
			//					   IOCTL_ES_LAUNCH 0001000248414341 00000001 0001c0fef3df2cfa 00000000 0001000248414341 ffff

            return true;
        }
        break;

    case IOCTL_ES_GETSTOREDTMDSIZE:
        _dbg_assert_msg_(WII_IPC_ES, 0, "IOCTL_ES_GETSTOREDTMDSIZE: this looks really wrong...");
        break;

    case IOCTL_ES_GETCONSUMPTION: // (Input: 8 bytes, Output: 0 bytes, 4 bytes)
        _dbg_assert_msg_(WII_IPC_ES, 0, "IOCTL_ES_GETCONSUMPTION: this looks really wrong...");
        break;

    case IOCTL_ES_DIGETTICKETVIEW: // (Input: none, Output: 216 bytes)
        _dbg_assert_msg_(WII_IPC_ES, 0, "IOCTL_ES_DIGETTICKETVIEW: this looks really wrong...");
        break;

    default:

        _dbg_assert_msg_(WII_IPC_ES, 0, "CWII_IPC_HLE_Device_es: 0x%x", Buffer.Parameter);

        DumpCommands(_CommandAddress, 8);
        INFO_LOG(WII_IPC_ES, "CWII_IPC_HLE_Device_es command:"
            "Parameter: 0x%08x", Buffer.Parameter);

        break;
    }

    // Write return value (0 means OK)
    Memory::Write_U32(0, _CommandAddress + 0x4);		

    return true;
}

const DiscIO::INANDContentLoader& CWII_IPC_HLE_Device_es::AccessContentDevice(u64 _TitleID)
{
    if (m_pContentLoader->IsValid() && m_pContentLoader->GetTitleID() == _TitleID)
        return* m_pContentLoader;
    
    CTitleToContentMap::iterator itr = m_NANDContent.find(_TitleID);
    if (itr != m_NANDContent.end())
        return *itr->second;

    std::string TitleFilename = CreateTitleContentPath(_TitleID);
    m_NANDContent[_TitleID] = &DiscIO::CNANDContentManager::Access().GetNANDLoader(TitleFilename);

    _dbg_assert_(WII_IPC_ES, m_NANDContent[_TitleID]->IsValid());
    return *m_NANDContent[_TitleID];
}

bool CWII_IPC_HLE_Device_es::IsValid(u64 _TitleID) const
{
    if (m_pContentLoader->IsValid() && m_pContentLoader->GetTitleID() == _TitleID)
        return true;

    return false;
}

std::string CWII_IPC_HLE_Device_es::CreateTicketFileName(u64 _TitleID) const
{
    char TicketFilename[1024];
    sprintf(TicketFilename, "%sticket/%08x/%08x.tik", FULL_WII_USER_DIR, (u32)(_TitleID >> 32), (u32)_TitleID);

    return TicketFilename;
}

std::string CWII_IPC_HLE_Device_es::CreateTitleContentPath(u64 _TitleID) const
{
    char TicketFilename[1024];
    sprintf(TicketFilename, "%stitle/%08x/%08x/content", FULL_WII_USER_DIR, (u32)(_TitleID >> 32), (u32)_TitleID);

    return TicketFilename;
}

void CWII_IPC_HLE_Device_es::FindValidTitleIDs()
{
    m_TitleIDs.clear();

    std::string TitlePath(FULL_WII_USER_DIR + std::string("title"));
    File::FSTEntry ParentEntry;
    u32 NumEntries = ScanDirectoryTree(TitlePath.c_str(), ParentEntry);  
    for(std::vector<File::FSTEntry>::iterator Level1 = ParentEntry.children.begin(); Level1 != ParentEntry.children.end(); ++Level1)
    {
        if (Level1->isDirectory)
        {
            for(std::vector<File::FSTEntry>::iterator Level2 = Level1->children.begin(); Level2 != Level1->children.end(); ++Level2)
            {
                if (Level2->isDirectory)
                {
                    // finally at /title/*/*/
                    // ...get titleID from content/title.tmd
                    std::string CurrentTMD(Level2->physicalName + DIR_SEP + "content" + DIR_SEP + "title.tmd");
                    if (File::Exists(CurrentTMD.c_str()))
                    {
                        FILE* pTMDFile = fopen(CurrentTMD.c_str(), "rb");
                        u64 TitleID = 0xDEADBEEFDEADBEEFULL;
                        fseek(pTMDFile, 0x18C, SEEK_SET);
                        fread(&TitleID, 8, 1, pTMDFile);
                        m_TitleIDs.push_back(Common::swap64(TitleID));
                        fclose(pTMDFile);
                    }
                }
            }
        }
    }
}