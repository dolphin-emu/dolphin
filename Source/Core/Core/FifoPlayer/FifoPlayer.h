// Copyright 2011 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>
#include <vector>

#include "Core/FifoPlayer/FifoPlaybackAnalyzer.h"

class FifoDataFile;
struct MemoryUpdate;
struct AnalyzedFrameInfo;

class FifoPlayer
{
public:
	typedef void(*CallbackFunc)(void);

	~FifoPlayer();

	bool Open(const std::string& filename);
	void Close();

	// Play is controlled by the state of PowerPC
	bool Play();

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
	FifoPlayer();

	void WriteFrame(const FifoFrameInfo &frame, const AnalyzedFrameInfo &info);
	void WriteFramePart(u32 dataStart, u32 dataEnd, u32 &nextMemUpdate, const FifoFrameInfo &frame, const AnalyzedFrameInfo &info);

	void WriteAllMemoryUpdates();
	void WriteMemory(const MemoryUpdate &memUpdate);

	// writes a range of data to the fifo
	// start and end must be relative to frame's fifo data so elapsed cycles are figured correctly
	void WriteFifo(u8 *data, u32 start, u32 end);

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

	FifoDataFile *m_File;

	std::vector<AnalyzedFrameInfo> m_FrameInfo;
};
