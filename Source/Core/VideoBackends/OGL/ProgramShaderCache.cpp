// Copyright 2011 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoBackends/OGL/ProgramShaderCache.h"

#include <limits>
#include <memory>
#include <string>

#include "Common/Align.h"
#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/GL/GLInterfaceBase.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Common/StringUtil.h"
#include "Common/Timer.h"

#include "Core/ConfigManager.h"
#include "Core/Host.h"

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
#include "VideoCommon/UberShaderPixel.h"
#include "VideoCommon/UberShaderVertex.h"
#include "VideoCommon/VertexLoaderManager.h"
#include "VideoCommon/VertexShaderManager.h"
#include "VideoCommon/VideoCommon.h"

namespace OGL
{
static constexpr u32 UBO_LENGTH = 32 * 1024 * 1024;
static constexpr u32 INVALID_VAO = std::numeric_limits<u32>::max();

std::unique_ptr<ProgramShaderCache::SharedContextAsyncShaderCompiler>
    ProgramShaderCache::s_async_compiler;
u32 ProgramShaderCache::s_ubo_buffer_size;
s32 ProgramShaderCache::s_ubo_align;
u32 ProgramShaderCache::s_last_VAO = INVALID_VAO;

static std::unique_ptr<StreamBuffer> s_buffer;
static int num_failures = 0;

static LinearDiskCache<SHADERUID, u8> s_program_disk_cache;
static LinearDiskCache<UBERSHADERUID, u8> s_uber_program_disk_cache;
static GLuint CurrentProgram = 0;
ProgramShaderCache::PCache ProgramShaderCache::pshaders;
ProgramShaderCache::UberPCache ProgramShaderCache::ubershaders;
ProgramShaderCache::PCacheEntry* ProgramShaderCache::last_entry;
ProgramShaderCache::PCacheEntry* ProgramShaderCache::last_uber_entry;
SHADERUID ProgramShaderCache::last_uid;
UBERSHADERUID ProgramShaderCache::last_uber_uid;
static std::string s_glsl_header = "";

static std::string GetGLSLVersionString()
{
  GLSL_VERSION v = g_ogl_config.eSupportedGLSLVersion;
  switch (v)
  {
  case GLSLES_300:
    return "#version 300 es";
  case GLSLES_310:
    return "#version 310 es";
  case GLSLES_320:
    return "#version 320 es";
  case GLSL_130:
    return "#version 130";
  case GLSL_140:
    return "#version 140";
  case GLSL_150:
    return "#version 150";
  case GLSL_330:
    return "#version 330";
  case GLSL_400:
    return "#version 400";
  case GLSL_430:
    return "#version 430";
  default:
    // Shouldn't ever hit this
    return "#version ERROR";
  }
}

void SHADER::SetProgramVariables()
{
  // Bind UBO and texture samplers
  if (!g_ActiveConfig.backend_info.bSupportsBindingLayout)
  {
    // glsl shader must be bind to set samplers if we don't support binding layout
    Bind();

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
    for (int a = 0; a <= 9; ++a)
    {
      std::string name = StringFromFormat(a < 8 ? "samp[%d]" : "samp%d", a);

      // Still need to get sampler locations since we aren't binding them statically in the shaders
      int loc = glGetUniformLocation(glprogid, name.c_str());
      if (loc != -1)
        glUniform1i(loc, a);
    }
  }
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

SHADER* ProgramShaderCache::SetShader(u32 primitive_type, const GLVertexFormat* vertex_format)
{
  if (g_ActiveConfig.bDisableSpecializedShaders)
    return SetUberShader(primitive_type, vertex_format);

  SHADERUID uid;
  std::memset(&uid, 0, sizeof(uid));
  uid.puid = GetPixelShaderUid();
  uid.vuid = GetVertexShaderUid();
  uid.guid = GetGeometryShaderUid(primitive_type);

  // Check if the shader is already set
  if (last_entry && uid == last_uid)
  {
    last_entry->shader.Bind();
    BindVertexFormat(vertex_format);
    return &last_entry->shader;
  }

  // Check if shader is already in cache
  auto iter = pshaders.find(uid);
  if (iter != pshaders.end())
  {
    PCacheEntry* entry = &iter->second;
    if (entry->pending)
      return SetUberShader(primitive_type, vertex_format);

    last_uid = uid;
    last_entry = entry;
    BindVertexFormat(vertex_format);
    last_entry->shader.Bind();
    return &last_entry->shader;
  }

  // Compile the new shader program.
  PCacheEntry& newentry = pshaders[uid];
  newentry.in_cache = false;
  newentry.pending = false;

  // Can we background compile this shader? Requires background shader compiling to be enabled,
  // and all ubershaders to have been successfully compiled.
  if (g_ActiveConfig.CanBackgroundCompileShaders() && !ubershaders.empty() && s_async_compiler)
  {
    newentry.pending = true;
    s_async_compiler->QueueWorkItem(s_async_compiler->CreateWorkItem<ShaderCompileWorkItem>(uid));
    return SetUberShader(primitive_type, vertex_format);
  }

  // Synchronous shader compiling.
  ShaderHostConfig host_config = ShaderHostConfig::GetCurrent();
  ShaderCode vcode = GenerateVertexShaderCode(APIType::OpenGL, host_config, uid.vuid.GetUidData());
  ShaderCode pcode = GeneratePixelShaderCode(APIType::OpenGL, host_config, uid.puid.GetUidData());
  ShaderCode gcode;
  if (g_ActiveConfig.backend_info.bSupportsGeometryShaders &&
      !uid.guid.GetUidData()->IsPassthrough())
    gcode = GenerateGeometryShaderCode(APIType::OpenGL, host_config, uid.guid.GetUidData());

  if (!CompileShader(newentry.shader, vcode.GetBuffer(), pcode.GetBuffer(), gcode.GetBuffer()))
    return nullptr;

  INCSTAT(stats.numPixelShadersCreated);
  SETSTAT(stats.numPixelShadersAlive, pshaders.size());

  last_uid = uid;
  last_entry = &newentry;
  BindVertexFormat(vertex_format);
  last_entry->shader.Bind();
  return &last_entry->shader;
}

SHADER* ProgramShaderCache::SetUberShader(u32 primitive_type, const GLVertexFormat* vertex_format)
{
  UBERSHADERUID uid;
  std::memset(&uid, 0, sizeof(uid));
  uid.puid = UberShader::GetPixelShaderUid();
  uid.vuid = UberShader::GetVertexShaderUid();
  uid.guid = GetGeometryShaderUid(primitive_type);

  // We need to use the ubershader vertex format with all attributes enabled.
  // Otherwise, the NV driver can generate variants for the vertex shaders.
  const GLVertexFormat* uber_vertex_format = static_cast<const GLVertexFormat*>(
      VertexLoaderManager::GetUberVertexFormat(vertex_format->GetVertexDeclaration()));

  // Check if the shader is already set
  if (last_uber_entry && last_uber_uid == uid)
  {
    BindVertexFormat(uber_vertex_format);
    last_uber_entry->shader.Bind();
    return &last_uber_entry->shader;
  }

  // Check if shader is already in cache
  auto iter = ubershaders.find(uid);
  if (iter != ubershaders.end())
  {
    PCacheEntry* entry = &iter->second;
    last_uber_uid = uid;
    last_uber_entry = entry;
    BindVertexFormat(uber_vertex_format);
    last_uber_entry->shader.Bind();
    return &last_uber_entry->shader;
  }

  // Make an entry in the table
  PCacheEntry& newentry = ubershaders[uid];
  newentry.in_cache = false;
  newentry.pending = false;

  ShaderHostConfig host_config = ShaderHostConfig::GetCurrent();
  ShaderCode vcode =
      UberShader::GenVertexShader(APIType::OpenGL, host_config, uid.vuid.GetUidData());
  ShaderCode pcode =
      UberShader::GenPixelShader(APIType::OpenGL, host_config, uid.puid.GetUidData());
  ShaderCode gcode;
  if (g_ActiveConfig.backend_info.bSupportsGeometryShaders &&
      !uid.guid.GetUidData()->IsPassthrough())
  {
    gcode = GenerateGeometryShaderCode(APIType::OpenGL, host_config, uid.guid.GetUidData());
  }

  if (!CompileShader(newentry.shader, vcode.GetBuffer(), pcode.GetBuffer(), gcode.GetBuffer()))
  {
    GFX_DEBUGGER_PAUSE_AT(NEXT_ERROR, true);
    return nullptr;
  }

  last_uber_uid = uid;
  last_uber_entry = &newentry;
  BindVertexFormat(uber_vertex_format);
  last_uber_entry->shader.Bind();
  return &last_uber_entry->shader;
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
      g_ogl_config.eSupportedGLSLVersion < GLSL_430)
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

  if (compileStatus != GL_TRUE || (length > 1 && DEBUG_GLSL))
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

    ERROR_LOG(VIDEO, "%s Shader info log:\n%s", prefix, info_log.c_str());

    std::string filename = StringFromFormat(
        "%sbad_%s_%04i.txt", File::GetUserPath(D_DUMP_IDX).c_str(), prefix, num_failures++);
    std::ofstream file;
    File::OpenFStream(file, filename, std::ios_base::out);
    file << s_glsl_header << code << info_log;
    file.close();

    if (compileStatus != GL_TRUE)
    {
      PanicAlert("Failed to compile %s shader: %s\n"
                 "Debug info (%s, %s, %s):\n%s",
                 prefix, filename.c_str(), g_ogl_config.gl_vendor, g_ogl_config.gl_renderer,
                 g_ogl_config.gl_version, info_log.c_str());
    }
  }
  if (compileStatus != GL_TRUE)
  {
    // Compile failed
    ERROR_LOG(VIDEO, "Shader compilation failed; see info log");
    return false;
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
  if (linkStatus != GL_TRUE || (length > 1 && DEBUG_GLSL))
  {
    std::string info_log;
    info_log.resize(length);
    glGetProgramInfoLog(id, length, &length, &info_log[0]);
    ERROR_LOG(VIDEO, "Program info log:\n%s", info_log.c_str());

    std::string filename =
        StringFromFormat("%sbad_p_%d.txt", File::GetUserPath(D_DUMP_IDX).c_str(), num_failures++);
    std::ofstream file;
    File::OpenFStream(file, filename, std::ios_base::out);
    file << s_glsl_header << vcode << s_glsl_header << pcode;
    if (!gcode.empty())
      file << s_glsl_header << gcode;
    file << info_log;
    file.close();

    if (linkStatus != GL_TRUE)
    {
      PanicAlert("Failed to link shaders: %s\n"
                 "Debug info (%s, %s, %s):\n%s",
                 filename.c_str(), g_ogl_config.gl_vendor, g_ogl_config.gl_renderer,
                 g_ogl_config.gl_version, info_log.c_str());

      return false;
    }
  }

  return true;
}

ProgramShaderCache::PCacheEntry ProgramShaderCache::GetShaderProgram()
{
  return *last_entry;
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

  // The GPU shader code appears to be context-specific on Mesa/i965.
  // This means that if we compiled the ubershaders asynchronously, they will be recompiled
  // on the main thread the first time they are used, causing stutter. Nouveau has been
  // reported to crash if draw calls are invoked on the shared context threads. For now,
  // disable asynchronous compilation on Mesa.
  if (!DriverDetails::HasBug(DriverDetails::BUG_SHARED_CONTEXT_SHADER_COMPILATION))
    s_async_compiler = std::make_unique<SharedContextAsyncShaderCompiler>();

  // Read our shader cache, only if supported and enabled
  if (g_ogl_config.bSupportsGLSLCache && g_ActiveConfig.bShaderCache)
    LoadProgramBinaries();

  CreateHeader();

  CurrentProgram = 0;
  last_entry = nullptr;
  last_uber_entry = nullptr;

  if (g_ActiveConfig.CanPrecompileUberShaders())
  {
    if (s_async_compiler)
      s_async_compiler->ResizeWorkerThreads(g_ActiveConfig.GetShaderPrecompilerThreads());
    PrecompileUberShaders();
  }

  if (s_async_compiler)
  {
    // No point using the async compiler without workers.
    s_async_compiler->ResizeWorkerThreads(g_ActiveConfig.GetShaderCompilerThreads());
    if (!s_async_compiler->HasWorkerThreads())
      s_async_compiler.reset();
  }
}

void ProgramShaderCache::RetrieveAsyncShaders()
{
  if (s_async_compiler)
    s_async_compiler->RetrieveWorkItems();
}

void ProgramShaderCache::Reload()
{
  if (s_async_compiler)
  {
    s_async_compiler->WaitUntilCompletion();
    s_async_compiler->RetrieveWorkItems();
  }

  const bool use_cache = g_ogl_config.bSupportsGLSLCache && g_ActiveConfig.bShaderCache;
  if (use_cache)
    SaveProgramBinaries();

  s_program_disk_cache.Close();
  s_uber_program_disk_cache.Close();
  DestroyShaders();

  if (use_cache)
    LoadProgramBinaries();

  if (g_ActiveConfig.CanPrecompileUberShaders())
    PrecompileUberShaders();

  InvalidateVertexFormat();
  CurrentProgram = 0;
  last_entry = nullptr;
  last_uber_entry = nullptr;
  last_uid = {};
  last_uber_uid = {};
}

void ProgramShaderCache::Shutdown()
{
  if (s_async_compiler)
  {
    s_async_compiler->WaitUntilCompletion();
    s_async_compiler->StopWorkerThreads();
    s_async_compiler->RetrieveWorkItems();
    s_async_compiler.reset();
  }

  // store all shaders in cache on disk
  if (g_ogl_config.bSupportsGLSLCache && g_ActiveConfig.bShaderCache)
    SaveProgramBinaries();
  s_program_disk_cache.Close();
  s_uber_program_disk_cache.Close();

  InvalidateVertexFormat();
  DestroyShaders();
  s_buffer.reset();
}

void ProgramShaderCache::BindVertexFormat(const GLVertexFormat* vertex_format)
{
  u32 new_VAO = vertex_format ? vertex_format->VAO : 0;
  if (s_last_VAO == new_VAO)
    return;

  glBindVertexArray(new_VAO);
  s_last_VAO = new_VAO;
}

void ProgramShaderCache::InvalidateVertexFormat()
{
  s_last_VAO = INVALID_VAO;
}

void ProgramShaderCache::BindLastVertexFormat()
{
  if (s_last_VAO != INVALID_VAO)
    glBindVertexArray(s_last_VAO);
  else
    glBindVertexArray(0);
}

GLuint ProgramShaderCache::CreateProgramFromBinary(const u8* value, u32 value_size)
{
  const u8* binary = value + sizeof(GLenum);
  GLint binary_size = value_size - sizeof(GLenum);
  GLenum prog_format;
  std::memcpy(&prog_format, value, sizeof(GLenum));

  GLuint progid = glCreateProgram();
  glProgramBinary(progid, prog_format, binary, binary_size);

  GLint success;
  glGetProgramiv(progid, GL_LINK_STATUS, &success);
  if (!success)
  {
    glDeleteProgram(progid);
    return 0;
  }

  return progid;
}

bool ProgramShaderCache::CreateCacheEntryFromBinary(PCacheEntry* entry, const u8* value,
                                                    u32 value_size)
{
  entry->in_cache = true;
  entry->pending = false;
  entry->shader.glprogid = CreateProgramFromBinary(value, value_size);
  if (entry->shader.glprogid == 0)
    return false;

  entry->shader.SetProgramVariables();
  return true;
}

void ProgramShaderCache::LoadProgramBinaries()
{
  GLint Supported;
  glGetIntegerv(GL_NUM_PROGRAM_BINARY_FORMATS, &Supported);
  if (!Supported)
  {
    ERROR_LOG(VIDEO, "GL_ARB_get_program_binary is supported, but no binary format is known. So "
                     "disable shader cache.");
    g_ogl_config.bSupportsGLSLCache = false;
  }
  else
  {
    // Load game-specific shaders.
    std::string cache_filename =
        GetDiskShaderCacheFileName(APIType::OpenGL, "ProgramBinaries", true, true);
    ProgramShaderCacheInserter<SHADERUID> inserter(pshaders);
    s_program_disk_cache.OpenAndRead(cache_filename, inserter);

    // Load global ubershaders.
    cache_filename =
        GetDiskShaderCacheFileName(APIType::OpenGL, "UberProgramBinaries", false, true);
    ProgramShaderCacheInserter<UBERSHADERUID> uber_inserter(ubershaders);
    s_uber_program_disk_cache.OpenAndRead(cache_filename, uber_inserter);
  }
  SETSTAT(stats.numPixelShadersAlive, pshaders.size());
}

static bool GetProgramBinary(const ProgramShaderCache::PCacheEntry& entry, std::vector<u8>& data)
{
  // Clear any prior error code
  glGetError();

  GLint link_status = GL_FALSE, delete_status = GL_TRUE, binary_size = 0;
  glGetProgramiv(entry.shader.glprogid, GL_LINK_STATUS, &link_status);
  glGetProgramiv(entry.shader.glprogid, GL_DELETE_STATUS, &delete_status);
  glGetProgramiv(entry.shader.glprogid, GL_PROGRAM_BINARY_LENGTH, &binary_size);
  if (glGetError() != GL_NO_ERROR || link_status == GL_FALSE || delete_status == GL_TRUE ||
      binary_size == 0)
  {
    return false;
  }

  data.resize(binary_size + sizeof(GLenum));

  GLsizei length = binary_size;
  GLenum prog_format;
  glGetProgramBinary(entry.shader.glprogid, binary_size, &length, &prog_format,
                     &data[sizeof(GLenum)]);
  if (glGetError() != GL_NO_ERROR)
    return false;

  std::memcpy(&data[0], &prog_format, sizeof(prog_format));
  return true;
}

template <typename CacheMapType, typename DiskCacheType>
static void SaveProgramBinaryMap(CacheMapType& program_map, DiskCacheType& disk_cache)
{
  std::vector<u8> binary_data;
  for (auto& entry : program_map)
  {
    if (entry.second.in_cache || entry.second.pending)
      continue;

    // Entry is now in cache (even if it fails, we don't want to try to save it again).
    entry.second.in_cache = true;
    if (!GetProgramBinary(entry.second, binary_data))
      continue;

    disk_cache.Append(entry.first, &binary_data[0], static_cast<u32>(binary_data.size()));
  }

  disk_cache.Sync();
}

void ProgramShaderCache::SaveProgramBinaries()
{
  SaveProgramBinaryMap(pshaders, s_program_disk_cache);
  SaveProgramBinaryMap(ubershaders, s_uber_program_disk_cache);
}

void ProgramShaderCache::DestroyShaders()
{
  glUseProgram(0);

  for (auto& entry : pshaders)
    entry.second.Destroy();
  pshaders.clear();

  for (auto& entry : ubershaders)
    entry.second.Destroy();
  ubershaders.clear();
}

void ProgramShaderCache::CreateHeader()
{
  GLSL_VERSION v = g_ogl_config.eSupportedGLSLVersion;
  bool is_glsles = v >= GLSLES_300;
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
  case ES_TEXBUF_TYPE::TEXBUF_EXT:
    SupportedESTextureBuffer = "#extension GL_EXT_texture_buffer : enable";
    break;
  case ES_TEXBUF_TYPE::TEXBUF_OES:
    SupportedESTextureBuffer = "#extension GL_OES_texture_buffer : enable";
    break;
  case ES_TEXBUF_TYPE::TEXBUF_CORE:
  case ES_TEXBUF_TYPE::TEXBUF_NONE:
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
      v < GLSL_140 ? "#extension GL_ARB_uniform_buffer_object : enable" : "", earlyz_string.c_str(),
      (g_ActiveConfig.backend_info.bSupportsBindingLayout && v < GLSLES_310) ?
          "#extension GL_ARB_shading_language_420pack : enable" :
          "",
      (g_ogl_config.bSupportsMSAA && v < GLSL_150) ?
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
      v < GLSL_400 && g_ActiveConfig.backend_info.bSupportsGSInstancing ?
          "#extension GL_ARB_gpu_shader5 : enable" :
          "",
      v < GLSL_400 && g_ActiveConfig.backend_info.bSupportsSSAA ?
          "#extension GL_ARB_sample_shading : enable" :
          "",
      SupportedESPointSize.c_str(),
      g_ogl_config.bSupportsAEP ? "#extension GL_ANDROID_extension_pack_es31a : enable" : "",
      v < GLSL_140 && g_ActiveConfig.backend_info.bSupportsPaletteConversion ?
          "#extension GL_ARB_texture_buffer_object : enable" :
          "",
      SupportedESTextureBuffer.c_str(),
      is_glsles && g_ActiveConfig.backend_info.bSupportsDualSourceBlend ?
          "#extension GL_EXT_blend_func_extended : enable" :
          ""

      ,
      g_ogl_config.bSupportsImageLoadStore &&
              ((!is_glsles && v < GLSL_430) || (is_glsles && v < GLSLES_310)) ?
          "#extension GL_ARB_shader_image_load_store : enable" :
          "",
      is_glsles ? "precision highp float;" : "", is_glsles ? "precision highp int;" : "",
      is_glsles ? "precision highp sampler2DArray;" : "",
      (is_glsles && g_ActiveConfig.backend_info.bSupportsPaletteConversion) ?
          "precision highp usamplerBuffer;" :
          "",
      v > GLSLES_300 ? "precision highp sampler2DMS;" : "",
      v >= GLSLES_310 ? "precision highp image2DArray;" : "");
}

void ProgramShaderCache::PrecompileUberShaders()
{
  bool success = true;

  UberShader::EnumerateVertexShaderUids([&](const UberShader::VertexShaderUid& vuid) {
    UberShader::EnumeratePixelShaderUids([&](const UberShader::PixelShaderUid& puid) {
      // UIDs must have compatible texgens, a mismatching combination will never be queried.
      if (vuid.GetUidData()->num_texgens != puid.GetUidData()->num_texgens)
        return;

      EnumerateGeometryShaderUids([&](const GeometryShaderUid& guid) {
        if (guid.GetUidData()->numTexGens != vuid.GetUidData()->num_texgens)
          return;

        UBERSHADERUID uid;
        std::memcpy(&uid.vuid, &vuid, sizeof(uid.vuid));
        std::memcpy(&uid.puid, &puid, sizeof(uid.puid));
        std::memcpy(&uid.guid, &guid, sizeof(uid.guid));

        // The ubershader may already exist if shader caching is enabled.
        if (!success || ubershaders.find(uid) != ubershaders.end())
          return;

        PCacheEntry& entry = ubershaders[uid];
        entry.in_cache = false;
        entry.pending = false;

        // Multi-context path?
        if (s_async_compiler)
        {
          entry.pending = true;
          s_async_compiler->QueueWorkItem(
              s_async_compiler->CreateWorkItem<UberShaderCompileWorkItem>(uid));
          return;
        }

        ShaderHostConfig host_config = ShaderHostConfig::GetCurrent();
        ShaderCode vcode =
            UberShader::GenVertexShader(APIType::OpenGL, host_config, uid.vuid.GetUidData());
        ShaderCode pcode =
            UberShader::GenPixelShader(APIType::OpenGL, host_config, uid.puid.GetUidData());
        ShaderCode gcode;
        if (g_ActiveConfig.backend_info.bSupportsGeometryShaders &&
            !uid.guid.GetUidData()->IsPassthrough())
        {
          GenerateGeometryShaderCode(APIType::OpenGL, host_config, uid.guid.GetUidData());
        }

        // Always background compile, even when it's not supported.
        // This way hopefully the driver can still compile the shaders in parallel.
        if (!CompileShader(entry.shader, vcode.GetBuffer(), pcode.GetBuffer(), gcode.GetBuffer()))
        {
          // Stop compiling shaders if any of them fail, no point continuing.
          success = false;
          return;
        }
      });
    });
  });

  if (s_async_compiler)
  {
    s_async_compiler->WaitUntilCompletion([](size_t completed, size_t total) {
      Host_UpdateProgressDialog(GetStringT("Compiling shaders...").c_str(),
                                static_cast<int>(completed), static_cast<int>(total));
    });
    s_async_compiler->RetrieveWorkItems();
    Host_UpdateProgressDialog("", -1, -1);
  }

  if (!success)
  {
    PanicAlert("One or more ubershaders failed to compile. Disabling ubershaders.");
    for (auto& it : ubershaders)
      it.second.Destroy();
    ubershaders.clear();
  }
}

bool ProgramShaderCache::SharedContextAsyncShaderCompiler::WorkerThreadInitMainThread(void** param)
{
  SharedContextData* ctx_data = new SharedContextData();
  ctx_data->context = GLInterface->CreateSharedContext();
  if (!ctx_data->context)
  {
    PanicAlert("Failed to create shared context for shader compiling.");
    delete ctx_data;
    return false;
  }

  *param = ctx_data;
  return true;
}

bool ProgramShaderCache::SharedContextAsyncShaderCompiler::WorkerThreadInitWorkerThread(void* param)
{
  SharedContextData* ctx_data = reinterpret_cast<SharedContextData*>(param);
  if (!ctx_data->context->MakeCurrent())
  {
    PanicAlert("Failed to make shared context current.");
    ctx_data->context->Shutdown();
    delete ctx_data;
    return false;
  }

  CreatePrerenderArrays(ctx_data);
  return true;
}

void ProgramShaderCache::SharedContextAsyncShaderCompiler::WorkerThreadExit(void* param)
{
  SharedContextData* ctx_data = reinterpret_cast<SharedContextData*>(param);
  DestroyPrerenderArrays(ctx_data);
  ctx_data->context->Shutdown();
  delete ctx_data;
}

ProgramShaderCache::ShaderCompileWorkItem::ShaderCompileWorkItem(const SHADERUID& uid)
{
  std::memcpy(&m_uid, &uid, sizeof(m_uid));
}

bool ProgramShaderCache::ShaderCompileWorkItem::Compile()
{
  ShaderHostConfig host_config = ShaderHostConfig::GetCurrent();
  ShaderCode vcode =
      GenerateVertexShaderCode(APIType::OpenGL, host_config, m_uid.vuid.GetUidData());
  ShaderCode pcode = GeneratePixelShaderCode(APIType::OpenGL, host_config, m_uid.puid.GetUidData());
  ShaderCode gcode;
  if (g_ActiveConfig.backend_info.bSupportsGeometryShaders &&
      !m_uid.guid.GetUidData()->IsPassthrough())
    gcode = GenerateGeometryShaderCode(APIType::OpenGL, host_config, m_uid.guid.GetUidData());

  CompileShader(m_program, vcode.GetBuffer(), pcode.GetBuffer(), gcode.GetBuffer());
  DrawPrerenderArray(m_program, m_uid.guid.GetUidData()->primitive_type);
  return true;
}

void ProgramShaderCache::ShaderCompileWorkItem::Retrieve()
{
  auto iter = pshaders.find(m_uid);
  if (iter != pshaders.end() && !iter->second.pending)
  {
    // Main thread already compiled this shader.
    m_program.Destroy();
    return;
  }

  PCacheEntry& entry = pshaders[m_uid];
  entry.shader = m_program;
  entry.in_cache = false;
  entry.pending = false;
}

ProgramShaderCache::UberShaderCompileWorkItem::UberShaderCompileWorkItem(const UBERSHADERUID& uid)
{
  std::memcpy(&m_uid, &uid, sizeof(m_uid));
}

bool ProgramShaderCache::UberShaderCompileWorkItem::Compile()
{
  ShaderHostConfig host_config = ShaderHostConfig::GetCurrent();
  ShaderCode vcode =
      UberShader::GenVertexShader(APIType::OpenGL, host_config, m_uid.vuid.GetUidData());
  ShaderCode pcode =
      UberShader::GenPixelShader(APIType::OpenGL, host_config, m_uid.puid.GetUidData());
  ShaderCode gcode;
  if (g_ActiveConfig.backend_info.bSupportsGeometryShaders &&
      !m_uid.guid.GetUidData()->IsPassthrough())
    gcode = GenerateGeometryShaderCode(APIType::OpenGL, host_config, m_uid.guid.GetUidData());

  CompileShader(m_program, vcode.GetBuffer(), pcode.GetBuffer(), gcode.GetBuffer());
  DrawPrerenderArray(m_program, m_uid.guid.GetUidData()->primitive_type);
  return true;
}

void ProgramShaderCache::UberShaderCompileWorkItem::Retrieve()
{
  auto iter = ubershaders.find(m_uid);
  if (iter != ubershaders.end() && !iter->second.pending)
  {
    // Main thread already compiled this shader.
    m_program.Destroy();
    return;
  }

  PCacheEntry& entry = ubershaders[m_uid];
  entry.shader = m_program;
  entry.in_cache = false;
  entry.pending = false;
}

void ProgramShaderCache::CreatePrerenderArrays(SharedContextData* data)
{
  // Create a framebuffer object to render into.
  // This is because in EGL, and potentially GLX, we have a surfaceless context.
  glGenTextures(1, &data->prerender_FBO_tex);
  glBindTexture(GL_TEXTURE_2D_ARRAY, data->prerender_FBO_tex);
  glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAX_LEVEL, 1);
  glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA, 1, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
  glGenTextures(1, &data->prerender_FBO_depth);
  glBindTexture(GL_TEXTURE_2D_ARRAY, data->prerender_FBO_depth);
  glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAX_LEVEL, 1);
  glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_DEPTH_COMPONENT32F, 1, 1, 1, 0, GL_DEPTH_COMPONENT,
               GL_FLOAT, nullptr);
  glGenFramebuffers(1, &data->prerender_FBO);
  glBindFramebuffer(GL_FRAMEBUFFER, data->prerender_FBO);
  glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, data->prerender_FBO_tex, 0, 0);
  glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, data->prerender_FBO_depth, 0, 0);

  // Create VAO for the prerender vertices.
  // We don't use the normal VAO map, since we need to change the VBO pointer.
  glGenVertexArrays(1, &data->prerender_VAO);
  glBindVertexArray(data->prerender_VAO);

  // Create and populate the prerender VBO. We need enough space to draw 3 triangles.
  static constexpr float vbo_data[] = {0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f};
  constexpr u32 vbo_stride = sizeof(float) * 3;
  glGenBuffers(1, &data->prerender_VBO);
  glBindBuffer(GL_ARRAY_BUFFER, data->prerender_VBO);
  glBufferData(GL_ARRAY_BUFFER, sizeof(vbo_data), vbo_data, GL_STATIC_DRAW);

  // We only need a position in our prerender vertex.
  glEnableVertexAttribArray(SHADER_POSITION_ATTRIB);
  glVertexAttribPointer(SHADER_POSITION_ATTRIB, 3, GL_FLOAT, GL_FALSE, vbo_stride, nullptr);

  // The other attributes have to be active to avoid variant generation.
  glEnableVertexAttribArray(SHADER_POSMTX_ATTRIB);
  glVertexAttribIPointer(SHADER_POSMTX_ATTRIB, 1, GL_UNSIGNED_BYTE, vbo_stride, nullptr);
  for (u32 i = 0; i < 3; i++)
  {
    glEnableVertexAttribArray(SHADER_NORM0_ATTRIB + i);
    glVertexAttribPointer(SHADER_NORM0_ATTRIB + i, 3, GL_FLOAT, GL_FALSE, vbo_stride, nullptr);
  }
  for (u32 i = 0; i < 2; i++)
  {
    glEnableVertexAttribArray(SHADER_COLOR0_ATTRIB + i);
    glVertexAttribPointer(SHADER_COLOR0_ATTRIB + i, 4, GL_UNSIGNED_BYTE, GL_TRUE, vbo_stride,
                          nullptr);
  }
  for (u32 i = 0; i < 8; i++)
  {
    glEnableVertexAttribArray(SHADER_TEXTURE0_ATTRIB + i);
    glVertexAttribPointer(SHADER_TEXTURE0_ATTRIB + i, 3, GL_FLOAT, GL_FALSE, vbo_stride, nullptr);
  }

  // We need an index buffer to set up the same drawing state on Mesa.
  static constexpr u16 ibo_data[] = {0, 1, 2};
  glGenBuffers(1, &data->prerender_IBO);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, data->prerender_IBO);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(ibo_data), ibo_data, GL_STATIC_DRAW);

  // Mesa also requires the primitive restart state matches?
  if (g_ActiveConfig.backend_info.bSupportsPrimitiveRestart)
  {
    if (GLInterface->GetMode() == GLInterfaceMode::MODE_OPENGLES3)
    {
      glEnable(GL_PRIMITIVE_RESTART_FIXED_INDEX);
    }
    else
    {
      if (GLExtensions::Version() >= 310)
      {
        glEnable(GL_PRIMITIVE_RESTART);
        glPrimitiveRestartIndex(65535);
      }
      else
      {
        glEnableClientState(GL_PRIMITIVE_RESTART_NV);
        glPrimitiveRestartIndexNV(65535);
      }
    }
  }
}

void ProgramShaderCache::DestroyPrerenderArrays(SharedContextData* data)
{
  if (data->prerender_VAO)
  {
    glDeleteVertexArrays(1, &data->prerender_VAO);
    data->prerender_VAO = 0;
  }
  if (data->prerender_VBO)
  {
    glDeleteBuffers(1, &data->prerender_VBO);
    data->prerender_VBO = 0;
  }
  if (data->prerender_IBO)
  {
    glDeleteBuffers(1, &data->prerender_IBO);
    data->prerender_IBO = 0;
  }
  if (data->prerender_FBO)
  {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &data->prerender_FBO);
    data->prerender_FBO = 0;
  }
  if (data->prerender_FBO_tex)
  {
    glDeleteTextures(1, &data->prerender_FBO_tex);
    data->prerender_FBO_tex = 0;
  }
  if (data->prerender_FBO_depth)
  {
    glDeleteTextures(1, &data->prerender_FBO_depth);
    data->prerender_FBO_depth = 0;
  }
}

void ProgramShaderCache::DrawPrerenderArray(const SHADER& shader, u32 primitive_type)
{
  // This is called on a worker thread, so we don't want to use the normal binding process.
  glUseProgram(shader.glprogid);

  // The number of primitives drawn depends on the type.
  switch (primitive_type)
  {
  case PRIMITIVE_POINTS:
    glDrawElements(GL_POINTS, 1, GL_UNSIGNED_SHORT, nullptr);
    break;
  case PRIMITIVE_LINES:
    glDrawElements(GL_LINES, 2, GL_UNSIGNED_SHORT, nullptr);
    break;
  case PRIMITIVE_TRIANGLES:
    glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_SHORT, nullptr);
    break;
  }

  // Has to be finished by the time the main thread picks it up.
  GLsync sync = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
  glClientWaitSync(sync, GL_SYNC_FLUSH_COMMANDS_BIT, GL_TIMEOUT_IGNORED);
  glDeleteSync(sync);
}

}  // namespace OGL
