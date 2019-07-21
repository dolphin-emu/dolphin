// Copyright 2011 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/FifoPlayer/FifoPlaybackAnalyzer.h"

#include <vector>

#include "Common/CommonTypes.h"
#include "Core/FifoPlayer/FifoAnalyzer.h"
#include "Core/FifoPlayer/FifoDataFile.h"

using namespace FifoAnalyzer;

// For debugging
#define LOG_FIFO_CMDS 0
struct CmdData
{
  u32 size;
  u32 offset;
  const u8* ptr;
};

void FifoPlaybackAnalyzer::AnalyzeFrames(FifoDataFile* file,
                                         std::vector<AnalyzedFrameInfo>& frameInfo)
{
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
    std::vector<CmdData> prevCmds;
#endif

    while (cmdStart < frame.fifoData.size())
    {
      // Add memory updates that have occurred before this point in the frame
      while (nextMemUpdate < frame.memoryUpdates.size() &&
             frame.memoryUpdates[nextMemUpdate].fifoPosition <= cmdStart)
      {
        analyzed.memoryUpdates.push_back(frame.memoryUpdates[nextMemUpdate]);
        ++nextMemUpdate;
      }

      const bool wasDrawing = s_DrawingObject;
      const u32 cmdSize =
          FifoAnalyzer::AnalyzeCommand(&frame.fifoData[cmdStart], DecodeMode::Playback);

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
