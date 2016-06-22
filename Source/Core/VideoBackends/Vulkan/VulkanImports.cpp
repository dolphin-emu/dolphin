#include <atomic>
#include <cstdarg>

#include "Common/Logging/Log.h"
#include "Common/StringUtil.h"

#include "VideoBackends/Vulkan/VulkanImports.h"

#if defined(WIN32)
	#include <Windows.h>
#endif

#define VULKAN_ENTRY_POINT(type, name, required) type name;
#include "VideoBackends/Vulkan/VulkanImports.inl"
#undef VULKAN_ENTRY_POINT

namespace Vulkan {

static void ResetVulkanLibraryFunctionPointers()
{
#define VULKAN_ENTRY_POINT(type, name, required) name = nullptr;
#include "VideoBackends/Vulkan/VulkanImports.inl"
#undef VULKAN_ENTRY_POINT
}

#if defined(WIN32)

static HMODULE vulkan_module;
static std::atomic_int vulkan_module_ref_count = { 0 };

bool LoadVulkanLibrary()
{
	// Not thread safe if a second thread calls the loader whilst the first is still in-progress.
	if (vulkan_module)
	{
		vulkan_module_ref_count++;
		return true;
	}

	vulkan_module = LoadLibraryA("vulkan-1.dll");
	if (!vulkan_module)
	{
		ERROR_LOG(VIDEO, "Failed to load vulkan-1.dll");
		return false;
	}

	bool required_functions_missing = false;
	auto LoadFunction = [&](FARPROC* func_ptr, const char* name, bool is_required)
	{
		*func_ptr = GetProcAddress(vulkan_module, name);
		if (!(*func_ptr))
		{
			ERROR_LOG(VIDEO, "Vulkan: Failed to load required function %s", name);
			required_functions_missing = true;
		}
	};

#define VULKAN_ENTRY_POINT(type, name, required) LoadFunction(reinterpret_cast<FARPROC*>(&name), #name, required);
#include "VideoBackends/Vulkan/VulkanImports.inl"
#undef VULKAN_ENTRY_POINT

	if (required_functions_missing)
	{
		ResetVulkanLibraryFunctionPointers();
		FreeLibrary(vulkan_module);
		vulkan_module = nullptr;
		return false;
	}

	vulkan_module_ref_count++;
	return true;
}

void UnloadVulkanLibrary()
{
	if ((--vulkan_module_ref_count) > 0)
		return;

	ResetVulkanLibraryFunctionPointers();
	FreeLibrary(vulkan_module);
	vulkan_module = nullptr;
}

#endif      // WIN32

const char* VkResultToString(VkResult res)
{
	switch (res)
	{
	case VK_SUCCESS:
		return "VK_SUCCESS";

	case VK_NOT_READY:
		return "VK_NOT_READY";

	case VK_TIMEOUT:
		return "VK_TIMEOUT";

	case VK_EVENT_SET:
		return "VK_EVENT_SET";

	case VK_EVENT_RESET:
		return "VK_EVENT_RESET";

	case VK_INCOMPLETE:
		return "VK_INCOMPLETE";

	case VK_ERROR_OUT_OF_HOST_MEMORY:
		return "VK_ERROR_OUT_OF_HOST_MEMORY";

	case VK_ERROR_OUT_OF_DEVICE_MEMORY:
		return "VK_ERROR_OUT_OF_DEVICE_MEMORY";

	case VK_ERROR_INITIALIZATION_FAILED:
		return "VK_ERROR_INITIALIZATION_FAILED";

	case VK_ERROR_DEVICE_LOST:
		return "VK_ERROR_DEVICE_LOST";

	case VK_ERROR_MEMORY_MAP_FAILED:
		return "VK_ERROR_MEMORY_MAP_FAILED";

	case VK_ERROR_LAYER_NOT_PRESENT:
		return "VK_ERROR_LAYER_NOT_PRESENT";

	case VK_ERROR_EXTENSION_NOT_PRESENT:
		return "VK_ERROR_EXTENSION_NOT_PRESENT";

	case VK_ERROR_FEATURE_NOT_PRESENT:
		return "VK_ERROR_FEATURE_NOT_PRESENT";

	case VK_ERROR_INCOMPATIBLE_DRIVER:
		return "VK_ERROR_INCOMPATIBLE_DRIVER";

	case VK_ERROR_TOO_MANY_OBJECTS:
		return "VK_ERROR_TOO_MANY_OBJECTS";

	case VK_ERROR_FORMAT_NOT_SUPPORTED:
		return "VK_ERROR_FORMAT_NOT_SUPPORTED";

	case VK_ERROR_SURFACE_LOST_KHR:
		return "VK_ERROR_SURFACE_LOST_KHR";

	case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR:
		return "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR";

	case VK_SUBOPTIMAL_KHR:
		return "VK_SUBOPTIMAL_KHR";

	case VK_ERROR_OUT_OF_DATE_KHR:
		return "VK_ERROR_OUT_OF_DATE_KHR";

	case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR:
		return "VK_ERROR_INCOMPATIBLE_DISPLAY_KHR";

	case VK_ERROR_VALIDATION_FAILED_EXT:
		return "VK_ERROR_VALIDATION_FAILED_EXT";

	case VK_ERROR_INVALID_SHADER_NV:
		return "VK_ERROR_INVALID_SHADER_NV";
	}

	return "UNKNOWN_VK_RESULT";
}

void LogVulkanResult(int level, const char* func_name, VkResult res, const char* msg, ...)
{
	std::va_list ap;
	va_start(ap, msg);
	std::string real_msg = StringFromFormatV(msg, ap);
	va_end(ap);

	real_msg = StringFromFormat("(%s) %s (%d)", func_name, real_msg.c_str(), VkResultToString(res), static_cast<int>(res));
	
	GENERIC_LOG(LogTypes::VIDEO, static_cast<LogTypes::LOG_LEVELS>(level), real_msg.c_str());
}

}		// namespace Vulkan
