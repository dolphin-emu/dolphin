// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <fstream>

#include <shaderc/shaderc.hpp>

#include "Core/ConfigManager.h"

#include "VideoBackends/Vulkan/ShaderCache.h"

#include "VideoCommon/Debugger.h"
#include "VideoCommon/Statistics.h"

namespace Vulkan {

// TODO: Get rid of this static
// It's also not thread safe...
static shaderc::Compiler s_compiler;

// Utility methods that are uid-type-dependant
template<typename Uid>
struct ShaderCacheFunctions
{
	static shaderc_shader_kind GetShaderKind();
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
VkShaderModule ShaderCache<Uid>::GetShaderForUid(const Uid& uid, u32 primitive_type, DSTALPHA_MODE dstalpha_mode)
{
	// Check if the shader is already in the cache
	auto iter = m_shaders.find(uid);
	if (iter != m_shaders.end())
		return iter->second;

	// Have to compile the shader, so generate the source code for it (still using OGL for now)
	ShaderCode code = ShaderCacheFunctions<Uid>::GenerateCode(dstalpha_mode, primitive_type);

	// Call out to shaderc to do the heavy lifting
	shaderc::Compiler compiler;
	shaderc::CompileOptions options;
	shaderc::SpvCompilationResult result = s_compiler.CompileGlslToSpv(
		code.GetBuffer(),
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
			stream << code.GetBuffer() << "\n"
				   << result.GetNumErrors() << " errors, "
				   << result.GetNumWarnings() << " warnings\n"
				   << result.GetErrorMessage();

			stream.close();
		}

		PanicAlert("Failed to compile shader (written to %s):\n%s", filename.c_str(), result.GetErrorMessage().c_str());
		m_shaders.emplace(uid, nullptr);
		return nullptr;
	}

	// Shader dumping
	if (g_ActiveConfig.iLog & CONF_SAVESHADERS)
	{
		std::string filename = ShaderCacheFunctions<Uid>::GetDumpFileName("");
		std::ofstream stream;
		OpenFStream(stream, filename, std::ios_base::out);
		if (stream.good())
		{
			shaderc::AssemblyCompilationResult resultasm = s_compiler.CompileGlslToSpvAssembly(
				code.GetBuffer(),
				ShaderCacheFunctions<Uid>::GetShaderKind(),
				"shader.glsl",
				options);

			stream << code.GetBuffer();
			
			if (resultasm.GetCompilationStatus() == shaderc_compilation_status_success)
				stream << "Compiled SPIR-V:\n" << resultasm.begin();

			stream.close();
		}
	}

	// Construct a vector of dwords containing the spir-v code
	std::vector<uint32_t> spirv(result.begin(), result.end());

	// Create a shader module on the device
	VkShaderModuleCreateInfo info = {
		VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		nullptr,
		0,
		static_cast<uint32_t>(spirv.size() * sizeof(uint32_t)),
		spirv.data()
	};

	VkShaderModule module;
	VkResult res = vkCreateShaderModule(m_device, &info, nullptr, &module);
	if (res != VK_SUCCESS)
	{
		LOG_VULKAN_ERROR(res, "vkCreateShaderModule failed: ");
		m_shaders.emplace(uid, nullptr);
		return nullptr;
	}

	ShaderCacheFunctions<VertexShaderUid>::IncrementCounter();

	// Store the shader bytecode to the disk cache
	m_disk_cache.Append(uid, spirv.data(), static_cast<u32>(spirv.size()));
	m_shaders.emplace(uid, module);
	return module;
}

// Vertex shader functions
template<>
struct ShaderCacheFunctions<VertexShaderUid>
{
	static shaderc_shader_kind GetShaderKind()
	{
		return shaderc_glsl_vertex_shader;
	}

	static ShaderCode GenerateCode(DSTALPHA_MODE dst_alpha_mode, u32 primitive_type)
	{
		return GenerateVertexShaderCode(API_OPENGL);
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
		return StringFromFormat("%s%s_vs_%04i.txt",
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
	static shaderc_shader_kind GetShaderKind()
	{
		return shaderc_glsl_geometry_shader;
	}

	static ShaderCode GenerateCode(DSTALPHA_MODE dst_alpha_mode, u32 primitive_type)
	{
		return GenerateGeometryShaderCode(primitive_type, API_OPENGL);
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
		return StringFromFormat("%s%s_gs_%04i.txt",
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
	static shaderc_shader_kind GetShaderKind()
	{
		return shaderc_glsl_fragment_shader;
	}

	static ShaderCode GenerateCode(DSTALPHA_MODE dst_alpha_mode, u32 primitive_type)
	{
		return GeneratePixelShaderCode(dst_alpha_mode, API_OPENGL);
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
		return StringFromFormat("%s%s_ps_%04i.txt",
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
