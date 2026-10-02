// Copyright 2011 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <functional>
#include <memory>
#include <mutex>
#include <vector>

#include "Common/Assert.h"
#include "Common/HookableEvent.h"
#include "Core/FifoPlayer/FifoDataFile.h"

namespace Core
{
class System;
}

class FifoRecorder
{
public:
  using CallbackFunc = std::function<void()>;

  explicit FifoRecorder(Core::System& system);
  FifoRecorder(const FifoRecorder&) = delete;
  FifoRecorder(FifoRecorder&&) = delete;
  FifoRecorder& operator=(const FifoRecorder&) = delete;
  FifoRecorder& operator=(FifoRecorder&&) = delete;
  ~FifoRecorder();

  void StartRecording(s32 numFrames, CallbackFunc finishedCb);
  void StopRecording();

  bool IsRecordingDone() const;

  FifoDataFile* GetRecordedFile() const;
  // Called from video thread

  // Must write one full GP command at a time
  void WriteGPCommand(const u8* data, u32 size, bool in_display_list);

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
  bool IsRecording() const;

private:
  class FifoRecordAnalyzer;

  void RecordInitialVideoMemory();

  // Accessed from both GUI and video threads

  std::recursive_mutex m_mutex;
  // True if video thread should send data
  bool m_IsRecording = false;
  // True if m_IsRecording was true during last frame
  bool m_WasRecording = false;
  bool m_RequestedRecordingEnd = false;
  s32 m_RecordFramesRemaining = 0;
  CallbackFunc m_FinishedCb;
  std::unique_ptr<FifoDataFile> m_File;

  // Accessed only from video thread

  bool m_SkipNextData = true;
  bool m_SkipFutureData = true;
  bool m_FrameEnded = false;
  FifoFrameInfo m_CurrentFrame;
  std::unique_ptr<FifoRecordAnalyzer> m_record_analyzer;
  std::vector<u8> m_FifoData;
  std::vector<u8> m_Ram;
  std::vector<u8> m_ExRam;

  Common::EventHook m_end_of_frame_event;

  Core::System& m_system;
};
