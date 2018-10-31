// Copyright 2011 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoBackends/OGL/ProgramShaderCache.h"

#include <limits>
#include <memory>
#include <string>

#include "Common/Align.h"
#include "Common/Assert.h"
#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/GL/GLContext.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Common/StringUtil.h"
#include "Common/Timer.h"

#include "Core/ConfigManager.h"
#include "Core/Host.h"

#include "VideoBackends/OGL/OGLShader.h"
#include "VideoBackends/OGL/Render.h"
#include "VideoBackends/OGL/StreamBuffer.h"
#include "VideoBackends/OGL/VertexManager.h"

#include "VideoCommon/AsyncShaderCompiler.h"
#include "VideoCommon/Debugger.h"
#include "VideoCommon/DriverDetails.h"
#include "VideoCommon/GeometryShaderManager.h"
#include "VideoCommon/ImageWrite.h"
#include "VideoCommon/PixelShaderManager.h"
#include "VideoCommon/Statistics.h"
#include "VideoCommon/VertexLoaderManager.h"
#include "VideoCommon/VertexShaderManager.h"
#include "VideoCommon/VideoCommon.h"

namespace OGL
{
static constexpr u32 UBO_LENGTH = 32 * 1024 * 1024;

u32 ProgramShaderCache::s_ubo_buffer_size;
s32 ProgramShaderCache::s_ubo_align;
GLuint ProgramShaderCache::s_attributeless_VBO = 0;
GLuint ProgramShaderCache::s_attributeless_VAO = 0;
GLuint ProgramShaderCache::s_last_VAO = 0;

static std::unique_ptr<StreamBuffer> s_buffer;
static int num_failures = 0;

static GLuint CurrentProgram = 0;
ProgramShaderCache::PipelineProgramMap ProgramShaderCache::s_pipeline_programs;
std::mutex ProgramShaderCache::s_pipeline_program_lock;
static std::string s_glsl_header = "";
static thread_local bool s_is_shared_context = false;

static std::string GetGLSLVersionString()
{
  GlslVersion v = g_ogl_config.eSupportedGLSLVersion;
  switch (v)
  {
  case GlslEs300:
    return "#version 300 es";
  case GlslEs310:
    return "#version 310 es";
  case GlslEs320:
    return "#version 320 es";
  case Glsl130:
    return "#version 130";
  case Glsl140:
    return "#version 140";
  case Glsl150:
    return "#version 150";
  case Glsl330:
    return "#version 330";
  case Glsl400:
    return "#version 400";
  case Glsl430:
    return "#version 430";
  default:
    // Shouldn't ever hit this
    return "#version ERROR";
  }
}

void SHADER::SetProgramVariables()
{
  if (g_ActiveConfig.backend_info.bSupportsBindingLayout)
    return;

  // To set uniform blocks/uniforms, the program must be active. We restore the
  // current binding at the end of this method to maintain the invariant.
  glUseProgram(glprogid);

  // Bind UBO and texture samplers
  GLint PSBlock_id = glGetUniformBlockIndex(glprogid, "PSBlock");
  GLint VSBlock_id = glGetUniformBlockIndex(glprogid, "VSBlock");
  GLint GSBlock_id = glGetUniformBlockIndex(glprogid, "GSBlock");
  GLint UBERBlock_id = glGetUniformBlockIndex(glprogid, "UBERBlock");
  if (PSBlock_id != -1)
    glUniformBlockBinding(glprogid, PSBlock_id, 1);
  if (VSBlock_id != -1)
    glUniformBlockBinding(glprogid, VSBlock_id, 2);
  if (GSBlock_id != -1)
    glUniformBlockBinding(glprogid, GSBlock_id, 3);
  if (UBERBlock_id != -1)
    glUniformBlockBinding(glprogid, UBERBlock_id, 4);

  // Bind Texture Samplers
  for (int a = 0; a < 10; ++a)
  {
    std::string name = StringFromFormat(a < 8 ? "samp[%d]" : "samp%d", a);

    // Still need to get sampler locations since we aren't binding them statically in the shaders
    int loc = glGetUniformLocation(glprogid, name.c_str());
    if (loc != -1)
      glUniform1i(loc, a);
  }

  // Restore previous program binding.
  glUseProgram(CurrentProgram);
}

void SHADER::SetProgramBindings(bool is_compute)
{
  if (!is_compute)
  {
    if (g_ActiveConfig.backend_info.bSupportsDualSourceBlend)
    {
      // So we do support extended blending
      // So we need to set a few more things here.
      // Bind our out locations
      glBindFragDataLocationIndexed(glprogid, 0, 0, "ocol0");
      glBindFragDataLocationIndexed(glprogid, 0, 1, "ocol1");
    }
    // Need to set some attribute locations
    glBindAttribLocation(glprogid, SHADER_POSITION_ATTRIB, "rawpos");

    glBindAttribLocation(glprogid, SHADER_POSMTX_ATTRIB, "posmtx");

    glBindAttribLocation(glprogid, SHADER_COLOR0_ATTRIB, "rawcolor0");
    glBindAttribLocation(glprogid, SHADER_COLOR1_ATTRIB, "rawcolor1");

    glBindAttribLocation(glprogid, SHADER_NORM0_ATTRIB, "rawnorm0");
    glBindAttribLocation(glprogid, SHADER_NORM1_ATTRIB, "rawnorm1");
    glBindAttribLocation(glprogid, SHADER_NORM2_ATTRIB, "rawnorm2");
  }

  for (int i = 0; i < 8; i++)
  {
    std::string attrib_name = StringFromFormat("rawtex%d", i);
    glBindAttribLocation(glprogid, SHADER_TEXTURE0_ATTRIB + i, attrib_name.c_str());
  }
}

void SHADER::Bind() const
{
  if (CurrentProgram != glprogid)
  {
    INCSTAT(stats.thisFrame.numShaderChanges);
    glUseProgram(glprogid);
    CurrentProgram = glprogid;
  }
}

void SHADER::DestroyShaders()
{
  if (vsid)
  {
    glDeleteShader(vsid);
    vsid = 0;
  }
  if (gsid)
  {
    glDeleteShader(gsid);
    gsid = 0;
  }
  if (psid)
  {
    glDeleteShader(psid);
    psid = 0;
  }
}

bool PipelineProgramKey::operator!=(const PipelineProgramKey& rhs) const
{
  return !operator==(rhs);
}

bool PipelineProgramKey::operator==(const PipelineProgramKey& rhs) const
{
  return std::tie(vertex_shader, geometry_shader, pixel_shader) ==
         std::tie(rhs.vertex_shader, rhs.geometry_shader, rhs.pixel_shader);
}

bool PipelineProgramKey::operator<(const PipelineProgramKey& rhs) const
{
  return std::tie(vertex_shader, geometry_shader, pixel_shader) <
         std::tie(rhs.vertex_shader, rhs.geometry_shader, rhs.pixel_shader);
}

std::size_t PipelineProgramKeyHash::operator()(const PipelineProgramKey& key) const
{
  // We would really want std::hash_combine for this..
  std::hash<const void*> hasher;
  return hasher(key.vertex_shader) + hasher(key.geometry_shader) + hasher(key.pixel_shader);
}

StreamBuffer* ProgramShaderCache::GetUniformBuffer()
{
  return s_buffer.get();
}

u32 ProgramShaderCache::GetUniformBufferAlignment()
{
  return s_ubo_align;
}

void ProgramShaderCache::InvalidateConstants()
{
  VertexShaderManager::dirty = true;
  GeometryShaderManager::dirty = true;
  PixelShaderManager::dirty = true;
}

void ProgramShaderCache::UploadConstants()
{
  if (PixelShaderManager::dirty || VertexShaderManager::dirty || GeometryShaderManager::dirty)
  {
    auto buffer = s_buffer->Map(s_ubo_buffer_size, s_ubo_align);

    memcpy(buffer.first, &PixelShaderManager::constants, sizeof(PixelShaderConstants));

    memcpy(buffer.first + Common::AlignUp(sizeof(PixelShaderConstants), s_ubo_align),
           &VertexShaderManager::constants, sizeof(VertexShaderConstants));

    memcpy(buffer.first + Common::AlignUp(sizeof(PixelShaderConstants), s_ubo_align) +
               Common::AlignUp(sizeof(VertexShaderConstants), s_ubo_align),
           &GeometryShaderManager::constants, sizeof(GeometryShaderConstants));

    s_buffer->Unmap(s_ubo_buffer_size);
    glBindBufferRange(GL_UNIFORM_BUFFER, 1, s_buffer->m_buffer, buffer.second,
                      sizeof(PixelShaderConstants));
    glBindBufferRange(GL_UNIFORM_BUFFER, 2, s_buffer->m_buffer,
                      buffer.second + Common::AlignUp(sizeof(PixelShaderConstants), s_ubo_align),
                      sizeof(VertexShaderConstants));
    glBindBufferRange(GL_UNIFORM_BUFFER, 3, s_buffer->m_buffer,
                      buffer.second + Common::AlignUp(sizeof(PixelShaderConstants), s_ubo_align) +
                          Common::AlignUp(sizeof(VertexShaderConstants), s_ubo_align),
                      sizeof(GeometryShaderConstants));

    PixelShaderManager::dirty = false;
    VertexShaderManager::dirty = false;
    GeometryShaderManager::dirty = false;

    ADDSTAT(stats.thisFrame.bytesUniformStreamed, s_ubo_buffer_size);
  }
}

bool ProgramShaderCache::CompileShader(SHADER& shader, const std::string& vcode,
                                       const std::string& pcode, const std::string& gcode)
{
#if defined(_DEBUG) || defined(DEBUGFAST)
  if (g_ActiveConfig.iLog & CONF_SAVESHADERS)
  {
    static int counter = 0;
    std::string filename =
        StringFromFormat("%svs_%04i.txt", File::GetUserPath(D_DUMP_IDX).c_str(), counter++);
    SaveData(filename, vcode.c_str());

    filename = StringFromFormat("%sps_%04i.txt", File::GetUserPath(D_DUMP_IDX).c_str(), counter++);
    SaveData(filename, pcode.c_str());

    if (!gcode.empty())
    {
      filename =
          StringFromFormat("%sgs_%04i.txt", File::GetUserPath(D_DUMP_IDX).c_str(), counter++);
      SaveData(filename, gcode.c_str());
    }
  }
#endif

  shader.vsid = CompileSingleShader(GL_VERTEX_SHADER, vcode);
  shader.psid = CompileSingleShader(GL_FRAGMENT_SHADER, pcode);

  // Optional geometry shader
  shader.gsid = 0;
  if (!gcode.empty())
    shader.gsid = CompileSingleShader(GL_GEOMETRY_SHADER, gcode);

  if (!shader.vsid || !shader.psid || (!gcode.empty() && !shader.gsid))
  {
    shader.Destroy();
    return false;
  }

  // Create and link the program.
  shader.glprogid = glCreateProgram();

  glAttachShader(shader.glprogid, shader.vsid);
  glAttachShader(shader.glprogid, shader.psid);
  if (shader.gsid)
    glAttachShader(shader.glprogid, shader.gsid);

  if (g_ogl_config.bSupportsGLSLCache)
    glProgramParameteri(shader.glprogid, GL_PROGRAM_BINARY_RETRIEVABLE_HINT, GL_TRUE);

  shader.SetProgramBindings(false);

  glLinkProgram(shader.glprogid);

  if (!CheckProgramLinkResult(shader.glprogid, vcode, pcode, gcode))
  {
    // Don't try to use this shader
    shader.Destroy();
    return false;
  }

  // For drivers that don't support binding layout, we need to bind it here.
  shader.SetProgramVariables();

  // Original shaders aren't needed any more.
  shader.DestroyShaders();
  return true;
}

bool ProgramShaderCache::CompileComputeShader(SHADER& shader, const std::string& code)
{
  // We need to enable GL_ARB_compute_shader for drivers that support the extension,
  // but not GLSL 4.3. Mesa is one example.
  std::string header;
  if (g_ActiveConfig.backend_info.bSupportsComputeShaders &&
      g_ogl_config.eSupportedGLSLVersion < Glsl430)
  {
    header = "#extension GL_ARB_compute_shader : enable\n";
  }

  std::string full_code = header + code;
  GLuint shader_id = CompileSingleShader(GL_COMPUTE_SHADER, full_code);
  if (!shader_id)
    return false;

  shader.glprogid = glCreateProgram();
  glAttachShader(shader.glprogid, shader_id);
  shader.SetProgramBindings(true);
  glLinkProgram(shader.glprogid);

  // original shaders aren't needed any more
  glDeleteShader(shader_id);

  if (!CheckProgramLinkResult(shader.glprogid, full_code, "", ""))
  {
    shader.Destroy();
    return false;
  }

  shader.SetProgramVariables();
  return true;
}

GLuint ProgramShaderCache::CompileSingleShader(GLenum type, const std::string& code)
{
  GLuint result = glCreateShader(type);

  const char* src[] = {s_glsl_header.c_str(), code.c_str()};

  glShaderSource(result, 2, src, nullptr);
  glCompileShader(result);

  if (!CheckShaderCompileResult(result, type, code))
  {
    // Don't try to use this shader
    glDeleteShader(result);
    return 0;
  }

  return result;
}

bool ProgramShaderCache::CheckShaderCompileResult(GLuint id, GLenum type, const std::string& code)
{
  GLint compileStatus;
  glGetShaderiv(id, GL_COMPILE_STATUS, &compileStatus);
  GLsizei length = 0;
  glGetShaderiv(id, GL_INFO_LOG_LENGTH, &length);
  if (compileStatus != GL_TRUE || length > 1)
  {
    std::string info_log;
    info_log.resize(length);
    glGetShaderInfoLog(id, length, &length, &info_log[0]);

    const char* prefix = "";
    switch (type)
    {
    case GL_VERTEX_SHADER:
      prefix = "vs";
      break;
    case GL_GEOMETRY_SHADER:
      prefix = "gs";
      break;
    case GL_FRAGMENT_SHADER:
      prefix = "ps";
      break;
    case GL_COMPUTE_SHADER:
      prefix = "cs";
      break;
    }

    if (compileStatus != GL_TRUE)
    {
      ERROR_LOG(VIDEO, "%s failed compilation:\n%s", prefix, info_log.c_str());

      std::string filename = StringFromFormat(
          "%sbad_%s_%04i.txt", File::GetUserPath(D_DUMP_IDX).c_str(), prefix, num_failures++);
      std::ofstream file;
      File::OpenFStream(file, filename, std::ios_base::out);
      file << s_glsl_header << code << info_log;
      file.close();

      PanicAlert("Failed to compile %s shader: %s\n"
                 "Debug info (%s, %s, %s):\n%s",
                 prefix, filename.c_str(), g_ogl_config.gl_vendor, g_ogl_config.gl_renderer,
                 g_ogl_config.gl_version, info_log.c_str());

      return false;
    }

    WARN_LOG(VIDEO, "%s compiled with warnings:\n%s", prefix, info_log.c_str());
  }

  return true;
}

bool ProgramShaderCache::CheckProgramLinkResult(GLuint id, const std::string& vcode,
                                                const std::string& pcode, const std::string& gcode)
{
  GLint linkStatus;
  glGetProgramiv(id, GL_LINK_STATUS, &linkStatus);
  GLsizei length = 0;
  glGetProgramiv(id, GL_INFO_LOG_LENGTH, &length);
  if (linkStatus != GL_TRUE || length > 1)
  {
    std::string info_log;
    info_log.resize(length);
    glGetProgramInfoLog(id, length, &length, &info_log[0]);
    if (linkStatus != GL_TRUE)
    {
      ERROR_LOG(VIDEO, "Program failed linking:\n%s", info_log.c_str());
      std::string filename =
          StringFromFormat("%sbad_p_%d.txt", File::GetUserPath(D_DUMP_IDX).c_str(), num_failures++);
      std::ofstream file;
      File::OpenFStream(file, filename, std::ios_base::out);
      file << s_glsl_header << vcode << s_glsl_header << pcode;
      if (!gcode.empty())
        file << s_glsl_header << gcode;
      file << info_log;
      file.close();

      PanicAlert("Failed to link shaders: %s\n"
                 "Debug info (%s, %s, %s):\n%s",
                 filename.c_str(), g_ogl_config.gl_vendor, g_ogl_config.gl_renderer,
                 g_ogl_config.gl_version, info_log.c_str());

      return false;
    }

    WARN_LOG(VIDEO, "Program linked with warnings:\n%s", info_log.c_str());
  }

  return true;
}

void ProgramShaderCache::Init()
{
  // We have to get the UBO alignment here because
  // if we generate a buffer that isn't aligned
  // then the UBO will fail.
  glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, &s_ubo_align);

  s_ubo_buffer_size =
      static_cast<u32>(Common::AlignUp(sizeof(PixelShaderConstants), s_ubo_align) +
                       Common::AlignUp(sizeof(VertexShaderConstants), s_ubo_align) +
                       Common::AlignUp(sizeof(GeometryShaderConstants), s_ubo_align));

  // We multiply by *4*4 because we need to get down to basic machine units.
  // So multiply by four to get how many floats we have from vec4s
  // Then once more to get bytes
  s_buffer = StreamBuffer::Create(GL_UNIFORM_BUFFER, UBO_LENGTH);

  CreateHeader();
  CreateAttributelessVAO();

  CurrentProgram = 0;
}

void ProgramShaderCache::Shutdown()
{
  s_buffer.reset();

  glBindVertexArray(0);
  glDeleteBuffers(1, &s_attributeless_VBO);
  glDeleteVertexArrays(1, &s_attributeless_VAO);
  s_attributeless_VBO = 0;
  s_attributeless_VAO = 0;
  s_last_VAO = 0;

  // All pipeline programs should have been released.
  DEBUG_ASSERT(s_pipeline_programs.empty());
  s_pipeline_programs.clear();
}

void ProgramShaderCache::CreateAttributelessVAO()
{
  glGenVertexArrays(1, &s_attributeless_VAO);

  // In a compatibility context, we require a valid, bound array buffer.
  glGenBuffers(1, &s_attributeless_VBO);

  // Initialize the buffer with nothing. 16 floats is an arbitrary size that may work around driver
  // issues.
  glBindBuffer(GL_ARRAY_BUFFER, s_attributeless_VBO);
  glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * 16, nullptr, GL_STATIC_DRAW);

  // We must also define vertex attribute 0.
  glBindVertexArray(s_attributeless_VAO);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
  glEnableVertexAttribArray(0);
}

void ProgramShaderCache::BindVertexFormat(const GLVertexFormat* vertex_format)
{
  u32 new_VAO = vertex_format ? vertex_format->VAO : s_attributeless_VAO;
  if (s_last_VAO == new_VAO)
    return;

  glBindVertexArray(new_VAO);
  s_last_VAO = new_VAO;
}

void ProgramShaderCache::InvalidateVertexFormat()
{
  s_last_VAO = 0;
}

void ProgramShaderCache::InvalidateLastProgram()
{
  CurrentProgram = 0;
}

const PipelineProgram* ProgramShaderCache::GetPipelineProgram(const GLVertexFormat* vertex_format,
                                                              const OGLShader* vertex_shader,
                                                              const OGLShader* geometry_shader,
                                                              const OGLShader* pixel_shader)
{
  PipelineProgramKey key = {vertex_shader, geometry_shader, pixel_shader};
  {
    std::lock_guard<std::mutex> guard(s_pipeline_program_lock);
    auto iter = s_pipeline_programs.find(key);
    if (iter != s_pipeline_programs.end())
    {
      iter->second->reference_count++;
      return iter->second.get();
    }
  }

  // We temporarily change the vertex array to the pipeline's vertex format.
  // This can prevent the NVIDIA OpenGL driver from recompiling on first use.
  GLuint vao = vertex_format ? vertex_format->VAO : s_attributeless_VAO;
  if (s_is_shared_context || vao != s_last_VAO)
    glBindVertexArray(vao);

  std::unique_ptr<PipelineProgram> prog = std::make_unique<PipelineProgram>();
  prog->key = key;

  // Attach shaders.
  ASSERT(vertex_shader && vertex_shader->GetStage() == ShaderStage::Vertex);
  ASSERT(pixel_shader && pixel_shader->GetStage() == ShaderStage::Pixel);
  prog->shader.glprogid = glCreateProgram();
  glAttachShader(prog->shader.glprogid, vertex_shader->GetGLShaderID());
  glAttachShader(prog->shader.glprogid, pixel_shader->GetGLShaderID());
  if (geometry_shader)
  {
    ASSERT(geometry_shader->GetStage() == ShaderStage::Geometry);
    glAttachShader(prog->shader.glprogid, geometry_shader->GetGLShaderID());
  }

  // Link program.
  prog->shader.SetProgramBindings(false);
  glLinkProgram(prog->shader.glprogid);

  // Restore VAO binding after linking.
  if (!s_is_shared_context && vao != s_last_VAO)
    glBindVertexArray(s_last_VAO);

  if (!ProgramShaderCache::CheckProgramLinkResult(prog->shader.glprogid, {}, {}, {}))
  {
    prog->shader.Destroy();
    return nullptr;
  }

  // Lock to insert. A duplicate program may have been created in the meantime.
  std::lock_guard<std::mutex> guard(s_pipeline_program_lock);
  auto iter = s_pipeline_programs.find(key);
  if (iter != s_pipeline_programs.end())
  {
    // Destroy this program, and use the one which was created first.
    prog->shader.Destroy();
    iter->second->reference_count++;
    return iter->second.get();
  }

  // Set program variables on the shader which will be returned.
  // This is only needed for drivers which don't support binding layout.
  prog->shader.SetProgramVariables();

  // If this is a shared context, ensure we sync before we return the program to
  // the main thread. If we don't do this, some driver can lock up (e.g. AMD).
  if (s_is_shared_context)
    glFinish();

  auto ip = s_pipeline_programs.emplace(key, std::move(prog));
  return ip.first->second.get();
}

void ProgramShaderCache::ReleasePipelineProgram(const PipelineProgram* prog)
{
  auto iter = s_pipeline_programs.find(prog->key);
  ASSERT(iter != s_pipeline_programs.end() && prog == iter->second.get());

  if (--iter->second->reference_count == 0)
  {
    iter->second->shader.Destroy();
    s_pipeline_programs.erase(iter);
  }
}

void ProgramShaderCache::CreateHeader()
{
  GlslVersion v = g_ogl_config.eSupportedGLSLVersion;
  bool is_glsles = v >= GlslEs300;
  std::string SupportedESPointSize;
  std::string SupportedESTextureBuffer;
  switch (g_ogl_config.SupportedESPointSize)
  {
  case 1:
    SupportedESPointSize = "#extension GL_OES_geometry_point_size : enable";
    break;
  case 2:
    SupportedESPointSize = "#extension GL_EXT_geometry_point_size : enable";
    break;
  default:
    SupportedESPointSize = "";
    break;
  }

  switch (g_ogl_config.SupportedESTextureBuffer)
  {
  case EsTexbufType::TexbufExt:
    SupportedESTextureBuffer = "#extension GL_EXT_texture_buffer : enable";
    break;
  case EsTexbufType::TexbufOes:
    SupportedESTextureBuffer = "#extension GL_OES_texture_buffer : enable";
    break;
  case EsTexbufType::TexbufCore:
  case EsTexbufType::TexbufNone:
    SupportedESTextureBuffer = "";
    break;
  }

  std::string earlyz_string = "";
  if (g_ActiveConfig.backend_info.bSupportsEarlyZ)
  {
    if (g_ogl_config.bSupportsImageLoadStore)
    {
      earlyz_string = "#define FORCE_EARLY_Z layout(early_fragment_tests) in\n";
    }
    else if (g_ogl_config.bSupportsConservativeDepth)
    {
      // See PixelShaderGen for details about this fallback.
      earlyz_string = "#define FORCE_EARLY_Z layout(depth_unchanged) out float gl_FragDepth\n";
      earlyz_string += "#extension GL_ARB_conservative_depth : enable\n";
    }
  }

  std::string framebuffer_fetch_string;
  switch (g_ogl_config.SupportedFramebufferFetch)
  {
  case EsFbFetchType::FbFetchExt:
    framebuffer_fetch_string = "#extension GL_EXT_shader_framebuffer_fetch: enable\n"
                               "#define FB_FETCH_VALUE real_ocol0\n"
                               "#define FRAGMENT_INOUT inout";
    break;
  case EsFbFetchType::FbFetchArm:
    framebuffer_fetch_string = "#extension GL_ARM_shader_framebuffer_fetch: enable\n"
                               "#define FB_FETCH_VALUE gl_LastFragColorARM\n"
                               "#define FRAGMENT_INOUT out";
    break;
  case EsFbFetchType::FbFetchNone:
    framebuffer_fetch_string = "";
    break;
  }

  s_glsl_header = StringFromFormat(
      "%s\n"
      "%s\n"  // ubo
      "%s\n"  // early-z
      "%s\n"  // 420pack
      "%s\n"  // msaa
      "%s\n"  // Input/output/sampler binding
      "%s\n"  // Varying location
      "%s\n"  // storage buffer
      "%s\n"  // shader5
      "%s\n"  // SSAA
      "%s\n"  // Geometry point size
      "%s\n"  // AEP
      "%s\n"  // texture buffer
      "%s\n"  // ES texture buffer
      "%s\n"  // ES dual source blend
      "%s\n"  // shader image load store
      "%s\n"  // shader framebuffer fetch

      // Precision defines for GLSL ES
      "%s\n"
      "%s\n"
      "%s\n"
      "%s\n"
      "%s\n"
      "%s\n"

      // Silly differences
      "#define float2 vec2\n"
      "#define float3 vec3\n"
      "#define float4 vec4\n"
      "#define uint2 uvec2\n"
      "#define uint3 uvec3\n"
      "#define uint4 uvec4\n"
      "#define int2 ivec2\n"
      "#define int3 ivec3\n"
      "#define int4 ivec4\n"

      // hlsl to glsl function translation
      "#define frac fract\n"
      "#define lerp mix\n"

      ,
      GetGLSLVersionString().c_str(),
      v < Glsl140 ? "#extension GL_ARB_uniform_buffer_object : enable" : "", earlyz_string.c_str(),
      (g_ActiveConfig.backend_info.bSupportsBindingLayout && v < GlslEs310) ?
          "#extension GL_ARB_shading_language_420pack : enable" :
          "",
      (g_ogl_config.bSupportsMSAA && v < Glsl150) ?
          "#extension GL_ARB_texture_multisample : enable" :
          "",
      // Attribute and fragment output bindings are still done via glBindAttribLocation and
      // glBindFragDataLocation. In the future this could be moved to the layout qualifier
      // in GLSL, but requires verification of GL_ARB_explicit_attrib_location.
      g_ActiveConfig.backend_info.bSupportsBindingLayout ?
          "#define ATTRIBUTE_LOCATION(x)\n"
          "#define FRAGMENT_OUTPUT_LOCATION(x)\n"
          "#define FRAGMENT_OUTPUT_LOCATION_INDEXED(x, y)\n"
          "#define UBO_BINDING(packing, x) layout(packing, binding = x)\n"
          "#define SAMPLER_BINDING(x) layout(binding = x)\n"
          "#define SSBO_BINDING(x) layout(binding = x)\n" :
          "#define ATTRIBUTE_LOCATION(x)\n"
          "#define FRAGMENT_OUTPUT_LOCATION(x)\n"
          "#define FRAGMENT_OUTPUT_LOCATION_INDEXED(x, y)\n"
          "#define UBO_BINDING(packing, x) layout(packing)\n"
          "#define SAMPLER_BINDING(x)\n",
      // Input/output blocks are matched by name during program linking
      "#define VARYING_LOCATION(x)\n",
      !is_glsles && g_ActiveConfig.backend_info.bSupportsFragmentStoresAndAtomics ?
          "#extension GL_ARB_shader_storage_buffer_object : enable" :
          "",
      v < Glsl400 && g_ActiveConfig.backend_info.bSupportsGSInstancing ?
          "#extension GL_ARB_gpu_shader5 : enable" :
          "",
      v < Glsl400 && g_ActiveConfig.backend_info.bSupportsSSAA ?
          "#extension GL_ARB_sample_shading : enable" :
          "",
      SupportedESPointSize.c_str(),
      g_ogl_config.bSupportsAEP ? "#extension GL_ANDROID_extension_pack_es31a : enable" : "",
      v < Glsl140 && g_ActiveConfig.backend_info.bSupportsPaletteConversion ?
          "#extension GL_ARB_texture_buffer_object : enable" :
          "",
      SupportedESTextureBuffer.c_str(),
      is_glsles && g_ActiveConfig.backend_info.bSupportsDualSourceBlend ?
          "#extension GL_EXT_blend_func_extended : enable" :
          ""

      ,
      g_ogl_config.bSupportsImageLoadStore &&
              ((!is_glsles && v < Glsl430) || (is_glsles && v < GlslEs310)) ?
          "#extension GL_ARB_shader_image_load_store : enable" :
          "",
      framebuffer_fetch_string.c_str(), is_glsles ? "precision highp float;" : "",
      is_glsles ? "precision highp int;" : "", is_glsles ? "precision highp sampler2DArray;" : "",
      (is_glsles && g_ActiveConfig.backend_info.bSupportsPaletteConversion) ?
          "precision highp usamplerBuffer;" :
          "",
      v > GlslEs300 ? "precision highp sampler2DMS;" : "",
      v >= GlslEs310 ? "precision highp image2DArray;" : "");
}

bool SharedContextAsyncShaderCompiler::WorkerThreadInitMainThread(void** param)
{
  std::unique_ptr<GLContext> context =
      static_cast<Renderer*>(g_renderer.get())->GetMainGLContext()->CreateSharedContext();
  if (!context)
  {
    PanicAlert("Failed to create shared context for shader compiling.");
    return false;
  }

  *param = context.release();
  return true;
}

bool SharedContextAsyncShaderCompiler::WorkerThreadInitWorkerThread(void* param)
{
  GLContext* context = static_cast<GLContext*>(param);
  if (!context->MakeCurrent())
    return false;

  s_is_shared_context = true;
  if (g_ActiveConfig.backend_info.bSupportsPrimitiveRestart)
    GLUtil::EnablePrimitiveRestart(context);

  return true;
}

void SharedContextAsyncShaderCompiler::WorkerThreadExit(void* param)
{
  GLContext* context = static_cast<GLContext*>(param);
  context->ClearCurrent();
  delete context;
}
}  // namespace OGL
