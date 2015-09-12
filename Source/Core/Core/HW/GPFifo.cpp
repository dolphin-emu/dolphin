// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cstring>

#include "Common/ChunkFile.h"
#include "Common/CommonFuncs.h"
#include "Common/CommonTypes.h"

#include "Core/HW/GPFifo.h"
#include "Core/HW/Memmap.h"
#include "Core/HW/ProcessorInterface.h"
#include "Core/PowerPC/JitInterface.h"
#include "Core/PowerPC/PowerPC.h"

#include "VideoCommon/VideoBackendBase.h"

namespace GPFifo
{

// 32 Byte gather pipe with extra space
// Overfilling is no problem (up to the real limit), CheckGatherPipe will blast the
// contents in nicely sized chunks
//
// Other optimizations to think about:
// - If the GP is NOT linked to the FIFO, just blast to memory byte by word
// - If the GP IS linked to the FIFO, use a fast wrapping buffer and skip writing to memory
//
// Both of these should actually work! Only problem is that we have to decide at run time,
// the same function could use both methods. Compile 2 different versions of each such block?

// More room for the fastmodes
alignas(32) u8 m_gatherPipe[GATHER_PIPE_SIZE * 16];

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

static void UpdateGatherPipe()
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
}

void FastCheckGatherPipe()
{
	if (m_gatherPipeCount >= GATHER_PIPE_SIZE)
	{
		UpdateGatherPipe();
	}
}

void CheckGatherPipe()
{
	if (m_gatherPipeCount >= GATHER_PIPE_SIZE)
	{
		UpdateGatherPipe();

		// Profile where slow FIFO writes are occurring.
		JitInterface::CompileExceptionCheck(JitInterface::ExceptionType::EXCEPTIONS_FIFO_WRITE);
	}
}

void Write8(const u8 value)
{
	FastWrite8(value);
	CheckGatherPipe();
}

void Write16(const u16 value)
{
	FastWrite16(value);
	CheckGatherPipe();
}

void Write32(const u32 value)
{
	FastWrite32(value);
	CheckGatherPipe();
}

void Write64(const u64 value)
{
	FastWrite64(value);
	CheckGatherPipe();
}

void FastWrite8(const u8 value)
{
	m_gatherPipe[m_gatherPipeCount] = value;
	++m_gatherPipeCount;
}

void FastWrite16(u16 value)
{
	value = Common::swap16(value);
	std::memcpy(&m_gatherPipe[m_gatherPipeCount], &value, sizeof(u16));
	m_gatherPipeCount += sizeof(u16);
}

void FastWrite32(u32 value)
{
	value = Common::swap32(value);
	std::memcpy(&m_gatherPipe[m_gatherPipeCount], &value, sizeof(u32));
	m_gatherPipeCount += sizeof(u32);
}

void FastWrite64(u64 value)
{
	value = Common::swap64(value);
	std::memcpy(&m_gatherPipe[m_gatherPipeCount], &value, sizeof(u64));
	m_gatherPipeCount += sizeof(u64);
}

} // end of namespace GPFifo
