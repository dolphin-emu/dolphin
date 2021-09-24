// Copyright 2011 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/FifoPlayer/FifoPlaybackAnalyzer.h"

#include <vector>

#include "Common/Assert.h"
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
  FifoAnalyzer::LoadCPReg(VCD_LO, cpMem[VCD_LO], s_CpMem);
  FifoAnalyzer::LoadCPReg(VCD_HI, cpMem[VCD_HI], s_CpMem);

  for (u32 i = 0; i < CP_NUM_VAT_REG; ++i)
  {
    FifoAnalyzer::LoadCPReg(CP_VAT_REG_A + i, cpMem[CP_VAT_REG_A + i], s_CpMem);
    FifoAnalyzer::LoadCPReg(CP_VAT_REG_B + i, cpMem[CP_VAT_REG_B + i], s_CpMem);
    FifoAnalyzer::LoadCPReg(CP_VAT_REG_C + i, cpMem[CP_VAT_REG_C + i], s_CpMem);
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
        analyzed.objectCPStates.clear();
        analyzed.objectEnds.clear();

        return;
      }

      if (wasDrawing != s_DrawingObject)
      {
        if (s_DrawingObject)
        {
          analyzed.objectStarts.push_back(cmdStart);
          analyzed.objectCPStates.push_back(s_CpMem);
        }
        else
        {
          analyzed.objectEnds.push_back(cmdStart);
        }
      }

      cmdStart += cmdSize;
    }

    if (analyzed.objectEnds.size() < analyzed.objectStarts.size())
      analyzed.objectEnds.push_back(cmdStart);

    ASSERT(analyzed.objectStarts.size() == analyzed.objectCPStates.size());
    ASSERT(analyzed.objectStarts.size() == analyzed.objectEnds.size());
  }
}
