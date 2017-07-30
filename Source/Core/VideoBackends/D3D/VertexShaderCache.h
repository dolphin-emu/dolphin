// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <map>

#include "VideoBackends/D3D/D3DBase.h"
#include "VideoBackends/D3D/D3DBlob.h"

#include "VideoCommon/AsyncShaderCompiler.h"
#include "VideoCommon/UberShaderVertex.h"
#include "VideoCommon/VertexShaderGen.h"

namespace DX11
{
class D3DVertexFormat;

class VertexShaderCache
{
public:
  static void Init();
  static void Reload();
  static void Clear();
  static void Shutdown();
  static bool SetShader(D3DVertexFormat* vertex_format);
  static bool SetUberShader(D3DVertexFormat* vertex_format);
  static void RetreiveAsyncShaders();
  static void QueueUberShaderCompiles();
  static void WaitForBackgroundCompilesToComplete();

  static ID3D11Buffer*& GetConstantBuffer();

  static ID3D11VertexShader* GetSimpleVertexShader();
  static ID3D11VertexShader* GetClearVertexShader();
  static ID3D11InputLayout* GetSimpleInputLayout();
  static ID3D11InputLayout* GetClearInputLayout();

  static bool InsertByteCode(const VertexShaderUid& uid, D3DBlob* blob);
  static bool InsertByteCode(const UberShader::VertexShaderUid& uid, D3DBlob* blob);
  static bool InsertShader(const VertexShaderUid& uid, ID3D11VertexShader* shader, D3DBlob* blob);
  static bool InsertShader(const UberShader::VertexShaderUid& uid, ID3D11VertexShader* shader,
                           D3DBlob* blob);

private:
  struct VSCacheEntry
  {
    ID3D11VertexShader* shader;
    D3DBlob* bytecode;  // needed to initialize the input layout
    bool pending;

    VSCacheEntry() : shader(nullptr), bytecode(nullptr), pending(false) {}
    void SetByteCode(D3DBlob* blob)
    {
      SAFE_RELEASE(bytecode);
      bytecode = blob;
      blob->AddRef();
    }
    void Destroy()
    {
      SAFE_RELEASE(shader);
      SAFE_RELEASE(bytecode);
    }
  };

  class VertexShaderCompilerWorkItem : public VideoCommon::AsyncShaderCompiler::WorkItem
  {
  public:
    VertexShaderCompilerWorkItem(const VertexShaderUid& uid);
    ~VertexShaderCompilerWorkItem() override;

    bool Compile() override;
    void Retrieve() override;

  private:
    VertexShaderUid m_uid;
    D3DBlob* m_bytecode = nullptr;
    ID3D11VertexShader* m_vs = nullptr;
  };

  class UberVertexShaderCompilerWorkItem : public VideoCommon::AsyncShaderCompiler::WorkItem
  {
  public:
    UberVertexShaderCompilerWorkItem(const UberShader::VertexShaderUid& uid);
    ~UberVertexShaderCompilerWorkItem() override;

    bool Compile() override;
    void Retrieve() override;

  private:
    UberShader::VertexShaderUid m_uid;
    D3DBlob* m_bytecode = nullptr;
    ID3D11VertexShader* m_vs = nullptr;
  };

  typedef std::map<VertexShaderUid, VSCacheEntry> VSCache;
  typedef std::map<UberShader::VertexShaderUid, VSCacheEntry> UberVSCache;

  static void LoadShaderCache();
  static void SetInputLayout();

  static VSCache vshaders;
  static UberVSCache ubervshaders;
  static const VSCacheEntry* last_entry;
  static const VSCacheEntry* last_uber_entry;
  static VertexShaderUid last_uid;
  static UberShader::VertexShaderUid last_uber_uid;
};

}  // namespace DX11
