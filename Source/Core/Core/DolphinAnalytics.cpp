// Copyright 2016 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/DolphinAnalytics.h"

#include <array>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include <fmt/format.h>
#include "Core/Config/GraphicsSettings.h"

#if defined(_WIN32)
#include <Windows.h>
#include "Common/WindowsRegistry.h"
#elif defined(__APPLE__)
#include <objc/message.h>
#elif defined(ANDROID)
#include <functional>
#include "Common/AndroidAnalytics.h"
#endif

#include "Common/Analytics.h"
#include "Common/CPUDetect.h"
#include "Common/CommonTypes.h"
#include "Common/Config/Config.h"
#include "Common/Crypto/SHA1.h"
#include "Common/EnumUtils.h"
#include "Common/Random.h"
#include "Common/Timer.h"
#include "Common/Version.h"
#include "Core/Config/MainSettings.h"
#include "Core/ConfigManager.h"
#include "Core/HW/GCPad.h"
#include "Core/Movie.h"
#include "Core/NetPlayProto.h"
#include "Core/System.h"
#include "InputCommon/GCAdapter.h"
#include "InputCommon/InputConfig.h"
#include "VideoCommon/VideoBackendBase.h"
#include "VideoCommon/VideoConfig.h"

namespace
{
constexpr char ANALYTICS_ENDPOINT[] = "https://analytics.dolphin-emu.org/report";
}  // namespace

#if defined(ANDROID)
static std::function<std::string(std::string)> s_get_val_func;
void DolphinAnalytics::AndroidSetGetValFunc(std::function<std::string(std::string)> func)
{
  s_get_val_func = std::move(func);
}
#endif

DolphinAnalytics::DolphinAnalytics()
{
  ReloadConfig();
  MakeBaseBuilder();
}

DolphinAnalytics& DolphinAnalytics::Instance()
{
  static DolphinAnalytics instance;
  return instance;
}

void DolphinAnalytics::ReloadConfig()
{
  std::lock_guard lk{m_reporter_mutex};

  // Install the HTTP backend if analytics support is enabled.
  std::unique_ptr<Common::AnalyticsReportingBackend> new_backend;
  if (Config::Get(Config::MAIN_ANALYTICS_ENABLED))
  {
#if defined(ANDROID)
    new_backend = std::make_unique<Common::AndroidAnalyticsBackend>(ANALYTICS_ENDPOINT);
#else
    new_backend = std::make_unique<Common::HttpAnalyticsBackend>(ANALYTICS_ENDPOINT);
#endif
  }
  m_reporter.SetBackend(std::move(new_backend));

  // Load the unique ID or generate it if needed.
  m_unique_id = Config::Get(Config::MAIN_ANALYTICS_ID);
  if (m_unique_id.empty())
  {
    GenerateNewIdentity();
  }
}

void DolphinAnalytics::GenerateNewIdentity()
{
  const u64 id_high = Common::Random::GenerateValue<u64>();
  const u64 id_low = Common::Random::GenerateValue<u64>();
  m_unique_id = fmt::format("{:016x}{:016x}", id_high, id_low);

  // Save the new id in the configuration.
  Config::SetBase(Config::MAIN_ANALYTICS_ID, m_unique_id);
  Config::Save();
}

std::string DolphinAnalytics::MakeUniqueId(std::string_view data) const
{
  const auto input = std::string{m_unique_id}.append(data);
  const auto digest = Common::SHA1::CalculateDigest(input);

  // Convert to hex string and truncate to 64 bits.
  std::string out;
  for (int i = 0; i < 8; ++i)
  {
    out += fmt::format("{:02x}", digest[i]);
  }
  return out;
}

void DolphinAnalytics::ReportDolphinStart(std::string_view ui_type)
{
  Common::AnalyticsReportBuilder builder(m_base_builder);
  builder.AddData("type", "dolphin-start");
  builder.AddData("ui-type", ui_type);
  builder.AddData("id", MakeUniqueId("dolphin-start"));
  Send(builder);
}

void DolphinAnalytics::ReportGameStart()
{
  MakePerGameBuilder();

  Common::AnalyticsReportBuilder builder(m_per_game_builder);
  builder.AddData("type", "game-start");
  Send(builder);

  // Reset per-game state.
  m_reported_quirks.fill(false);
  InitializePerformanceSampling();
}

// Keep in sync with enum class GameQuirk definition.
constexpr std::array GAME_QUIRKS_NAMES{
    "uses-DVDLowStopLaser",
    "uses-DVDLowOffset",
    "uses-DVDLowReadDiskBca",
    "uses-DVDLowRequestDiscStatus",
    "uses-DVDLowRequestRetryNumber",
    "uses-DVDLowSerMeasControl",
    "uses-different-partition-command",
    "uses-di-interrupt-command",
    "mismatched-gpu-texgens-between-xf-and-bp",
    "mismatched-gpu-colors-between-xf-and-bp",
    "uses-uncommon-wd-mode",
    "uses-wd-unimplemented-ioctl",
    "uses-unknown-bp-command",
    "uses-unknown-cp-command",
    "uses-unknown-xf-command",
    "uses-maybe-invalid-cp-command",
    "uses-cp-perf-command",
    "uses-unimplemented-ax-command",
    "uses-ax-initial-time-delay",
    "uses-ax-wiimote-lowpass",
    "uses-ax-wiimote-biquad",
    "sets-xf-clipdisable-bit-0",
    "sets-xf-clipdisable-bit-1",
    "sets-xf-clipdisable-bit-2",
    "mismatched-gpu-colors-between-cp-and-xf",
    "mismatched-gpu-normals-between-cp-and-xf",
    "mismatched-gpu-tex-coords-between-cp-and-xf",
    "mismatched-gpu-matrix-indices-between-cp-and-xf",
    "reads-bounding-box",
    "invalid-position-component-format",
    "invalid-normal-component-format",
    "invalid-texture-coordinate-component-format",
    "invalid-color-component-format",
};
static_assert(GAME_QUIRKS_NAMES.size() == static_cast<u32>(GameQuirk::COUNT),
              "Game quirks names and enum definition are out of sync.");

void DolphinAnalytics::ReportGameQuirk(GameQuirk quirk)
{
  u32 quirk_idx = static_cast<u32>(quirk);

  // Only report once per run.
  if (m_reported_quirks[quirk_idx])
    return;
  m_reported_quirks[quirk_idx] = true;

  Common::AnalyticsReportBuilder builder(m_per_game_builder);
  builder.AddData("type", "quirk");
  builder.AddData("quirk", GAME_QUIRKS_NAMES[quirk_idx]);
  Send(builder);
}

void DolphinAnalytics::ReportPerformanceInfo(PerformanceSample&& sample)
{
  if (ShouldStartPerformanceSampling())
  {
    m_sampling_performance_info = true;
  }

  if (m_sampling_performance_info)
  {
    m_performance_samples.emplace_back(std::move(sample));
  }

  if (m_performance_samples.size() >= NUM_PERFORMANCE_SAMPLES_PER_REPORT)
  {
    std::vector<u32> speed_times_1000(m_performance_samples.size());
    std::vector<u32> num_prims(m_performance_samples.size());
    std::vector<u32> num_draw_calls(m_performance_samples.size());
    for (size_t i = 0; i < m_performance_samples.size(); ++i)
    {
      speed_times_1000[i] = static_cast<u32>(m_performance_samples[i].speed_ratio * 1000);
      num_prims[i] = m_performance_samples[i].num_prims;
      num_draw_calls[i] = m_performance_samples[i].num_draw_calls;
    }

    // The per game builder should already exist -- there is no way we can be reporting performance
    // info without a game start event having been generated.
    Common::AnalyticsReportBuilder builder(m_per_game_builder);
    builder.AddData("type", "performance");
    builder.AddData("speed", speed_times_1000);
    builder.AddData("prims", num_prims);
    builder.AddData("draw-calls", num_draw_calls);

    Send(builder);

    // Clear up and stop sampling until next time ShouldStartPerformanceSampling() says so.
    m_performance_samples.clear();
    m_sampling_performance_info = false;
  }
}

void DolphinAnalytics::InitializePerformanceSampling()
{
  m_performance_samples.clear();
  m_sampling_performance_info = false;

  u64 wait_us =
      PERFORMANCE_SAMPLING_INITIAL_WAIT_TIME_SECS * 1000000 +
      Common::Random::GenerateValue<u64>() % (PERFORMANCE_SAMPLING_WAIT_TIME_JITTER_SECS * 1000000);
  m_sampling_next_start_us = Common::Timer::NowUs() + wait_us;
}

bool DolphinAnalytics::ShouldStartPerformanceSampling()
{
  if (Common::Timer::NowUs() < m_sampling_next_start_us)
    return false;

  u64 wait_us =
      PERFORMANCE_SAMPLING_INTERVAL_SECS * 1000000 +
      Common::Random::GenerateValue<u64>() % (PERFORMANCE_SAMPLING_WAIT_TIME_JITTER_SECS * 1000000);
  m_sampling_next_start_us = Common::Timer::NowUs() + wait_us;
  return true;
}

void DolphinAnalytics::MakeBaseBuilder()
{
  Common::AnalyticsReportBuilder builder;

  // Version information.
  builder.AddData("version-desc", Common::GetScmDescStr());
  builder.AddData("version-hash", Common::GetScmRevGitStr());
  builder.AddData("version-branch", Common::GetScmBranchStr());
  builder.AddData("version-dist", Common::GetScmDistributorStr());

  // Auto-Update information.
  builder.AddData("update-track", Config::Get(Config::MAIN_AUTOUPDATE_UPDATE_TRACK));

  // CPU information.
  builder.AddData("cpu-summary", cpu_info.Summarize());

// OS information.
#if defined(_WIN32)
  builder.AddData("os-type", "windows");

  const auto winver = WindowsRegistry::GetOSVersion();
  builder.AddData("win-ver-major", static_cast<u32>(winver.dwMajorVersion));
  builder.AddData("win-ver-minor", static_cast<u32>(winver.dwMinorVersion));
  builder.AddData("win-ver-build", static_cast<u32>(winver.dwBuildNumber));
#elif defined(ANDROID)
  builder.AddData("os-type", "android");
  builder.AddData("android-manufacturer", s_get_val_func("DEVICE_MANUFACTURER"));
  builder.AddData("android-model", s_get_val_func("DEVICE_MODEL"));
  builder.AddData("android-version", s_get_val_func("DEVICE_OS"));
#elif defined(__APPLE__)
  builder.AddData("os-type", "osx");

  // id processInfo = [NSProcessInfo processInfo]
  id processInfo = reinterpret_cast<id (*)(Class, SEL)>(objc_msgSend)(
      objc_getClass("NSProcessInfo"), sel_getUid("processInfo"));
  if (processInfo)
  {
    struct OSVersion  // NSOperatingSystemVersion
    {
      s64 major_version;  // NSInteger majorVersion
      s64 minor_version;  // NSInteger minorVersion
      s64 patch_version;  // NSInteger patchVersion
    };
    // Under arm64, we need to call objc_msgSend to receive a struct.
    // On x86_64, we need to explicitly call objc_msgSend_stret for a struct.
#ifdef _M_ARM_64
#define msgSend objc_msgSend
#else
#define msgSend objc_msgSend_stret
#endif
    // NSOperatingSystemVersion version = [processInfo operatingSystemVersion]
    OSVersion version = reinterpret_cast<OSVersion (*)(id, SEL)>(msgSend)(
        processInfo, sel_getUid("operatingSystemVersion"));
#undef msgSend
    builder.AddData("osx-ver-major", version.major_version);
    builder.AddData("osx-ver-minor", version.minor_version);
    builder.AddData("osx-ver-bugfix", version.patch_version);
  }
#elif defined(__linux__)
  builder.AddData("os-type", "linux");
#elif defined(__FreeBSD__)
  builder.AddData("os-type", "freebsd");
#elif defined(__OpenBSD__)
  builder.AddData("os-type", "openbsd");
#elif defined(__NetBSD__)
  builder.AddData("os-type", "netbsd");
#elif defined(__HAIKU__)
  builder.AddData("os-type", "haiku");
#else
  builder.AddData("os-type", "unknown");
#endif

  m_base_builder = builder;
}

static const char* GetShaderCompilationMode()
{
  switch (Config::Get(Config::GFX_SHADER_COMPILATION_MODE))
  {
  case ShaderCompilationMode::AsynchronousUberShaders:
    return "async-ubershaders";
  case ShaderCompilationMode::AsynchronousSkipRendering:
    return "async-skip-rendering";
  case ShaderCompilationMode::SynchronousUberShaders:
    return "sync-ubershaders";
  case ShaderCompilationMode::Synchronous:
  default:
    return "sync";
  }
}

static bool UseVertexRounding()
{
  return Config::Get(Config::GFX_HACK_VERTEX_ROUNDING) && Config::Get(Config::GFX_EFB_SCALE) != 1;
}

void DolphinAnalytics::MakePerGameBuilder()
{
  Common::AnalyticsReportBuilder builder(m_base_builder);

  // Gameid.
  builder.AddData("gameid", SConfig::GetInstance().GetGameID());

  // Unique id bound to the gameid.
  builder.AddData("id", MakeUniqueId(SConfig::GetInstance().GetGameID()));

  // Configuration.
  builder.AddData("cfg-dsp-hle", Config::Get(Config::MAIN_DSP_HLE));
  builder.AddData("cfg-dsp-jit", Config::Get(Config::MAIN_DSP_JIT));
  builder.AddData("cfg-dsp-thread", Config::Get(Config::MAIN_DSP_THREAD));
  builder.AddData("cfg-cpu-thread", Config::Get(Config::MAIN_CPU_THREAD));
  builder.AddData("cfg-fastmem", Config::Get(Config::MAIN_FASTMEM));
  builder.AddData("cfg-syncgpu", Config::Get(Config::MAIN_SYNC_GPU));
  builder.AddData("cfg-audio-backend", Config::Get(Config::MAIN_AUDIO_BACKEND));
  builder.AddData("cfg-oc-enable", Config::Get(Config::MAIN_OVERCLOCK_ENABLE));
  builder.AddData("cfg-oc-factor", Config::Get(Config::MAIN_OVERCLOCK));
  builder.AddData("cfg-render-to-main", Config::Get(Config::MAIN_RENDER_TO_MAIN));
  if (g_video_backend)
  {
    builder.AddData("cfg-video-backend", g_video_backend->GetName());
  }

  // Video configuration.
  builder.AddData("cfg-gfx-multisamples", Config::Get(Config::GFX_MSAA));
  builder.AddData("cfg-gfx-ssaa", Config::Get(Config::GFX_SSAA));
  builder.AddData("cfg-gfx-anisotropy", Config::Get(Config::GFX_ENHANCE_MAX_ANISOTROPY));
  builder.AddData("cfg-gfx-vsync", Config::Get(Config::GFX_VSYNC));
  builder.AddData("cfg-gfx-aspect-ratio",
                  Common::ToUnderlying(Config::Get(Config::GFX_ASPECT_RATIO)));
  builder.AddData("cfg-gfx-efb-access", Config::Get(Config::GFX_HACK_EFB_ACCESS_ENABLE));
  builder.AddData("cfg-gfx-efb-copy-format-changes",
                  Config::Get(Config::GFX_HACK_EFB_EMULATE_FORMAT_CHANGES));
  builder.AddData("cfg-gfx-efb-copy-ram", !Config::Get(Config::GFX_HACK_SKIP_EFB_COPY_TO_RAM));
  builder.AddData("cfg-gfx-xfb-copy-ram", !Config::Get(Config::GFX_HACK_SKIP_XFB_COPY_TO_RAM));
  builder.AddData("cfg-gfx-defer-efb-copies", Config::Get(Config::GFX_HACK_DEFER_EFB_COPIES));
  builder.AddData("cfg-gfx-immediate-xfb", !Config::Get(Config::GFX_HACK_IMMEDIATE_XFB));
  builder.AddData("cfg-gfx-efb-copy-scaled", Config::Get(Config::GFX_HACK_COPY_EFB_SCALED));
  builder.AddData("cfg-gfx-internal-resolution", Config::Get(Config::GFX_EFB_SCALE));
  builder.AddData("cfg-gfx-tc-samples", Config::Get(Config::GFX_SAFE_TEXTURE_CACHE_COLOR_SAMPLES));
  builder.AddData("cfg-gfx-stereo-mode",
                  Common::ToUnderlying(Config::Get(Config::GFX_STEREO_MODE)));
  builder.AddData("cfg-gfx-stereo-per-eye-resolution-full",
                  Config::Get(Config::GFX_STEREO_PER_EYE_RESOLUTION_FULL));
  builder.AddData("cfg-gfx-hdr", Config::Get(Config::GFX_ENHANCE_HDR_OUTPUT));
  builder.AddData("cfg-gfx-per-pixel-lighting", Config::Get(Config::GFX_ENABLE_PIXEL_LIGHTING));
  builder.AddData("cfg-gfx-shader-compilation-mode", GetShaderCompilationMode());
  builder.AddData("cfg-gfx-wait-for-shaders",
                  Config::Get(Config::GFX_WAIT_FOR_SHADERS_BEFORE_STARTING));
  builder.AddData("cfg-gfx-fast-depth", Config::Get(Config::GFX_FAST_DEPTH_CALC));
  builder.AddData("cfg-gfx-vertex-rounding", UseVertexRounding());

  // GPU features.
  const int adapter_index = Config::Get(Config::GFX_ADAPTER);
  if (adapter_index < static_cast<int>(g_backend_info.Adapters.size()))
  {
    builder.AddData("gpu-adapter", g_backend_info.Adapters[adapter_index]);
  }
  else if (!g_backend_info.AdapterName.empty())
  {
    builder.AddData("gpu-adapter", g_backend_info.AdapterName);
  }
  builder.AddData("gpu-has-exclusive-fullscreen", g_backend_info.bSupportsExclusiveFullscreen);
  builder.AddData("gpu-has-dual-source-blend", g_backend_info.bSupportsDualSourceBlend);
  builder.AddData("gpu-has-primitive-restart", g_backend_info.bSupportsPrimitiveRestart);
  builder.AddData("gpu-has-geometry-shaders", g_backend_info.bSupportsGeometryShaders);
  builder.AddData("gpu-has-3d-vision", g_backend_info.bSupports3DVision);
  builder.AddData("gpu-has-early-z", g_backend_info.bSupportsEarlyZ);
  builder.AddData("gpu-has-binding-layout", g_backend_info.bSupportsBindingLayout);
  builder.AddData("gpu-has-bbox", g_backend_info.bSupportsBBox);
  builder.AddData("gpu-has-fragment-stores-and-atomics",
                  g_backend_info.bSupportsFragmentStoresAndAtomics);
  builder.AddData("gpu-has-gs-instancing", g_backend_info.bSupportsGSInstancing);
  builder.AddData("gpu-has-post-processing", g_backend_info.bSupportsPostProcessing);
  builder.AddData("gpu-has-palette-conversion", g_backend_info.bSupportsPaletteConversion);
  builder.AddData("gpu-has-clip-control", g_backend_info.bSupportsClipControl);
  builder.AddData("gpu-has-ssaa", g_backend_info.bSupportsSSAA);
  builder.AddData("gpu-has-logic-ops", g_backend_info.bSupportsLogicOp);
  builder.AddData("gpu-has-framebuffer-fetch", g_backend_info.bSupportsFramebufferFetch);

  // NetPlay / recording.
  builder.AddData("netplay", NetPlay::IsNetPlayRunning());
  builder.AddData("movie", Core::System::GetInstance().GetMovie().IsMovieActive());

  // Controller information
  // We grab enough to tell what percentage of our users are playing with keyboard/mouse, some kind
  // of gamepad, or the official GameCube adapter.
  builder.AddData("gcadapter-detected", GCAdapter::IsDetected(nullptr));
  builder.AddData("has-controller", Pad::GetConfig()->IsControllerControlledByGamepadDevice(0) ||
                                        GCAdapter::IsDetected(nullptr));

  m_per_game_builder = builder;
}
