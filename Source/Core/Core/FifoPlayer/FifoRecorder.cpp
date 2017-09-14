// Copyright 2011 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/FifoPlayer/FifoRecorder.h"

#include <algorithm>
#include <cstring>

#include "Common/MsgHandler.h"
#include "Common/Thread.h"
#include "Core/ConfigManager.h"
#include "Core/FifoPlayer/FifoAnalyzer.h"
#include "Core/FifoPlayer/FifoRecordAnalyzer.h"
#include "Core/HW/Memmap.h"

static FifoRecorder instance;

FifoRecorder::FifoRecorder() = default;

void FifoRecorder::StartRecording(s32 numFrames, CallbackFunc finishedCb)
{
  std::lock_guard<std::recursive_mutex> lk(m_mutex);

  FifoAnalyzer::Init();

  m_File = std::make_unique<FifoDataFile>();

  // TODO: This, ideally, would be deallocated when done recording.
  //       However, care needs to be taken since global state
  //       and multithreading don't play well nicely together.
  //       The video thread may call into functions that utilize these
  //       despite 'end recording' being requested via StopRecording().
  //       (e.g. OpcodeDecoder calling UseMemory())
  //
  // Basically:
  //   - Singletons suck
  //   - Global variables suck
  //   - Multithreading with the above two sucks
  //
  m_Ram.resize(Memory::RAM_SIZE);
  m_ExRam.resize(Memory::EXRAM_SIZE);

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
}

void FifoRecorder::StopRecording()
{
  std::lock_guard<std::recursive_mutex> lk(m_mutex);
  m_RequestedRecordingEnd = true;
}

FifoDataFile* FifoRecorder::GetRecordedFile() const
{
  return m_File.get();
}

void FifoRecorder::WriteGPCommand(const u8* data, u32 size)
{
  if (!m_SkipNextData)
  {
    // Assumes data contains all information for the command
    // Calls FifoRecorder::UseMemory
    u32 analyzed_size = FifoAnalyzer::AnalyzeCommand(data, FifoAnalyzer::DECODE_RECORD);

    // Make sure FifoPlayer's command analyzer agrees about the size of the command.
    if (analyzed_size != size)
      PanicAlert("FifoRecorder: Expected command to be %i bytes long, we were given %i bytes",
                 analyzed_size, size);

    // Copy data to buffer
    size_t currentSize = m_FifoData.size();
    m_FifoData.resize(currentSize + size);
    memcpy(&m_FifoData[currentSize], data, size);
  }

  if (m_FrameEnded && m_FifoData.size() > 0)
  {
    m_CurrentFrame.fifoData = m_FifoData;

    {
      std::lock_guard<std::recursive_mutex> lk(m_mutex);

      // Copy frame to file
      // The file will be responsible for freeing the memory allocated for each frame's fifoData
      m_File->AddFrame(m_CurrentFrame);

      if (m_FinishedCb && m_RequestedRecordingEnd)
        m_FinishedCb();
    }

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
    memUpdate.type = type;
    memUpdate.data.resize(size);
    std::copy(newData, newData + size, memUpdate.data.begin());

    m_CurrentFrame.memoryUpdates.push_back(std::move(memUpdate));
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
  std::lock_guard<std::recursive_mutex> lk(m_mutex);

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
}

void FifoRecorder::SetVideoMemory(const u32* bpMem, const u32* cpMem, const u32* xfMem,
                                  const u32* xfRegs, u32 xfRegsSize, const u8* texMem)
{
  std::lock_guard<std::recursive_mutex> lk(m_mutex);

  if (m_File)
  {
    memcpy(m_File->GetBPMem(), bpMem, FifoDataFile::BP_MEM_SIZE * 4);
    memcpy(m_File->GetCPMem(), cpMem, FifoDataFile::CP_MEM_SIZE * 4);
    memcpy(m_File->GetXFMem(), xfMem, FifoDataFile::XF_MEM_SIZE * 4);

    u32 xfRegsCopySize = std::min((u32)FifoDataFile::XF_REGS_SIZE, xfRegsSize);
    memcpy(m_File->GetXFRegs(), xfRegs, xfRegsCopySize * 4);

    memcpy(m_File->GetTexMem(), texMem, FifoDataFile::TEX_MEM_SIZE);
  }

  FifoRecordAnalyzer::Initialize(cpMem);
}

bool FifoRecorder::IsRecording() const
{
  return m_IsRecording;
}

FifoRecorder& FifoRecorder::GetInstance()
{
  return instance;
}
