// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Common/ChunkFile.h"
#include "Common/Common.h"

#include "Core/HW/GPFifo.h"
#include "Core/HW/Memmap.h"
#include "Core/HW/ProcessorInterface.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/PowerPC/JitCommon/JitBase.h"

#include "VideoCommon/VideoBackendBase.h"

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

u8 GC_ALIGNED32(m_gatherPipe[GATHER_PIPE_SIZE*16]); //more room, for the fastmodes

// pipe counter
u32 m_gatherPipeCount = 0;

void DoState(PointerWrap &p)
{
	p.Do(m_gatherPipe);
	p.Do(m_gatherPipeCount);
}

void Init()
{
	ResetGatherPipe();
	memset(m_gatherPipe, 0, sizeof(m_gatherPipe));
}

bool IsEmpty()
{
	return m_gatherPipeCount == 0;
}

void ResetGatherPipe()
{
	m_gatherPipeCount = 0;
}

void STACKALIGN CheckGatherPipe()
{
	if (m_gatherPipeCount >= GATHER_PIPE_SIZE)
	{
		u32 cnt;
		u8* curMem = Memory::GetPointer(ProcessorInterface::Fifo_CPUWritePointer);
		for (cnt = 0; m_gatherPipeCount >= GATHER_PIPE_SIZE; cnt += GATHER_PIPE_SIZE)
		{
			// copy the GatherPipe
			memcpy(curMem, m_gatherPipe + cnt, GATHER_PIPE_SIZE);
			m_gatherPipeCount -= GATHER_PIPE_SIZE;

			// increase the CPUWritePointer
			if (ProcessorInterface::Fifo_CPUWritePointer == ProcessorInterface::Fifo_CPUEnd)
			{
				ProcessorInterface::Fifo_CPUWritePointer = ProcessorInterface::Fifo_CPUBase;
				curMem = Memory::GetPointer(ProcessorInterface::Fifo_CPUWritePointer);
			}
			else
			{
				curMem += GATHER_PIPE_SIZE;
				ProcessorInterface::Fifo_CPUWritePointer += GATHER_PIPE_SIZE;
			}

			g_video_backend->Video_GatherPipeBursted();
		}

		// move back the spill bytes
		memmove(m_gatherPipe, m_gatherPipe + cnt, m_gatherPipeCount);

		// Profile where the FIFO writes are occurring.
		if (jit && PC != 0 && (jit->js.fifoWriteAddresses.find(PC)) == (jit->js.fifoWriteAddresses.end()))
		{
			// Log only stores, fp stores and ps stores, filtering out other instructions arrived via optimizeGatherPipe
			int type = GetOpInfo(Memory::ReadUnchecked_U32(PC))->type;
			if (type == OPTYPE_STORE || type == OPTYPE_STOREFP || (type == OPTYPE_PS && !strcmp(GetOpInfo(Memory::ReadUnchecked_U32(PC))->opname, "psq_st")))
			{
				jit->js.fifoWriteAddresses.insert(PC);

				// Invalidate the JIT block so that it gets recompiled with the external exception check included.
				jit->GetBlockCache()->InvalidateICache(PC, 4);
			}
		}
	}
}

void Write8(const u8 iValue, const u32 iAddress)
{
//	LOG(GPFIFO, "GPFIFO #%x: 0x%02x",ProcessorInterface::Fifo_CPUWritePointer+m_gatherPipeCount, iValue);
	FastWrite8(iValue);
	CheckGatherPipe();
}

void Write16(const u16 iValue, const u32 iAddress)
{
//	LOG(GPFIFO, "GPFIFO #%x: 0x%04x",ProcessorInterface::Fifo_CPUWritePointer+m_gatherPipeCount, iValue);
	FastWrite16(iValue);
	CheckGatherPipe();
}

void Write32(const u32 iValue, const u32 iAddress)
{
//#ifdef _DEBUG
//	float floatvalue = *(float*)&iValue;
//	LOG(GPFIFO, "GPFIFO #%x: 0x%08x / %f",ProcessorInterface::Fifo_CPUWritePointer+m_gatherPipeCount, iValue, floatvalue);
//#endif
	FastWrite32(iValue);
	CheckGatherPipe();
}

void Write64(const u64 iValue, const u32 iAddress)
{
	FastWrite64(iValue);
	CheckGatherPipe();
}

void FastWrite8(const u8 iValue)
{
	m_gatherPipe[m_gatherPipeCount] = iValue;
	++m_gatherPipeCount;
}

void FastWrite16(const u16 iValue)
{
	*(u16*)(&m_gatherPipe[m_gatherPipeCount]) = Common::swap16(iValue);
	m_gatherPipeCount += 2;
}

void FastWrite32(const u32 iValue)
{
	*(u32*)(&m_gatherPipe[m_gatherPipeCount]) = Common::swap32(iValue);
	m_gatherPipeCount += 4;
}

void FastWrite64(const u64 iValue)
{
	*(u64*)(&m_gatherPipe[m_gatherPipeCount]) = Common::swap64(iValue);
	m_gatherPipeCount += 8;
}

void FastWriteEnd()
{
	CheckGatherPipe();
}

} // end of namespace GPFifo
