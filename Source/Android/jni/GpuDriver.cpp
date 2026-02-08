// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

// Based on: Skyline Emulator Project
// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <jni.h>

#include "Common/EnumUtils.h"
#include "Common/IniFile.h"
#include "jni/AndroidCommon/AndroidCommon.h"
#include "jni/AndroidCommon/IDCache.h"

#include <dlfcn.h>
#include <fcntl.h>
#include <jni.h>
#include <unistd.h>
#include "adrenotools/driver.h"

#include "VideoBackends/Vulkan/VulkanContext.h"
#include "VideoBackends/Vulkan/VulkanLoader.h"

extern "C" {

#if defined(_M_ARM_64)

JNIEXPORT jobjectArray JNICALL
Java_org_dolphinemu_dolphinemu_utils_GpuDriverHelper_00024Companion_getSystemDriverInfo(JNIEnv* env,
                                                                                        jobject)
{
  if (!Vulkan::LoadVulkanLibrary(true))
  {
    return nullptr;
  }

  u32 vk_api_version = 0;
  VkInstance instance = Vulkan::VulkanContext::CreateVulkanInstance(WindowSystemType::Headless,
                                                                    false, false, &vk_api_version);
  if (!instance)
  {
    return nullptr;
  }

  if (!Vulkan::LoadVulkanInstanceFunctions(instance))
  {
    vkDestroyInstance(instance, nullptr);
    return nullptr;
  }

  Vulkan::VulkanContext::GPUList gpu_list = Vulkan::VulkanContext::EnumerateGPUs(instance);

  if (gpu_list.empty())
  {
    vkDestroyInstance(instance, nullptr);
    Vulkan::UnloadVulkanLibrary();
    return nullptr;
  }

  VkPhysicalDeviceProperties properties;
  vkGetPhysicalDeviceProperties(gpu_list.front(), &properties);

  std::string driverId;
  if (vkGetPhysicalDeviceProperties2 && vk_api_version >= VK_VERSION_1_1)
  {
    VkPhysicalDeviceDriverProperties driverProperties;
    driverProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES;
    driverProperties.pNext = nullptr;
    VkPhysicalDeviceProperties2 properties2;
    properties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    properties2.pNext = &driverProperties;
    vkGetPhysicalDeviceProperties2(gpu_list.front(), &properties2);
    driverId = fmt::format("{}", Common::ToUnderlying(driverProperties.driverID));
  }
  else
  {
    driverId = "Unknown";
  }

  std::string driverVersion =
      fmt::format("{}.{}.{}", VK_API_VERSION_MAJOR(properties.driverVersion),
                  VK_API_VERSION_MINOR(properties.driverVersion),
                  VK_API_VERSION_PATCH(properties.driverVersion));

  vkDestroyInstance(instance, nullptr);
  Vulkan::UnloadVulkanLibrary();

  auto array = env->NewObjectArray(2, env->FindClass("java/lang/String"), nullptr);
  env->SetObjectArrayElement(array, 0, ToJString(env, driverId));
  env->SetObjectArrayElement(array, 1, ToJString(env, driverVersion));
  return array;
}

JNIEXPORT jboolean JNICALL
Java_org_dolphinemu_dolphinemu_utils_GpuDriverHelper_00024Companion_supportsCustomDriverLoading(
    JNIEnv* env, jobject instance)
{
  // If the KGSL device exists custom drivers can be loaded using adrenotools
  return Vulkan::SupportsCustomDriver();
}

JNIEXPORT jboolean JNICALL
Java_org_dolphinemu_dolphinemu_utils_GpuDriverHelper_00024Companion_supportsForceMaxGpuClocks(
    JNIEnv* env, jobject instance)
{
  // If the KGSL device exists adrenotools can be used to set GPU turbo mode
  return Vulkan::SupportsCustomDriver();
}

JNIEXPORT void JNICALL
Java_org_dolphinemu_dolphinemu_utils_GpuDriverHelper_00024Companion_forceMaxGpuClocks(
    JNIEnv* env, jobject instance, jboolean enable)
{
  adrenotools_set_turbo(enable);
}

#else

JNIEXPORT jobjectArray JNICALL
Java_org_dolphinemu_dolphinemu_utils_GpuDriverHelper_00024Companion_getSystemDriverInfo(
    JNIEnv* env, jobject instance)
{
  auto array = env->NewObjectArray(0, env->FindClass("java/lang/String"), nullptr);
  return array;
}

JNIEXPORT jboolean JNICALL
Java_org_dolphinemu_dolphinemu_utils_GpuDriverHelper_00024Companion_supportsCustomDriverLoading(
    JNIEnv* env, jobject instance)
{
  return false;
}

JNIEXPORT jboolean JNICALL
Java_org_dolphinemu_dolphinemu_utils_GpuDriverHelper_00024Companion_supportsForceMaxGpuClocks(
    JNIEnv* env, jobject instance)
{
  return false;
}

JNIEXPORT void JNICALL
Java_org_dolphinemu_dolphinemu_utils_GpuDriverHelper_00024Companion_forceMaxGpuClocks(
    JNIEnv* env, jobject instance, jboolean enable)
{
}

#endif
}
