// Copyright 2011 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <tuple>

#include "Common/GL/GLUtil.h"
#include "Common/LinearDiskCache.h"

#include "VideoCommon/AsyncShaderCompiler.h"
#include "VideoCommon/GeometryShaderGen.h"
#include "VideoCommon/PixelShaderGen.h"
#include "VideoCommon/UberShaderPixel.h"
#include "VideoCommon/UberShaderVertex.h"
#include "VideoCommon/VertexShaderGen.h"

class cInterfaceBase;

namespace OGL
{
class GLVertexFormat;

class SHADERUID
{
public:
  VertexShaderUid vuid;
  PixelShaderUid puid;
  GeometryShaderUid guid;

  bool operator<(const SHADERUID& r) const
  {
    return std::tie(vuid, puid, guid) < std::tie(r.vuid, r.puid, r.guid);
  }

  bool operator==(const SHADERUID& r) const
  {
    return std::tie(vuid, puid, guid) == std::tie(r.vuid, r.puid, r.guid);
  }
};
class UBERSHADERUID
{
public:
  UberShader::VertexShaderUid vuid;
  UberShader::PixelShaderUid puid;
  GeometryShaderUid guid;

  bool operator<(const UBERSHADERUID& r) const
  {
    return std::tie(vuid, puid, guid) < std::tie(r.vuid, r.puid, r.guid);
  }

  bool operator==(const UBERSHADERUID& r) const
  {
    return std::tie(vuid, puid, guid) == std::tie(r.vuid, r.puid, r.guid);
  }
};

struct SHADER
{
  void Destroy()
  {
    DestroyShaders();
    if (glprogid)
    {
      glDeleteProgram(glprogid);
      glprogid = 0;
    }
  }

  GLuint vsid = 0;
  GLuint gsid = 0;
  GLuint psid = 0;
  GLuint glprogid = 0;

  void SetProgramVariables();
  void SetProgramBindings(bool is_compute);
  void Bind() const;
  void DestroyShaders();
};

class ProgramShaderCache
{
public:
  struct PCacheEntry
  {
    SHADER shader;
    bool in_cache;
    bool pending;

    void Destroy() { shader.Destroy(); }
  };

  static PCacheEntry GetShaderProgram();
  static SHADER* SetShader(u32 primitive_type, const GLVertexFormat* vertex_format);
  static SHADER* SetUberShader(u32 primitive_type, const GLVertexFormat* vertex_format);
  static void BindVertexFormat(const GLVertexFormat* vertex_format);
  static void InvalidateVertexFormat();
  static void BindLastVertexFormat();

  static bool CompileShader(SHADER& shader, const std::string& vcode, const std::string& pcode,
                            const std::string& gcode = "");
  static bool CompileComputeShader(SHADER& shader, const std::string& code);
  static GLuint CompileSingleShader(GLenum type, const std::string& code);
  static bool CheckShaderCompileResult(GLuint id, GLenum type, const std::string& code);
  static bool CheckProgramLinkResult(GLuint id, const std::string& vcode, const std::string& pcode,
                                     const std::string& gcode);
  static void UploadConstants();

  static void Init();
  static void Reload();
  static void Shutdown();
  static void CreateHeader();
  static void RetrieveAsyncShaders();
  static void PrecompileUberShaders();

private:
  template <typename UIDType>
  class ProgramShaderCacheInserter : public LinearDiskCacheReader<UIDType, u8>
  {
  public:
    ProgramShaderCacheInserter(std::map<UIDType, PCacheEntry>& shader_map)
        : m_shader_map(shader_map)
    {
    }

    void Read(const UIDType& key, const u8* value, u32 value_size) override
    {
      if (m_shader_map.find(key) != m_shader_map.end())
        return;

      PCacheEntry& entry = m_shader_map[key];
      if (!CreateCacheEntryFromBinary(&entry, value, value_size))
      {
        m_shader_map.erase(key);
        return;
      }
    }

  private:
    std::map<UIDType, PCacheEntry>& m_shader_map;
  };

  class SharedContextAsyncShaderCompiler : public VideoCommon::AsyncShaderCompiler
  {
  protected:
    virtual bool WorkerThreadInitMainThread(void** param) override;
    virtual bool WorkerThreadInitWorkerThread(void* param) override;
    virtual void WorkerThreadExit(void* param) override;
  };

  struct SharedContextData
  {
    std::unique_ptr<cInterfaceBase> context;
    GLuint prerender_FBO;
    GLuint prerender_FBO_tex;
    GLuint prerender_FBO_depth;
    GLuint prerender_VBO;
    GLuint prerender_VAO;
    GLuint prerender_IBO;
  };

  class ShaderCompileWorkItem : public VideoCommon::AsyncShaderCompiler::WorkItem
  {
  public:
    ShaderCompileWorkItem(const SHADERUID& uid);

    bool Compile() override;
    void Retrieve() override;

  private:
    SHADERUID m_uid;
    SHADER m_program;
  };

  class UberShaderCompileWorkItem : public VideoCommon::AsyncShaderCompiler::WorkItem
  {
  public:
    UberShaderCompileWorkItem(const UBERSHADERUID& uid);

    bool Compile() override;
    void Retrieve() override;

  private:
    UBERSHADERUID m_uid;
    SHADER m_program;
  };

  typedef std::map<SHADERUID, PCacheEntry> PCache;
  typedef std::map<UBERSHADERUID, PCacheEntry> UberPCache;

  static GLuint CreateProgramFromBinary(const u8* value, u32 value_size);
  static bool CreateCacheEntryFromBinary(PCacheEntry* entry, const u8* value, u32 value_size);
  static void LoadProgramBinaries();
  static void SaveProgramBinaries();
  static void DestroyShaders();
  static void CreatePrerenderArrays(SharedContextData* data);
  static void DestroyPrerenderArrays(SharedContextData* data);
  static void DrawPrerenderArray(const SHADER& shader, u32 primitive_type);

  static PCache pshaders;
  static UberPCache ubershaders;
  static PCacheEntry* last_entry;
  static PCacheEntry* last_uber_entry;
  static SHADERUID last_uid;
  static UBERSHADERUID last_uber_uid;

  static std::unique_ptr<SharedContextAsyncShaderCompiler> s_async_compiler;
  static u32 s_ubo_buffer_size;
  static s32 s_ubo_align;
  static u32 s_last_VAO;
};

}  // namespace OGL
