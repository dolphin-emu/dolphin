// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <Eigen/Dense>

#include "Common/CommonTypes.h"
#include "VideoCommon/GraphicsModSystem/Types.h"

namespace VideoCommon
{
struct SkeletonInstance
{
  std::vector<Eigen::Affine3f> bone_matrices;
  std::vector<bool> solved_this_frame;
  std::vector<float> best_confidence;
  u64 last_seen_xfb = 0;
};

class SkeletonManager
{
public:
  void XFBTriggered();

  // Returns the correct skeleton for a specific draw call in the current frame
  SkeletonInstance& GetSkeletonForChunk(GraphicsModSystem::DrawCallID draw_call_id,
                                        std::string_view group_name, std::size_t num_bones);

private:
  u64 m_xfb_count = 0;
  std::unordered_map<GraphicsModSystem::DrawCallID, int> m_draw_call_occurrence_count;
  std::unordered_map<std::string, SkeletonInstance> m_registry;  // group or id to data
};
}  // namespace VideoCommon
