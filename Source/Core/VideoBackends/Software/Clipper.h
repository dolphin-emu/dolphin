// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

struct OutputVertexData;

namespace Clipper
{
void Init();

void ProcessTriangle(OutputVertexData* v0, OutputVertexData* v1, OutputVertexData* v2);

void ProcessLine(OutputVertexData* v0, OutputVertexData* v1);

bool CullTest(const OutputVertexData* v0, const OutputVertexData* v1, const OutputVertexData* v2,
              bool& backface);

void PerspectiveDivide(OutputVertexData* vertex);
}  // namespace Clipper
