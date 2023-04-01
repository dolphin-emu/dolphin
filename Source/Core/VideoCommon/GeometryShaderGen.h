// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "VideoCommon/ShaderGenCommon.h"
#include "VideoCommon/VertexManagerBase.h"
#include "VideoCommon/VideoCommon.h"

#pragma pack(1)

struct geometry_shader_uid_data
{
	u32 NumValues() const { return sizeof(geometry_shader_uid_data); }
	bool IsPassthrough() const { return primitive_type == PRIMITIVE_TRIANGLES && !stereo && !wireframe; }

	u32 stereo : 1;
	u32 numTexGens : 4;
	u32 pixel_lighting : 1;
	u32 primitive_type : 2;
	u32 wireframe : 1;
};

#pragma pack()

typedef ShaderUid<geometry_shader_uid_data> GeometryShaderUid;

ShaderCode GenerateGeometryShaderCode(u32 primitive_type, API_TYPE ApiType);
GeometryShaderUid GetGeometryShaderUid(u32 primitive_type, API_TYPE ApiType);
