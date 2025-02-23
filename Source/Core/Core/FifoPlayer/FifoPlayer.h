// Copyright 2011 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <functional>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "Common/Assert.h"
#include "Common/Config/Config.h"
#include "Core/FifoPlayer/FifoDataFile.h"
#include "Core/PowerPC/CPUCoreBase.h"
#include "VideoCommon/CPMemory.h"
#include "VideoCommon/OpcodeDecoding.h"

class FifoDataFile;
struct MemoryUpdate;

namespace Core
{
class System;
}
namespace CPU
{
enum class State;
}

// Story time:
// When FifoRecorder was created, efb copies weren't really used or they used efb2tex which ignored
// the underlying memory, so FifoRecorder didn't do anything special about the memory backing efb
// copies. This means the memory underlying efb copies go treated like regular textures and was
// baked into the fifo log. If you recorded with efb2ram on, the result of efb2ram would be baked
// into the fifo. If you recorded with efb2tex or efb off, random data would be included in the fifo
// log.
// Later the behaviour of efb2tex was changed to zero the underlying memory and check the hash of
// that.
// But this broke a whole lot of fifologs due to the following sequence of events:
//    1. fifoplayer would trigger the efb copy
//    2. Texture cache would zero the memory backing the texture and hash it.
//    3. Time passes.
//    4. fifoplayer would encounter the drawcall using the efb copy
//    5. fifoplayer would overwrite the memory backing the efb copy back to it's state when
//    recording.
//    6. Texture cache would hash the memory and see that the hash no-longer matches
//    7. Texture cache would load whatever data was now in memory as a texture either a baked in
//       efb2ram copy from recording time or just random data.
//    8. The output of fifoplayer would be wrong.

// To keep compatibility with old fifologs, we have this flag which signals texture cache to not
// bother hashing the memory and just assume the hash matched.
// At a later point proper efb copy support should be added to fiforecorder and this flag will
// change based on the version of the .dff file, but until then it will always be true when a
// fifolog is playing.

// Shitty global to fix a shitty problem
extern bool IsPlayingBackFifologWithBrokenEFBCopies;

enum class FramePartType
{
  Commands,
  PrimitiveData,
  EFBCopy,
};

struct FramePart
{
  constexpr FramePart(FramePartType type, u32 start, u32 end, const CPState& cpmem)
      : m_type(type), m_start(start), m_end(end), m_cpmem(cpmem)
  {
  }

  const FramePartType m_type;
  const u32 m_start;
  const u32 m_end;
  const CPState m_cpmem;
};

struct AnalyzedFrameInfo
{
  std::vector<FramePart> parts;
  Common::EnumMap<u32, FramePartType::EFBCopy> part_type_counts;
  std::vector<MemoryUpdate> memory_updates;

  void AddPart(FramePartType type, u32 start, u32 end, const CPState& cpmem)
  {
    parts.emplace_back(type, start, end, cpmem);
    part_type_counts[type]++;
  }
};

class FifoPlayer
{
public:
  using CallbackFunc = std::function<void()>;

  explicit FifoPlayer(Core::System& system);
  FifoPlayer(const FifoPlayer&) = delete;
  FifoPlayer(FifoPlayer&&) = delete;
  FifoPlayer& operator=(const FifoPlayer&) = delete;
  FifoPlayer& operator=(FifoPlayer&&) = delete;
  ~FifoPlayer();

  bool Open(const std::string& filename);
  void Close();

  // Returns a CPUCoreBase instance that can be injected into PowerPC as a
  // pseudo-CPU. The instance is only valid while the FifoPlayer is Open().
  // Returns nullptr if the FifoPlayer is not initialized correctly.
  // Play/Pause/Stop of the FifoLog can be controlled normally via the
  // PowerPC state.
  std::unique_ptr<CPUCoreBase> GetCPUCore();

  bool IsPlaying() const;

  FifoDataFile* GetFile() const { return m_File.get(); }
  u32 GetMaxObjectCount() const;
  u32 GetFrameObjectCount(u32 frame) const;
  u32 GetCurrentFrameObjectCount() const;
  u32 GetCurrentFrameNum() const { return m_CurrentFrame; }
  const AnalyzedFrameInfo& GetAnalyzedFrameInfo(u32 frame) const { return m_FrameInfo[frame]; }
  // Frame range
  u32 GetFrameRangeStart() const { return m_FrameRangeStart; }
  void SetFrameRangeStart(u32 start);

  u32 GetFrameRangeEnd() const { return m_FrameRangeEnd; }
  void SetFrameRangeEnd(u32 end);

  // Object range
  u32 GetObjectRangeStart() const { return m_ObjectRangeStart; }
  void SetObjectRangeStart(u32 start) { m_ObjectRangeStart = start; }
  u32 GetObjectRangeEnd() const { return m_ObjectRangeEnd; }
  void SetObjectRangeEnd(u32 end) { m_ObjectRangeEnd = end; }

  // Callbacks
  void SetFileLoadedCallback(CallbackFunc callback);
  void SetFrameWrittenCallback(CallbackFunc callback) { m_FrameWrittenCb = std::move(callback); }

  bool IsRunningWithFakeVideoInterfaceUpdates() const;

private:
  class CPUCore;
  friend class CPUCore;

  CPU::State AdvanceFrame();

  void WriteFrame(const FifoFrameInfo& frame, const AnalyzedFrameInfo& info);
  void WriteFramePart(const FramePart& part, u32* next_mem_update, const FifoFrameInfo& frame,
                      const AnalyzedFrameInfo& info);

  void WriteAllMemoryUpdates();
  void WriteMemory(const MemoryUpdate& memUpdate);

  // writes a range of data to the fifo
  // start and end must be relative to frame's fifo data so elapsed cycles are figured correctly
  void WriteFifo(const u8* data, u32 start, u32 end);

  void SetupFifo();

  void LoadMemory();
  void LoadRegisters();
  void LoadTextureMemory();
  void ClearEfb();

  void WriteCP(u32 address, u16 value);
  void WritePI(u32 address, u32 value);

  void FlushWGP();
  void WaitForGPUInactive();

  void LoadBPReg(u8 reg, u32 value);
  void LoadCPReg(u8 reg, u32 value);
  void LoadXFReg(u16 reg, u32 value);
  void LoadXFMem16(u16 address, const u32* data);

  bool ShouldLoadBP(u8 address);
  bool ShouldLoadXF(u8 address);

  bool IsIdleSet() const;
  bool IsHighWatermarkSet() const;

  void RefreshConfig();

  Core::System& m_system;

  bool m_Loop = true;
  // If enabled then all memory updates happen at once before the first frame
  bool m_EarlyMemoryUpdates = false;

  u32 m_CurrentFrame = 0;
  u32 m_FrameRangeStart = 0;
  u32 m_FrameRangeEnd = 0;

  u32 m_ObjectRangeStart = 0;
  u32 m_ObjectRangeEnd = 10000;

  u64 m_CyclesPerFrame = 0;
  u32 m_ElapsedCycles = 0;
  u32 m_FrameFifoSize = 0;

  CallbackFunc m_FileLoadedCb = nullptr;
  CallbackFunc m_FrameWrittenCb = nullptr;
  Config::ConfigChangedCallbackID m_config_changed_callback_id;

  std::unique_ptr<FifoDataFile> m_File;

  std::vector<AnalyzedFrameInfo> m_FrameInfo;
};
