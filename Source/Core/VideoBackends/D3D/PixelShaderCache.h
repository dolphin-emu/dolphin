// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <d3d11.h>
#include <map>

#include "VideoCommon/AsyncShaderCompiler.h"
#include "VideoCommon/PixelShaderGen.h"
#include "VideoCommon/UberShaderPixel.h"

namespace DX11
{
class D3DBlob;

class PixelShaderCache
{
public:
  static void Init();
  static void Reload();
  static void Clear();
  static void Shutdown();
  static bool SetShader();
  static bool SetUberShader();
  static bool InsertByteCode(const PixelShaderUid& uid, const u8* data, size_t len);
  static bool InsertByteCode(const UberShader::PixelShaderUid& uid, const u8* data, size_t len);
  static bool InsertShader(const PixelShaderUid& uid, ID3D11PixelShader* shader);
  static bool InsertShader(const UberShader::PixelShaderUid& uid, ID3D11PixelShader* shader);
  static void QueueUberShaderCompiles();

  static ID3D11Buffer* GetConstantBuffer();

  static ID3D11PixelShader* GetColorMatrixProgram(bool multisampled);
  static ID3D11PixelShader* GetColorCopyProgram(bool multisampled);
  static ID3D11PixelShader* GetDepthMatrixProgram(bool multisampled);
  static ID3D11PixelShader* GetClearProgram();
  static ID3D11PixelShader* GetAnaglyphProgram();
  static ID3D11PixelShader* GetDepthResolveProgram();
  static ID3D11PixelShader* ReinterpRGBA6ToRGB8(bool multisampled);
  static ID3D11PixelShader* ReinterpRGB8ToRGBA6(bool multisampled);

  static void InvalidateMSAAShaders();

private:
  struct PSCacheEntry
  {
    ID3D11PixelShader* shader;
    bool pending;

    PSCacheEntry() : shader(nullptr), pending(false) {}
    void Destroy() { SAFE_RELEASE(shader); }
  };

  class PixelShaderCompilerWorkItem : public VideoCommon::AsyncShaderCompiler::WorkItem
  {
  public:
    PixelShaderCompilerWorkItem(const PixelShaderUid& uid);
    ~PixelShaderCompilerWorkItem() override;

    bool Compile() override;
    void Retrieve() override;

  private:
    PixelShaderUid m_uid;
    ID3D11PixelShader* m_shader = nullptr;
    D3DBlob* m_bytecode = nullptr;
  };

  class UberPixelShaderCompilerWorkItem : public VideoCommon::AsyncShaderCompiler::WorkItem
  {
  public:
    UberPixelShaderCompilerWorkItem(const UberShader::PixelShaderUid& uid);
    ~UberPixelShaderCompilerWorkItem() override;

    bool Compile() override;
    void Retrieve() override;

  private:
    UberShader::PixelShaderUid m_uid;
    ID3D11PixelShader* m_shader = nullptr;
    D3DBlob* m_bytecode = nullptr;
  };

  typedef std::map<PixelShaderUid, PSCacheEntry> PSCache;
  typedef std::map<UberShader::PixelShaderUid, PSCacheEntry> UberPSCache;

  static void LoadShaderCache();

  static PSCache PixelShaders;
  static UberPSCache UberPixelShaders;
  static const PSCacheEntry* last_entry;
  static const PSCacheEntry* last_uber_entry;
  static PixelShaderUid last_uid;
  static UberShader::PixelShaderUid last_uber_uid;
};

}  // namespace DX11
