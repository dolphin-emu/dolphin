// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <d3d11.h>
#include <map>

#include "VideoCommon/GeometryShaderGen.h"

namespace DX11
{
class GeometryShaderCache
{
public:
  static void Init();
  static void Clear();
  static void Shutdown();
  static bool SetShader(u32 primitive_type);  // TODO: Should be renamed to LoadShader
  static bool InsertByteCode(const GeometryShaderUid& uid, const void* bytecode,
                             unsigned int bytecodelen);

  static ID3D11GeometryShader* GetClearGeometryShader();
  static ID3D11GeometryShader* GetCopyGeometryShader();

  static ID3D11GeometryShader* GetActiveShader() { return last_entry->shader.Get(); }
  static ID3D11Buffer* GetConstantBuffer();

private:
  struct GSCacheEntry
  {
    ComPtr<ID3D11GeometryShader> shader;

    void Destroy() { shader.Reset(); }
  };

  typedef std::map<GeometryShaderUid, GSCacheEntry> GSCache;

  static GSCache GeometryShaders;
  static const GSCacheEntry* last_entry;
  static GeometryShaderUid last_uid;
  static const GSCacheEntry pass_entry;
};

}  // namespace DX11
