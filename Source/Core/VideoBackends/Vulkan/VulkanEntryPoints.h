// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

// Expands the ACTION macro for each function when this file is included.
// Parameters: Function name, is required
// VULKAN_EACH_MODULE_ENTRY_POINT is for functions in vulkan-1.dll
// VULKAN_EACH_INSTANCE_ENTRY_POINT is for instance-specific functions.
// VULKAN_EACH_DEVICE_ENTRY_POINT is for device-specific functions.
// VULKAN_EACH_ENTRY_POINT is for all vulkan functions

#pragma once

#define VULKAN_EACH_MODULE_ENTRY_POINT(ACTION)                                                     \
  ACTION(vkCreateInstance, true)                                                                   \
  ACTION(vkEnumerateInstanceExtensionProperties, true)                                             \
  ACTION(vkEnumerateInstanceLayerProperties, true)                                                 \
  ACTION(vkGetDeviceProcAddr, true)                                                                \
  ACTION(vkGetInstanceProcAddr, true)

#if defined(VK_USE_PLATFORM_WIN32_KHR)
#define VULKAN_EACH_PLATFORM_INSTANCE_ENTRY_POINT(ACTION)                                          \
  ACTION(vkCreateWin32SurfaceKHR, false)                                                           \
  ACTION(vkGetPhysicalDeviceWin32PresentationSupportKHR, false)

#elif defined(VK_USE_PLATFORM_XLIB_KHR)
#define VULKAN_EACH_PLATFORM_INSTANCE_ENTRY_POINT(ACTION)                                          \
  ACTION(vkCreateXlibSurfaceKHR, false)                                                            \
  ACTION(vkGetPhysicalDeviceXlibPresentationSupportKHR, false)

#elif defined(VK_USE_PLATFORM_XCB_KHR)
#define VULKAN_EACH_PLATFORM_INSTANCE_ENTRY_POINT(ACTION)                                          \
  ACTION(vkCreateXcbSurfaceKHR, false)                                                             \
  ACTION(vkGetPhysicalDeviceXcbPresentationSupportKHR, false)

#elif defined(VK_USE_PLATFORM_ANDROID_KHR)
#define VULKAN_EACH_PLATFORM_INSTANCE_ENTRY_POINT(ACTION) ACTION(vkCreateAndroidSurfaceKHR, false)

#else
#define VULKAN_EACH_PLATFORM_INSTANCE_ENTRY_POINT(ACTION)

#endif

#define VULKAN_EACH_INSTANCE_ENTRY_POINT(ACTION)                                                   \
  VULKAN_EACH_PLATFORM_INSTANCE_ENTRY_POINT(ACTION)                                                \
  ACTION(vkAllocateCommandBuffers, true)                                                           \
  ACTION(vkAllocateDescriptorSets, true)                                                           \
  ACTION(vkAllocateMemory, true)                                                                   \
  ACTION(vkBeginCommandBuffer, true)                                                               \
  ACTION(vkBindBufferMemory, true)                                                                 \
  ACTION(vkBindImageMemory, true)                                                                  \
  ACTION(vkCmdBeginQuery, true)                                                                    \
  ACTION(vkCmdBeginRenderPass, true)                                                               \
  ACTION(vkCmdBindDescriptorSets, true)                                                            \
  ACTION(vkCmdBindIndexBuffer, true)                                                               \
  ACTION(vkCmdBindPipeline, true)                                                                  \
  ACTION(vkCmdBindVertexBuffers, true)                                                             \
  ACTION(vkCmdBlitImage, true)                                                                     \
  ACTION(vkCmdClearAttachments, true)                                                              \
  ACTION(vkCmdClearColorImage, true)                                                               \
  ACTION(vkCmdClearDepthStencilImage, true)                                                        \
  ACTION(vkCmdCopyBuffer, true)                                                                    \
  ACTION(vkCmdCopyBufferToImage, true)                                                             \
  ACTION(vkCmdCopyImage, true)                                                                     \
  ACTION(vkCmdCopyImageToBuffer, true)                                                             \
  ACTION(vkCmdCopyQueryPoolResults, true)                                                          \
  ACTION(vkCmdDispatch, true)                                                                      \
  ACTION(vkCmdDispatchIndirect, true)                                                              \
  ACTION(vkCmdDraw, true)                                                                          \
  ACTION(vkCmdDrawIndexed, true)                                                                   \
  ACTION(vkCmdDrawIndexedIndirect, true)                                                           \
  ACTION(vkCmdDrawIndirect, true)                                                                  \
  ACTION(vkCmdEndQuery, true)                                                                      \
  ACTION(vkCmdEndRenderPass, true)                                                                 \
  ACTION(vkCmdExecuteCommands, true)                                                               \
  ACTION(vkCmdFillBuffer, true)                                                                    \
  ACTION(vkCmdNextSubpass, true)                                                                   \
  ACTION(vkCmdPipelineBarrier, true)                                                               \
  ACTION(vkCmdPushConstants, true)                                                                 \
  ACTION(vkCmdResetEvent, true)                                                                    \
  ACTION(vkCmdResetQueryPool, true)                                                                \
  ACTION(vkCmdResolveImage, true)                                                                  \
  ACTION(vkCmdSetBlendConstants, true)                                                             \
  ACTION(vkCmdSetDepthBias, true)                                                                  \
  ACTION(vkCmdSetDepthBounds, true)                                                                \
  ACTION(vkCmdSetEvent, true)                                                                      \
  ACTION(vkCmdSetLineWidth, true)                                                                  \
  ACTION(vkCmdSetScissor, true)                                                                    \
  ACTION(vkCmdSetStencilCompareMask, true)                                                         \
  ACTION(vkCmdSetStencilReference, true)                                                           \
  ACTION(vkCmdSetStencilWriteMask, true)                                                           \
  ACTION(vkCmdSetViewport, true)                                                                   \
  ACTION(vkCmdUpdateBuffer, true)                                                                  \
  ACTION(vkCmdWaitEvents, true)                                                                    \
  ACTION(vkCmdWriteTimestamp, true)                                                                \
  ACTION(vkCreateBuffer, true)                                                                     \
  ACTION(vkCreateBufferView, true)                                                                 \
  ACTION(vkCreateCommandPool, true)                                                                \
  ACTION(vkCreateComputePipelines, true)                                                           \
  ACTION(vkCreateDebugReportCallbackEXT, false)                                                    \
  ACTION(vkCreateDescriptorPool, true)                                                             \
  ACTION(vkCreateDescriptorSetLayout, true)                                                        \
  ACTION(vkCreateDevice, true)                                                                     \
  ACTION(vkCreateEvent, true)                                                                      \
  ACTION(vkCreateFence, true)                                                                      \
  ACTION(vkCreateFramebuffer, true)                                                                \
  ACTION(vkCreateGraphicsPipelines, true)                                                          \
  ACTION(vkCreateImage, true)                                                                      \
  ACTION(vkCreateImageView, true)                                                                  \
  ACTION(vkCreatePipelineCache, true)                                                              \
  ACTION(vkCreatePipelineLayout, true)                                                             \
  ACTION(vkCreateQueryPool, true)                                                                  \
  ACTION(vkCreateRenderPass, true)                                                                 \
  ACTION(vkCreateSampler, true)                                                                    \
  ACTION(vkCreateSemaphore, true)                                                                  \
  ACTION(vkCreateShaderModule, true)                                                               \
  ACTION(vkDebugReportMessageEXT, false)                                                           \
  ACTION(vkDestroyBuffer, true)                                                                    \
  ACTION(vkDestroyBufferView, true)                                                                \
  ACTION(vkDestroyCommandPool, true)                                                               \
  ACTION(vkDestroyDebugReportCallbackEXT, false)                                                   \
  ACTION(vkDestroyDescriptorPool, true)                                                            \
  ACTION(vkDestroyDescriptorSetLayout, true)                                                       \
  ACTION(vkDestroyDevice, true)                                                                    \
  ACTION(vkDestroyEvent, true)                                                                     \
  ACTION(vkDestroyFence, true)                                                                     \
  ACTION(vkDestroyFramebuffer, true)                                                               \
  ACTION(vkDestroyImage, true)                                                                     \
  ACTION(vkDestroyImageView, true)                                                                 \
  ACTION(vkDestroyInstance, true)                                                                  \
  ACTION(vkDestroyPipeline, true)                                                                  \
  ACTION(vkDestroyPipelineCache, true)                                                             \
  ACTION(vkDestroyPipelineLayout, true)                                                            \
  ACTION(vkDestroyQueryPool, true)                                                                 \
  ACTION(vkDestroyRenderPass, true)                                                                \
  ACTION(vkDestroySampler, true)                                                                   \
  ACTION(vkDestroySemaphore, true)                                                                 \
  ACTION(vkDestroyShaderModule, true)                                                              \
  ACTION(vkDestroySurfaceKHR, false)                                                               \
  ACTION(vkDeviceWaitIdle, true)                                                                   \
  ACTION(vkEndCommandBuffer, true)                                                                 \
  ACTION(vkEnumerateDeviceExtensionProperties, true)                                               \
  ACTION(vkEnumerateDeviceLayerProperties, true)                                                   \
  ACTION(vkEnumeratePhysicalDevices, true)                                                         \
  ACTION(vkFlushMappedMemoryRanges, true)                                                          \
  ACTION(vkFreeCommandBuffers, true)                                                               \
  ACTION(vkFreeDescriptorSets, true)                                                               \
  ACTION(vkFreeMemory, true)                                                                       \
  ACTION(vkGetBufferMemoryRequirements, true)                                                      \
  ACTION(vkGetDeviceMemoryCommitment, true)                                                        \
  ACTION(vkGetDeviceQueue, true)                                                                   \
  ACTION(vkGetEventStatus, true)                                                                   \
  ACTION(vkGetFenceStatus, true)                                                                   \
  ACTION(vkGetImageMemoryRequirements, true)                                                       \
  ACTION(vkGetImageSparseMemoryRequirements, true)                                                 \
  ACTION(vkGetImageSubresourceLayout, true)                                                        \
  ACTION(vkGetPhysicalDeviceFeatures, true)                                                        \
  ACTION(vkGetPhysicalDeviceFormatProperties, true)                                                \
  ACTION(vkGetPhysicalDeviceImageFormatProperties, true)                                           \
  ACTION(vkGetPhysicalDeviceMemoryProperties, true)                                                \
  ACTION(vkGetPhysicalDeviceProperties, true)                                                      \
  ACTION(vkGetPhysicalDeviceQueueFamilyProperties, true)                                           \
  ACTION(vkGetPhysicalDeviceSparseImageFormatProperties, true)                                     \
  ACTION(vkGetPhysicalDeviceSurfaceCapabilitiesKHR, false)                                         \
  ACTION(vkGetPhysicalDeviceSurfaceFormatsKHR, false)                                              \
  ACTION(vkGetPhysicalDeviceSurfacePresentModesKHR, false)                                         \
  ACTION(vkGetPhysicalDeviceSurfaceSupportKHR, false)                                              \
  ACTION(vkGetPipelineCacheData, true)                                                             \
  ACTION(vkGetQueryPoolResults, true)                                                              \
  ACTION(vkGetRenderAreaGranularity, true)                                                         \
  ACTION(vkInvalidateMappedMemoryRanges, true)                                                     \
  ACTION(vkMapMemory, true)                                                                        \
  ACTION(vkMergePipelineCaches, true)                                                              \
  ACTION(vkQueueBindSparse, true)                                                                  \
  ACTION(vkQueueSubmit, true)                                                                      \
  ACTION(vkQueueWaitIdle, true)                                                                    \
  ACTION(vkResetCommandBuffer, true)                                                               \
  ACTION(vkResetCommandPool, true)                                                                 \
  ACTION(vkResetDescriptorPool, true)                                                              \
  ACTION(vkResetEvent, true)                                                                       \
  ACTION(vkResetFences, true)                                                                      \
  ACTION(vkSetEvent, true)                                                                         \
  ACTION(vkUnmapMemory, true)                                                                      \
  ACTION(vkUpdateDescriptorSets, true)                                                             \
  ACTION(vkWaitForFences, true)

#define VULKAN_EACH_DEVICE_ENTRY_POINT(ACTION)                                                     \
  ACTION(vkAcquireNextImageKHR, false)                                                             \
  ACTION(vkCreateSwapchainKHR, false)                                                              \
  ACTION(vkDestroySwapchainKHR, false)                                                             \
  ACTION(vkGetSwapchainImagesKHR, false)                                                           \
  ACTION(vkQueuePresentKHR, false)

#define VULKAN_EACH_ENTRY_POINT(ACTION)                                                            \
  VULKAN_EACH_MODULE_ENTRY_POINT(ACTION)                                                           \
  VULKAN_EACH_INSTANCE_ENTRY_POINT(ACTION)                                                         \
  VULKAN_EACH_DEVICE_ENTRY_POINT(ACTION)
