// Copyright 2011 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/CommonTypes.h"

namespace FifoRecordAnalyzer
{
// Must call this before analyzing Fifo commands with FifoAnalyzer::AnalyzeCommand()
void Initialize(const u32* cpMem);

void ProcessLoadIndexedXf(u32 val, int array);
void WriteVertexArray(int arrayIndex, const u8* vertexData, int vertexSize, int numVertices);
}  // namespace FifoRecordAnalyzer
