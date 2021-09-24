// Copyright 2019 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <deque>
#include <utility>

#include "Common/CommonTypes.h"
#include "VideoBackends/D3D12/Common.h"

namespace DX12
{
class StreamBuffer
{
public:
  StreamBuffer();
  ~StreamBuffer();

  bool AllocateBuffer(u32 size);

  ID3D12Resource* GetBuffer() const { return m_buffer; }
  D3D12_GPU_VIRTUAL_ADDRESS GetGPUPointer() const { return m_gpu_pointer; }
  u8* GetHostPointer() const { return m_host_pointer; }
  u8* GetCurrentHostPointer() const { return m_host_pointer + m_current_offset; }
  D3D12_GPU_VIRTUAL_ADDRESS GetCurrentGPUPointer() const
  {
    return m_gpu_pointer + m_current_offset;
  }
  u32 GetSize() const { return m_size; }
  u32 GetCurrentOffset() const { return m_current_offset; }
  bool ReserveMemory(u32 num_bytes, u32 alignment);
  void CommitMemory(u32 final_num_bytes);

private:
  void UpdateCurrentFencePosition();
  void UpdateGPUPosition();

  // Waits for as many fences as needed to allocate num_bytes bytes from the buffer.
  bool WaitForClearSpace(u32 num_bytes);

  u32 m_size = 0;
  u32 m_current_offset = 0;
  u32 m_current_gpu_position = 0;
  u32 m_last_allocation_size = 0;

  ID3D12Resource* m_buffer = nullptr;
  D3D12_GPU_VIRTUAL_ADDRESS m_gpu_pointer = {};
  u8* m_host_pointer = nullptr;

  // List of fences and the corresponding positions in the buffer
  std::deque<std::pair<u64, u32>> m_tracked_fences;
};

}  // namespace DX12
