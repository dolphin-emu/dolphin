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
  static void Reload();
  static void Clear();
  static void Shutdown();
  static bool SetShader(u32 primitive_type);
  static bool CompileShader(const GeometryShaderUid& uid);
  static bool InsertByteCode(const GeometryShaderUid& uid, const u8* bytecode, size_t len);
  static void PrecompileShaders();

  static ID3D11GeometryShader* GetClearGeometryShader();
  static ID3D11GeometryShader* GetCopyGeometryShader();

  static ID3D11Buffer*& GetConstantBuffer();

private:
  struct GSCacheEntry
  {
    ID3D11GeometryShader* shader;

    GSCacheEntry() : shader(nullptr) {}
    void Destroy() { SAFE_RELEASE(shader); }
  };

  typedef std::map<GeometryShaderUid, GSCacheEntry> GSCache;

  static void LoadShaderCache();

  static GSCache GeometryShaders;
  static const GSCacheEntry* last_entry;
  static GeometryShaderUid last_uid;
  static const GSCacheEntry pass_entry;
};

}  // namespace DX11
