#include "Core/Analytics.h"

#include <cinttypes>
#include <mbedtls/sha1.h>
#include <memory>
#include <mutex>
#include <random>
#include <string>

#if defined(_WIN32)
#include <windows.h>
#elif defined(__APPLE__)
#include <CoreServices/CoreServices.h>
#endif

#include "Common/Analytics.h"
#include "Common/CPUDetect.h"
#include "Common/CommonTypes.h"
#include "Common/StringUtil.h"
#include "Common/Version.h"
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
#if defined(_MSC_VER) && _MSC_VER <= 1800
const char* ANALYTICS_ENDPOINT = "https://analytics.dolphin-emu.org/report";
#else
constexpr const char* ANALYTICS_ENDPOINT = "https://analytics.dolphin-emu.org/report";
#endif
}  // namespace

std::mutex DolphinAnalytics::s_instance_mutex;
std::shared_ptr<DolphinAnalytics> DolphinAnalytics::s_instance;

DolphinAnalytics::DolphinAnalytics()
{
  ReloadConfig();
  MakeBaseBuilder();
}

std::shared_ptr<DolphinAnalytics> DolphinAnalytics::Instance()
{
  std::lock_guard<std::mutex> lk(s_instance_mutex);
  if (!s_instance)
  {
    s_instance.reset(new DolphinAnalytics());
  }
  return s_instance;
}

void DolphinAnalytics::ReloadConfig()
{
  std::lock_guard<std::mutex> lk(m_reporter_mutex);

  // Install the HTTP backend if analytics support is enabled.
  std::unique_ptr<Common::AnalyticsReportingBackend> new_backend;
  if (SConfig::GetInstance().m_analytics_enabled)
  {
    new_backend = std::make_unique<Common::HttpAnalyticsBackend>(ANALYTICS_ENDPOINT);
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
  std::random_device rd;
  u64 id_high = (static_cast<u64>(rd()) << 32) | rd();
  u64 id_low = (static_cast<u64>(rd()) << 32) | rd();
  m_unique_id = StringFromFormat("%016" PRIx64 "%016" PRIx64, id_high, id_low);

  // Save the new id in the configuration.
  SConfig::GetInstance().m_analytics_id = m_unique_id;
  SConfig::GetInstance().SaveSettings();
}

std::string DolphinAnalytics::MakeUniqueId(const std::string& data)
{
  u8 digest[20];
  std::string input = m_unique_id + data;
  mbedtls_sha1(reinterpret_cast<const u8*>(input.c_str()), input.size(), digest);

  // Convert to hex string and truncate to 64 bits.
  std::string out;
  for (int i = 0; i < 8; ++i)
  {
    out += StringFromFormat("%02hhx", digest[i]);
  }
  return out;
}

void DolphinAnalytics::ReportDolphinStart(const std::string& ui_type)
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
}

void DolphinAnalytics::MakeBaseBuilder()
{
  Common::AnalyticsReportBuilder builder;

  // Version information.
  builder.AddData("version-desc", Common::scm_desc_str);
  builder.AddData("version-hash", Common::scm_rev_git_str);
  builder.AddData("version-branch", Common::scm_branch_str);
  builder.AddData("version-dist", Common::scm_distributor_str);

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

static const char* GetUbershaderMode(const VideoConfig& video_config)
{
  if (video_config.bDisableSpecializedShaders)
    return "exclusive";

  if (video_config.bBackgroundShaderCompiling)
    return "hybrid";

  return "disabled";
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
  builder.AddData("cfg-idle-skip", SConfig::GetInstance().bSkipIdle);
  builder.AddData("cfg-fastmem", SConfig::GetInstance().bFastmem);
  builder.AddData("cfg-syncgpu", SConfig::GetInstance().bSyncGPU);
  builder.AddData("cfg-audio-backend", SConfig::GetInstance().sBackend);
  builder.AddData("cfg-oc-enable", SConfig::GetInstance().m_OCEnable);
  builder.AddData("cfg-oc-factor", SConfig::GetInstance().m_OCFactor);
  builder.AddData("cfg-render-to-main", SConfig::GetInstance().bRenderToMain);
  if (g_video_backend)
  {
    builder.AddData("cfg-video-backend", g_video_backend->GetName());
  }

  // Video configuration.
  builder.AddData("cfg-gfx-multisamples", g_Config.iMultisamples);
  builder.AddData("cfg-gfx-ssaa", g_Config.bSSAA);
  builder.AddData("cfg-gfx-anisotropy", g_Config.iMaxAnisotropy);
  builder.AddData("cfg-gfx-realxfb", g_Config.RealXFBEnabled());
  builder.AddData("cfg-gfx-virtualxfb", g_Config.VirtualXFBEnabled());
  builder.AddData("cfg-gfx-vsync", g_Config.bVSync);
  builder.AddData("cfg-gfx-aspect-ratio", g_Config.iAspectRatio);
  builder.AddData("cfg-gfx-efb-access", g_Config.bEFBAccessEnable);
  builder.AddData("cfg-gfx-efb-scale", g_Config.iEFBScale);
  builder.AddData("cfg-gfx-efb-copy-format-changes", g_Config.bEFBEmulateFormatChanges);
  builder.AddData("cfg-gfx-efb-copy-ram", !g_Config.bSkipEFBCopyToRam);
  builder.AddData("cfg-gfx-efb-copy-scaled", g_Config.bCopyEFBScaled);
  builder.AddData("cfg-gfx-internal-resolution", g_Config.iInternalResolution);
  builder.AddData("cfg-gfx-tc-samples", g_Config.iSafeTextureCache_ColorSamples);
  builder.AddData("cfg-gfx-stereo-mode", g_Config.iStereoMode);
  builder.AddData("cfg-gfx-per-pixel-lighting", g_Config.bEnablePixelLighting);
  builder.AddData("cfg-gfx-ubershader-mode", GetUbershaderMode(g_Config));
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
  builder.AddData("gpu-has-primitive-restart", g_Config.backend_info.bSupportsPrimitiveRestart);
  builder.AddData("gpu-has-oversized-viewports", g_Config.backend_info.bSupportsOversizedViewports);
  builder.AddData("gpu-has-geometry-shaders", g_Config.backend_info.bSupportsGeometryShaders);
  builder.AddData("gpu-has-3d-vision", g_Config.backend_info.bSupports3DVision);
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
  builder.AddData("gcadapter-detected", GCAdapter::IsDetected());
  builder.AddData("has-controller", Pad::GetConfig()->IsControllerControlledByGamepadDevice(0) ||
                                        GCAdapter::IsDetected());

  m_per_game_builder = builder;
}
