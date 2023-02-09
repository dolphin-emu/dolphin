// Copyright 2011 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/VideoBackendBase.h"

#include <algorithm>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include <fmt/format.h>

#include "Common/CommonTypes.h"
#include "Common/Event.h"
#include "Common/FileUtil.h"
#include "Common/Logging/Log.h"

#include "Core/Config/MainSettings.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/System.h"

#include "VideoCommon/RenderBase.h"
#include "VideoCommon/TextureCacheBase.h"
#include "VideoCommon/VideoConfig.h"

// TODO: ugly
#ifdef _WIN32
#include "VideoBackends/D3D/VideoBackend.h"
#include "VideoBackends/D3D12/VideoBackend.h"
#endif
#include "VideoBackends/Null/VideoBackend.h"
#ifdef HAS_OPENGL
#include "VideoBackends/OGL/VideoBackend.h"
#include "VideoBackends/Software/VideoBackend.h"
#endif
#ifdef HAS_VULKAN
#include "VideoBackends/Vulkan/VideoBackend.h"
#endif
#ifdef __APPLE__
#include "VideoBackends/Metal/VideoBackend.h"
#endif

std::string VideoBackendBase::BadShaderFilename(const char* shader_stage, int counter) const
{
  return fmt::format("{}bad_{}_{}_{}.txt", File::GetUserPath(D_DUMP_IDX), shader_stage, GetName(),
                     counter);
}

static VideoBackendBase* GetDefaultVideoBackend()
{
  const auto& backends = VideoBackendBase::GetAvailableBackends();
  if (backends.empty())
    return nullptr;
  return backends.front().get();
}

std::string VideoBackendBase::GetDefaultBackendName()
{
  auto* default_backend = GetDefaultVideoBackend();
  return default_backend ? default_backend->GetName() : "";
}

const std::vector<std::unique_ptr<VideoBackendBase>>& VideoBackendBase::GetAvailableBackends()
{
  static auto s_available_backends = [] {
    std::vector<std::unique_ptr<VideoBackendBase>> backends;

    // OGL > D3D11 > D3D12 > Vulkan > SW > Null
    // On macOS, we prefer Vulkan over OpenGL due to OpenGL support being deprecated by Apple.
#ifdef HAS_OPENGL
    backends.push_back(std::make_unique<OGL::VideoBackend>());
#endif
#ifdef _WIN32
    backends.push_back(std::make_unique<DX11::VideoBackend>());
    backends.push_back(std::make_unique<DX12::VideoBackend>());
#endif
#ifdef __APPLE__
    backends.push_back(std::make_unique<Metal::VideoBackend>());
#endif
#ifdef HAS_VULKAN
#ifdef __APPLE__
    // Emplace the Vulkan backend at the beginning so it takes precedence over OpenGL.
    backends.emplace(backends.begin(), std::make_unique<Vulkan::VideoBackend>());
#else
    backends.push_back(std::make_unique<Vulkan::VideoBackend>());
#endif
#endif
#ifdef HAS_OPENGL
    backends.push_back(std::make_unique<SW::VideoSoftware>());
#endif
    backends.push_back(std::make_unique<Null::VideoBackend>());

    return backends;
  }();
  return s_available_backends;
}

std::unique_ptr<Renderer> VideoBackendBase::CreateRenderer(AbstractGfx* gfx)
{
  return std::make_unique<Renderer>();
}

std::unique_ptr<TextureCacheBase> VideoBackendBase::CreateTextureCache(AbstractGfx* gfx)
{
  return std::make_unique<TextureCacheBase>();
}

VideoBackendBase* VideoBackendBase::GetConfiguredBackend()
{
  std::string name = Config::Get(Config::MAIN_GFX_BACKEND);
  // If empty, set it to the default backend (expected behavior)
  if (name.empty())
    return GetDefaultVideoBackend();

  const auto& backends = GetAvailableBackends();
  const auto iter = std::find_if(backends.begin(), backends.end(), [&name](const auto& backend) {
    return name == backend->GetName();
  });

  if (iter == backends.end())
    return GetDefaultVideoBackend();

  return iter->get();
}

void VideoBackendBase::PopulateBackendInfo()
{
  g_Config.Refresh();
  // Reset backend_info so if the backend forgets to initialize something it doesn't end up using
  // a value from the previously used renderer
  backend_info = {};

  backend_info.DisplayName = GetDisplayName();
  InitBackendInfo();
  // We validate the config after initializing the backend info, as system-specific settings
  // such as anti-aliasing, or the selected adapter may be invalid, and should be checked.
  g_Config.VerifyValidity(backend_info);
}

void VideoBackendBase::PopulateBackendInfoFromUI()
{
  // If the core is running, the backend info will have been populated already.
  // If we did it here, the UI thread can race with the with the GPU thread.
  if (!Core::IsRunning())
  {
    auto backend = GetConfiguredBackend();
    backend->PopulateBackendInfo();
  }
}
