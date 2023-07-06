// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/CommonTypes.h"
#include "VideoBackends/Vulkan/VulkanLoader.h"

namespace Vulkan
{

void StartPresentWaitThread();
void StopPresentWaitThread();

void PresentQueued(u64 present_id, VkSwapchainKHR swapchain, bool vsync);
void FlushPresentWaitQueue();

}  // namespace Vulkan
