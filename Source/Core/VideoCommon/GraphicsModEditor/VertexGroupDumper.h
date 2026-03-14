// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <map>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

#include <Eigen/Dense>

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

  void OnXFBCreated(const std::string& hash);
  void OnFramePresented(std::span<std::string> xfbs_presented);

  void Record(const VertexGroupRecordingRequest& request);
  void StopRecord();
  bool IsRecording() const;

  struct FinalData
  {
    std::vector<Eigen::Matrix3Xd> mesh_poses;
    std::map<int, std::size_t> frame_id_to_mesh_index;
    Eigen::MatrixXi rest_pose_face_indexes;
    std::size_t original_vertex_count;
  };

private:
  void Save();

  std::optional<VertexGroupRecordingRequest> m_recording_request;
  std::map<GraphicsModSystem::DrawCallID, FinalData> m_draw_call_to_data;
  using XFBData = std::vector<std::pair<GraphicsModSystem::DrawCallID, Eigen::Matrix3Xd>>;
  XFBData m_current_xfb_data;
  std::map<std::string, XFBData> m_xfb_to_draw_data;
  int m_frame_id = 0;
};
}  // namespace GraphicsModEditor
