// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoCommon/FramebufferManagerBase.h"

#include <algorithm>
#include <array>
#include <memory>
#include <tuple>

#include "VideoCommon/RenderBase.h"
#include "VideoCommon/VideoConfig.h"

std::unique_ptr<FramebufferManagerBase> g_framebuffer_manager;

unsigned int FramebufferManagerBase::m_EFBLayers = 1;

FramebufferManagerBase::~FramebufferManagerBase() = default;

