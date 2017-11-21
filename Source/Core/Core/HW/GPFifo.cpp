// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/HW/GPFifo.h"

#include <cstddef>
#include <cstring>

#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "Common/Swap.h"
#include "Core/HW/Memmap.h"
#include "Core/HW/ProcessorInterface.h"
#include "Core/PowerPC/JitInterface.h"
#include "Core/PowerPC/PowerPC.h"
#include "VideoCommon/CommandProcessor.h"

namespace GPFifo
{
// 32 Byte gather pipe with extra space
// Overfilling is no problem (up to the real limit), CheckGatherPipe will blast the
// contents in nicely sized chunks
//
// Other optimizations to think about:
// - If the GP is NOT linked to the FIFO, just blast to memory byte by word
// - If the GP IS linked to the FIFO, use a fast wrapping buffer and skip writing to memory
//
// Both of these should actually work! Only problem is that we have to decide at run time,
// the same function could use both methods. Compile 2 different versions of each such block?

// More room for the fastmodes
alignas(32) static u8 s_gather_pipe[GATHER_PIPE_SIZE * 16];

static size_t GetGatherPipeCount()
{
  return PowerPC::ppcState.gather_pipe_ptr - s_gather_pipe;
}

static void SetGatherPipeCount(size_t size)
{
  PowerPC::ppcState.gather_pipe_ptr = s_gather_pipe + size;
}

void DoState(PointerWrap& p)
{
  p.Do(s_gather_pipe);
  u32 pipe_count = static_cast<u32>(GetGatherPipeCount());
  p.Do(pipe_count);
  SetGatherPipeCount(pipe_count);
}

void Init()
{
  ResetGatherPipe();
  PowerPC::ppcState.gather_pipe_base_ptr = s_gather_pipe;
  memset(s_gather_pipe, 0, sizeof(s_gather_pipe));
}

bool IsEmpty()
{
  return GetGatherPipeCount() == 0;
}

void ResetGatherPipe()
{
  SetGatherPipeCount(0);
}

void UpdateGatherPipe()
{
  size_t pipe_count = GetGatherPipeCount();
  size_t processed;
  u8* cur_mem = Memory::GetPointer(ProcessorInterface::Fifo_CPUWritePointer);
  for (processed = 0; pipe_count >= GATHER_PIPE_SIZE; processed += GATHER_PIPE_SIZE)
  {
    // copy the GatherPipe
    memcpy(cur_mem, s_gather_pipe + processed, GATHER_PIPE_SIZE);
    pipe_count -= GATHER_PIPE_SIZE;

    // increase the CPUWritePointer
    if (ProcessorInterface::Fifo_CPUWritePointer == ProcessorInterface::Fifo_CPUEnd)
    {
      ProcessorInterface::Fifo_CPUWritePointer = ProcessorInterface::Fifo_CPUBase;
      cur_mem = Memory::GetPointer(ProcessorInterface::Fifo_CPUWritePointer);
    }
    else
    {
      cur_mem += GATHER_PIPE_SIZE;
      ProcessorInterface::Fifo_CPUWritePointer += GATHER_PIPE_SIZE;
    }

    CommandProcessor::GatherPipeBursted();
  }

  // move back the spill bytes
  memmove(s_gather_pipe, s_gather_pipe + processed, pipe_count);
  SetGatherPipeCount(pipe_count);
}

void FastCheckGatherPipe()
{
  if (GetGatherPipeCount() >= GATHER_PIPE_SIZE)
  {
    UpdateGatherPipe();
  }
}

void CheckGatherPipe()
{
  if (GetGatherPipeCount() >= GATHER_PIPE_SIZE)
  {
    UpdateGatherPipe();

    // Profile where slow FIFO writes are occurring.
    JitInterface::CompileExceptionCheck(JitInterface::ExceptionType::FIFOWrite);
  }
}

void Write8(const u8 value)
{
  FastWrite8(value);
  CheckGatherPipe();
}

void Write16(const u16 value)
{
  FastWrite16(value);
  CheckGatherPipe();
}

void Write32(const u32 value)
{
  FastWrite32(value);
  CheckGatherPipe();
}

void Write64(const u64 value)
{
  FastWrite64(value);
  CheckGatherPipe();
}

void FastWrite8(const u8 value)
{
  *PowerPC::ppcState.gather_pipe_ptr = value;
  PowerPC::ppcState.gather_pipe_ptr += sizeof(u8);
}

void FastWrite16(u16 value)
{
  value = Common::swap16(value);
  std::memcpy(PowerPC::ppcState.gather_pipe_ptr, &value, sizeof(u16));
  PowerPC::ppcState.gather_pipe_ptr += sizeof(u16);
}

void FastWrite32(u32 value)
{
  value = Common::swap32(value);
  std::memcpy(PowerPC::ppcState.gather_pipe_ptr, &value, sizeof(u32));
  PowerPC::ppcState.gather_pipe_ptr += sizeof(u32);
}

void FastWrite64(u64 value)
{
  value = Common::swap64(value);
  std::memcpy(PowerPC::ppcState.gather_pipe_ptr, &value, sizeof(u64));
  PowerPC::ppcState.gather_pipe_ptr += sizeof(u64);
}

}  // end of namespace GPFifo
