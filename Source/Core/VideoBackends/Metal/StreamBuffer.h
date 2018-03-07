// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <cstddef>
#include <deque>
#include <memory>
#include <utility>

#include "Common/CommonTypes.h"
#include "VideoBackends/Metal/Common.h"

namespace Metal
{
class StreamBuffer
{
public:
  StreamBuffer(u32 max_size);
  ~StreamBuffer();

  mtlpp::Buffer& GetBuffer() { return m_buffer; }
  u8* GetHostPointer() const { return m_host_pointer; }
  u8* GetCurrentHostPointer() const { return m_host_pointer + m_current_offset; }
  u32 GetCurrentSize() const { return m_current_size; }
  u32 GetCurrentOffset() const { return m_current_offset; }
  bool ReserveMemory(u32 num_bytes, u32 alignment, bool allow_reuse = true,
                     bool allow_growth = true, bool reallocate_if_full = false);
  void CommitMemory(u32 final_num_bytes);

  static std::unique_ptr<StreamBuffer> Create(u32 initial_size, u32 max_size);

private:
  bool ResizeBuffer(u32 size);
  void OnCommandBufferQueued(CommandBufferId id);
  void OnCommandBufferExecuted(CommandBufferId id);

  // Waits for as many fences as needed to allocate num_bytes bytes from the buffer.
  bool WaitForClearSpace(u32 num_bytes);

  u32 m_current_size = 0;
  u32 m_maximum_size;
  u32 m_current_offset = 0;
  u32 m_current_gpu_position = 0;
  u32 m_last_allocation_size = 0;

  mtlpp::Buffer m_buffer;
  u8* m_host_pointer = nullptr;

  // List of fences and the corresponding positions in the buffer
  std::deque<std::pair<CommandBufferId, size_t>> m_tracked_fences;
};

}  // namespace Metal
