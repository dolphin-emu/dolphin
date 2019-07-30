#include "Core/Analytics.h"

#include <array>
#include <cinttypes>
#include <mbedtls/sha1.h>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#elif defined(__APPLE__)
#include <CoreServices/CoreServices.h>
#elif defined(ANDROID)
#include <functional>
#include "Common/AndroidAnalytics.h"
#endif

#include "Common/Analytics.h"
#include "Common/CPUDetect.h"
#include "Common/CommonTypes.h"
#include "Common/Random.h"
#include "Common/StringUtil.h"
#include "Common/Timer.h"
#include "Common/Version.h"
#include "Core/Config/MainSettings.h"
#include "Core/ConfigManager.h"
#include "Core/HW/GCPad.h"
#include "Core/Movie.h"
#include "Core/NetPlayProto.h"
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
  if (SConfig::GetInstance().m_analytics_enabled)
  {
#if defined(ANDROID)
    new_backend = std::make_unique<Common::AndroidAnalyticsBackend>(ANALYTICS_ENDPOINT);
#else
    new_backend = std::make_unique<Common::HttpAnalyticsBackend>(ANALYTICS_ENDPOINT);
#endif
  }
  m_reporter.SetBackend(std::move(new_backend));

  // Load the unique ID or generate it if needed.
  m_unique_id = SConfig::GetInstance().m_analytics_id;
  if (m_unique_id.empty())
  {
    GenerateNewIdentity();
  }
}

void DolphinAnalytics::GenerateNewIdentity()
{
  const u64 id_high = Common::Random::GenerateValue<u64>();
  const u64 id_low = Common::Random::GenerateValue<u64>();
  m_unique_id = StringFromFormat("%016" PRIx64 "%016" PRIx64, id_high, id_low);

  // Save the new id in the configuration.
  SConfig::GetInstance().m_analytics_id = m_unique_id;
  SConfig::GetInstance().SaveSettings();
}

std::string DolphinAnalytics::MakeUniqueId(std::string_view data) const
{
  std::array<u8, 20> digest;
  const auto input = std::string{m_unique_id}.append(data);
  mbedtls_sha1_ret(reinterpret_cast<const u8*>(input.c_str()), input.size(), digest.data());

  // Convert to hex string and truncate to 64 bits.
  std::string out;
  for (int i = 0; i < 8; ++i)
  {
    out += StringFromFormat("%02hhx", digest[i]);
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
constexpr std::array<const char*, 2> GAME_QUIRKS_NAMES{
    "icache-matters",
    "directly-reads-wiimote-input",
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
  m_sampling_next_start_us = Common::Timer::GetTimeUs() + wait_us;
}

bool DolphinAnalytics::ShouldStartPerformanceSampling()
{
  if (Common::Timer::GetTimeUs() < m_sampling_next_start_us)
    return false;

  u64 wait_us =
      PERFORMANCE_SAMPLING_INTERVAL_SECS * 1000000 +
      Common::Random::GenerateValue<u64>() % (PERFORMANCE_SAMPLING_WAIT_TIME_JITTER_SECS * 1000000);
  m_sampling_next_start_us = Common::Timer::GetTimeUs() + wait_us;
  return true;
}

void DolphinAnalytics::MakeBaseBuilder()
{
  Common::AnalyticsReportBuilder builder;

  // Version information.
  builder.AddData("version-desc", Common::scm_desc_str);
  builder.AddData("version-hash", Common::scm_rev_git_str);
  builder.AddData("version-branch", Common::scm_branch_str);
  builder.AddData("version-dist", Common::scm_distributor_str);

  // Auto-Update information.
  builder.AddData("update-track", SConfig::GetInstance().m_auto_update_track);

  // CPU information.
  builder.AddData("cpu-summary", cpu_info.Summarize());

// OS information.
#if defined(_WIN32)
  builder.AddData("os-type", "windows");

  // Windows 8 removes support for GetVersionEx and such. Stupid.
  DWORD(WINAPI * RtlGetVersion)(LPOSVERSIONINFOEXW);
  *(FARPROC*)&RtlGetVersion = GetProcAddress(GetModuleHandle(TEXT("ntdll")), "RtlGetVersion");

  OSVERSIONINFOEXW winver;
  winver.dwOSVersionInfoSize = sizeof(winver);
  if (RtlGetVersion != nullptr)
  {
    RtlGetVersion(&winver);
    builder.AddData("win-ver-major", static_cast<u32>(winver.dwMajorVersion));
    builder.AddData("win-ver-minor", static_cast<u32>(winver.dwMinorVersion));
    builder.AddData("win-ver-build", static_cast<u32>(winver.dwBuildNumber));
    builder.AddData("win-ver-spmajor", static_cast<u32>(winver.wServicePackMajor));
    builder.AddData("win-ver-spminor", static_cast<u32>(winver.wServicePackMinor));
  }
#elif defined(ANDROID)
  builder.AddData("os-type", "android");
  builder.AddData("android-manufacturer", s_get_val_func("DEVICE_MANUFACTURER"));
  builder.AddData("android-model", s_get_val_func("DEVICE_MODEL"));
  builder.AddData("android-version", s_get_val_func("DEVICE_OS"));
#elif defined(__APPLE__)
  builder.AddData("os-type", "osx");

  SInt32 osxmajor, osxminor, osxbugfix;
// Gestalt is deprecated, but the replacement (NSProcessInfo
// operatingSystemVersion) is only available on OS X 10.10, so we need to use
// it anyway.  Change this someday when Dolphin depends on 10.10+.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
  Gestalt(gestaltSystemVersionMajor, &osxmajor);
  Gestalt(gestaltSystemVersionMinor, &osxminor);
  Gestalt(gestaltSystemVersionBugFix, &osxbugfix);
#pragma GCC diagnostic pop

  builder.AddData("osx-ver-major", osxmajor);
  builder.AddData("osx-ver-minor", osxminor);
  builder.AddData("osx-ver-bugfix", osxbugfix);
#elif defined(__linux__)
  builder.AddData("os-type", "linux");
#elif defined(__FreeBSD__)
  builder.AddData("os-type", "freebsd");
#else
  builder.AddData("os-type", "unknown");
#endif

  m_base_builder = builder;
}

static const char* GetShaderCompilationMode(const VideoConfig& video_config)
{
  switch (video_config.iShaderCompilationMode)
  {
  case ShaderCompilationMode::AsynchronousSkipRendering:
    return "async-skip-rendering";
  case ShaderCompilationMode::Synchronous:
  default:
    return "sync";
  }
}

void DolphinAnalytics::MakePerGameBuilder()
{
  Common::AnalyticsReportBuilder builder(m_base_builder);

  // Gameid.
  builder.AddData("gameid", SConfig::GetInstance().GetGameID());

  // Unique id bound to the gameid.
  builder.AddData("id", MakeUniqueId(SConfig::GetInstance().GetGameID()));

  // Configuration.
  builder.AddData("cfg-dsp-hle", SConfig::GetInstance().bDSPHLE);
  builder.AddData("cfg-dsp-jit", SConfig::GetInstance().m_DSPEnableJIT);
  builder.AddData("cfg-dsp-thread", SConfig::GetInstance().bDSPThread);
  builder.AddData("cfg-cpu-thread", SConfig::GetInstance().bCPUThread);
  builder.AddData("cfg-fastmem", SConfig::GetInstance().bFastmem);
  builder.AddData("cfg-syncgpu", SConfig::GetInstance().bSyncGPU);
  builder.AddData("cfg-audio-backend", SConfig::GetInstance().sBackend);
  builder.AddData("cfg-oc-enable", SConfig::GetInstance().m_OCEnable);
  builder.AddData("cfg-oc-factor", SConfig::GetInstance().m_OCFactor);
  builder.AddData("cfg-render-to-main", Config::Get(Config::MAIN_RENDER_TO_MAIN));
  if (g_video_backend)
  {
    builder.AddData("cfg-video-backend", g_video_backend->GetName());
  }

  // Video configuration.
  builder.AddData("cfg-gfx-multisamples", g_Config.iMultisamples);
  builder.AddData("cfg-gfx-ssaa", g_Config.bSSAA);
  builder.AddData("cfg-gfx-anisotropy", g_Config.iMaxAnisotropy);
  builder.AddData("cfg-gfx-vsync", g_Config.bVSync);
  builder.AddData("cfg-gfx-aspect-ratio", static_cast<int>(g_Config.aspect_mode));
  builder.AddData("cfg-gfx-efb-access", g_Config.bEFBAccessEnable);
  builder.AddData("cfg-gfx-efb-copy-format-changes", g_Config.bEFBEmulateFormatChanges);
  builder.AddData("cfg-gfx-efb-copy-ram", !g_Config.bSkipEFBCopyToRam);
  builder.AddData("cfg-gfx-xfb-copy-ram", !g_Config.bSkipXFBCopyToRam);
  builder.AddData("cfg-gfx-defer-efb-copies", g_Config.bDeferEFBCopies);
  builder.AddData("cfg-gfx-immediate-xfb", !g_Config.bImmediateXFB);
  builder.AddData("cfg-gfx-efb-copy-scaled", g_Config.bCopyEFBScaled);
  builder.AddData("cfg-gfx-internal-resolution", g_Config.iEFBScale);
  builder.AddData("cfg-gfx-tc-samples", g_Config.iSafeTextureCache_ColorSamples);
  builder.AddData("cfg-gfx-per-pixel-lighting", g_Config.bEnablePixelLighting);
  builder.AddData("cfg-gfx-shader-compilation-mode", GetShaderCompilationMode(g_Config));
  builder.AddData("cfg-gfx-wait-for-shaders", g_Config.bWaitForShadersBeforeStarting);
  builder.AddData("cfg-gfx-fast-depth", g_Config.bFastDepthCalc);
  builder.AddData("cfg-gfx-vertex-rounding", g_Config.UseVertexRounding());

  // GPU features.
  if (g_Config.iAdapter < static_cast<int>(g_Config.backend_info.Adapters.size()))
  {
    builder.AddData("gpu-adapter", g_Config.backend_info.Adapters[g_Config.iAdapter]);
  }
  else if (!g_Config.backend_info.AdapterName.empty())
  {
    builder.AddData("gpu-adapter", g_Config.backend_info.AdapterName);
  }
  builder.AddData("gpu-has-exclusive-fullscreen",
                  g_Config.backend_info.bSupportsExclusiveFullscreen);
  builder.AddData("gpu-has-dual-source-blend", g_Config.backend_info.bSupportsDualSourceBlend);
  builder.AddData("gpu-has-oversized-viewports", g_Config.backend_info.bSupportsOversizedViewports);
  builder.AddData("gpu-has-geometry-shaders", g_Config.backend_info.bSupportsGeometryShaders);
  builder.AddData("gpu-has-early-z", g_Config.backend_info.bSupportsEarlyZ);
  builder.AddData("gpu-has-binding-layout", g_Config.backend_info.bSupportsBindingLayout);
  builder.AddData("gpu-has-bbox", g_Config.backend_info.bSupportsBBox);
  builder.AddData("gpu-has-fragment-stores-and-atomics",
                  g_Config.backend_info.bSupportsFragmentStoresAndAtomics);
  builder.AddData("gpu-has-gs-instancing", g_Config.backend_info.bSupportsGSInstancing);
  builder.AddData("gpu-has-post-processing", g_Config.backend_info.bSupportsPostProcessing);
  builder.AddData("gpu-has-palette-conversion", g_Config.backend_info.bSupportsPaletteConversion);
  builder.AddData("gpu-has-clip-control", g_Config.backend_info.bSupportsClipControl);
  builder.AddData("gpu-has-ssaa", g_Config.backend_info.bSupportsSSAA);

  // NetPlay / recording.
  builder.AddData("netplay", NetPlay::IsNetPlayRunning());
  builder.AddData("movie", Movie::IsMovieActive());

  // Controller information
  // We grab enough to tell what percentage of our users are playing with keyboard/mouse, some kind
  // of gamepad
  // or the official gamecube adapter.
  builder.AddData("gcadapter-detected", GCAdapter::IsDetected(nullptr));
  builder.AddData("has-controller", Pad::GetConfig()->IsControllerControlledByGamepadDevice(0) ||
                                        GCAdapter::IsDetected(nullptr));

  m_per_game_builder = builder;
}
