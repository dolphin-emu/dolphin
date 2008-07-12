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

#include "GPFifo.h"

#include "PeripheralInterface.h"
#include "CommandProcessor.h"
#include "Memmap.h"
#include "../PowerPC/PowerPC.h"

namespace GPFifo
{

// 32 Byte gather pipe with extra space
// Overfilling is no problem (up to the real limit), CheckGatherPipe will blast the 
// contents in nicely sized chunks

// Other optimizations to think about:

// If the gp is NOT linked to the fifo, just blast to memory byte by word
// If the gp IS linked to the fifo, use a fast wrapping buffer and skip writing to memory

// Both of these should actually work! Only problem is that we have to decide at run time, 
// the same function could use both methods. Compile 2 different versions of each such block?

u8 m_gatherPipe[GATHER_PIPE_SIZE*16]; //more room, for the fastmodes

// pipe counter
u32 m_gatherPipeCount = 0;				

void Init()
{
	ResetGatherPipe();
}

void ResetGatherPipe()
{
	m_gatherPipeCount = 0;
}

void CheckGatherPipe()
{
	while (m_gatherPipeCount >= GATHER_PIPE_SIZE)
	{	
		// copy the GatherPipe
		memcpy(Memory::GetPointer(CPeripheralInterface::Fifo_CPUWritePointer), m_gatherPipe, GATHER_PIPE_SIZE);

        // [F|RES]: i thought GP is forced to mem1 ... strange 
        // memcpy(&Memory::GetMainRAMPtr()[CPeripheralInterface::Fifo_CPUWritePointer], m_gatherPipe, GATHER_PIPE_SIZE);

		// move back the spill bytes
		m_gatherPipeCount -= GATHER_PIPE_SIZE;
		// This could be dangerous, memmove or ?
		// Assuming that memcpy does its thing in the ordinary direction, there should be no problem.
		// Actually, this shouldn't ever be necessary. If we're above 2 "blocks" of data,
		// the above memcpy could take care of that easily. TODO
		memmove(m_gatherPipe, m_gatherPipe + GATHER_PIPE_SIZE, m_gatherPipeCount);

		// increase the CPUWritePointer
		CPeripheralInterface::Fifo_CPUWritePointer += GATHER_PIPE_SIZE; 
		if (CPeripheralInterface::Fifo_CPUWritePointer > CPeripheralInterface::Fifo_CPUEnd)
			_assert_msg_(DYNA_REC,0,"ARGH");

		if (CPeripheralInterface::Fifo_CPUWritePointer >= CPeripheralInterface::Fifo_CPUEnd)
			CPeripheralInterface::Fifo_CPUWritePointer = CPeripheralInterface::Fifo_CPUBase;		

		CommandProcessor::GatherPipeBursted();
	}
}

void Write8(const u8 _iValue, const u32 _iAddress)
{
//	LOG(GPFIFO, "GPFIFO #%x: 0x%02x",CPeripheralInterface::Fifo_CPUWritePointer+m_gatherPipeCount, _iValue);
	m_gatherPipe[m_gatherPipeCount] = _iValue;
	m_gatherPipeCount++;
	CheckGatherPipe();
}

void Write16(const u16 _iValue, const u32 _iAddress)
{
//	LOG(GPFIFO, "GPFIFO #%x: 0x%04x",CPeripheralInterface::Fifo_CPUWritePointer+m_gatherPipeCount, _iValue);
	*(u16*)(&m_gatherPipe[m_gatherPipeCount]) = Common::swap16(_iValue);
	m_gatherPipeCount+=2;
	CheckGatherPipe();
}

void Write32(const u32 _iValue, const u32 _iAddress)
{
#ifdef _DEBUG
	float floatvalue = *(float*)&_iValue;
//	LOG(GPFIFO, "GPFIFO #%x: 0x%08x / %f",CPeripheralInterface::Fifo_CPUWritePointer+m_gatherPipeCount, _iValue, floatvalue);
#endif
	*(u32*)(&m_gatherPipe[m_gatherPipeCount]) = Common::swap32(_iValue);
	m_gatherPipeCount+=4;
	CheckGatherPipe();
}

void FastWrite8(const u8 _iValue)
{
	m_gatherPipe[m_gatherPipeCount] = _iValue;
	m_gatherPipeCount++;
}

void FastWrite16(const u16 _iValue)
{
	*(u16*)(&m_gatherPipe[m_gatherPipeCount]) = Common::swap16(_iValue);
	m_gatherPipeCount += 2;
}

void FastWrite32(const u32 _iValue)
{
	*(u32*)(&m_gatherPipe[m_gatherPipeCount]) = Common::swap32(_iValue);
	m_gatherPipeCount += 4;
}

void FastWriteEnd()
{
	CheckGatherPipe();
}

} // end of namespace GPFifo
