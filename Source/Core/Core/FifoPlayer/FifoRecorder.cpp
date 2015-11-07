// Copyright 2011 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include <mutex>

#include "Common/Thread.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/FifoPlayer/FifoRecorder.h"
#include "Core/HW/Memmap.h"

static FifoRecorder instance;
static std::recursive_mutex sMutex;

FifoRecorder::FifoRecorder() :
	m_IsRecording(false),
	m_WasRecording(false),
	m_RequestedRecordingEnd(false),
	m_RecordFramesRemaining(0),
	m_FinishedCb(nullptr),
	m_File(nullptr),
	m_SkipNextData(true),
	m_SkipFutureData(true),
	m_FrameEnded(false),
	m_Ram(Memory::RAM_SIZE),
	m_ExRam(Memory::EXRAM_SIZE)
{
}

FifoRecorder::~FifoRecorder()
{
	m_IsRecording = false;
}

void FifoRecorder::StartRecording(s32 numFrames, CallbackFunc finishedCb)
{
	sMutex.lock();

	delete m_File;

	m_File = new FifoDataFile;
	std::fill(m_Ram.begin(), m_Ram.end(), 0);
	std::fill(m_ExRam.begin(), m_ExRam.end(), 0);

	m_File->SetIsWii(SConfig::GetInstance().bWii);

	if (!m_IsRecording)
	{
		m_WasRecording = false;
		m_IsRecording = true;
		m_RecordFramesRemaining = numFrames;
	}

	m_RequestedRecordingEnd = false;
	m_FinishedCb = finishedCb;

	sMutex.unlock();
}

void FifoRecorder::StopRecording()
{
	m_RequestedRecordingEnd = true;
}

void FifoRecorder::WriteGPCommand(u8* data, u32 size)
{
	if (!m_SkipNextData)
	{
		FifoRecordAnalyzer::AnalyzeGPCommand(data);

		// Copy data to buffer
		size_t currentSize = m_FifoData.size();
		m_FifoData.resize(currentSize + size);
		memcpy(&m_FifoData[currentSize], data, size);
	}

	if (m_FrameEnded && m_FifoData.size() > 0)
	{
		size_t dataSize = m_FifoData.size();
		m_CurrentFrame.fifoDataSize = (u32)dataSize;
		m_CurrentFrame.fifoData = new u8[dataSize];
		memcpy(m_CurrentFrame.fifoData, m_FifoData.data(), dataSize);

		sMutex.lock();

		// Copy frame to file
		// The file will be responsible for freeing the memory allocated for each frame's fifoData
		m_File->AddFrame(m_CurrentFrame);

		if (m_FinishedCb && m_RequestedRecordingEnd)
			m_FinishedCb();

		sMutex.unlock();

		m_CurrentFrame.memoryUpdates.clear();
		m_FifoData.clear();
		m_FrameEnded = false;
	}

	m_SkipNextData = m_SkipFutureData;
}

void FifoRecorder::UseMemory(u32 address, u32 size, MemoryUpdate::Type type, bool dynamicUpdate)
{
	u8* curData;
	u8* newData;
	if (address & 0x10000000)
	{
		curData = &m_ExRam[address & Memory::EXRAM_MASK];
		newData = &Memory::m_pEXRAM[address & Memory::EXRAM_MASK];
	}
	else
	{
		curData = &m_Ram[address & Memory::RAM_MASK];
		newData = &Memory::m_pRAM[address & Memory::RAM_MASK];
	}

	if (!dynamicUpdate && memcmp(curData, newData, size) != 0)
	{
		// Update current memory
		memcpy(curData, newData, size);

		// Record memory update
		MemoryUpdate memUpdate;
		memUpdate.address = address;
		memUpdate.fifoPosition = (u32)(m_FifoData.size());
		memUpdate.size = size;
		memUpdate.type = type;
		memUpdate.data = new u8[size];
		memcpy(memUpdate.data, newData, size);

		m_CurrentFrame.memoryUpdates.push_back(memUpdate);
	}
	else if (dynamicUpdate)
	{
		// Shadow the data so it won't be recorded as changed by a future UseMemory
		memcpy(curData, newData, size);
	}
}

void FifoRecorder::EndFrame(u32 fifoStart, u32 fifoEnd)
{
	// m_IsRecording is assumed to be true at this point, otherwise this function would not be called

	sMutex.lock();

	m_FrameEnded = true;

	m_CurrentFrame.fifoStart = fifoStart;
	m_CurrentFrame.fifoEnd = fifoEnd;

	if (m_WasRecording)
	{
		// If recording a fixed number of frames then check if the end of the recording was reached
		if (m_RecordFramesRemaining > 0)
		{
			--m_RecordFramesRemaining;
			if (m_RecordFramesRemaining == 0)
				m_RequestedRecordingEnd = true;
		}
	}
	else
	{
		m_WasRecording = true;

		// Skip the first data which will be the frame copy command
		m_SkipNextData = true;
		m_SkipFutureData = false;

		m_FrameEnded = false;

		m_FifoData.reserve(1024 * 1024 * 4);
		m_FifoData.clear();
	}

	if (m_RequestedRecordingEnd)
	{
		// Skip data after the next time WriteFifoData is called
		m_SkipFutureData = true;
		// Signal video backend that it should not call this function when the next frame ends
		m_IsRecording = false;
	}

	sMutex.unlock();
}

void FifoRecorder::SetVideoMemory(u32 *bpMem, u32 *cpMem, u32 *xfMem, u32 *xfRegs, u32 xfRegsSize)
{
	sMutex.lock();

	if (m_File)
	{
		memcpy(m_File->GetBPMem(), bpMem, FifoDataFile::BP_MEM_SIZE * 4);
		memcpy(m_File->GetCPMem(), cpMem, FifoDataFile::CP_MEM_SIZE * 4);
		memcpy(m_File->GetXFMem(), xfMem, FifoDataFile::XF_MEM_SIZE * 4);

		u32 xfRegsCopySize = std::min((u32)FifoDataFile::XF_REGS_SIZE, xfRegsSize);
		memcpy(m_File->GetXFRegs(), xfRegs, xfRegsCopySize * 4);
	}

	FifoRecordAnalyzer::Initialize(cpMem);

	sMutex.unlock();
}

FifoRecorder& FifoRecorder::GetInstance()
{
	return instance;
}
