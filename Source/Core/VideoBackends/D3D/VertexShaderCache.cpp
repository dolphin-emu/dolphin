// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <string>

#include "Common/Align.h"
#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/LinearDiskCache.h"
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
#include "VideoCommon/VertexShaderManager.h"

namespace DX11
{
VertexShaderCache::VSCache VertexShaderCache::vshaders;
VertexShaderCache::UberVSCache VertexShaderCache::ubervshaders;
const VertexShaderCache::VSCacheEntry* VertexShaderCache::last_entry;
const VertexShaderCache::VSCacheEntry* VertexShaderCache::last_uber_entry;
VertexShaderUid VertexShaderCache::last_uid;
UberShader::VertexShaderUid VertexShaderCache::last_uber_uid;

static ID3D11VertexShader* SimpleVertexShader = nullptr;
static ID3D11VertexShader* ClearVertexShader = nullptr;
static ID3D11InputLayout* SimpleLayout = nullptr;
static ID3D11InputLayout* ClearLayout = nullptr;

LinearDiskCache<VertexShaderUid, u8> g_vs_disk_cache;
LinearDiskCache<UberShader::VertexShaderUid, u8> g_uber_vs_disk_cache;
std::unique_ptr<VideoCommon::AsyncShaderCompiler> g_async_compiler;

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

ID3D11Buffer* vscbuf = nullptr;

ID3D11Buffer*& VertexShaderCache::GetConstantBuffer()
{
  // TODO: divide the global variables of the generated shaders into about 5 constant buffers to
  // speed this up
  if (VertexShaderManager::dirty)
  {
    D3D11_MAPPED_SUBRESOURCE map;
    D3D::context->Map(vscbuf, 0, D3D11_MAP_WRITE_DISCARD, 0, &map);
    memcpy(map.pData, &VertexShaderManager::constants, sizeof(VertexShaderConstants));
    D3D::context->Unmap(vscbuf, 0);
    VertexShaderManager::dirty = false;

    ADDSTAT(stats.thisFrame.bytesUniformStreamed, sizeof(VertexShaderConstants));
  }
  return vscbuf;
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
    "float  vTexCoord1 : TEXCOORD1;\n"
    "};\n"
    "VSOUTPUT main(float4 inPosition : POSITION,float4 inTEX0 : TEXCOORD0)\n"
    "{\n"
    "VSOUTPUT OUT;\n"
    "OUT.vPosition = inPosition;\n"
    "OUT.vTexCoord = inTEX0.xyz;\n"
    "OUT.vTexCoord1 = inTEX0.w;\n"
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

  unsigned int cbsize = Common::AlignUp(static_cast<unsigned int>(sizeof(VertexShaderConstants)),
                                        16);  // must be a multiple of 16
  D3D11_BUFFER_DESC cbdesc = CD3D11_BUFFER_DESC(cbsize, D3D11_BIND_CONSTANT_BUFFER,
                                                D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE);
  HRESULT hr = D3D::device->CreateBuffer(&cbdesc, nullptr, &vscbuf);
  CHECK(hr == S_OK, "Create vertex shader constant buffer (size=%u)", cbsize);
  D3D::SetDebugObjectName((ID3D11DeviceChild*)vscbuf,
                          "vertex shader constant buffer used to emulate the GX pipeline");

  D3DBlob* blob;
  D3D::CompileVertexShader(simple_shader_code, &blob);
  D3D::device->CreateInputLayout(simpleelems, 2, blob->Data(), blob->Size(), &SimpleLayout);
  SimpleVertexShader = D3D::CreateVertexShaderFromByteCode(blob);
  if (SimpleLayout == nullptr || SimpleVertexShader == nullptr)
    PanicAlert("Failed to create simple vertex shader or input layout at %s %d\n", __FILE__,
               __LINE__);
  blob->Release();
  D3D::SetDebugObjectName((ID3D11DeviceChild*)SimpleVertexShader, "simple vertex shader");
  D3D::SetDebugObjectName((ID3D11DeviceChild*)SimpleLayout, "simple input layout");

  D3D::CompileVertexShader(clear_shader_code, &blob);
  D3D::device->CreateInputLayout(clearelems, 2, blob->Data(), blob->Size(), &ClearLayout);
  ClearVertexShader = D3D::CreateVertexShaderFromByteCode(blob);
  if (ClearLayout == nullptr || ClearVertexShader == nullptr)
    PanicAlert("Failed to create clear vertex shader or input layout at %s %d\n", __FILE__,
               __LINE__);
  blob->Release();
  D3D::SetDebugObjectName((ID3D11DeviceChild*)ClearVertexShader, "clear vertex shader");
  D3D::SetDebugObjectName((ID3D11DeviceChild*)ClearLayout, "clear input layout");

  Clear();

  SETSTAT(stats.numVertexShadersCreated, 0);
  SETSTAT(stats.numVertexShadersAlive, 0);

  if (g_ActiveConfig.bShaderCache)
    LoadShaderCache();

  g_async_compiler = std::make_unique<VideoCommon::AsyncShaderCompiler>();
  g_async_compiler->ResizeWorkerThreads(g_ActiveConfig.CanPrecompileUberShaders() ?
                                            g_ActiveConfig.GetShaderPrecompilerThreads() :
                                            g_ActiveConfig.GetShaderCompilerThreads());

  if (g_ActiveConfig.CanPrecompileUberShaders())
    QueueUberShaderCompiles();
}

void VertexShaderCache::LoadShaderCache()
{
  VertexShaderCacheInserter<VertexShaderUid> inserter;
  g_vs_disk_cache.OpenAndRead(GetDiskShaderCacheFileName(APIType::D3D, "VS", true, true), inserter);

  VertexShaderCacheInserter<UberShader::VertexShaderUid> uber_inserter;
  g_uber_vs_disk_cache.OpenAndRead(GetDiskShaderCacheFileName(APIType::D3D, "UberVS", false, true),
                                   uber_inserter);
}

void VertexShaderCache::Reload()
{
  g_async_compiler->WaitUntilCompletion();
  g_async_compiler->RetrieveWorkItems();

  g_vs_disk_cache.Sync();
  g_vs_disk_cache.Close();
  g_uber_vs_disk_cache.Sync();
  g_uber_vs_disk_cache.Close();
  Clear();

  if (g_ActiveConfig.bShaderCache)
    LoadShaderCache();

  if (g_ActiveConfig.CanPrecompileUberShaders())
    QueueUberShaderCompiles();
}

void VertexShaderCache::Clear()
{
  for (auto& iter : vshaders)
    iter.second.Destroy();
  for (auto& iter : ubervshaders)
    iter.second.Destroy();
  vshaders.clear();
  ubervshaders.clear();

  last_uid = {};
  last_uber_uid = {};
  last_entry = nullptr;
  last_uber_entry = nullptr;
  last_uid = {};
  last_uber_uid = {};
}

void VertexShaderCache::Shutdown()
{
  g_async_compiler->StopWorkerThreads();
  g_async_compiler->RetrieveWorkItems();

  SAFE_RELEASE(vscbuf);

  SAFE_RELEASE(SimpleVertexShader);
  SAFE_RELEASE(ClearVertexShader);

  SAFE_RELEASE(SimpleLayout);
  SAFE_RELEASE(ClearLayout);

  Clear();
  g_vs_disk_cache.Sync();
  g_vs_disk_cache.Close();
  g_uber_vs_disk_cache.Sync();
  g_uber_vs_disk_cache.Close();
}

bool VertexShaderCache::SetShader(D3DVertexFormat* vertex_format)
{
  if (g_ActiveConfig.bDisableSpecializedShaders)
    return SetUberShader(vertex_format);

  VertexShaderUid uid = GetVertexShaderUid();
  if (last_entry && uid == last_uid)
  {
    if (last_entry->pending)
      return SetUberShader(vertex_format);

    if (!last_entry->shader)
      return false;

    vertex_format->SetInputLayout(last_entry->bytecode);
    D3D::stateman->SetVertexShader(last_entry->shader);
    return true;
  }

  auto iter = vshaders.find(uid);
  if (iter != vshaders.end())
  {
    const VSCacheEntry& entry = iter->second;
    if (entry.pending)
      return SetUberShader(vertex_format);

    last_uid = uid;
    last_entry = &entry;

    GFX_DEBUGGER_PAUSE_AT(NEXT_VERTEX_SHADER_CHANGE, true);
    if (!last_entry->shader)
      return false;

    vertex_format->SetInputLayout(last_entry->bytecode);
    D3D::stateman->SetVertexShader(last_entry->shader);
    return true;
  }

  // Background compiling?
  if (g_ActiveConfig.CanBackgroundCompileShaders())
  {
    // Create a pending entry
    VSCacheEntry entry;
    entry.pending = true;
    vshaders[uid] = entry;

    // Queue normal shader compiling and use ubershader
    g_async_compiler->QueueWorkItem(
        g_async_compiler->CreateWorkItem<VertexShaderCompilerWorkItem>(uid));
    return SetUberShader(vertex_format);
  }

  // Need to compile a new shader
  D3DBlob* bytecode = nullptr;
  ShaderCode code =
      GenerateVertexShaderCode(APIType::D3D, ShaderHostConfig::GetCurrent(), uid.GetUidData());
  D3D::CompileVertexShader(code.GetBuffer(), &bytecode);
  if (!InsertByteCode(uid, bytecode))
  {
    SAFE_RELEASE(bytecode);
    return false;
  }

  g_vs_disk_cache.Append(uid, bytecode->Data(), bytecode->Size());
  bytecode->Release();
  return SetShader(vertex_format);
}

bool VertexShaderCache::SetUberShader(D3DVertexFormat* vertex_format)
{
  D3DVertexFormat* uber_vertex_format = static_cast<D3DVertexFormat*>(
      VertexLoaderManager::GetUberVertexFormat(vertex_format->GetVertexDeclaration()));
  UberShader::VertexShaderUid uid = UberShader::GetVertexShaderUid();
  if (last_uber_entry && last_uber_uid == uid)
  {
    if (!last_uber_entry->shader)
      return false;

    uber_vertex_format->SetInputLayout(last_uber_entry->bytecode);
    D3D::stateman->SetVertexShader(last_uber_entry->shader);
    return true;
  }

  auto iter = ubervshaders.find(uid);
  if (iter != ubervshaders.end())
  {
    const VSCacheEntry& entry = iter->second;
    last_uber_uid = uid;
    last_uber_entry = &entry;

    GFX_DEBUGGER_PAUSE_AT(NEXT_VERTEX_SHADER_CHANGE, true);
    if (!last_uber_entry->shader)
      return false;

    uber_vertex_format->SetInputLayout(last_uber_entry->bytecode);
    D3D::stateman->SetVertexShader(last_uber_entry->shader);
    return true;
  }

  // Need to compile a new shader
  D3DBlob* bytecode = nullptr;
  ShaderCode code =
      UberShader::GenVertexShader(APIType::D3D, ShaderHostConfig::GetCurrent(), uid.GetUidData());
  D3D::CompileVertexShader(code.GetBuffer(), &bytecode);
  if (!InsertByteCode(uid, bytecode))
  {
    SAFE_RELEASE(bytecode);
    return false;
  }

  g_uber_vs_disk_cache.Append(uid, bytecode->Data(), bytecode->Size());
  bytecode->Release();
  return SetUberShader(vertex_format);
}

bool VertexShaderCache::InsertByteCode(const VertexShaderUid& uid, D3DBlob* blob)
{
  ID3D11VertexShader* shader =
      blob ? D3D::CreateVertexShaderFromByteCode(blob->Data(), blob->Size()) : nullptr;
  bool result = InsertShader(uid, shader, blob);
  SAFE_RELEASE(shader);
  return result;
}

bool VertexShaderCache::InsertByteCode(const UberShader::VertexShaderUid& uid, D3DBlob* blob)
{
  ID3D11VertexShader* shader =
      blob ? D3D::CreateVertexShaderFromByteCode(blob->Data(), blob->Size()) : nullptr;
  bool result = InsertShader(uid, shader, blob);
  SAFE_RELEASE(shader);
  return result;
}

bool VertexShaderCache::InsertShader(const VertexShaderUid& uid, ID3D11VertexShader* shader,
                                     D3DBlob* blob)
{
  auto iter = vshaders.find(uid);
  if (iter != vshaders.end() && !iter->second.pending)
    return false;

  VSCacheEntry& newentry = vshaders[uid];
  newentry.pending = false;
  if (!shader || !blob)
    return false;

  shader->AddRef();
  newentry.SetByteCode(blob);
  newentry.shader = shader;

  INCSTAT(stats.numPixelShadersCreated);
  SETSTAT(stats.numPixelShadersAlive, static_cast<int>(vshaders.size()));
  return true;
}

bool VertexShaderCache::InsertShader(const UberShader::VertexShaderUid& uid,
                                     ID3D11VertexShader* shader, D3DBlob* blob)
{
  auto iter = ubervshaders.find(uid);
  if (iter != ubervshaders.end() && !iter->second.pending)
    return false;

  VSCacheEntry& newentry = ubervshaders[uid];
  newentry.pending = false;
  if (!shader || !blob)
    return false;

  shader->AddRef();
  newentry.SetByteCode(blob);
  newentry.shader = shader;
  return true;
}

void VertexShaderCache::RetreiveAsyncShaders()
{
  g_async_compiler->RetrieveWorkItems();
}

void VertexShaderCache::QueueUberShaderCompiles()
{
  UberShader::EnumerateVertexShaderUids([&](const UberShader::VertexShaderUid& uid) {
    if (ubervshaders.find(uid) != ubervshaders.end())
      return;

    g_async_compiler->QueueWorkItem(
        g_async_compiler->CreateWorkItem<UberVertexShaderCompilerWorkItem>(uid));
  });
}

void VertexShaderCache::WaitForBackgroundCompilesToComplete()
{
  g_async_compiler->WaitUntilCompletion([](size_t completed, size_t total) {
    Host_UpdateProgressDialog(GetStringT("Compiling shaders...").c_str(),
                              static_cast<int>(completed), static_cast<int>(total));
  });
  g_async_compiler->RetrieveWorkItems();
  Host_UpdateProgressDialog("", -1, -1);

  // Switch from precompile -> runtime compiler threads.
  g_async_compiler->ResizeWorkerThreads(g_ActiveConfig.GetShaderCompilerThreads());
}

VertexShaderCache::VertexShaderCompilerWorkItem::VertexShaderCompilerWorkItem(
    const VertexShaderUid& uid)
{
  std::memcpy(&m_uid, &uid, sizeof(uid));
}

VertexShaderCache::VertexShaderCompilerWorkItem::~VertexShaderCompilerWorkItem()
{
  SAFE_RELEASE(m_bytecode);
  SAFE_RELEASE(m_vs);
}

bool VertexShaderCache::VertexShaderCompilerWorkItem::Compile()
{
  ShaderCode code =
      GenerateVertexShaderCode(APIType::D3D, ShaderHostConfig::GetCurrent(), m_uid.GetUidData());

  if (D3D::CompileVertexShader(code.GetBuffer(), &m_bytecode))
    m_vs = D3D::CreateVertexShaderFromByteCode(m_bytecode);

  return true;
}

void VertexShaderCache::VertexShaderCompilerWorkItem::Retrieve()
{
  if (InsertShader(m_uid, m_vs, m_bytecode))
    g_vs_disk_cache.Append(m_uid, m_bytecode->Data(), m_bytecode->Size());
}

VertexShaderCache::UberVertexShaderCompilerWorkItem::UberVertexShaderCompilerWorkItem(
    const UberShader::VertexShaderUid& uid)
{
  std::memcpy(&m_uid, &uid, sizeof(uid));
}

VertexShaderCache::UberVertexShaderCompilerWorkItem::~UberVertexShaderCompilerWorkItem()
{
  SAFE_RELEASE(m_bytecode);
  SAFE_RELEASE(m_vs);
}

bool VertexShaderCache::UberVertexShaderCompilerWorkItem::Compile()
{
  ShaderCode code =
      UberShader::GenVertexShader(APIType::D3D, ShaderHostConfig::GetCurrent(), m_uid.GetUidData());

  if (D3D::CompileVertexShader(code.GetBuffer(), &m_bytecode))
    m_vs = D3D::CreateVertexShaderFromByteCode(m_bytecode);

  return true;
}

void VertexShaderCache::UberVertexShaderCompilerWorkItem::Retrieve()
{
  if (InsertShader(m_uid, m_vs, m_bytecode))
    g_uber_vs_disk_cache.Append(m_uid, m_bytecode->Data(), m_bytecode->Size());
}

}  // namespace DX11
