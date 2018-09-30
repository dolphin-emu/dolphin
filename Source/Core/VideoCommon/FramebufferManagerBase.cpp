// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoCommon/FramebufferManagerBase.h"
#include <memory>

std::unique_ptr<FramebufferManagerBase> g_framebuffer_manager;

unsigned int FramebufferManagerBase::m_EFBLayers = 1;

