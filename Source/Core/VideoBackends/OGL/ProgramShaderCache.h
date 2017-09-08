// Copyright 2011 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <atomic>
#include <memory>
#include <tuple>
#include <unordered_map>

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
class OGLShader;
class GLVertexFormat;
class StreamBuffer;

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

struct PipelineProgramKey
{
  const OGLShader* vertex_shader;
  const OGLShader* geometry_shader;
  const OGLShader* pixel_shader;

  bool operator==(const PipelineProgramKey& rhs) const;
  bool operator!=(const PipelineProgramKey& rhs) const;
  bool operator<(const PipelineProgramKey& rhs) const;
};

struct PipelineProgramKeyHash
{
  std::size_t operator()(const PipelineProgramKey& key) const;
};

struct PipelineProgram
{
  PipelineProgramKey key;
  SHADER shader;
  std::atomic_size_t reference_count{1};
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
  static SHADER* SetShader(PrimitiveType primitive_type, const GLVertexFormat* vertex_format);
  static SHADER* SetUberShader(PrimitiveType primitive_type, const GLVertexFormat* vertex_format);
  static void BindVertexFormat(const GLVertexFormat* vertex_format);
  static void InvalidateVertexFormat();
  static void InvalidateLastProgram();

  static bool CompileShader(SHADER& shader, const std::string& vcode, const std::string& pcode,
                            const std::string& gcode = "");
  static bool CompileComputeShader(SHADER& shader, const std::string& code);
  static GLuint CompileSingleShader(GLenum type, const std::string& code);
  static bool CheckShaderCompileResult(GLuint id, GLenum type, const std::string& code);
  static bool CheckProgramLinkResult(GLuint id, const std::string& vcode, const std::string& pcode,
                                     const std::string& gcode);
  static StreamBuffer* GetUniformBuffer();
  static u32 GetUniformBufferAlignment();
  static void InvalidateConstants();
  static void UploadConstants();

  static void Init();
  static void Reload();
  static void Shutdown();
  static void CreateHeader();
  static void RetrieveAsyncShaders();
  static void PrecompileUberShaders();

  static const PipelineProgram* GetPipelineProgram(const OGLShader* vertex_shader,
                                                   const OGLShader* geometry_shader,
                                                   const OGLShader* pixel_shader);
  static void ReleasePipelineProgram(const PipelineProgram* prog);

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
    bool WorkerThreadInitMainThread(void** param) override;
    bool WorkerThreadInitWorkerThread(void* param) override;
    void WorkerThreadExit(void* param) override;
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
    explicit ShaderCompileWorkItem(const SHADERUID& uid);

    bool Compile() override;
    void Retrieve() override;

  private:
    SHADERUID m_uid;
    SHADER m_program;
  };

  class UberShaderCompileWorkItem : public VideoCommon::AsyncShaderCompiler::WorkItem
  {
  public:
    explicit UberShaderCompileWorkItem(const UBERSHADERUID& uid);

    bool Compile() override;
    void Retrieve() override;

  private:
    UBERSHADERUID m_uid;
    SHADER m_program;
  };

  typedef std::map<SHADERUID, PCacheEntry> PCache;
  typedef std::map<UBERSHADERUID, PCacheEntry> UberPCache;
  typedef std::unordered_map<PipelineProgramKey, std::unique_ptr<PipelineProgram>,
                             PipelineProgramKeyHash>
      PipelineProgramMap;

  static void CreateAttributelessVAO();
  static GLuint CreateProgramFromBinary(const u8* value, u32 value_size);
  static bool CreateCacheEntryFromBinary(PCacheEntry* entry, const u8* value, u32 value_size);
  static void LoadProgramBinaries();
  static void SaveProgramBinaries();
  static void DestroyShaders();
  static void CreatePrerenderArrays(SharedContextData* data);
  static void DestroyPrerenderArrays(SharedContextData* data);
  static void DrawPrerenderArray(const SHADER& shader, PrimitiveType primitive_type);

  static PCache pshaders;
  static UberPCache ubershaders;
  static PipelineProgramMap pipelineprograms;
  static PCacheEntry* last_entry;
  static PCacheEntry* last_uber_entry;
  static SHADERUID last_uid;
  static UBERSHADERUID last_uber_uid;

  static std::unique_ptr<SharedContextAsyncShaderCompiler> s_async_compiler;
  static u32 s_ubo_buffer_size;
  static s32 s_ubo_align;

  static GLuint s_attributeless_VBO;
  static GLuint s_attributeless_VAO;
  static GLuint s_last_VAO;
};

}  // namespace OGL
