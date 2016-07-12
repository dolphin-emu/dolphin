// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <fstream>

// glslang includes
#include "GlslangToSpv.h"
#include "ShHandle.h"
#include "disassemble.h"

#include "StandAlone/DefaultResourceLimits.h"

#include "Common/FileUtil.h"
#include "Common/MsgHandler.h"

#include "Core/ConfigManager.h"

#include "VideoCommon/Statistics.h"

#include "VideoBackends/Vulkan/ShaderCache.h"

namespace Vulkan
{
// Registers itself for cleanup via atexit
bool InitializeGlslang();

// Shader header prepended to all shaders
std::string GetShaderHeader();

// Utility methods that are uid-type-dependant
template <typename Uid>
struct ShaderCacheFunctions
{
  static EShLanguage GetStage();
  static ShaderCode GenerateCode(const Uid& uid, DSTALPHA_MODE dstalpha_mode);
  static std::string GetDiskCacheFileName();
  static std::string GetDumpFileName(const char* prefix);
  static void ResetCounters();
  static void IncrementCounter();
};

// Cache inserter that is called back when reading from the file
template <typename Uid>
struct ShaderCacheInserter : public LinearDiskCacheReader<Uid, u32>
{
  ShaderCacheInserter(VkDevice device, std::map<Uid, VkShaderModule>& shader_map)
      : m_device(device), m_shader_map(shader_map)
  {
  }

  virtual void Read(const Uid& key, const u32* value, u32 value_size) override
  {
    // Create a shader module on the device
    VkShaderModuleCreateInfo info = {VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, nullptr, 0,
                                     value_size * sizeof(u32), value};

    // We don't insert null modules here since creation could succeed later on
    VkShaderModule module;
    VkResult res = vkCreateShaderModule(m_device, &info, nullptr, &module);
    if (res != VK_SUCCESS)
    {
      LOG_VULKAN_ERROR(res, "vkCreateShaderModule failed: ");
      return;
    }

    ShaderCacheFunctions<Uid>::IncrementCounter();
    m_shader_map.emplace(key, module);
  }

  VkDevice m_device;
  std::map<Uid, VkShaderModule>& m_shader_map;
};

template <typename Uid>
ShaderCache<Uid>::ShaderCache(VkDevice device) : m_device(device)
{
  ShaderCacheFunctions<Uid>::ResetCounters();

  // Open the disk cache
  LoadShadersFromDisk();
}

template <typename Uid>
ShaderCache<Uid>::~ShaderCache()
{
  for (const auto& it : m_shaders)
  {
    if (it.second != VK_NULL_HANDLE)
      vkDestroyShaderModule(m_device, it.second, nullptr);
  }

  ShaderCacheFunctions<Uid>::ResetCounters();
}

template <typename Uid>
void Vulkan::ShaderCache<Uid>::LoadShadersFromDisk()
{
  std::string disk_cache_filename = ShaderCacheFunctions<Uid>::GetDiskCacheFileName();

  ShaderCacheInserter<Uid> inserter(m_device, m_shaders);
  m_disk_cache.OpenAndRead(disk_cache_filename, inserter);
}

template <typename Uid>
bool Vulkan::ShaderCache<Uid>::CompileShaderToSPV(const std::string& shader_source,
                                                  std::vector<unsigned int>* spv)
{
  if (!InitializeGlslang())
    return false;

  // TODO: Handle ES profile
  EShLanguage stage = ShaderCacheFunctions<Uid>::GetStage();
  std::unique_ptr<glslang::TShader> shader = std::make_unique<glslang::TShader>(stage);
  std::unique_ptr<glslang::TProgram> program;
  glslang::TShader::ForbidInclude includer;
  EProfile profile = ECoreProfile;
  EShMessages messages =
      static_cast<EShMessages>(EShMsgDefault | EShMsgSpvRules | EShMsgVulkanRules);
  int default_version = 450;

  int shader_source_length = static_cast<int>(shader_source.length());
  const char* shader_source_ptr = shader_source.c_str();
  shader->setStringsWithLengths(&shader_source_ptr, &shader_source_length, 1);

  auto DumpBadShader = [&](const char* msg) {
    std::string filename = ShaderCacheFunctions<Uid>::GetDumpFileName("bad_");
    std::ofstream stream;
    OpenFStream(stream, filename, std::ios_base::out);
    if (stream.good())
    {
      stream << shader_source << std::endl;
      stream << msg << std::endl;
      stream << "Shader Info Log:" << std::endl;
      stream << shader->getInfoLog() << std::endl;
      stream << shader->getInfoDebugLog() << std::endl;
      if (program)
      {
        stream << "Program Info Log:" << std::endl;
        stream << program->getInfoLog() << std::endl;
        stream << program->getInfoDebugLog() << std::endl;
      }
      stream.close();
    }

    PanicAlert("%s (written to %s)", msg, filename.c_str());
  };

  if (!shader->parse(&glslang::DefaultTBuiltInResource, default_version, profile, false, true,
                     messages, includer))
  {
    DumpBadShader("Failed to parse shader");
    return false;
  }

  // Even though there's only a single shader, we still need to link it to generate SPV
  program = std::make_unique<glslang::TProgram>();
  program->addShader(shader.get());
  if (!program->link(messages))
  {
    DumpBadShader("Failed to link program");
    return false;
  }

  glslang::TIntermediate* intermediate = program->getIntermediate(stage);
  if (!intermediate)
  {
    DumpBadShader("Failed to generate SPIR-V");
    return false;
  }

  spv::SpvBuildLogger logger;
  glslang::GlslangToSpv(*intermediate, *spv, &logger);

  // Write out messages
  // Temporary: skip if it contains "Warning, version 450 is not yet complete; most version-specific
  // features are present, but some are missing."
  if (strlen(shader->getInfoLog()) > 108)
    WARN_LOG(VIDEO, "Shader info log: %s", shader->getInfoLog());
  if (strlen(shader->getInfoDebugLog()) > 0)
    WARN_LOG(VIDEO, "Shader debug info log: %s", shader->getInfoDebugLog());
  if (strlen(program->getInfoLog()) > 25)
    WARN_LOG(VIDEO, "Program info log: %s", program->getInfoLog());
  if (strlen(program->getInfoDebugLog()) > 0)
    WARN_LOG(VIDEO, "Program debug info log: %s", program->getInfoDebugLog());
  std::string spv_messages = logger.getAllMessages();
  if (!spv_messages.empty())
    WARN_LOG(VIDEO, "SPIR-V conversion messages: %s", spv_messages.c_str());

  // Shader dumping
  if (g_ActiveConfig.iLog & CONF_SAVESHADERS)
  {
    std::string filename = ShaderCacheFunctions<Uid>::GetDumpFileName("");
    std::ofstream stream;
    OpenFStream(stream, filename, std::ios_base::out);
    if (stream.good())
    {
      stream << shader_source << std::endl;
      stream << "Shader Info Log:" << std::endl;
      stream << shader->getInfoLog() << std::endl;
      stream << shader->getInfoDebugLog() << std::endl;
      stream << "Program Info Log:" << std::endl;
      stream << program->getInfoLog() << std::endl;
      stream << program->getInfoDebugLog() << std::endl;
      stream << "SPIR-V conversion messages: " << std::endl;
      stream << spv_messages;
      stream << "SPIR-V:" << std::endl;
      spv::Disassemble(stream, *spv);

      stream.close();
    }
  }

  return true;
}

template <typename Uid>
VkShaderModule ShaderCache<Uid>::CompileAndCreateShader(const std::string& shader_source,
                                                        bool prepend_header)
{
  // Pass it the header and source through
  std::vector<unsigned int> spv;
  if (prepend_header)
  {
    if (!CompileShaderToSPV(GetShaderHeader() + shader_source, &spv))
      return nullptr;
  }
  else
  {
    if (!CompileShaderToSPV(shader_source, &spv))
      return nullptr;
  }

  // Create a shader module on the device
  VkShaderModuleCreateInfo info = {VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, nullptr, 0,
                                   static_cast<uint32_t>(spv.size() * sizeof(uint32_t)),
                                   spv.data()};

  VkShaderModule module;
  VkResult res = vkCreateShaderModule(m_device, &info, nullptr, &module);
  if (res != VK_SUCCESS)
  {
    LOG_VULKAN_ERROR(res, "vkCreateShaderModule failed: ");
    return nullptr;
  }

  return module;
}

template <typename Uid>
VkShaderModule ShaderCache<Uid>::GetShaderForUid(const Uid& uid, DSTALPHA_MODE dstalpha_mode)
{
  // Check if the shader is already in the cache
  auto iter = m_shaders.find(uid);
  if (iter != m_shaders.end())
    return iter->second;

  // Have to compile the shader, so generate the source code for it
  ShaderCode code = ShaderCacheFunctions<Uid>::GenerateCode(uid, dstalpha_mode);
  std::string full_code = GetShaderHeader() + code.GetBuffer();

  // Actually compile the shader, this may not succeed if we did something bad
  std::vector<unsigned int> spv;
  if (!CompileShaderToSPV(full_code, &spv))
  {
    // Store a null pointer, no point re-attempting compilation multiple times
    m_shaders.emplace(uid, nullptr);
    return nullptr;
  }

  // Create a shader module on the device
  VkShaderModuleCreateInfo info = {VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, nullptr, 0,
                                   static_cast<uint32_t>(spv.size() * sizeof(uint32_t)),
                                   spv.data()};

  VkShaderModule module;
  VkResult res = vkCreateShaderModule(m_device, &info, nullptr, &module);
  if (res != VK_SUCCESS)
  {
    LOG_VULKAN_ERROR(res, "vkCreateShaderModule failed: ");
    m_shaders.emplace(uid, nullptr);
    return nullptr;
  }

  ShaderCacheFunctions<Uid>::IncrementCounter();

  // Store the shader bytecode to the disk cache
  m_disk_cache.Append(uid, spv.data(), static_cast<u32>(spv.size()));
  m_shaders.emplace(uid, module);
  return module;
}

bool InitializeGlslang()
{
  static bool glslang_initialized = false;
  if (glslang_initialized)
    return true;

  if (!glslang::InitializeProcess())
  {
    PanicAlert("Failed to initialize glslang shader compiler");
    return false;
  }

  std::atexit([]() { glslang::FinalizeProcess(); });

  glslang_initialized = true;
  return true;
}

std::string GetShaderHeader()
{
  return StringFromFormat("#version 450 core\n"
                          "#define SAMPLER_BINDING(x) layout(set = 1, binding = x)\n"

                          // hlsl to glsl function translation
                          "#define float2 vec2\n"
                          "#define float3 vec3\n"
                          "#define float4 vec4\n"
                          "#define uint2 uvec2\n"
                          "#define uint3 uvec3\n"
                          "#define uint4 uvec4\n"
                          "#define int2 ivec2\n"
                          "#define int3 ivec3\n"
                          "#define int4 ivec4\n"
                          "#define frac fract\n"
                          "#define lerp mix\n"

                          // These were changed in Vulkan
                          "#define gl_VertexID gl_VertexIndex\n"
                          "#define gl_InstanceID gl_InstanceIndex\n");
}

// Vertex shader functions
template <>
struct ShaderCacheFunctions<VertexShaderUid>
{
  static EShLanguage GetStage() { return EShLangVertex; }
  static ShaderCode GenerateCode(const VertexShaderUid& uid, DSTALPHA_MODE dstalpha_mode)
  {
    return GenerateVertexShaderCode(API_VULKAN, uid.GetUidData());
  }

  static std::string GetDiskCacheFileName()
  {
    if (!File::Exists(File::GetUserPath(D_SHADERCACHE_IDX)))
      File::CreateDir(File::GetUserPath(D_SHADERCACHE_IDX));

    return StringFromFormat("%svulkan-%s-vs.cache", File::GetUserPath(D_SHADERCACHE_IDX).c_str(),
                            SConfig::GetInstance().m_strUniqueID.c_str());
  }

  static std::string GetDumpFileName(const char* prefix)
  {
    if (!File::Exists(File::GetUserPath(D_DUMP_IDX)))
      File::CreateDir(File::GetUserPath(D_DUMP_IDX));

    static int counter = 0;
    return StringFromFormat("%s%svs_%04i.txt", File::GetUserPath(D_DUMP_IDX).c_str(), prefix,
                            counter++);
  }

  static void ResetCounters()
  {
    SETSTAT(stats.numVertexShadersCreated, 0);
    SETSTAT(stats.numVertexShadersAlive, 0);
  }

  static void IncrementCounter() { INCSTAT(stats.numVertexShadersCreated); }
};
template class ShaderCache<VertexShaderUid>;

// Geometry shader functions
template <>
struct ShaderCacheFunctions<GeometryShaderUid>
{
  static EShLanguage GetStage() { return EShLangGeometry; }
  static ShaderCode GenerateCode(const GeometryShaderUid& uid, DSTALPHA_MODE dstalpha_mode)
  {
    return GenerateGeometryShaderCode(API_VULKAN, uid.GetUidData());
  }

  static std::string GetDiskCacheFileName()
  {
    if (!File::Exists(File::GetUserPath(D_SHADERCACHE_IDX)))
      File::CreateDir(File::GetUserPath(D_SHADERCACHE_IDX));

    return StringFromFormat("%svulkan-%s-gs.cache", File::GetUserPath(D_SHADERCACHE_IDX).c_str(),
                            SConfig::GetInstance().m_strUniqueID.c_str());
  }

  static std::string GetDumpFileName(const char* prefix)
  {
    if (!File::Exists(File::GetUserPath(D_DUMP_IDX)))
      File::CreateDir(File::GetUserPath(D_DUMP_IDX));

    static int counter = 0;
    return StringFromFormat("%s%sgs_%04i.txt", File::GetUserPath(D_DUMP_IDX).c_str(), prefix,
                            counter++);
  }

  static void ResetCounters()
  {
    // SETSTAT(stats.numGeometryShadersCreated, 0);
    // SETSTAT(stats.numGeometryShadersAlive, 0);
  }

  static void IncrementCounter()
  {
    // INCSTAT(stats.numGeometryShadersCreated);
  }
};
template class ShaderCache<GeometryShaderUid>;

// Pixel shader functions
template <>
struct ShaderCacheFunctions<PixelShaderUid>
{
  static EShLanguage GetStage() { return EShLangFragment; }
  static ShaderCode GenerateCode(const PixelShaderUid& uid, DSTALPHA_MODE dstalpha_mode)
  {
    return GeneratePixelShaderCode(dstalpha_mode, API_VULKAN, uid.GetUidData());
  }

  static std::string GetDiskCacheFileName()
  {
    if (!File::Exists(File::GetUserPath(D_SHADERCACHE_IDX)))
      File::CreateDir(File::GetUserPath(D_SHADERCACHE_IDX));

    return StringFromFormat("%svulkan-%s-fs.cache", File::GetUserPath(D_SHADERCACHE_IDX).c_str(),
                            SConfig::GetInstance().m_strUniqueID.c_str());
  }

  static std::string GetDumpFileName(const char* prefix)
  {
    if (!File::Exists(File::GetUserPath(D_DUMP_IDX)))
      File::CreateDir(File::GetUserPath(D_DUMP_IDX));

    static int counter = 0;
    return StringFromFormat("%s%sps_%04i.txt", File::GetUserPath(D_DUMP_IDX).c_str(), prefix,
                            counter++);
  }

  static void ResetCounters()
  {
    SETSTAT(stats.numPixelShadersCreated, 0);
    SETSTAT(stats.numPixelShadersAlive, 0);
  }

  static void IncrementCounter() { INCSTAT(stats.numPixelShadersCreated); }
};
template class ShaderCache<PixelShaderUid>;
}
