// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "UICommon/VideoBackends.h"

// OpenGL is not available on Windows-on-ARM64
#if !defined(_WIN32) || !defined(_M_ARM64)
#define HAS_OPENGL 1
#endif

#ifdef _WIN32
#include "VideoBackends/D3D/VideoBackend.h"
#include "VideoBackends/D3D12/VideoBackend.h"
#endif
#include "VideoBackends/Null/VideoBackend.h"
#ifdef HAS_OPENGL
#include "VideoBackends/OGL/VideoBackend.h"
#include "VideoBackends/Software/VideoBackend.h"
#endif
#include "VideoBackends/Vulkan/VideoBackend.h"

#include <algorithm>

namespace UICommon {

// This function has to exit outside of videocommon to prevent circular dependencies
void RegisterVideoBackends()
{
  // OGL > D3D11 > Vulkan > SW > Null
#ifdef HAS_OPENGL
  VideoBackendBase::RegisterBackend(std::make_unique<OGL::VideoBackend>());
#endif
#ifdef _WIN32
  VideoBackendBase::RegisterBackend(std::make_unique<DX11::VideoBackend>());
  VideoBackendBase::RegisterBackend(std::make_unique<DX12::VideoBackend>());
#endif
  VideoBackendBase::RegisterBackend(std::make_unique<Vulkan::VideoBackend>());
#ifdef HAS_OPENGL
  VideoBackendBase::RegisterBackend(std::make_unique<SW::VideoSoftware>());
#endif
  VideoBackendBase::RegisterBackend(std::make_unique<Null::VideoBackend>());
}

}