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

#include "Globals.h"
#include "DataReader.h"

#if !defined(DATAREADER_INLINE) || defined(DATAREADER_DEBUG)
// =================================================================================================
// CDataReader_Fifo
// =================================================================================================

IDataReader* g_pDataReader = NULL;
extern u8 FAKE_ReadFifo8();
extern u16 FAKE_ReadFifo16();
extern u32 FAKE_ReadFifo32();
extern int FAKE_GetPosition();
extern u8* FAKE_GetFifoCurrentPtr();
extern void FAKE_SkipFifo(u32 skip);

IDataReader::~IDataReader()
{
}

CDataReader_Fifo::CDataReader_Fifo(void)
{
}

u8 CDataReader_Fifo::Read8(void)
{
    return FAKE_ReadFifo8();
};

u16 CDataReader_Fifo::Read16(void)
{
    return FAKE_ReadFifo16();
};

u32 CDataReader_Fifo::Read32(void)
{
    return FAKE_ReadFifo32();
};

void CDataReader_Fifo::Skip(u32 skip)
{
    return FAKE_SkipFifo(skip);
}

int CDataReader_Fifo::GetPosition()
{
    return FAKE_GetPosition();
}
u8* CDataReader_Fifo::GetRealCurrentPtr()
{
	return FAKE_GetFifoCurrentPtr();
}


// =================================================================================================
// CDataReader_Memory
// =================================================================================================

CDataReader_Memory::CDataReader_Memory(u32 _uAddress) : 
    m_uReadAddress(_uAddress)
{
    //m_pMemory = g_VideoInitialize.pGetMemoryPointer(0x00);
}

u32 CDataReader_Memory::GetReadAddress()
{
    return m_uReadAddress;
}

int CDataReader_Memory::GetPosition()
{
    return m_uReadAddress;
}

u8 CDataReader_Memory::Read8()
{
    u8 tmp = Memory_Read_U8(m_uReadAddress);//m_pMemory[m_uReadAddress];
    m_uReadAddress++;
    return tmp;
}

u16 CDataReader_Memory::Read16()
{
    u16 tmp = Memory_Read_U16(m_uReadAddress);//_byteswap_ushort(*(u16*)&m_pMemory[m_uReadAddress]);
    m_uReadAddress += 2;
    return tmp;
}

u32 CDataReader_Memory::Read32()
{
    u32 tmp = Memory_Read_U32(m_uReadAddress);//_byteswap_ulong(*(u32*)&m_pMemory[m_uReadAddress]);
    m_uReadAddress += 4;
    return tmp;
}

void CDataReader_Memory::Skip(u32 skip)
{
    m_uReadAddress += skip;
}

u8* CDataReader_Memory::GetRealCurrentPtr()
{
	return Memory_GetPtr(m_uReadAddress);
}
#endif
