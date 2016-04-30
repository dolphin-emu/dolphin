// Copyright 2011 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <string>
#include <vector>

#include "Core/FifoPlayer/FifoPlaybackAnalyzer.h"
#include "Core/PowerPC/CPUCoreBase.h"

class FifoDataFile;
struct MemoryUpdate;
struct AnalyzedFrameInfo;

// Story time:
// When FifoRecorder was created, efb copies weren't really used or they used efb2tex which ignored
// the underlying memory, so FifoRecorder didn't do anything special about the memory backing efb
// copies. This means the memory underlying efb copies go treated like regular textures and was
// baked into the fifo log. If you recorded with efb2ram on, the result of efb2ram would be baked
// into the fifo. If you recorded with efb2tex or efb off, random data would be included in the fifo
// log.
// Later the behaviour of efb2tex was changed to zero the underlying memory and check the hash of that.
// But this broke a whole lot of fifologs due to the following sequence of events:
//    1. fifoplayer would trigger the efb copy
//    2. Texture cache would zero the memory backing the texture and hash it.
//    3. Time passes.
//    4. fifoplayer would encounter the drawcall using the efb copy
//    5. fifoplayer would overwrite the memory backing the efb copy back to it's state when recording.
//    6. Texture cache would hash the memory and see that the hash no-longer matches
//    7. Texture cache would load whatever data was now in memory as a texture either a baked in
//       efb2ram copy from recording time or just random data.
//    8. The output of fifoplayer would be wrong.

// To keep compatibility with old fifologs, we have this flag which signals texture cache to not bother
// hashing the memory and just assume the hash matched.
// At a later point proper efb copy support should be added to fiforecorder and this flag will change
// based on the version of the .dff file, but until then it will always be true when a fifolog is playing.

// Shitty global to fix a shitty problem
extern bool IsPlayingBackFifologWithBrokenEFBCopies;

class FifoPlayer
{
public:
	typedef void(*CallbackFunc)(void);

	~FifoPlayer();

	bool Open(const std::string& filename);
	void Close();

	// Returns a CPUCoreBase instance that can be injected into PowerPC as a
	// pseudo-CPU. The instance is only valid while the FifoPlayer is Open().
	// Returns nullptr if the FifoPlayer is not initialized correctly.
	// Play/Pause/Stop of the FifoLog can be controlled normally via the
	// PowerPC state.
	std::unique_ptr<CPUCoreBase> GetCPUCore();

	FifoDataFile *GetFile() { return m_File; }

	u32 GetFrameObjectCount();
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
	void SetObjectRangeEnd(u32 end)  { m_ObjectRangeEnd = end; }

	// If enabled then all memory updates happen at once before the first frame
	// Default is disabled
	void SetEarlyMemoryUpdates(bool enabled) { m_EarlyMemoryUpdates = enabled; }

	// Callbacks
	void SetFileLoadedCallback(CallbackFunc callback) { m_FileLoadedCb = callback; }
	void SetFrameWrittenCallback(CallbackFunc callback) { m_FrameWrittenCb = callback; }

	static FifoPlayer &GetInstance();

private:
	class CPUCore;

	FifoPlayer();

	int AdvanceFrame();

	void WriteFrame(const FifoFrameInfo& frame, const AnalyzedFrameInfo &info);
	void WriteFramePart(u32 dataStart, u32 dataEnd, u32 &nextMemUpdate, const FifoFrameInfo& frame, const AnalyzedFrameInfo& info);

	void WriteAllMemoryUpdates();
	void WriteMemory(const MemoryUpdate &memUpdate);

	// writes a range of data to the fifo
	// start and end must be relative to frame's fifo data so elapsed cycles are figured correctly
	void WriteFifo(u8* data, u32 start, u32 end);

	void SetupFifo();

	void LoadMemory();

	void WriteCP(u32 address, u16 value);
	void WritePI(u32 address, u32 value);

	void FlushWGP();

	void LoadBPReg(u8 reg, u32 value);
	void LoadCPReg(u8 reg, u32 value);
	void LoadXFReg(u16 reg, u32 value);
	void LoadXFMem16(u16 address, u32 *data);

	bool ShouldLoadBP(u8 address);

	static bool IsIdleSet();
	static bool IsHighWatermarkSet();

	bool m_Loop;

	u32 m_CurrentFrame;
	u32 m_FrameRangeStart;
	u32 m_FrameRangeEnd;

	u32 m_ObjectRangeStart;
	u32 m_ObjectRangeEnd;

	bool m_EarlyMemoryUpdates;

	u64 m_CyclesPerFrame;
	u32 m_ElapsedCycles;
	u32 m_FrameFifoSize;

	CallbackFunc m_FileLoadedCb;
	CallbackFunc m_FrameWrittenCb;

	FifoDataFile* m_File;

	std::vector<AnalyzedFrameInfo> m_FrameInfo;
};
