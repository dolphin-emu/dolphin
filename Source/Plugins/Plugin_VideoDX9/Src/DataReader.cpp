#include <stdlib.h>

#include "Utils.h"
#include "Common.h"
#include "main.h"
#include "DataReader.h"

// =================================================================================================
// CDataReader_Fifo
// =================================================================================================

IDataReader* g_pDataReader = NULL;
extern u8 FAKE_ReadFifo8();
extern u16 FAKE_ReadFifo16();
extern u32 FAKE_ReadFifo32();

CDataReader_Fifo::CDataReader_Fifo(void)
{
	m_szName = "CDataReader_Fifo";
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

// =================================================================================================
// CDataReader_Memory
// =================================================================================================

CDataReader_Memory::CDataReader_Memory(u32 _uAddress) : 
	m_uReadAddress(_uAddress)
{
// F|RES: this wont work anymore caused by Mem2

//	m_pMemory = g_VideoInitialize.pGetMemoryPointer(0x00);
	m_szName = "CDataReader_Memory";
}

u32 CDataReader_Memory::GetReadAddress(void)
{
	return m_uReadAddress;
}

u8 CDataReader_Memory::Read8(void)
{
	u8 tmp = Memory_Read_U8(m_uReadAddress); //m_pMemory[m_uReadAddress];
	m_uReadAddress++;
	return tmp;
}

u16 CDataReader_Memory::Read16(void)
{
	u16 tmp = Memory_Read_U16(m_uReadAddress); //_byteswap_ushort(*(u16*)&m_pMemory[m_uReadAddress]);
	m_uReadAddress += 2;
	return tmp;
}

u32 CDataReader_Memory::Read32(void)
{
	u32 tmp =Memory_Read_U32(m_uReadAddress); // _byteswap_ulong(*(u32*)&m_pMemory[m_uReadAddress]);
	m_uReadAddress += 4;
	return tmp;
}