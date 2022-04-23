// Copyright 2009 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

struct InputVertexData;
struct OutputVertexData;

namespace TransformUnit
{
void TransformPosition(const InputVertexData* src, OutputVertexData* dst);
void TransformNormal(const InputVertexData* src, OutputVertexData* dst);
void TransformColor(const InputVertexData* src, OutputVertexData* dst);
void TransformTexCoord(const InputVertexData* src, OutputVertexData* dst);
}  // namespace TransformUnit
