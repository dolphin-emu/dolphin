// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <functional>
#include "Common/CommonTypes.h"
#include "VideoCommon/ShaderGenCommon.h"
#include "VideoCommon/VertexManagerBase.h"

enum class APIType;

#pragma pack(1)

struct geometry_shader_uid_data
{
  u32 NumValues() const { return sizeof(geometry_shader_uid_data); }
  bool IsPassthrough() const;

  u32 numTexGens : 4;
  u32 primitive_type : 2;
};

#pragma pack()

typedef ShaderUid<geometry_shader_uid_data> GeometryShaderUid;

ShaderCode GenerateGeometryShaderCode(APIType ApiType, const ShaderHostConfig& host_config,
                                      const geometry_shader_uid_data* uid_data);
GeometryShaderUid GetGeometryShaderUid(u32 primitive_type);
void EnumerateGeometryShaderUids(const std::function<void(const GeometryShaderUid&)>& callback);
