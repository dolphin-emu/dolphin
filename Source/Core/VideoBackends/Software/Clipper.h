// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>

#include "VideoBackends/Software/NativeVertexFormat.h"

namespace Clipper
{
class Clipper
{
public:
  Clipper();

  void ProcessTriangle(OutputVertexData* v0, OutputVertexData* v1, OutputVertexData* v2);
  void ProcessLine(OutputVertexData* lineV0, OutputVertexData* lineV1);
  void ProcessPoint(OutputVertexData* center);

private:
  enum
  {
    NUM_CLIPPED_VERTICES = 33,
    NUM_INDICES = NUM_CLIPPED_VERTICES + 3
  };

  void Init();

  void AddInterpolatedVertex(float t, int out, int in, int* num_vertices);

  void ClipTriangle(int* indices, int* num_indices);
  void ClipLine(int* indices);

  std::array<OutputVertexData, NUM_CLIPPED_VERTICES> m_clipped_vertices{};
  std::array<OutputVertexData*, NUM_INDICES> m_vertices{};
};
}  // namespace Clipper
