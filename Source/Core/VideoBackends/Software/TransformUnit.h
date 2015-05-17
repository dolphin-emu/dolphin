// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

struct InputVertexData;
struct OutputVertexData;

namespace TransformUnit
{
	void MultiplyVec2Mat24(const float *vec, const float *mat, float *result);
	void MultiplyVec2Mat34(const float *vec, const float *mat, float *result);
	void MultiplyVec3Mat33(const float *vec, const float *mat, float *result);
	void MultiplyVec3Mat34(const float *vec, const float *mat, float *result);

	void TransformPosition(const InputVertexData *src, OutputVertexData *dst);
	void TransformNormal(const InputVertexData *src, bool nbt, OutputVertexData *dst);
	void TransformColor(const InputVertexData *src, OutputVertexData *dst);
	void TransformTexCoord(const InputVertexData *src, OutputVertexData *dst, bool specialCase);
}
