// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <vector>

#include "Core/Host.h"

#include "VideoBackends/Vulkan/CommandBufferManager.h"
#include "VideoBackends/Vulkan/FramebufferManager.h"
#include "VideoBackends/Vulkan/Globals.h"
#include "VideoBackends/Vulkan/Helpers.h"
#include "VideoBackends/Vulkan/ObjectCache.h"
#include "VideoBackends/Vulkan/PerfQuery.h"
#include "VideoBackends/Vulkan/Renderer.h"
#include "VideoBackends/Vulkan/ShaderCache.h"
#include "VideoBackends/Vulkan/StateTracker.h"
#include "VideoBackends/Vulkan/SwapChain.h"
#include "VideoBackends/Vulkan/TextureCache.h"
#include "VideoBackends/Vulkan/VertexManager.h"
#include "VideoBackends/Vulkan/VideoBackend.h"
#include "VideoBackends/Vulkan/VulkanImports.h"

#include "VideoCommon/BPStructs.h"
#include "VideoCommon/CommandProcessor.h"
#include "VideoCommon/Fifo.h"
#include "VideoCommon/IndexGenerator.h"
#include "VideoCommon/OnScreenDisplay.h"
#include "VideoCommon/OpcodeDecoding.h"
#include "VideoCommon/PixelEngine.h"
#include "VideoCommon/PixelShaderManager.h"
#include "VideoCommon/VertexLoaderManager.h"
#include "VideoCommon/VertexShaderManager.h"
#include "VideoCommon/VideoBackendBase.h"
#include "VideoCommon/VideoConfig.h"

namespace Vulkan
{

// Can we do anything about these globals?
static VkInstance s_vkInstance;
static VkPhysicalDevice s_vkPhysicalDevice;
static VkDevice s_vkDevice;
static VkSurfaceKHR s_vkSurface;
static std::unique_ptr<CommandBufferManager> s_command_buffer_mgr;
static std::unique_ptr<ObjectCache> s_object_cache;
static std::unique_ptr<SwapChain> s_swap_chain;
static std::unique_ptr<StateTracker> s_state_tracker;

void VideoBackend::InitBackendInfo()
{
	g_Config.backend_info.APIType = API_VULKAN;
	g_Config.backend_info.bSupportsExclusiveFullscreen = false;
	g_Config.backend_info.bSupportsDualSourceBlend = true;
	g_Config.backend_info.bSupportsEarlyZ = true;
	g_Config.backend_info.bSupportsPrimitiveRestart = true;
	g_Config.backend_info.bSupportsOversizedViewports = true;
	g_Config.backend_info.bSupportsGeometryShaders = true;
	g_Config.backend_info.bSupports3DVision = false;
	g_Config.backend_info.bSupportsPostProcessing = false;
	g_Config.backend_info.bSupportsPaletteConversion = true;
	g_Config.backend_info.bSupportsClipControl = false;
	g_Config.backend_info.bSupportsBindingLayout = true;
	g_Config.backend_info.bSupportsPrimitiveRestart = true;

	// aamodes
	g_Config.backend_info.AAModes = {1};
	g_Config.backend_info.Adapters.clear();

	// TODO: error handling
	if (LoadVulkanLibrary())
	{
		VkInstance temp_instance = CreateVulkanInstance();
		if (temp_instance)
		{
			std::vector<VkPhysicalDevice> devices = EnumerateVulkanPhysicalDevices(temp_instance);
			for (VkPhysicalDevice device : devices)
			{
				VkPhysicalDeviceProperties properties;
				vkGetPhysicalDeviceProperties(device, &properties);
				g_Config.backend_info.Adapters.push_back(properties.deviceName);
			}

			vkDestroyInstance(temp_instance, nullptr);
		}
		UnloadVulkanLibrary();
	}
}

bool VideoBackend::Initialize(void* window_handle)
{
	if (!LoadVulkanLibrary())
	{
		PanicAlert("Failed to load vulkan library.");
		return false;
	}

	InitBackendInfo();
	InitializeShared();

	// Create vulkan instance
	s_vkInstance = CreateVulkanInstance();
	if (!s_vkInstance)
	{
		PanicAlert("Failed to create vulkan instance.");
		goto CLEANUP_LIBRARY;
	}

	// Create vulkan surface
	s_vkSurface = CreateVulkanSurface(s_vkInstance, window_handle);
	if (!s_vkSurface)
	{
		PanicAlert("Failed to create vulkan surface.");
		goto CLEANUP_INSTANCE;
	}

	s_vkPhysicalDevice = SelectVulkanPhysicalDevice(s_vkInstance, g_ActiveConfig.iAdapter);
	if (!s_vkPhysicalDevice)
	{
		PanicAlert("Failed to select an appropriate vulkan physical device");
		goto CLEANUP_INSTANCE;
	}

	// Create vulkan device and grab queues
	uint32_t graphics_queue_family_index, present_queue_family_index;
	VkQueue graphics_queue, present_queue;
	s_vkDevice = CreateVulkanDevice(s_vkPhysicalDevice, s_vkSurface, &graphics_queue_family_index, &graphics_queue, &present_queue_family_index, &present_queue);
	if (!s_vkDevice)
	{
		PanicAlert("Failed to create vulkan device");
		goto CLEANUP_SURFACE;
	}

	// create command buffers
	s_command_buffer_mgr = std::make_unique<CommandBufferManager>(s_vkDevice, graphics_queue_family_index, graphics_queue);
	if (!s_command_buffer_mgr->Initialize())
	{
		PanicAlert("Failed to create vulkan command buffers");
		goto CLEANUP_DEVICE;
	}

	// create object cache
	s_object_cache = std::make_unique<ObjectCache>(s_vkInstance, s_vkPhysicalDevice, s_vkDevice, s_command_buffer_mgr.get());
	if (!s_object_cache->Initialize())
	{
		PanicAlert("Failed to create vulkan object cache");
		goto CLEANUP_DEVICE;
	}

	// create swap chain and buffers
	s_swap_chain = std::make_unique<SwapChain>();
	if (!s_swap_chain->Initialize(s_vkPhysicalDevice, s_vkDevice, s_vkSurface, present_queue, s_command_buffer_mgr->GetCurrentCommandBuffer()))
	{
		PanicAlert("Failed to create vulkan swap chain");
		goto CLEANUP_DEVICE;
	}

	return true;

CLEANUP_DEVICE:
	s_command_buffer_mgr.reset();
	s_object_cache.reset();
	s_swap_chain.reset();

	vkDestroyDevice(s_vkDevice, nullptr);
	s_vkDevice = nullptr;

CLEANUP_SURFACE:
	vkDestroySurfaceKHR(s_vkInstance, s_vkSurface, nullptr);
	s_vkSurface = nullptr;

CLEANUP_INSTANCE:
	vkDestroyInstance(s_vkInstance, nullptr);
	s_vkInstance = nullptr;

CLEANUP_LIBRARY:
	UnloadVulkanLibrary();
	return false;
}

// This is called after Initialize() from the Core
// Run from the graphics thread
void VideoBackend::Video_Prepare()
{
	s_state_tracker = std::make_unique<StateTracker>(s_object_cache.get(), s_command_buffer_mgr.get());
	g_vertex_manager = std::make_unique<VertexManager>(s_object_cache.get(), s_command_buffer_mgr.get(), s_state_tracker.get());
	g_perf_query = std::make_unique<PerfQuery>();
	g_framebuffer_manager = std::make_unique<FramebufferManager>(s_object_cache.get(), s_command_buffer_mgr.get());
	g_texture_cache = std::make_unique<TextureCache>(s_object_cache.get(), s_command_buffer_mgr.get(), s_state_tracker.get());
	g_renderer = std::make_unique<Renderer>(s_object_cache.get(), s_command_buffer_mgr.get(), s_swap_chain.get(), s_state_tracker.get());
}

void VideoBackend::Shutdown()
{
	s_swap_chain.reset();
	s_object_cache.reset();
	s_command_buffer_mgr.reset();

	vkDestroyDevice(s_vkDevice, nullptr);
	s_vkDevice = nullptr;

	vkDestroySurfaceKHR(s_vkInstance, s_vkSurface, nullptr);
	s_vkSurface = nullptr;

	vkDestroyInstance(s_vkInstance, nullptr);
	s_vkInstance = nullptr;

	UnloadVulkanLibrary();

	ShutdownShared();
}

void VideoBackend::Video_Cleanup()
{
	vkDeviceWaitIdle(s_vkDevice);

	g_texture_cache.reset();
	g_perf_query.reset();
	g_vertex_manager.reset();
	g_renderer.reset();
	g_framebuffer_manager.reset();

	s_state_tracker.reset();

	CleanupShared();
}

}
