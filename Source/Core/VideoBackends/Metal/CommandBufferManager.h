// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <cstddef>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

#include "Common/Flag.h"
#include "VideoBackends/Metal/Common.h"
#include "VideoCommon/VideoCommon.h"

namespace Metal
{
class CommandBufferManager
{
public:
  CommandBufferManager();
  ~CommandBufferManager();

  bool Initialize();

  mtlpp::CommandQueue& GetCommandQueue() { return m_command_queue; }
  mtlpp::CommandBuffer& GetCurrentBuffer()
  {
    return m_command_buffers[m_current_command_buffer_index].draw_buffer;
  }
  mtlpp::CommandBuffer& GetCurrentInitBuffer();
  mtlpp::BlitCommandEncoder& GetCurrentInitBlitEncoder();
  CommandBufferId GetCompletedCommandBufferId() const { return m_completed_command_buffer_id; }
  // Submits the current command buffer to the GPU. A new command buffer will be created.
  void SubmitCommandBuffer(bool wait_for_completion = false);

  // Waits for the specified command buffer to be executed by the GPU.
  void WaitForCommandBuffer(CommandBufferId id);

  // Waits for all previously-submitted command buffers to complete.
  void WaitForGPUIdle();

  // Instruct the manager to fire the specified callback when a fence is flagged to be signaled.
  // This happens when command buffers are executed, and can be tested if signaled, which means
  // that all commands up to the point when the callback was fired have completed.
  using CommandBufferQueuedCallback = std::function<void(CommandBufferId)>;
  using CommandBufferExecutedCallback = std::function<void(CommandBufferId)>;
  void AddFencePointCallback(const void* key, const CommandBufferQueuedCallback& queued_callback,
                             const CommandBufferExecutedCallback& executed_callback);
  void RemoveFencePointCallback(const void* key);

  // Schedule a Metal resource for destruction later on. This will occur when the command buffer
  // is next re-used, and the GPU has finished working with the specified resource.
  void DeferBufferDestruction(mtlpp::Buffer& buffer);
  void DeferTextureDestruction(mtlpp::Texture& texture);

private:
  static constexpr u32 NUM_COMMAND_BUFFERS = 2;

  struct CommandBuffer
  {
    mtlpp::CommandBuffer init_buffer;
    mtlpp::CommandBuffer draw_buffer;
    mtlpp::BlitCommandEncoder blit_encoder;
    std::vector<std::function<void()>> cleanup_resources;
    CommandBufferId id = 0;
    Common::Flag completed{false};  // Written to from callback from dispatch thread?
  };

  static void CommandBufferCompletionHandler(const mtlpp::CommandBuffer& buf);

  void SetupCommandBuffer();
  void SubmitCurrentCommandBuffer();
  void WaitForCommandBuffer(CommandBuffer& buf);

  void OnCommandBufferSubmitted(CommandBuffer& buf);
  void OnCommandBufferExecuted(CommandBuffer& buf);

  mtlpp::CommandQueue m_command_queue;
  std::array<CommandBuffer, NUM_COMMAND_BUFFERS> m_command_buffers;
  CommandBufferId m_completed_command_buffer_id = 0;
  CommandBufferId m_next_command_buffer_id = 1;
  u32 m_current_command_buffer_index = 0;

  // callbacks when a fence point is set
  std::map<const void*, std::pair<CommandBufferQueuedCallback, CommandBufferExecutedCallback>>
      m_fence_point_callbacks;
};

extern std::unique_ptr<CommandBufferManager> g_command_buffer_mgr;

}  // namespace Metal
