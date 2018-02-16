// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include <cstdint>

#include "Common/Assert.h"
#include "Common/CommonFuncs.h"
#include "Common/MsgHandler.h"

#include "VideoBackends/Metal/CommandBufferManager.h"
#include "VideoBackends/Metal/MetalContext.h"

namespace Metal
{
CommandBufferManager::CommandBufferManager()
{
}

CommandBufferManager::~CommandBufferManager()
{
  // Submit the current command buffer, and do not set up another.
  CommandBufferId id = m_command_buffers[m_current_command_buffer_index].id;
  if (id != 0)
  {
    SubmitCurrentCommandBuffer();
    WaitForCommandBuffer(id);
  }
}

bool CommandBufferManager::Initialize()
{
  // Get a command queue from the metal device.
  m_command_queue = g_metal_context->GetDevice().NewCommandQueue();
  if (!m_command_queue)
  {
    PanicAlert("Failed to create Metal command queue.");
    return false;
  }

  // Create the first (initial) command buffer, ready for pushing commands.
  SetupCommandBuffer();
  return true;
}

void CommandBufferManager::WaitForGPUIdle()
{
  u32 cmd_buf_idx = (m_current_command_buffer_index + 1) % NUM_COMMAND_BUFFERS;
  while (cmd_buf_idx != m_current_command_buffer_index)
  {
    CommandBuffer& buf = m_command_buffers[cmd_buf_idx];
    if (buf.id != 0)
      WaitForCommandBuffer(buf);

    cmd_buf_idx = (cmd_buf_idx + 1) % NUM_COMMAND_BUFFERS;
  }
}

mtlpp::CommandBuffer& CommandBufferManager::GetCurrentInitBuffer()
{
  CommandBuffer& buf = m_command_buffers[m_current_command_buffer_index];
  if (!buf.init_buffer)
  {
    // buf.init_buffer = m_command_queue.CommandBuffer();
    buf.init_buffer = m_command_queue.CommandBufferWithUnretainedReferences();
    if (!buf.init_buffer)
      PanicAlert("Failed to create init command buffer.");
  }
  else
  {
    // Flush any encoders. If we're returning the cmdbuffer, then it might do something else.
    if (buf.blit_encoder)
    {
      buf.blit_encoder.EndEncoding();
      buf.blit_encoder = {};
    }
  }

  return buf.init_buffer;
}

mtlpp::BlitCommandEncoder& CommandBufferManager::GetCurrentInitBlitEncoder()
{
  CommandBuffer& buf = m_command_buffers[m_current_command_buffer_index];
  if (!buf.blit_encoder)
  {
    buf.blit_encoder = GetCurrentInitBuffer().BlitCommandEncoder();
    if (!buf.blit_encoder)
      PanicAlert("Failed to create blit command encoder.");
  }

  return buf.blit_encoder;
}

void CommandBufferManager::SubmitCommandBuffer(bool wait_for_completion)
{
  // Submit the current command buffer.
  CommandBufferId id = m_command_buffers[m_current_command_buffer_index].id;
  SubmitCurrentCommandBuffer();

  // Set up the next command buffer.
  m_current_command_buffer_index = (m_current_command_buffer_index + 1) % NUM_COMMAND_BUFFERS;
  SetupCommandBuffer();

  // Do we need to wait?
  if (wait_for_completion)
    WaitForCommandBuffer(id);
}

void CommandBufferManager::SetupCommandBuffer()
{
  // If the current command buffer is valid, we must wait for it.
  CommandBuffer& buf = m_command_buffers[m_current_command_buffer_index];
  if (buf.id != 0)
    WaitForCommandBuffer(buf);

  // buf.draw_buffer = m_command_queue.CommandBuffer();
  buf.draw_buffer = m_command_queue.CommandBufferWithUnretainedReferences();
  buf.id = m_next_command_buffer_id++;
  if (!buf.draw_buffer)
  {
    PanicAlert("Failed to create draw command buffer.");
    return;
  }

  // Run our callback if the buffer completes before we wait for it.
  buf.draw_buffer.AddCompletedHandler(CommandBufferCompletionHandler);
}

void CommandBufferManager::SubmitCurrentCommandBuffer()
{
  // Submit the init buffer (if any), and the draw buffer.
  CommandBuffer& buf = m_command_buffers[m_current_command_buffer_index];
  if (buf.init_buffer)
  {
    if (buf.blit_encoder)
    {
      buf.blit_encoder.EndEncoding();
      buf.blit_encoder = {};
    }
    buf.init_buffer.Commit();
  }
  buf.draw_buffer.Commit();

  OnCommandBufferSubmitted(buf);
}

void CommandBufferManager::OnCommandBufferSubmitted(CommandBuffer& buf)
{
  // Fire fence tracking callbacks.
  for (auto iter = m_fence_point_callbacks.begin(); iter != m_fence_point_callbacks.end();)
  {
    auto backup_iter = iter++;
    backup_iter->second.first(buf.id);
  }
}

void CommandBufferManager::OnCommandBufferExecuted(CommandBuffer& buf)
{
  // Fire fence tracking callbacks.
  for (auto iter = m_fence_point_callbacks.begin(); iter != m_fence_point_callbacks.end();)
  {
    auto backup_iter = iter++;
    backup_iter->second.second(buf.id);
  }

  // Clean up all objects pending destruction on this command buffer
  for (auto& it : buf.cleanup_resources)
    it();
}

void CommandBufferManager::WaitForCommandBuffer(CommandBufferId id)
{
  if (m_completed_command_buffer_id >= id)
    return;

  // Wait for all the command buffers up to and including buf.
  u32 wait_buf_idx = (m_current_command_buffer_index + 1) % NUM_COMMAND_BUFFERS;
  while (wait_buf_idx != m_current_command_buffer_index)
  {
    CommandBuffer& buf = m_command_buffers[wait_buf_idx];
    CommandBufferId buf_id = buf.id;
    wait_buf_idx = (wait_buf_idx + 1) % NUM_COMMAND_BUFFERS;
    if (buf_id == 0)
      continue;

    WaitForCommandBuffer(buf);

    // Is this the one we wanted to wait for?
    if (buf_id == id)
      break;
  }
}

void CommandBufferManager::WaitForCommandBuffer(CommandBuffer& buf)
{
  _dbg_assert_(VIDEO, buf.id != 0);
  if (!buf.completed.TestAndClear())
  {
    // The init buffer is submitted before the draw buffer, and the ordering constraint
    // means that it will always complete before the draw buffer. So it is only necessary
    // to wait for the draw buffer.
    _dbg_assert_(VIDEO, buf.draw_buffer);
    buf.draw_buffer.WaitUntilCompleted();
    buf.completed.Clear();
  }

  // Update last completed buffer.
  _dbg_assert_(VIDEO, m_completed_command_buffer_id < buf.id);
  m_completed_command_buffer_id = buf.id;

  // Fire callbacks.
  OnCommandBufferExecuted(buf);

  // Release references.
  buf.init_buffer = {};
  buf.draw_buffer = {};
  buf.id = 0;
}

void CommandBufferManager::CommandBufferCompletionHandler(const mtlpp::CommandBuffer& buf)
{
  // NOTE: This is called asynchronously from the dispatch thread, therefore should avoid
  // manipulating any state which could be prone to races. Setting the flag is safe in
  // this manner as even if we wait for the cmdbuffer while this callback is executing,
  // it is safe to wait for an already-completed cmdbuffer.
  CommandBufferManager* this_ptr = g_command_buffer_mgr.get();
  for (auto& cmdbuffer : this_ptr->m_command_buffers)
  {
    if (cmdbuffer.draw_buffer.GetPtr() == buf.GetPtr())
      cmdbuffer.completed.Set();
  }
}

void CommandBufferManager::AddFencePointCallback(
    const void* key, const CommandBufferQueuedCallback& queued_callback,
    const CommandBufferExecutedCallback& executed_callback)
{
  // Shouldn't be adding twice.
  _assert_(m_fence_point_callbacks.find(key) == m_fence_point_callbacks.end());
  m_fence_point_callbacks.emplace(key, std::make_pair(queued_callback, executed_callback));
}

void CommandBufferManager::RemoveFencePointCallback(const void* key)
{
  auto iter = m_fence_point_callbacks.find(key);
  _assert_(iter != m_fence_point_callbacks.end());
  m_fence_point_callbacks.erase(iter);
}

void CommandBufferManager::DeferBufferDestruction(mtlpp::Buffer& buffer)
{
  m_command_buffers[m_current_command_buffer_index].cleanup_resources.push_back([buffer]() {});
}

void CommandBufferManager::DeferTextureDestruction(mtlpp::Texture& texture)
{
  m_command_buffers[m_current_command_buffer_index].cleanup_resources.push_back([texture]() {});
}

std::unique_ptr<CommandBufferManager> g_command_buffer_mgr;
}  // namespace Metal
