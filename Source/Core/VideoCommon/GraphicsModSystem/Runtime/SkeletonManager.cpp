// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/GraphicsModSystem/Runtime/SkeletonManager.h"

#include <fmt/format.h>

#include "Common/Logging/Log.h"

namespace VideoCommon
{
void SkeletonManager::XFBTriggered()
{
  m_xfb_count++;
  m_draw_call_occurrence_count.clear();
}

// Returns the correct skeleton for a specific chunk in the current frame
SkeletonInstance& SkeletonManager::GetSkeletonForChunk(GraphicsModSystem::DrawCallID draw_call_id,
                                                       std::string_view group_name,
                                                       std::size_t num_bones)
{
  // 1. Handle Instancing: Is this the 1st, 2nd, or 3rd time we've seen this chunk?
  int occurrence = m_draw_call_occurrence_count[draw_call_id]++;

  // 2. Generate a unique key
  // If it's a group, all chunks in that group share one skeleton per occurrence.
  // If no group, the chunk gets its own private skeleton per occurrence.
  const std::string instance_key =
      (group_name.empty()) ?
          fmt::format("chunk_{}_{}", std::to_underlying(draw_call_id), occurrence) :
          fmt::format("group_{}_{}", group_name, occurrence);

  auto& instance = m_registry[instance_key];

  // 3. Initialize if it's a brand new entity or has been gone too long (Teleport Fix)
  if (instance.bone_matrices.empty() || (m_xfb_count - instance.last_seen_xfb) > 60)
  {
    instance.bone_matrices.assign(num_bones, Eigen::Affine3f::Identity());
    instance.solved_this_frame.assign(num_bones, false);
    instance.best_confidence.assign(num_bones, 0);
  }

  // 4. Reset "solved" flags if this is the first time we see this instance this frame
  if (instance.last_seen_xfb != m_xfb_count)
  {
    std::fill(instance.solved_this_frame.begin(), instance.solved_this_frame.end(), false);
    std::fill(instance.best_confidence.begin(), instance.best_confidence.end(), 0);
    instance.last_seen_xfb = m_xfb_count;
  }

  return instance;
}
}  // namespace VideoCommon
