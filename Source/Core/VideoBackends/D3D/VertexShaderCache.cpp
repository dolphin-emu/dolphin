// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <string>

#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/MsgHandler.h"
#include "Common/StringUtil.h"

#include "Core/ConfigManager.h"
#include "Core/Host.h"

#include "VideoBackends/D3D/D3DShader.h"
#include "VideoBackends/D3D/D3DState.h"
#include "VideoBackends/D3D/VertexManager.h"
#include "VideoBackends/D3D/VertexShaderCache.h"

#include "VideoCommon/Debugger.h"
#include "VideoCommon/Statistics.h"
#include "VideoCommon/UberShaderVertex.h"
#include "VideoCommon/VertexLoaderManager.h"
#include "VideoCommon/VertexShaderGen.h"

namespace DX11
{
static ID3D11VertexShader* SimpleVertexShader = nullptr;
static ID3D11VertexShader* ClearVertexShader = nullptr;
static ID3D11InputLayout* SimpleLayout = nullptr;
static ID3D11InputLayout* ClearLayout = nullptr;

ID3D11VertexShader* VertexShaderCache::GetSimpleVertexShader()
{
  return SimpleVertexShader;
}
ID3D11VertexShader* VertexShaderCache::GetClearVertexShader()
{
  return ClearVertexShader;
}
ID3D11InputLayout* VertexShaderCache::GetSimpleInputLayout()
{
  return SimpleLayout;
}
ID3D11InputLayout* VertexShaderCache::GetClearInputLayout()
{
  return ClearLayout;
}

// this class will load the precompiled shaders into our cache
template <typename UidType>
class VertexShaderCacheInserter : public LinearDiskCacheReader<UidType, u8>
{
public:
  void Read(const UidType& key, const u8* value, u32 value_size)
  {
    D3DBlob* blob = new D3DBlob(value_size, value);
    VertexShaderCache::InsertByteCode(key, blob);
    blob->Release();
  }
};

const char simple_shader_code[] = {
    "struct VSOUTPUT\n"
    "{\n"
    "float4 vPosition : POSITION;\n"
    "float3 vTexCoord : TEXCOORD0;\n"
    "};\n"
    "VSOUTPUT main(float4 inPosition : POSITION,float3 inTEX0 : TEXCOORD0)\n"
    "{\n"
    "VSOUTPUT OUT;\n"
    "OUT.vPosition = inPosition;\n"
    "OUT.vTexCoord = inTEX0;\n"
    "return OUT;\n"
    "}\n"};

const char clear_shader_code[] = {
    "struct VSOUTPUT\n"
    "{\n"
    "float4 vPosition   : POSITION;\n"
    "float4 vColor0   : COLOR0;\n"
    "};\n"
    "VSOUTPUT main(float4 inPosition : POSITION,float4 inColor0: COLOR0)\n"
    "{\n"
    "VSOUTPUT OUT;\n"
    "OUT.vPosition = inPosition;\n"
    "OUT.vColor0 = inColor0;\n"
    "return OUT;\n"
    "}\n"};

void VertexShaderCache::Init()
{
  const D3D11_INPUT_ELEMENT_DESC simpleelems[2] = {
      {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
      {"TEXCOORD", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},

  };
  const D3D11_INPUT_ELEMENT_DESC clearelems[2] = {
      {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
      {"COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
  };

  D3DBlob* blob;
  D3D::CompileVertexShader(simple_shader_code, &blob);
  D3D::device->CreateInputLayout(simpleelems, 2, blob->Data(), blob->Size(), &SimpleLayout);
  SimpleVertexShader = D3D::CreateVertexShaderFromByteCode(blob);
  if (SimpleLayout == nullptr || SimpleVertexShader == nullptr)
    PanicAlert("Failed to create simple vertex shader or input layout at %s %d\n", __FILE__,
               __LINE__);
  blob->Release();
  D3D::SetDebugObjectName(SimpleVertexShader, "simple vertex shader");
  D3D::SetDebugObjectName(SimpleLayout, "simple input layout");

  D3D::CompileVertexShader(clear_shader_code, &blob);
  D3D::device->CreateInputLayout(clearelems, 2, blob->Data(), blob->Size(), &ClearLayout);
  ClearVertexShader = D3D::CreateVertexShaderFromByteCode(blob);
  if (ClearLayout == nullptr || ClearVertexShader == nullptr)
    PanicAlert("Failed to create clear vertex shader or input layout at %s %d\n", __FILE__,
               __LINE__);
  blob->Release();
  D3D::SetDebugObjectName(ClearVertexShader, "clear vertex shader");
  D3D::SetDebugObjectName(ClearLayout, "clear input layout");

  SETSTAT(stats.numVertexShadersCreated, 0);
  SETSTAT(stats.numVertexShadersAlive, 0);
}

void VertexShaderCache::Shutdown()
{
  SAFE_RELEASE(SimpleVertexShader);
  SAFE_RELEASE(ClearVertexShader);

  SAFE_RELEASE(SimpleLayout);
  SAFE_RELEASE(ClearLayout);
}
}  // namespace DX11
