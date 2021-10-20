// Copyright 2011 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/FifoPlayer/FifoPlaybackAnalyzer.h"

#include <vector>

#include "Common/Assert.h"
#include "Common/CommonTypes.h"
#include "Core/FifoPlayer/FifoAnalyzer.h"
#include "Core/FifoPlayer/FifoDataFile.h"
#include "Core/HW/Memmap.h"

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

  // We have a private memory buffer while analyzing
  auto ram = std::make_unique<u8[]>(file->GetRamSizeReal());
  auto exram = std::make_unique<u8[]>(file->GetExRamSizeReal());
  memset(ram.get(), 0, file->GetRamSizeReal());
  memset(exram.get(), 0, file->GetExRamSizeReal());

  auto getMemPtr = [&](u32 addr) { return (addr & 0x10000000 ? exram.get() : ram.get()) + (addr & 0x0fffffff); };

  for (u32 frameIdx = 0; frameIdx < file->GetFrameCount(); ++frameIdx)
  {
    const FifoFrameInfo& frame = file->GetFrame(frameIdx);
    AnalyzedFrameInfo& analyzed = frameInfo[frameIdx];


    u32 nextMemUpdate = 0;

#if LOG_FIFO_CMDS
    // Debugging
    std::vector<CmdData> prevCmds;
#endif

    bool error = false;

    enum Mode {
      AnalyzePrimaryList,
      AnalyzeDisplayList,
      ProcessCPmem,
    };

    std::function<void(const u8*, size_t, Mode, std::vector<Object>&)> AnalyzeCommands =
       [&](const u8* data, size_t length, Mode mode, std::vector<Object>& objects)
    {
      u32 cmdStart = 0;
      bool wasDrawing = false;

      while (cmdStart < length)
      {
        // Add memory updates that have occurred before this point in the frame
        while (mode == AnalyzePrimaryList && nextMemUpdate < frame.memoryUpdates.size() &&
              frame.memoryUpdates[nextMemUpdate].fifoPosition <= cmdStart)
        {
          auto update = frame.memoryUpdates[nextMemUpdate++];
          analyzed.memoryUpdates.push_back(update);

          // We need to apply "display list" updates so we can analyze the commands
          if (update.type == MemoryUpdate::DISPLAY_LIST)
          {
            memcpy(getMemPtr(update.address), update.data.data(), update.data.size());
          }
        }

        CmdInfo info{};
        FifoAnalyzer::AnalyzeCommand(&data[cmdStart], DecodeMode::Playback, false, &info);

        if (mode == ProcessCPmem) {
          cmdStart += info.cmd_length;
          continue;
        }

  #if LOG_FIFO_CMDS
        CmdData cmdData;
        cmdData.offset = cmdStart;
        cmdData.ptr = &data[cmdStart];
        cmdData.size = info.cmd_length;
        prevCmds.push_back(cmdData);
  #endif

        // Check for error
        if (info.cmd_length == 0)
        {
          error = true;
          return;
        }

        bool drawing = info.type == CmdInfo::DRAW;

        if (wasDrawing != drawing)
        {
          if (drawing)
          {
            objects.push_back(Object{cmdStart, 0, s_CpMem});
          }
          else
          {
            objects.back().end = cmdStart;
          }
          wasDrawing = drawing;
        }

        if (info.type == CmdInfo::DL_CALL && mode == AnalyzePrimaryList)
        {
          u8* cmd_data = getMemPtr(info.display_list_address);
          u32 size = info.display_list_length;

          // Reuse display list if backing memory is unchanged
          auto it = std::find_if(std::begin(analyzed.displayLists), std::end(analyzed.displayLists), [&](const DisplayList& n) {
            return n.address == info.display_list_address
              && n.cmdData.size() == size
              && memcmp(cmd_data, n.cmdData.data(), size) == 0;
          });

          if (it == std::end(analyzed.displayLists))
          {
            u32 id = analyzed.displayLists.size();
            analyzed.displayLists.emplace_back(DisplayList{id, info.display_list_address, 1, {}, {}});
            it = --std::end(analyzed.displayLists);

            // Analyze the displaylist commands for objects the first time we come across it
            AnalyzeCommands(cmd_data, size, AnalyzeDisplayList, it->objects);


            // Copy data out of our memory buffer to the display list object
            it->cmdData.insert(std::end(it->cmdData), &cmd_data[0], &cmd_data[size]);

          } else
          {
            it->refCount++;
            // The display list might update CPmem, so we need to process it again
            AnalyzeCommands(cmd_data, size, ProcessCPmem, it->objects);
          }

          if (error)
            return;

          analyzed.displayListCalls.push_back(DisplayListCall{cmdStart, it->id});
        }

        cmdStart += info.cmd_length;
      }

      // Complete the final object
      if (!objects.empty() && objects.back().end == 0)
        objects.back().end = length;
    };

    AnalyzeCommands(frame.fifoData.data(), frame.fifoData.size(), AnalyzePrimaryList, analyzed.objects);

    if (error) {
      analyzed.objects.clear();
      analyzed.displayLists.clear();
      analyzed.displayListCalls.clear();
      analyzed.memoryUpdates.clear();
    }
  }
}
