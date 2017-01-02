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

  void EndFrame();

  // This function must be called before writing GP commands
  // bpMem must point to the actual bp mem array used by the plugin because it will be read as fifo
  // data is recorded
  void SetVideoMemory();

  // Checked once per frame prior to callng EndFrame()
  bool IsRecording() const { return m_CurrentlyRecording; }
  static FifoRecorder& GetInstance();

private:
  // Accessed from both GUI and video threads, protected by a mutex

  bool m_CurrentlyRecording;
  bool m_RequestedRecordingEnd;
  s32 m_RecordFramesRemaining;
  CallbackFunc m_FinishedCb;

  FifoDataFile* m_File;

private:
  // Accessed only from video thread

  bool m_SkipNextData;
  bool m_SkipFutureData;
  bool m_FrameEnded;
  FifoFrameInfo m_CurrentFrame;
  std::vector<u8> m_FifoData;
  std::vector<u8> m_Ram;
  std::vector<u8> m_ExRam;
};
