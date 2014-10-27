// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include "VideoCommon/ShaderGenCommon.h"
#include "VideoCommon/VideoCommon.h"

#pragma pack(1)

struct geometry_shader_uid_data
{
	u32 NumValues() const { return sizeof(geometry_shader_uid_data); }

	u32 stereo : 1;
};

#pragma pack()

typedef ShaderUid<geometry_shader_uid_data> GeometryShaderUid;

void GenerateGeometryShaderCode(ShaderCode& object, u32 components, API_TYPE ApiType);
void GetGeometryShaderUid(GeometryShaderUid& object, u32 components, API_TYPE ApiType);
