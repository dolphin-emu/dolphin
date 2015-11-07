// Copyright 2011 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/Assert.h"
#include "Common/CommonTypes.h"
#include "Core/FifoPlayer/FifoAnalyzer.h"
#include "Core/FifoPlayer/FifoDataFile.h"
#include "Core/FifoPlayer/FifoPlaybackAnalyzer.h"
#include "VideoCommon/OpcodeDecoding.h"
#include "VideoCommon/TextureDecoder.h"
#include "VideoCommon/VertexLoader.h"

using namespace FifoAnalyzer;

// For debugging
#define LOG_FIFO_CMDS 0
struct CmdData
{
	u32 size;
	u32 offset;
	u8* ptr;
};

struct MemoryRange
{
	u32 begin;
	u32 end;
};

static std::vector<MemoryRange> s_WrittenMemory;

static void AddMemoryUpdate(MemoryUpdate memUpdate, AnalyzedFrameInfo& frameInfo);
static void StoreWrittenRegion(u32 address, u32 size);

void FifoPlaybackAnalyzer::AnalyzeFrames(FifoDataFile* file, std::vector<AnalyzedFrameInfo>& frameInfo)
{
	// Load BP memory
	u32* bpMem = file->GetBPMem();
	memcpy(&s_BpMem, bpMem, sizeof(BPMemory));

	u32* cpMem = file->GetCPMem();
	FifoAnalyzer::LoadCPReg(0x50, cpMem[0x50], s_CpMem);
	FifoAnalyzer::LoadCPReg(0x60, cpMem[0x60], s_CpMem);

	for (int i = 0; i < 8; ++i)
	{
		FifoAnalyzer::LoadCPReg(0x70 + i, cpMem[0x70 + i], s_CpMem);
		FifoAnalyzer::LoadCPReg(0x80 + i, cpMem[0x80 + i], s_CpMem);
		FifoAnalyzer::LoadCPReg(0x90 + i, cpMem[0x90 + i], s_CpMem);
	}

	frameInfo.clear();
	frameInfo.resize(file->GetFrameCount());

	for (u32 frameIdx = 0; frameIdx < file->GetFrameCount(); ++frameIdx)
	{
		const FifoFrameInfo& frame = file->GetFrame(frameIdx);
		AnalyzedFrameInfo& analyzed = frameInfo[frameIdx];

		s_DrawingObject = false;

		u32 cmdStart = 0;
		u32 nextMemUpdate = 0;

#if LOG_FIFO_CMDS
		// Debugging
		vector<CmdData> prevCmds;
#endif

		while (cmdStart < frame.fifoDataSize)
		{
			// Add memory updates that have occurred before this point in the frame
			while (nextMemUpdate < frame.memoryUpdates.size() && frame.memoryUpdates[nextMemUpdate].fifoPosition <= cmdStart)
			{
				AddMemoryUpdate(frame.memoryUpdates[nextMemUpdate], analyzed);
				++nextMemUpdate;
			}

			bool wasDrawing = s_DrawingObject;

			u32 cmdSize = FifoAnalyzer::AnalyzeCommand(&frame.fifoData[cmdStart], DECODE_PLAYBACK);

#if LOG_FIFO_CMDS
			CmdData cmdData;
			cmdData.offset = cmdStart;
			cmdData.ptr = &frame.fifoData[cmdStart];
			cmdData.size = cmdSize;
			prevCmds.push_back(cmdData);
#endif

			// Check for error
			if (cmdSize == 0)
			{
				// Clean up frame analysis
				analyzed.objectStarts.clear();
				analyzed.objectEnds.clear();

				return;
			}

			if (wasDrawing != s_DrawingObject)
			{
				if (s_DrawingObject)
					analyzed.objectStarts.push_back(cmdStart);
				else
					analyzed.objectEnds.push_back(cmdStart);
			}

			cmdStart += cmdSize;
		}

		if (analyzed.objectEnds.size() < analyzed.objectStarts.size())
			analyzed.objectEnds.push_back(cmdStart);
	}
}

static void AddMemoryUpdate(MemoryUpdate memUpdate, AnalyzedFrameInfo& frameInfo)
{
	u32 begin = memUpdate.address;
	u32 end = memUpdate.address + memUpdate.size;

	// Remove portions of memUpdate that overlap with memory ranges that have been written by the GP
	for (const auto& range : s_WrittenMemory)
	{
		if (range.begin < end &&
		    range.end > begin)
		{
			s32 preSize = range.begin - begin;
			s32 postSize = end - range.end;

			if (postSize > 0)
			{
				if (preSize > 0)
				{
					memUpdate.size = preSize;
					AddMemoryUpdate(memUpdate, frameInfo);
				}

				u32 bytesToRangeEnd = range.end - memUpdate.address;
				memUpdate.data += bytesToRangeEnd;
				memUpdate.size = postSize;
				memUpdate.address = range.end;
			}
			else if (preSize > 0)
			{
				memUpdate.size = preSize;
			}
			else
			{
				// Ignore all of memUpdate
				return;
			}
		}
	}

	frameInfo.memoryUpdates.push_back(memUpdate);
}

void FifoPlaybackAnalyzer::StoreEfbCopyRegion()
{
	UPE_Copy peCopy = s_BpMem.triggerEFBCopy;

	u32 copyfmt = peCopy.tp_realFormat();
	bool bFromZBuffer = s_BpMem.zcontrol.pixel_format == PEControl::Z24;
	u32 address = bpmem.copyTexDest << 5;

	u32 format = copyfmt;

	if (peCopy.copy_to_xfb)
	{
		// Fake format to calculate size correctly
		format = GX_TF_IA8;
	}
	else if (bFromZBuffer)
	{
		format |= _GX_TF_ZTF;
		if (copyfmt == 11)
		{
			format = GX_TF_Z16;
		}
		else if (format < GX_TF_Z8 || format > GX_TF_Z24X8)
		{
			format |= _GX_TF_CTF;
		}
	}
	else
	{
		if (copyfmt > GX_TF_RGBA8 || (copyfmt < GX_TF_RGB565 && !peCopy.intensity_fmt))
			format |= _GX_TF_CTF;
	}

	int width = (s_BpMem.copyTexSrcWH.x + 1) >> peCopy.half_scale;
	int height = (s_BpMem.copyTexSrcWH.y + 1) >> peCopy.half_scale;

	u16 blkW = TexDecoder_GetBlockWidthInTexels(format) - 1;
	u16 blkH = TexDecoder_GetBlockHeightInTexels(format) - 1;

	s32 expandedWidth = (width + blkW) & (~blkW);
	s32 expandedHeight = (height + blkH) & (~blkH);

	int sizeInBytes = TexDecoder_GetTextureSizeInBytes(expandedWidth, expandedHeight, format);

	StoreWrittenRegion(address, sizeInBytes);
}

static void StoreWrittenRegion(u32 address, u32 size)
{
	u32 end = address + size;
	auto newRangeIter = s_WrittenMemory.end();

	// Search for overlapping memory regions and expand them to include the new region
	for (auto iter = s_WrittenMemory.begin(); iter != s_WrittenMemory.end();)
	{
		MemoryRange &range = *iter;

		if (range.begin < end && range.end > address)
		{
			// range at iterator and new range overlap

			if (newRangeIter == s_WrittenMemory.end())
			{
				// Expand range to include the written region
				range.begin = std::min(address, range.begin);
				range.end = std::max(end, range.end);
				newRangeIter = iter;

				++iter;
			}
			else
			{
				// Expand region at rangeIter to include this range
				MemoryRange &used = *newRangeIter;
				used.begin = std::min(used.begin, range.begin);
				used.end = std::max(used.end, range.end);

				// Remove this entry
				iter = s_WrittenMemory.erase(iter);
			}
		}
		else
		{
			++iter;
		}
	}

	if (newRangeIter == s_WrittenMemory.end())
	{
		MemoryRange range;
		range.begin = address;
		range.end = end;

		s_WrittenMemory.push_back(range);
	}
}
