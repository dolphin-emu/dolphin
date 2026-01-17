// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <map>
#include <optional>
#include <set>
#include <unordered_set>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/Matrix.h"

#include "imgui/../../eigen/eigen/Eigen/Dense"
#include "imgui/../../eigen/eigen/Eigen/Geometry"

#include "VideoCommon/GraphicsModSystem/Types.h"

namespace GraphicsModEditor
{
struct VertexGroupRecordingRequest
{
  std::unordered_set<GraphicsModSystem::DrawCallID> m_draw_call_ids;
};

class VertexGroupDumper
{
public:
  bool IsDrawCallInRecording(GraphicsModSystem::DrawCallID draw_call_id) const;
  void AddDataToRecording(GraphicsModSystem::DrawCallID draw_call_id,
                          const GraphicsModSystem::DrawDataView& draw_data);

  void Record(const VertexGroupRecordingRequest& request);
  void StopRecord();
  bool IsRecording() const;

private:
  std::optional<VertexGroupRecordingRequest> m_recording_request;
  struct Data
  {
    // std::vector<std::optional<std::size_t>> group_assignments;
    // std::vector<std::size_t> group_sizes;
    // std::vector<Common::Vec3> vertex_positions;
    std::vector<Eigen::Matrix3Xd> vertex_meshes;
    Eigen::MatrixXi rest_pose_face_indexes;
  };
  std::map<GraphicsModSystem::DrawCallID, Data> m_draw_call_to_data;
};
}  // namespace GraphicsModEditor
