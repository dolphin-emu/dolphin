// Copyright 2011 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <memory>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"
#include "VideoCommon/XFMemory.h"

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

  u32 fifoPosition = 0;
  u32 address = 0;
  std::vector<u8> data;
  Type type{};
};

struct FifoFrameInfo
{
  std::vector<u8> fifoData;

  u32 fifoStart = 0;
  u32 fifoEnd = 0;

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
    XF_REGS_SIZE = 88,
    TEX_MEM_SIZE = 1024 * 1024,
  };
  static_assert((XF_MEM_SIZE + XF_REGS_SIZE) * sizeof(u32) == sizeof(XFMemory));

  FifoDataFile();
  ~FifoDataFile();

  void SetIsWii(bool isWii);
  bool GetIsWii() const;
  bool HasBrokenEFBCopies() const;
  bool ShouldGenerateFakeVIUpdates() const;

  u32* GetBPMem() { return m_BPMem.data(); }
  u32* GetCPMem() { return m_CPMem.data(); }
  u32* GetXFMem() { return m_XFMem.data(); }
  u32* GetXFRegs() { return m_XFRegs.data(); }
  u8* GetTexMem() { return m_TexMem.data(); }
  u32 GetRamSizeReal() { return m_ram_size_real; }
  u32 GetExRamSizeReal() { return m_exram_size_real; }

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

  std::array<u32, BP_MEM_SIZE> m_BPMem{};
  std::array<u32, CP_MEM_SIZE> m_CPMem{};
  std::array<u32, XF_MEM_SIZE> m_XFMem{};
  std::array<u32, XF_REGS_SIZE> m_XFRegs{};
  std::array<u8, TEX_MEM_SIZE> m_TexMem{};
  u32 m_ram_size_real = 0;
  u32 m_exram_size_real = 0;

  u32 m_Flags = 0;
  u32 m_Version = 0;

  std::vector<FifoFrameInfo> m_Frames;
};
