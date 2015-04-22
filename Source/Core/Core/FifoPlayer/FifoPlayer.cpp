// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <algorithm>

#include "Common/CommonTypes.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/CoreTiming.h"
#include "Core/Host.h"
#include "Core/FifoPlayer/FifoDataFile.h"
#include "Core/FifoPlayer/FifoPlayer.h"
#include "Core/HW/GPFifo.h"
#include "Core/HW/Memmap.h"
#include "Core/HW/SystemTimers.h"
#include "Core/PowerPC/PowerPC.h"
#include "VideoCommon/BPMemory.h"

FifoPlayer::~FifoPlayer()
{
	delete m_File;
}

bool FifoPlayer::Open(const std::string& filename)
{
	Close();

	m_File = FifoDataFile::Load(filename, false);

	if (m_File)
	{
		FifoPlaybackAnalyzer analyzer;
		analyzer.AnalyzeFrames(m_File, m_FrameInfo);

		m_FrameRangeEnd = m_File->GetFrameCount();
	}

	if (m_FileLoadedCb)
		m_FileLoadedCb();

	return (m_File != nullptr);
}

void FifoPlayer::Close()
{
	delete m_File;
	m_File = nullptr;

	m_FrameRangeStart = 0;
	m_FrameRangeEnd = 0;
}

bool FifoPlayer::Play()
{
	if (!m_File)
		return false;

	if (m_File->GetFrameCount() == 0)
		return false;

	m_CurrentFrame = m_FrameRangeStart;

	LoadMemory();

	// This loop replaces the CPU loop that occurs when a game is run
	while (PowerPC::GetState() != PowerPC::CPU_POWERDOWN)
	{
		if (PowerPC::GetState() == PowerPC::CPU_RUNNING)
		{
			if (m_CurrentFrame >= m_FrameRangeEnd)
			{
				if (m_Loop)
				{
					m_CurrentFrame = m_FrameRangeStart;

					PowerPC::ppcState.downcount = 0;
					CoreTiming::Advance();
				}
				else
				{
					PowerPC::Stop();
					Host_Message(WM_USER_STOP);
				}
			}
			else
			{
				if (m_FrameWrittenCb)
					m_FrameWrittenCb();

				if (m_EarlyMemoryUpdates && m_CurrentFrame == m_FrameRangeStart)
					WriteAllMemoryUpdates();

				WriteFrame(m_File->GetFrame(m_CurrentFrame), m_FrameInfo[m_CurrentFrame]);

				++m_CurrentFrame;
			}
		}
	}

	return true;
}

u32 FifoPlayer::GetFrameObjectCount()
{
	if (m_CurrentFrame < m_FrameInfo.size())
	{
		return (u32)(m_FrameInfo[m_CurrentFrame].objectStarts.size());
	}

	return 0;
}

void FifoPlayer::SetFrameRangeStart(u32 start)
{
	if (m_File)
	{
		u32 frameCount = m_File->GetFrameCount();
		if (start > frameCount)
			start = frameCount;

		m_FrameRangeStart = start;
		if (m_FrameRangeEnd < start)
			m_FrameRangeEnd = start;

		if (m_CurrentFrame < m_FrameRangeStart)
			m_CurrentFrame = m_FrameRangeStart;
	}
}

void FifoPlayer::SetFrameRangeEnd(u32 end)
{
	if (m_File)
	{
		u32 frameCount = m_File->GetFrameCount();
		if (end > frameCount)
			end = frameCount;

		m_FrameRangeEnd = end;
		if (m_FrameRangeStart > end)
			m_FrameRangeStart = end;

		if (m_CurrentFrame >= m_FrameRangeEnd)
			m_CurrentFrame = m_FrameRangeStart;
	}
}

FifoPlayer &FifoPlayer::GetInstance()
{
	static FifoPlayer instance;
	return instance;
}

FifoPlayer::FifoPlayer() :
	m_CurrentFrame(0),
	m_FrameRangeStart(0),
	m_FrameRangeEnd(0),
	m_ObjectRangeStart(0),
	m_ObjectRangeEnd(10000),
	m_EarlyMemoryUpdates(false),
	m_FileLoadedCb(nullptr),
	m_FrameWrittenCb(nullptr),
	m_File(nullptr)
{
	m_Loop = SConfig::GetInstance().m_LocalCoreStartupParameter.bLoopFifoReplay;
}

void FifoPlayer::WriteFrame(const FifoFrameInfo &frame, const AnalyzedFrameInfo &info)
{
	// Core timing information
	m_CyclesPerFrame = SystemTimers::GetTicksPerSecond() / 60;
	m_ElapsedCycles = 0;
	m_FrameFifoSize = frame.fifoDataSize;

	// Determine start and end objects
	u32 numObjects = (u32)(info.objectStarts.size());
	u32 drawStart = std::min(numObjects, m_ObjectRangeStart);
	u32 drawEnd = std::min(numObjects - 1, m_ObjectRangeEnd);

	u32 position = 0;
	u32 memoryUpdate = 0;

	// Skip memory updates during frame if true
	if (m_EarlyMemoryUpdates)
	{
		memoryUpdate = (u32)(frame.memoryUpdates.size());
	}

	if (numObjects > 0)
	{
		u32 objectNum = 0;

		// Write fifo data skipping objects before the draw range
		while (objectNum < drawStart)
		{
			WriteFramePart(position, info.objectStarts[objectNum], memoryUpdate, frame, info);

			position = info.objectEnds[objectNum];
			++objectNum;
		}

		// Write objects in draw range
		if (objectNum < numObjects && drawStart <= drawEnd)
		{
			objectNum = drawEnd;
			WriteFramePart(position, info.objectEnds[objectNum], memoryUpdate, frame, info);
			position = info.objectEnds[objectNum];
			++objectNum;
		}

		// Write fifo data skipping objects after the draw range
		while (objectNum < numObjects)
		{
			WriteFramePart(position, info.objectStarts[objectNum], memoryUpdate, frame, info);

			position = info.objectEnds[objectNum];
			++objectNum;
		}
	}

	// Write data after the last object
	WriteFramePart(position, frame.fifoDataSize, memoryUpdate, frame, info);

	FlushWGP();
}

void FifoPlayer::WriteFramePart(u32 dataStart, u32 dataEnd, u32 &nextMemUpdate, const FifoFrameInfo &frame, const AnalyzedFrameInfo &info)
{
	u8 *data = frame.fifoData;

	while (nextMemUpdate < frame.memoryUpdates.size() && dataStart < dataEnd)
	{
		const MemoryUpdate &memUpdate = info.memoryUpdates[nextMemUpdate];

		if (memUpdate.fifoPosition < dataEnd)
		{
			if (dataStart < memUpdate.fifoPosition)
			{
				WriteFifo(data, dataStart, memUpdate.fifoPosition);
				dataStart = memUpdate.fifoPosition;
			}

			WriteMemory(memUpdate);

			++nextMemUpdate;
		}
		else
		{
			WriteFifo(data, dataStart, dataEnd);
			dataStart = dataEnd;
		}
	}

	if (dataStart < dataEnd)
		WriteFifo(data, dataStart, dataEnd);
}

void FifoPlayer::WriteAllMemoryUpdates()
{
	_assert_(m_File);

	for (u32 frameNum = 0; frameNum < m_File->GetFrameCount(); ++frameNum)
	{
		const FifoFrameInfo &frame = m_File->GetFrame(frameNum);
		for (auto& update : frame.memoryUpdates)
		{
			WriteMemory(update);
		}
	}
}

void FifoPlayer::WriteMemory(const MemoryUpdate& memUpdate)
{
	u8 *mem = nullptr;

	if (memUpdate.address & 0x10000000)
		mem = &Memory::m_pEXRAM[memUpdate.address & Memory::EXRAM_MASK];
	else
		mem = &Memory::m_pRAM[memUpdate.address & Memory::RAM_MASK];

	memcpy(mem, memUpdate.data, memUpdate.size);
}

void FifoPlayer::WriteFifo(u8 *data, u32 start, u32 end)
{
	u32 written = start;
	u32 lastBurstEnd = end - 1;

	// Write up to 256 bytes at a time
	while (written < end)
	{
		u32 burstEnd = std::min(written + 255, lastBurstEnd);

		while (written < burstEnd)
			GPFifo::FastWrite8(data[written++]);

		GPFifo::Write8(data[written++]);

		// Advance core timing
		u32 elapsedCycles = u32(((u64)written * m_CyclesPerFrame) / m_FrameFifoSize);
		u32 cyclesUsed = elapsedCycles - m_ElapsedCycles;
		m_ElapsedCycles = elapsedCycles;

		PowerPC::ppcState.downcount -= cyclesUsed;
		CoreTiming::Advance();
	}
}

void FifoPlayer::SetupFifo()
{
	WriteCP(0x02, 0); // disable read, BP, interrupts
	WriteCP(0x04, 7); // clear overflow, underflow, metrics

	const FifoFrameInfo& frame = m_File->GetFrame(m_CurrentFrame);

	// Set fifo bounds
	WriteCP(0x20, frame.fifoStart);
	WriteCP(0x22, frame.fifoStart >> 16);
	WriteCP(0x24, frame.fifoEnd);
	WriteCP(0x26, frame.fifoEnd >> 16);

	// Set watermarks
	u32 fifoSize = frame.fifoEnd - frame.fifoStart;
	WriteCP(0x28, fifoSize);
	WriteCP(0x2a, fifoSize >> 16);
	WriteCP(0x2c, 0);
	WriteCP(0x2e, 0);

	// Set R/W pointers to fifo start
	WriteCP(0x30, 0);
	WriteCP(0x32, 0);
	WriteCP(0x34, frame.fifoStart);
	WriteCP(0x36, frame.fifoStart >> 16);
	WriteCP(0x38, frame.fifoStart);
	WriteCP(0x3a, frame.fifoStart >> 16);

	// Set fifo bounds
	WritePI(12, frame.fifoStart);
	WritePI(16, frame.fifoEnd);

	// Set write pointer
	WritePI(20, frame.fifoStart);
	FlushWGP();
	WritePI(20, frame.fifoStart);

	WriteCP(0x02, 17); // enable read & GP link
}

void FifoPlayer::LoadMemory()
{
	UReg_MSR newMSR;
	newMSR.DR = 1;
	newMSR.IR = 1;
	MSR = newMSR.Hex;
	PowerPC::ppcState.spr[SPR_IBAT0U] = 0x80001fff;
	PowerPC::ppcState.spr[SPR_IBAT0L] = 0x00000002;
	PowerPC::ppcState.spr[SPR_DBAT0U] = 0x80001fff;
	PowerPC::ppcState.spr[SPR_DBAT0L] = 0x00000002;
	PowerPC::ppcState.spr[SPR_DBAT1U] = 0xc0001fff;
	PowerPC::ppcState.spr[SPR_DBAT1L] = 0x0000002a;

	Memory::Clear();

	SetupFifo();

	u32 *regs = m_File->GetBPMem();
	for (int i = 0; i < FifoDataFile::BP_MEM_SIZE; ++i)
	{
		if (ShouldLoadBP(i))
			LoadBPReg(i, regs[i]);
	}

	regs = m_File->GetCPMem();
	LoadCPReg(0x30, regs[0x30]);
	LoadCPReg(0x40, regs[0x40]);
	LoadCPReg(0x50, regs[0x50]);
	LoadCPReg(0x60, regs[0x60]);

	for (int i = 0; i < 8; ++i)
	{
		LoadCPReg(0x70 + i, regs[0x70 + i]);
		LoadCPReg(0x80 + i, regs[0x80 + i]);
		LoadCPReg(0x90 + i, regs[0x90 + i]);
	}

	for (int i = 0; i < 16; ++i)
	{
		LoadCPReg(0xa0 + i, regs[0xa0 + i]);
		LoadCPReg(0xb0 + i, regs[0xb0 + i]);
	}

	regs = m_File->GetXFMem();
	for (int i = 0; i < FifoDataFile::XF_MEM_SIZE; i += 16)
		LoadXFMem16(i, &regs[i]);

	regs = m_File->GetXFRegs();
	for (int i = 0; i < FifoDataFile::XF_REGS_SIZE; ++i)
		LoadXFReg(i, regs[i]);

	FlushWGP();
}

void FifoPlayer::WriteCP(u32 address, u16 value)
{
	PowerPC::Write_U16(value, 0xCC000000 | address);
}

void FifoPlayer::WritePI(u32 address, u32 value)
{
	PowerPC::Write_U32(value, 0xCC003000 | address);
}

void FifoPlayer::FlushWGP()
{
	// Send 31 0s through the WGP
	for (int i = 0; i < 7; ++i)
		GPFifo::Write32(0);
	GPFifo::Write16(0);
	GPFifo::Write8(0);

	GPFifo::ResetGatherPipe();
}

void FifoPlayer::LoadBPReg(u8 reg, u32 value)
{
	GPFifo::Write8(0x61); // load BP reg

	u32 cmd = (reg << 24) & 0xff000000;
	cmd |= (value & 0x00ffffff);
	GPFifo::Write32(cmd);
}

void FifoPlayer::LoadCPReg(u8 reg, u32 value)
{
	GPFifo::Write8(0x08); // load CP reg
	GPFifo::Write8(reg);
	GPFifo::Write32(value);
}

void FifoPlayer::LoadXFReg(u16 reg, u32 value)
{
	GPFifo::Write8(0x10); // load XF reg
	GPFifo::Write32((reg & 0x0fff) | 0x1000); // load 4 bytes into reg
	GPFifo::Write32(value);
}

void FifoPlayer::LoadXFMem16(u16 address, u32 *data)
{
	// Loads 16 * 4 bytes in xf memory starting at address
	GPFifo::Write8(0x10); // load XF reg
	GPFifo::Write32(0x000f0000 | (address & 0xffff)); // load 16 * 4 bytes into address
	for (int i = 0; i < 16; ++i)
		GPFifo::Write32(data[i]);
}

bool FifoPlayer::ShouldLoadBP(u8 address)
{
	switch (address)
	{
	case BPMEM_SETDRAWDONE:
	case BPMEM_PE_TOKEN_ID:
	case BPMEM_PE_TOKEN_INT_ID:
	case BPMEM_TRIGGER_EFB_COPY:
	case BPMEM_LOADTLUT1:
	case BPMEM_PERF1:
		return false;
	default:
		return true;
	}
}
