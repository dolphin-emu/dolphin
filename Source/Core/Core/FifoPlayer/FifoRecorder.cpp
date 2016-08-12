// Copyright 2011 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include <mutex>

#include "Core/FifoPlayer/FifoRecorder.h"

#include "Common/Thread.h"
#include "Core/ConfigManager.h"
#include "Core/FifoPlayer/FifoAnalyzer.h"
#include "Core/FifoPlayer/FifoRecordAnalyzer.h"
#include "Core/HW/Memmap.h"
#include "Common/Hooks.h"
#include "VideoCommon/BPMemory.h"
#include "VideoCommon/CommandProcessor.h"
#include "VideoCommon/XFMemory.h"

static FifoRecorder instance;
static std::recursive_mutex sMutex;

FifoRecorder::FifoRecorder()
    : m_CurrentlyRecording(false), m_RequestedRecordingEnd(false), m_RecordFramesRemaining(0),
      m_FinishedCb(nullptr), m_File(nullptr), m_SkipNextData(true), m_SkipFutureData(true),
      m_FrameEnded(false), m_Ram(Memory::RAM_SIZE), m_ExRam(Memory::EXRAM_SIZE)
{
}

void FifoRecorder::StartRecording(s32 numFrames, CallbackFunc finishedCb)
{
  std::lock_guard<std::recursive_mutex> lk(sMutex);

  delete m_File;

  FifoAnalyzer::Init();

  m_File = new FifoDataFile;
  std::fill(m_Ram.begin(), m_Ram.end(), 0);
  std::fill(m_ExRam.begin(), m_ExRam.end(), 0);

  m_File->SetIsWii(SConfig::GetInstance().bWii);

  if (!m_RecordFramesRemaining)
  {
    // Actual recording doesn't start until the end of the current frame
    m_CurrentlyRecording = false;
    m_RecordFramesRemaining = numFrames;
  }

  m_RequestedRecordingEnd = false;
  m_FinishedCb = finishedCb;

  Hook::XFBCopyEvent.RegisterCallback("FifoRecorder", [&](u32, u32, u32) { this->EndFrame(); });
}

void FifoRecorder::StopRecording()
{
  m_RequestedRecordingEnd = true;
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
      std::lock_guard<std::recursive_mutex> lk(sMutex);

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

void FifoRecorder::EndFrame()
{
  // The event handler for this frame only gets installed when we are recording
  std::lock_guard<std::recursive_mutex> lk(sMutex);

  m_FrameEnded = true;

  // TODO: It's not really valid to assume the fifo stays in a single address over the entire frame.
  //       Perhaps in the future we hook writes to the command processor registers.
  m_CurrentFrame.fifoStart = CommandProcessor::fifo.CPBase;
  m_CurrentFrame.fifoEnd = CommandProcessor::fifo.CPEnd;

  if (m_CurrentlyRecording)
  {
    // If recording a fixed number of frames then check if the end of the recording was reached
    if (m_RecordFramesRemaining > 0)
    {
      --m_RecordFramesRemaining;
      if (m_RecordFramesRemaining == 0)
        m_RequestedRecordingEnd = true;
    }
  }
  else  // Start recording
  {
    m_CurrentlyRecording = true;

    // Skip the first data which will be the frame copy command
    m_SkipNextData = true;
    m_SkipFutureData = false;

    m_FrameEnded = false;

    m_FifoData.reserve(1024 * 1024 * 4);
    m_FifoData.clear();

    // Copy the starting state into the fifolog
    SetVideoMemory();
  }

  if (m_RequestedRecordingEnd)
  {
    // Skip data after the next time WriteFifoData is called
    m_SkipFutureData = true;
    // Delete our callback, so this function isn't called when the next frame ends
    Hook::XFBCopyEvent.DeleteCallback("FifoRecorder");
  }
}

void FifoRecorder::SetVideoMemory()
{
  std::lock_guard<std::recursive_mutex> lk(sMutex);

  if (m_File)
  {
    memcpy(m_File->GetBPMem(), &bpmem, FifoDataFile::BP_MEM_SIZE * 4);
    memset(m_File->GetCPMem(), 0, FifoDataFile::CP_MEM_SIZE * 4);
    FillCPMemoryArray(m_File->GetCPMem());
    memcpy(m_File->GetXFMem(), &xfmem, FifoDataFile::XF_MEM_SIZE * 4);
    void* xfRegs = &xfmem + FifoDataFile::XF_MEM_SIZE;
    memcpy(m_File->GetXFRegs(), xfRegs, FifoDataFile::XF_REGS_SIZE * 4);
  }

  FifoRecordAnalyzer::Initialize(m_File->GetCPMem());
}

FifoRecorder& FifoRecorder::GetInstance()
{
  return instance;
}
