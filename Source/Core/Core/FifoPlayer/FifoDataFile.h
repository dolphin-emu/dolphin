// Copyright 2011 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"

namespace File
{
class IOFile;
}

struct MemoryUpdate
{
  enum Type
  {
    TEXTURE_MAP = 0x01,
    XF_DATA = 0x02,
    VERTEX_STREAM = 0x04,
    TMEM = 0x08,
  };

  u32 fifoPosition;
  u32 address;
  std::vector<u8> data;
  Type type;
};

struct FifoFrameInfo
{
  std::vector<u8> fifoData;

  u32 fifoStart;
  u32 fifoEnd;

  // Must be sorted by fifoPosition
  std::vector<MemoryUpdate> memoryUpdates;
};

class FifoDataFile
{
public:
  enum
  {
    BP_MEM_SIZE = 256,
    CP_MEM_SIZE = 256,
    XF_MEM_SIZE = 4096,
    XF_REGS_SIZE = 96,
    TEX_MEM_SIZE = 1024 * 1024,
  };

  FifoDataFile();
  ~FifoDataFile();

  void SetIsWii(bool isWii);
  bool GetIsWii() const;
  bool HasBrokenEFBCopies() const;

  u32* GetBPMem() { return m_BPMem; }
  u32* GetCPMem() { return m_CPMem; }
  u32* GetXFMem() { return m_XFMem; }
  u32* GetXFRegs() { return m_XFRegs; }
  u8* GetTexMem() { return m_TexMem; }
  void AddFrame(const FifoFrameInfo& frameInfo);
  const FifoFrameInfo& GetFrame(u32 frame) const { return m_Frames[frame]; }
  u32 GetFrameCount() const { return static_cast<u32>(m_Frames.size()); }
  bool Save(const std::string& filename);

  static std::unique_ptr<FifoDataFile> Load(const std::string& filename, bool flagsOnly);

private:
  enum
  {
    FLAG_IS_WII = 1
  };

  void PadFile(size_t numBytes, File::IOFile& file);

  void SetFlag(u32 flag, bool set);
  bool GetFlag(u32 flag) const;

  u64 WriteMemoryUpdates(const std::vector<MemoryUpdate>& memUpdates, File::IOFile& file);
  static void ReadMemoryUpdates(u64 fileOffset, u32 numUpdates,
                                std::vector<MemoryUpdate>& memUpdates, File::IOFile& file);

  u32 m_BPMem[BP_MEM_SIZE];
  u32 m_CPMem[CP_MEM_SIZE];
  u32 m_XFMem[XF_MEM_SIZE];
  u32 m_XFRegs[XF_REGS_SIZE];
  u8 m_TexMem[TEX_MEM_SIZE];

  u32 m_Flags = 0;
  u32 m_Version = 0;

  std::vector<FifoFrameInfo> m_Frames;
};
