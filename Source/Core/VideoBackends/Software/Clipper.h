// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

struct OutputVertexData;

namespace Clipper
{
void Init();

void ProcessTriangle(OutputVertexData* v0, OutputVertexData* v1, OutputVertexData* v2);

void ProcessLine(OutputVertexData* v0, OutputVertexData* v1);

void ProcessPoint(OutputVertexData* v);
}  // namespace Clipper
