// Copyright 2016 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VKDebug.h"

#include "Common/Assert.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"

#include "VideoBackends/Vulkan/CommandBufferManager.h"
#include "VideoBackends/Vulkan/StateTracker.h"

namespace Vulkan
{
// TODO: Add better implementation using VK_NV_device_diagnostic_checkpoints and
// VK_AMD_buffer_marker

constexpr u64 STAGING_BUFFER_SIZE = 4 << 20;

VkDebug::VkDebug(bool enabled) : m_enabled(enabled)
{
  m_readback_buffer = StagingBuffer::Create(STAGING_BUFFER_TYPE_READBACK, sizeof(u64),
                                            VK_BUFFER_USAGE_TRANSFER_DST_BIT);
  m_readback_buffer->Map();
  u64* ptr = reinterpret_cast<u64*>(m_readback_buffer->GetMapPointer());
  *ptr = 0;
  m_readback_buffer->Unmap();
}

void VkDebug::Reset()
{
  m_staging_buffer_index = 0;
  m_staging_buffer_offset = 0ul;
  m_markers.clear();
}

void VkDebug::EnsureStagingBufferCapacity()
{
  if (m_buffers.size() != 0 && m_staging_buffer_offset < STAGING_BUFFER_SIZE - sizeof(u64))
      [[likely]]
  {
    return;
  }

  if (m_buffers.size() != 0)
  {
    m_buffers[m_staging_buffer_index]->Unmap();
  }

  std::unique_ptr<StagingBuffer> buffer = StagingBuffer::Create(
      STAGING_BUFFER_TYPE_UPLOAD, STAGING_BUFFER_SIZE, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
  buffer->Map();
  m_buffers.push_back(std::move(buffer));
  m_staging_buffer_offset = 0ul;

  if (m_buffers.size() != 1)
  {
    m_staging_buffer_index++;
  }
}

void VkDebug::EmitMarker(VkDebugCommand cmd, u64 aux0, u64 aux1, u64 aux2, u64 aux3)
{
  bool is_in_renderpass = StateTracker::GetInstance()->InRenderPass();
  StateTracker::GetInstance()->EndRenderPass();

  EnsureStagingBufferCapacity();
  std::unique_ptr<StagingBuffer>& buffer = m_buffers[m_staging_buffer_index];
  u64* ptr = reinterpret_cast<u64*>(buffer->GetMapPointer() + m_staging_buffer_offset);
  uint64_t seq = m_sequence_number++;
  *ptr = seq;
  std::array<u64, 4> aux = {aux0, aux1, aux2, aux3};
  VkDebugMarker marker = {aux, seq, cmd};
  m_markers.emplace_back(marker);

  VkMemoryBarrier memory_barrier = {VK_STRUCTURE_TYPE_MEMORY_BARRIER, nullptr,
                                    VK_ACCESS_MEMORY_WRITE_BIT,
                                    VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT};
  vkCmdPipelineBarrier(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                       VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 1,
                       &memory_barrier, 0, nullptr, 0, nullptr);
  VkBufferCopy copy = {m_staging_buffer_offset, 0, sizeof(u64)};
  vkCmdCopyBuffer(g_command_buffer_mgr->GetCurrentCommandBuffer(), buffer->GetBuffer(),
                  m_readback_buffer->GetBuffer(), 1, &copy);
  m_readback_buffer->FlushGPUCache(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                                   VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
                                   sizeof(u64));

  m_staging_buffer_offset += sizeof(u64);

  if (is_in_renderpass)
    StateTracker::GetInstance()->BeginRenderPass();
}

const VkDebugMarker* VkDebug::FindFault()
{
  m_readback_buffer->InvalidateCPUCache(0, sizeof(u64));
  m_readback_buffer->Map();
  u64* ptr = reinterpret_cast<u64*>(m_readback_buffer->GetMapPointer());
  u64 seq = *ptr;
  m_readback_buffer->Unmap();

  auto result = m_markers.end();
  for (auto iter = m_markers.begin(); iter != m_markers.end(); iter++)
  {
    if (iter->sequence_number == seq)
    {
      result = iter;
      break;
    }
  }
  if (result == m_markers.end())
  {
    return nullptr;
  }
  result++;
  if (result == m_markers.end())
  {
    return nullptr;
  }
  return &*result;
}

void VkDebug::PrintFault()
{
  const VkDebugMarker* fault = FindFault();
  if (fault == nullptr)
  {
    return;
  }

  std::string command_name;
  switch (fault->cmd)
  {
  case VkDebugCommand::Draw:
    command_name = "Draw";
    break;

  case VkDebugCommand::DrawIndexed:
    command_name = "DrawIndexed";
    break;

  case VkDebugCommand::Dispatch:
    command_name = "Dispatch";
    break;

  case VkDebugCommand::BoundingBoxRead:
    command_name = "BoundingBoxRead";
    break;

  case VkDebugCommand::ClearRegion:
    command_name = "ClearRegion";
    break;

  case VkDebugCommand::DisableQuery:
    command_name = "DisableQuery";
    break;

  case VkDebugCommand::ResetQuery:
    command_name = "ResetQuery";
    break;

  case VkDebugCommand::StagingCopyFromTexture:
    command_name = "StagingCopyFromTexture";
    break;

  case VkDebugCommand::StagingCopyToTexture:
    command_name = "StagingCopyToTexture";
    break;

  case VkDebugCommand::StagingCopyRectFromTexture:
    command_name = "StagingCopyRectFromTexture";
    break;

  case VkDebugCommand::ReadbackQuery:
    command_name = "ReadbackQuery";
    break;

  case VkDebugCommand::TransitionToComputeLayout:
    command_name = "TransitionToComputeLayout";
    break;

  case VkDebugCommand::TransitionToLayout:
    command_name = "TransitionToLayout";
    break;

  default:
    command_name = fmt::format("{}", uint32_t(fault->cmd));
    break;
  }

  ERROR_LOG_FMT(HOST_GPU, "Vulkan fault in {}, aux0: {}, aux1: {}, aux2: {}, aux3: {}",
                command_name, fault->aux[0], fault->aux[1], fault->aux[2], fault->aux[3]);
}
}  // namespace Vulkan
