// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

struct InputVertexData;
struct OutputVertexData;

namespace TransformUnit
{
	void TransformPosition(const InputVertexData *src, OutputVertexData *dst);
	void TransformNormal(const InputVertexData *src, bool nbt, OutputVertexData *dst);
	void TransformColor(const InputVertexData *src, OutputVertexData *dst);
	void TransformTexCoord(const InputVertexData *src, OutputVertexData *dst, bool specialCase);
}
