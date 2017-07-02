// Copyright 2011 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <vector>

#include "Core/FifoPlayer/FifoDataFile.h"

class FifoRecorder
{
public:
  typedef void (*CallbackFunc)(void);

  FifoRecorder();
  ~FifoRecorder();

  void StartRecording(s32 numFrames, CallbackFunc finishedCb);
  void StopRecording();

  FifoDataFile* GetRecordedFile() const { return m_File; }
  // Called from video thread

  // Must write one full GP command at a time
  void WriteGPCommand(const u8* data, u32 size);

  // Track memory that has been used and write it to the fifolog if it has changed.
  // If memory is updated by the video backend (dynamicUpdate == true) take special care to make
  // sure the data
  // isn't baked into the fifolog.
  void UseMemory(u32 address, u32 size, MemoryUpdate::Type type, bool dynamicUpdate = false);

  void EndFrame(u32 fifoStart, u32 fifoEnd);

  // This function must be called before writing GP commands
  // bpMem must point to the actual bp mem array used by the plugin because it will be read as fifo
  // data is recorded
  void SetVideoMemory(const u32* bpMem, const u32* cpMem, const u32* xfMem, const u32* xfRegs,
                      u32 xfRegsSize, const u8* texMem);

  // Checked once per frame prior to callng EndFrame()
  bool IsRecording() const { return m_IsRecording; }
  static FifoRecorder& GetInstance();

private:
  // Accessed from both GUI and video threads

  // True if video thread should send data
  volatile bool m_IsRecording = false;
  // True if m_IsRecording was true during last frame
  volatile bool m_WasRecording = false;
  volatile bool m_RequestedRecordingEnd = false;
  volatile s32 m_RecordFramesRemaining = 0;
  volatile CallbackFunc m_FinishedCb = nullptr;

  FifoDataFile* volatile m_File = nullptr;

  // Accessed only from video thread

  bool m_SkipNextData = true;
  bool m_SkipFutureData = true;
  bool m_FrameEnded = false;
  FifoFrameInfo m_CurrentFrame;
  std::vector<u8> m_FifoData;
  std::vector<u8> m_Ram;
  std::vector<u8> m_ExRam;
};
