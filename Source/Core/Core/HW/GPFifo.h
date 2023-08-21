// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/CommonTypes.h"

class PointerWrap;

namespace Core
{
class System;
}

namespace GPFifo
{
// This address is configurable in the WPAR SPR, but all games put it at this address
// (and presumably the hardware backing this system uses this address).
constexpr u32 GATHER_PIPE_PHYSICAL_ADDRESS = 0x0C008000;

constexpr u32 GATHER_PIPE_SIZE = 32;
constexpr u32 GATHER_PIPE_EXTRA_SIZE = GATHER_PIPE_SIZE * 16;

class GPFifoManager final
{
public:
  explicit GPFifoManager(Core::System& system);

  // Init
  void Init();
  void DoState(PointerWrap& p);

  // ResetGatherPipe
  void ResetGatherPipe();
  void UpdateGatherPipe();
  void CheckGatherPipe();
  void FastCheckGatherPipe();

  bool IsBNE() const;

  // Write
  void Write8(u8 value);
  void Write16(u16 value);
  void Write32(u32 value);
  void Write64(u64 value);

  // These expect pre-byteswapped values
  // Also there's an upper limit of about 512 per batch
  // Most likely these should be inlined into JIT instead
  void FastWrite8(u8 value);
  void FastWrite16(u16 value);
  void FastWrite32(u32 value);
  void FastWrite64(u64 value);

private:
  size_t GetGatherPipeCount();
  void SetGatherPipeCount(size_t size);

  // More room for the fastmodes
  alignas(GATHER_PIPE_SIZE) u8 m_gather_pipe[GATHER_PIPE_EXTRA_SIZE]{};

  Core::System& m_system;
};

// For use from the JIT.
void UpdateGatherPipe(GPFifoManager& gpfifo);
void FastCheckGatherPipe(GPFifoManager& gpfifo);
}  // namespace GPFifo
