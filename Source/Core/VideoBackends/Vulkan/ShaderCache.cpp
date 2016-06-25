// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <fstream>

#ifdef WIN32
	//#define HAS_SHADERC
#endif

#ifdef HAS_SHADERC
#include <shaderc/shaderc.hpp>
#endif

#include "Common/FileUtil.h"
#include "Common/MsgHandler.h"

#include "Core/ConfigManager.h"

#include "VideoCommon/Statistics.h"

#include "VideoBackends/Vulkan/ShaderCache.h"

namespace Vulkan {

#ifdef HAS_SHADERC
// TODO: Get rid of this static
// It's also not thread safe...
static shaderc::Compiler& GetShaderCompiler();
#endif

// Shader header prepended to all shaders
std::string GetShaderHeader();

// Utility methods that are uid-type-dependant
template<typename Uid>
struct ShaderCacheFunctions
{
#ifdef HAS_SHADERC
	static shaderc_shader_kind GetShaderKind();
#endif
	static ShaderCode GenerateCode(DSTALPHA_MODE dst_alpha_mode, u32 primitive_type);
	static std::string GetDiskCacheFileName();
	static std::string GetDumpFileName(const char* prefix);
	static void ResetCounters();
	static void IncrementCounter();
};

// Cache inserter that is called back when reading from the file
template<typename Uid>
struct ShaderCacheInserter : public LinearDiskCacheReader<Uid, u32>
{
	ShaderCacheInserter(VkDevice device, std::map<Uid, VkShaderModule>& shader_map)
		: m_device(device)
		, m_shader_map(shader_map)
	{
	}

	virtual void Read(const Uid& key, const u32* value, u32 value_size) override
	{
		// Create a shader module on the device
		VkShaderModuleCreateInfo info = {
			VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
			nullptr,
			0,
			value_size * sizeof(u32),
			value
		};

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
ShaderCache<Uid>::ShaderCache(VkDevice device)
	: m_device(device)
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
bool Vulkan::ShaderCache<Uid>::CompileShaderToSPV(const std::string& shader_source, std::vector<uint32_t>* spv)
{
#ifdef HAS_SHADERC
	// Call out to shaderc to do the heavy lifting
	shaderc::Compiler& compiler = GetShaderCompiler();
	shaderc::CompileOptions options;
	shaderc::SpvCompilationResult result = compiler.CompileGlslToSpv(
		shader_source,
		ShaderCacheFunctions<Uid>::GetShaderKind(),
		"shader.glsl",
		options);

	// Check for failed compilation
	if (result.GetCompilationStatus() != shaderc_compilation_status_success)
	{
		// Compile failed, write the bad shader to a file
		std::string filename = ShaderCacheFunctions<Uid>::GetDumpFileName("bad_");
		std::ofstream stream;
		OpenFStream(stream, filename, std::ios_base::out);
		if (stream.good())
		{
			stream << shader_source << "\n"
			       << result.GetNumErrors() << " errors, "
			       << result.GetNumWarnings() << " warnings\n"
			       << result.GetErrorMessage();

			stream.close();
		}

		PanicAlert("Failed to compile shader (written to %s):\n%s", filename.c_str(), result.GetErrorMessage().c_str());
		return false;

	}

	// Log warnings if any
	if (result.GetNumWarnings() > 0)
		WARN_LOG(VIDEO, "Shader compiler produced warnings: \n%s", result.GetErrorMessage().c_str());

	// Shader dumping
	if (g_ActiveConfig.iLog & CONF_SAVESHADERS)
	{
		std::string filename = ShaderCacheFunctions<Uid>::GetDumpFileName("");
		std::ofstream stream;
		OpenFStream(stream, filename, std::ios_base::out);
		if (stream.good())
		{
			shaderc::AssemblyCompilationResult resultasm = compiler.CompileGlslToSpvAssembly(
				shader_source,
				ShaderCacheFunctions<Uid>::GetShaderKind(),
				"shader.glsl",
				options);

			stream << shader_source;

			if (resultasm.GetCompilationStatus() == shaderc_compilation_status_success)
				stream << "Compiled SPIR-V:\n" << resultasm.begin();

			stream.close();
		}
	}

	// Construct a vector of dwords containing the spir-v code
	*spv = std::move(std::vector<uint32_t>(result.begin(), result.end()));
	return true;
#else
	return false;
#endif
}

template <typename Uid>
VkShaderModule ShaderCache<Uid>::CompileAndCreateShader(const std::string& shader_source, bool prepend_header)
{
	// Pass it the header and source through
	std::vector<uint32_t> spv;
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
	VkShaderModuleCreateInfo info = {
		VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		nullptr,
		0,
		static_cast<uint32_t>(spv.size() * sizeof(uint32_t)),
		spv.data()
	};

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
VkShaderModule ShaderCache<Uid>::GetShaderForUid(const Uid& uid, u32 primitive_type, DSTALPHA_MODE dstalpha_mode)
{
	// Check if the shader is already in the cache
	auto iter = m_shaders.find(uid);
	if (iter != m_shaders.end())
		return iter->second;

	// Have to compile the shader, so generate the source code for it (still using OGL for now)
	ShaderCode code = ShaderCacheFunctions<Uid>::GenerateCode(dstalpha_mode, primitive_type);

	// Append header to code
	std::string full_code = GetShaderHeader() + code.GetBuffer();

	// Actually compile the shader, this may not succeed if we did something bad
	std::vector<uint32_t> spv;
	if (!CompileShaderToSPV(full_code, &spv))
	{
		// Store a null pointer, no point re-attempting compilation multiple times
		m_shaders.emplace(uid, nullptr);
		return nullptr;
	}

	// Create a shader module on the device
	VkShaderModuleCreateInfo info = {
		VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		nullptr,
		0,
		static_cast<uint32_t>(spv.size() * sizeof(uint32_t)),
		spv.data()
	};

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

#ifdef HAS_SHADERC
shaderc::Compiler& GetShaderCompiler()
{
	static shaderc::Compiler lazy_initialized_compiler;
	return lazy_initialized_compiler;
}
#endif

std::string GetShaderHeader()
{
	// TODO: Handle GLSL versions/extensions/mobile
	return StringFromFormat(
		"#version 450 core\n"
		//"#extension GL_KHR_vulkan_glsl : require\n"
		"#extension GL_ARB_shading_language_420pack : require\n" // 420pack
		"#extension GL_ARB_shader_image_load_store : enable\n" // early-z
		"#define SAMPLER_BINDING(x) layout(binding = x)\n" // Sampler binding
		"#define FORCE_EARLY_Z layout(early_fragment_tests) in\n" // early-z

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

		// OGL->Vulkan stupidity
		"#define gl_VertexID gl_VertexIndex\n"
		"#define gl_InstanceID gl_InstanceIndex\n"
	);
}

// Vertex shader functions
template<>
struct ShaderCacheFunctions<VertexShaderUid>
{
#ifdef HAS_SHADERC
	static shaderc_shader_kind GetShaderKind()
	{
		return shaderc_glsl_vertex_shader;
	}
#endif

	static ShaderCode GenerateCode(DSTALPHA_MODE dst_alpha_mode, u32 primitive_type)
	{
		return GenerateVertexShaderCode(API_VULKAN);
	}

	static std::string GetDiskCacheFileName()
	{
		if (!File::Exists(File::GetUserPath(D_SHADERCACHE_IDX)))
			File::CreateDir(File::GetUserPath(D_SHADERCACHE_IDX));

		return StringFromFormat(
			"%svulkan-%s-vs.cache",
			File::GetUserPath(D_SHADERCACHE_IDX).c_str(),
			SConfig::GetInstance().m_strUniqueID.c_str());
	}

	static std::string GetDumpFileName(const char* prefix)
	{
		if (!File::Exists(File::GetUserPath(D_DUMP_IDX)))
			File::CreateDir(File::GetUserPath(D_DUMP_IDX));

		static int counter = 0;
		return StringFromFormat("%s%svs_%04i.txt",
			File::GetUserPath(D_DUMP_IDX).c_str(),
			prefix,
			counter++);
	}

	static void ResetCounters()
	{
		SETSTAT(stats.numVertexShadersCreated, 0);
		SETSTAT(stats.numVertexShadersAlive, 0);
	}

	static void IncrementCounter()
	{
		INCSTAT(stats.numVertexShadersCreated);
	}
};
template class ShaderCache<VertexShaderUid>;

// Geometry shader functions
template<>
struct ShaderCacheFunctions<GeometryShaderUid>
{
#ifdef HAS_SHADERC
	static shaderc_shader_kind GetShaderKind()
	{
		return shaderc_glsl_geometry_shader;
	}
#endif

	static ShaderCode GenerateCode(DSTALPHA_MODE dst_alpha_mode, u32 primitive_type)
	{
		return GenerateGeometryShaderCode(primitive_type, API_VULKAN);
	}

	static std::string GetDiskCacheFileName()
	{
		if (!File::Exists(File::GetUserPath(D_SHADERCACHE_IDX)))
			File::CreateDir(File::GetUserPath(D_SHADERCACHE_IDX));

		return StringFromFormat(
			"%svulkan-%s-gs.cache",
			File::GetUserPath(D_SHADERCACHE_IDX).c_str(),
			SConfig::GetInstance().m_strUniqueID.c_str());
	}

	static std::string GetDumpFileName(const char* prefix)
	{
		if (!File::Exists(File::GetUserPath(D_DUMP_IDX)))
			File::CreateDir(File::GetUserPath(D_DUMP_IDX));

		static int counter = 0;
		return StringFromFormat("%s%sgs_%04i.txt",
			File::GetUserPath(D_DUMP_IDX).c_str(),
			prefix,
			counter++);
	}

	static void ResetCounters()
	{
		//SETSTAT(stats.numGeometryShadersCreated, 0);
		//SETSTAT(stats.numGeometryShadersAlive, 0);
	}

	static void IncrementCounter()
	{
		//INCSTAT(stats.numGeometryShadersCreated);
	}
};
template class ShaderCache<GeometryShaderUid>;

// Pixel shader functions
template<>
struct ShaderCacheFunctions<PixelShaderUid>
{
#ifdef HAS_SHADERC
	static shaderc_shader_kind GetShaderKind()
	{
		return shaderc_glsl_fragment_shader;
	}
#endif

	static ShaderCode GenerateCode(DSTALPHA_MODE dst_alpha_mode, u32 primitive_type)
	{
		return GeneratePixelShaderCode(dst_alpha_mode, API_VULKAN);
	}

	static std::string GetDiskCacheFileName()
	{
		if (!File::Exists(File::GetUserPath(D_SHADERCACHE_IDX)))
			File::CreateDir(File::GetUserPath(D_SHADERCACHE_IDX));

		return StringFromFormat(
			"%svulkan-%s-fs.cache",
			File::GetUserPath(D_SHADERCACHE_IDX).c_str(),
			SConfig::GetInstance().m_strUniqueID.c_str());
	}

	static std::string GetDumpFileName(const char* prefix)
	{
		if (!File::Exists(File::GetUserPath(D_DUMP_IDX)))
			File::CreateDir(File::GetUserPath(D_DUMP_IDX));

		static int counter = 0;
		return StringFromFormat("%s%sps_%04i.txt",
			File::GetUserPath(D_DUMP_IDX).c_str(),
			prefix,
			counter++);
	}

	static void ResetCounters()
	{
		SETSTAT(stats.numPixelShadersCreated, 0);
		SETSTAT(stats.numPixelShadersAlive, 0);
	}

	static void IncrementCounter()
	{
		INCSTAT(stats.numPixelShadersCreated);
	}
};
template class ShaderCache<PixelShaderUid>;

}
