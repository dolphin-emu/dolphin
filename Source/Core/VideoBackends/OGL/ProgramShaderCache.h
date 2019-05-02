// Copyright 2011 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <tuple>
#include <unordered_map>

#include "Common/GL/GLUtil.h"
#include "VideoCommon/AsyncShaderCompiler.h"

namespace OGL
{
class OGLShader;
class GLVertexFormat;
class StreamBuffer;

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
  u64 vertex_shader_id;
  u64 geometry_shader_id;
  u64 pixel_shader_id;

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
  bool binary_retrieved = false;
};

class ProgramShaderCache
{
public:
  static void BindVertexFormat(const GLVertexFormat* vertex_format);
  static bool IsValidVertexFormatBound();
  static void InvalidateVertexFormat();
  static void InvalidateVertexFormatIfBound(GLuint vao);
  static void InvalidateLastProgram();

  static bool CompileComputeShader(SHADER& shader, const std::string& code);
  static GLuint CompileSingleShader(GLenum type, const std::string& code);
  static bool CheckShaderCompileResult(GLuint id, GLenum type, const std::string& code);
  static bool CheckProgramLinkResult(GLuint id, const std::string* vcode, const std::string* pcode,
                                     const std::string* gcode);
  static StreamBuffer* GetUniformBuffer();
  static u32 GetUniformBufferAlignment();
  static void UploadConstants();
  static void UploadConstants(const void* data, u32 data_size);

  static void Init();
  static void Shutdown();
  static void CreateHeader();

  // This counter increments with each shader object allocated, in order to give it a unique ID.
  // Since the shaders can be destroyed after a pipeline is created, we can't use the shader pointer
  // as a key for GL programs. For the same reason, we can't use the GL objects either. This ID is
  // guaranteed to be unique for the emulation session, even if the memory allocator or GL driver
  // re-uses pointers, therefore we won't have any collisions where the shaders attached to a
  // pipeline do not match the pipeline configuration.
  static u64 GenerateShaderID();

  static PipelineProgram* GetPipelineProgram(const GLVertexFormat* vertex_format,
                                             const OGLShader* vertex_shader,
                                             const OGLShader* geometry_shader,
                                             const OGLShader* pixel_shader, const void* cache_data,
                                             size_t cache_data_size);
  static void ReleasePipelineProgram(PipelineProgram* prog);

private:
  typedef std::unordered_map<PipelineProgramKey, std::unique_ptr<PipelineProgram>,
                             PipelineProgramKeyHash>
      PipelineProgramMap;

  static void CreateAttributelessVAO();

  static PipelineProgramMap s_pipeline_programs;
  static std::mutex s_pipeline_program_lock;

  static u32 s_ubo_buffer_size;
  static s32 s_ubo_align;

  static GLuint s_attributeless_VBO;
  static GLuint s_attributeless_VAO;
  static GLuint s_last_VAO;
};

class SharedContextAsyncShaderCompiler : public VideoCommon::AsyncShaderCompiler
{
protected:
  bool WorkerThreadInitMainThread(void** param) override;
  bool WorkerThreadInitWorkerThread(void* param) override;
  void WorkerThreadExit(void* param) override;
};

}  // namespace OGL
