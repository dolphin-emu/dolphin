// Copyright 2013 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/DriverDetails.h"

#include <map>

#include "Common/Logging/LogManager.h"
#include "Core/DolphinAnalytics.h"

namespace DriverDetails
{
struct BugInfo
{
  API m_api;              // Which API has the issue
  u32 m_os;               // Which OS has the issue
  Vendor m_vendor;        // Which vendor has the error
  Driver m_driver;        // Which driver has the error
  Family m_family;        // Which family of hardware has the issue
  Bug m_bug;              // Which bug it is
  double m_versionstart;  // When it started
  double m_versionend;    // When it ended
  bool m_hasbug;          // Does it have it?
};

// Local members
#ifdef _WIN32
constexpr u32 m_os = OS_ALL | OS_WINDOWS;
#elif ANDROID
constexpr u32 m_os = OS_ALL | OS_ANDROID;
#elif __APPLE__
constexpr u32 m_os = OS_ALL | OS_OSX;
#elif __linux__
constexpr u32 m_os = OS_ALL | OS_LINUX;
#elif __FreeBSD__
constexpr u32 m_os = OS_ALL | OS_FREEBSD;
#elif __OpenBSD__
constexpr u32 m_os = OS_ALL | OS_OPENBSD;
#elif __NetBSD__
constexpr u32 m_os = OS_ALL | OS_NETBSD;
#elif __HAIKU__
constexpr u32 m_os = OS_ALL | OS_HAIKU;
#endif

static API m_api = API_OPENGL;
static Vendor m_vendor = VENDOR_UNKNOWN;
static Driver m_driver = DRIVER_UNKNOWN;
static Family m_family = Family::UNKNOWN;
static double m_version = 0.0;
static std::string m_name;

// This is a list of all known bugs for each vendor
// We use this to check if the device and driver has a issue
constexpr BugInfo m_known_bugs[] = {
    {API_OPENGL, OS_ALL, VENDOR_QUALCOMM, DRIVER_QUALCOMM, Family::UNKNOWN,
     BUG_BROKEN_BUFFER_STREAM, -1.0, -1.0, true},
    {API_OPENGL, OS_ALL, VENDOR_QUALCOMM, DRIVER_QUALCOMM, Family::UNKNOWN,
     BUG_BROKEN_NEGATED_BOOLEAN, -1.0, -1.0, true},
    {API_OPENGL, OS_ALL, VENDOR_ARM, DRIVER_ARM, Family::UNKNOWN, BUG_BROKEN_BUFFER_STREAM, -1.0,
     -1.0, true},
    {API_OPENGL, OS_ALL, VENDOR_ARM, DRIVER_ARM, Family::UNKNOWN, BUG_BROKEN_VSYNC, -1.0, -1.0,
     true},
    {API_OPENGL, OS_ALL, VENDOR_IMGTEC, DRIVER_IMGTEC, Family::UNKNOWN, BUG_BROKEN_BUFFER_STREAM,
     -1.0, -1.0, true},
    {API_OPENGL, OS_ALL, VENDOR_MESA, DRIVER_NOUVEAU, Family::UNKNOWN, BUG_BROKEN_UBO, 900, 916,
     true},
    {API_OPENGL, OS_ALL, VENDOR_MESA, DRIVER_R600, Family::UNKNOWN, BUG_BROKEN_UBO, 900, 913, true},
    {API_OPENGL, OS_ALL, VENDOR_MESA, DRIVER_R600, Family::UNKNOWN, BUG_BROKEN_GEOMETRY_SHADERS,
     -1.0, 1112.0, true},
    {API_OPENGL, OS_ALL, VENDOR_MESA, DRIVER_I965, Family::INTEL_SANDY, BUG_BROKEN_GEOMETRY_SHADERS,
     -1.0, 1120.0, true},
    {API_OPENGL, OS_ALL, VENDOR_MESA, DRIVER_I965, Family::UNKNOWN, BUG_BROKEN_UBO, 900, 920, true},
    {API_OPENGL, OS_ALL, VENDOR_MESA, DRIVER_ALL, Family::UNKNOWN, BUG_BROKEN_COPYIMAGE, -1.0,
     1064.0, true},
    {API_OPENGL, OS_LINUX, VENDOR_ATI, DRIVER_ATI, Family::UNKNOWN, BUG_BROKEN_PINNED_MEMORY, -1.0,
     -1.0, true},
    {API_OPENGL, OS_ALL, VENDOR_MESA, DRIVER_R600, Family::UNKNOWN, BUG_BROKEN_PINNED_MEMORY, -1.0,
     -1.0, true},
    {API_OPENGL, OS_LINUX, VENDOR_NVIDIA, DRIVER_NVIDIA, Family::UNKNOWN, BUG_BROKEN_BUFFER_STORAGE,
     -1.0, 33138.0, true},
    {API_OPENGL, OS_OSX, VENDOR_INTEL, DRIVER_INTEL, Family::INTEL_SANDY, BUG_PRIMITIVE_RESTART,
     -1.0, -1.0, true},
    {API_OPENGL, OS_WINDOWS, VENDOR_NVIDIA, DRIVER_NVIDIA, Family::UNKNOWN,
     BUG_BROKEN_UNSYNC_MAPPING, -1.0, -1.0, true},
    {API_OPENGL, OS_LINUX, VENDOR_NVIDIA, DRIVER_NVIDIA, Family::UNKNOWN, BUG_BROKEN_UNSYNC_MAPPING,
     -1.0, -1.0, true},
    {API_OPENGL, OS_ALL, VENDOR_ARM, DRIVER_ARM, Family::UNKNOWN, BUG_BROKEN_UNSYNC_MAPPING, -1.0,
     -1.0, true},
    {API_OPENGL, OS_WINDOWS, VENDOR_INTEL, DRIVER_INTEL, Family::UNKNOWN,
     BUG_INTEL_BROKEN_BUFFER_STORAGE, 101810.3907, 101810.3960, true},
    {API_OPENGL, OS_ALL, VENDOR_ATI, DRIVER_ATI, Family::UNKNOWN, BUG_SLOW_GETBUFFERSUBDATA, -1.0,
     -1.0, true},
    {API_OPENGL, OS_ALL, VENDOR_MESA, DRIVER_I965, Family::UNKNOWN, BUG_BROKEN_CLIP_DISTANCE, -1.0,
     -1.0, true},
    {API_OPENGL, OS_WINDOWS, VENDOR_ATI, DRIVER_ATI, Family::UNKNOWN,
     BUG_BROKEN_DUAL_SOURCE_BLENDING, -1.0, -1.0, true},
    {API_OPENGL, OS_OSX, VENDOR_INTEL, DRIVER_INTEL, Family::UNKNOWN,
     BUG_BROKEN_DUAL_SOURCE_BLENDING, -1.0, -1.0, true},
    {API_VULKAN, OS_OSX, VENDOR_INTEL, DRIVER_PORTABILITY, Family::UNKNOWN,
     BUG_BROKEN_DUAL_SOURCE_BLENDING, -1.0, -1.0, true},
    {API_METAL, OS_OSX, VENDOR_INTEL, DRIVER_APPLE, Family::UNKNOWN,
     BUG_BROKEN_DUAL_SOURCE_BLENDING, -1.0, -1.0, true},
    {API_OPENGL, OS_ALL, VENDOR_IMGTEC, DRIVER_IMGTEC, Family::UNKNOWN,
     BUG_BROKEN_BITWISE_OP_NEGATION, -1.0, 108.4693462, true},
    {API_VULKAN, OS_WINDOWS, VENDOR_ATI, DRIVER_ATI, Family::UNKNOWN, BUG_PRIMITIVE_RESTART, -1.0,
     -1.0, true},
    {API_VULKAN, OS_ALL, VENDOR_ARM, DRIVER_ARM, Family::UNKNOWN, BUG_PRIMITIVE_RESTART, -1.0, -1.0,
     true},
    {API_OPENGL, OS_LINUX, VENDOR_MESA, DRIVER_I965, Family::UNKNOWN,
     BUG_SHARED_CONTEXT_SHADER_COMPILATION, -1.0, -1.0, true},
    {API_OPENGL, OS_LINUX, VENDOR_MESA, DRIVER_NOUVEAU, Family::UNKNOWN,
     BUG_SHARED_CONTEXT_SHADER_COMPILATION, -1.0, -1.0, true},
    {API_VULKAN, OS_ALL, VENDOR_NVIDIA, DRIVER_NVIDIA, Family::UNKNOWN, BUG_BROKEN_MSAA_CLEAR, -1.0,
     -1.0, true},
    {API_VULKAN, OS_ALL, VENDOR_IMGTEC, DRIVER_IMGTEC, Family::UNKNOWN,
     BUG_BROKEN_CLEAR_LOADOP_RENDERPASS, -1.0, -1.0, true},
    {API_VULKAN, OS_ALL, VENDOR_QUALCOMM, DRIVER_QUALCOMM, Family::UNKNOWN, BUG_BROKEN_D32F_CLEAR,
     -1.0, -1.0, true},
    {API_VULKAN, OS_ALL, VENDOR_MESA, DRIVER_I965, Family::UNKNOWN, BUG_BROKEN_REVERSED_DEPTH_RANGE,
     -1.0, -1.0, true},
    {API_VULKAN, OS_ALL, VENDOR_QUALCOMM, DRIVER_QUALCOMM, Family::UNKNOWN,
     BUG_BROKEN_REVERSED_DEPTH_RANGE, -1.0, -1.0, true},
    {API_VULKAN, OS_OSX, VENDOR_ALL, DRIVER_PORTABILITY, Family::UNKNOWN,
     BUG_BROKEN_REVERSED_DEPTH_RANGE, -1.0, -1.0, true},
    {API_METAL, OS_OSX, VENDOR_ALL, DRIVER_APPLE, Family::UNKNOWN, BUG_BROKEN_REVERSED_DEPTH_RANGE,
     -1.0, -1.0, true},
    {API_VULKAN, OS_ALL, VENDOR_ARM, DRIVER_ARM, Family::UNKNOWN, BUG_SLOW_CACHED_READBACK_MEMORY,
     -1.0, -1.0, true},
    {API_VULKAN, OS_ALL, VENDOR_QUALCOMM, DRIVER_QUALCOMM, Family::UNKNOWN,
     BUG_SLOW_CACHED_READBACK_MEMORY, -1.0, -1.0, true},
    {API_OPENGL, OS_ALL, VENDOR_ARM, DRIVER_ARM, Family::UNKNOWN, BUG_BROKEN_VECTOR_BITWISE_AND,
     -1.0, -1.0, true},
    {API_VULKAN, OS_ALL, VENDOR_ARM, DRIVER_ARM, Family::UNKNOWN, BUG_BROKEN_VECTOR_BITWISE_AND,
     -1.0, -1.0, true},
    {API_VULKAN, OS_OSX, VENDOR_ATI, DRIVER_PORTABILITY, Family::UNKNOWN, BUG_INVERTED_IS_HELPER,
     -1.0, -1.0, true},
    {API_METAL, OS_OSX, VENDOR_ATI, DRIVER_APPLE, Family::UNKNOWN, BUG_INVERTED_IS_HELPER, -1.0,
     -1.0, true},
    {API_VULKAN, OS_OSX, VENDOR_INTEL, DRIVER_PORTABILITY, Family::UNKNOWN,
     BUG_BROKEN_SUBGROUP_OPS_WITH_DISCARD, -1.0, -1.0, true},
    {API_METAL, OS_OSX, VENDOR_INTEL, DRIVER_APPLE, Family::UNKNOWN,
     BUG_BROKEN_SUBGROUP_OPS_WITH_DISCARD, -1.0, -1.0, true},
    {API_OPENGL, OS_ANDROID, VENDOR_ALL, DRIVER_ALL, Family::UNKNOWN,
     BUG_BROKEN_MULTITHREADED_SHADER_PRECOMPILATION, -1.0, -1.0, true},
    {API_VULKAN, OS_ANDROID, VENDOR_ALL, DRIVER_ALL, Family::UNKNOWN,
     BUG_BROKEN_MULTITHREADED_SHADER_PRECOMPILATION, -1.0, -1.0, true},
    {API_OPENGL, OS_ALL, VENDOR_QUALCOMM, DRIVER_QUALCOMM, Family::UNKNOWN, BUG_PRIMITIVE_RESTART,
     -1.0, -1.0, true},
    {API_VULKAN, OS_ALL, VENDOR_QUALCOMM, DRIVER_QUALCOMM, Family::UNKNOWN, BUG_PRIMITIVE_RESTART,
     -1.0, -1.0, true},
    {API_VULKAN, OS_OSX, VENDOR_APPLE, DRIVER_PORTABILITY, Family::UNKNOWN,
     BUG_BROKEN_DISCARD_WITH_EARLY_Z, -1.0, -1.0, true},
    {API_METAL, OS_OSX, VENDOR_APPLE, DRIVER_APPLE, Family::UNKNOWN,
     BUG_BROKEN_DISCARD_WITH_EARLY_Z, -1.0, -1.0, true},
    {API_VULKAN, OS_OSX, VENDOR_INTEL, DRIVER_PORTABILITY, Family::UNKNOWN,
     BUG_BROKEN_DYNAMIC_SAMPLER_INDEXING, -1.0, -1.0, true},
    {API_METAL, OS_OSX, VENDOR_INTEL, DRIVER_APPLE, Family::UNKNOWN,
     BUG_BROKEN_DYNAMIC_SAMPLER_INDEXING, -1.0, -1.0, true},
    {API_VULKAN, OS_ANDROID, VENDOR_QUALCOMM, DRIVER_QUALCOMM, Family::UNKNOWN,
     BUG_SLOW_OPTIMAL_IMAGE_TO_BUFFER_COPY, -1.0, -1.0, true},
};

static std::map<Bug, BugInfo> m_bugs;

void Init(API api, Vendor vendor, Driver driver, const double version, const Family family,
          std::string name)
{
  m_api = api;
  m_vendor = vendor;
  m_driver = driver;
  m_version = version;
  m_family = family;
  m_name = std::move(name);

  if (driver == DRIVER_UNKNOWN)
  {
    switch (vendor)
    {
    case VENDOR_NVIDIA:
    case VENDOR_TEGRA:
      m_driver = DRIVER_NVIDIA;
      break;
    case VENDOR_ATI:
      m_driver = DRIVER_ATI;
      break;
    case VENDOR_INTEL:
      m_driver = DRIVER_INTEL;
      break;
    case VENDOR_IMGTEC:
      m_driver = DRIVER_IMGTEC;
      break;
    case VENDOR_VIVANTE:
      m_driver = DRIVER_VIVANTE;
      break;
    default:
      break;
    }
  }

  // Clear bug list, as the API may have changed
  m_bugs.clear();

  for (const auto& bug : m_known_bugs)
  {
    if ((bug.m_api & api) && (bug.m_os & m_os) &&
        (bug.m_vendor == m_vendor || bug.m_vendor == VENDOR_ALL) &&
        (bug.m_driver == m_driver || bug.m_driver == DRIVER_ALL) &&
        (bug.m_family == m_family || bug.m_family == Family::UNKNOWN) &&
        (bug.m_versionstart <= m_version || bug.m_versionstart == -1) &&
        (bug.m_versionend > m_version || bug.m_versionend == -1))
    {
      m_bugs.emplace(bug.m_bug, bug);
    }
  }
}

bool HasBug(Bug bug)
{
  const auto it = m_bugs.find(bug);
  if (it == m_bugs.end())
    return false;
  return it->second.m_hasbug;
}

#ifdef __clang__
// Make sure we handle all these switch cases
#pragma clang diagnostic error "-Wswitch"
#pragma clang diagnostic error "-Wcovered-switch-default"
#endif

// clang-format off

static const char* to_string(API api)
{
  switch (api)
  {
    case API_OPENGL: return "OpenGL";
    case API_VULKAN: return "Vulkan";
    case API_METAL:  return "Metal";
  }
  return "Unknown";
}

static const char* to_string(Driver driver)
{
  switch (driver)
  {
    case DRIVER_ALL:         return "All";
    case DRIVER_NVIDIA:      return "Nvidia";
    case DRIVER_NOUVEAU:     return "Nouveau";
    case DRIVER_ATI:         return "ATI";
    case DRIVER_R600:        return "R600";
    case DRIVER_INTEL:       return "Intel";
    case DRIVER_I965:        return "I965";
    case DRIVER_ARM:         return "ARM";
    case DRIVER_LIMA:        return "Lima";
    case DRIVER_QUALCOMM:    return "Qualcomm";
    case DRIVER_FREEDRENO:   return "Freedreno";
    case DRIVER_IMGTEC:      return "Imgtech";
    case DRIVER_VIVANTE:     return "Vivante";
    case DRIVER_PORTABILITY: return "Portability";
    case DRIVER_APPLE:       return "Apple";
    case DRIVER_UNKNOWN:     return "Unknown";
  }
  return "Unknown";
}

static const char* to_string(Bug bug)
{
  switch (bug)
  {
    case BUG_BROKEN_UBO:                                 return "broken-ubo";
    case BUG_BROKEN_PINNED_MEMORY:                       return "broken-pinned-memory";
    case BUG_BROKEN_BUFFER_STREAM:                       return "broken-buffer-stream";
    case BUG_BROKEN_BUFFER_STORAGE:                      return "broken-buffer-storage";
    case BUG_PRIMITIVE_RESTART:                          return "primitive-restart";
    case BUG_BROKEN_UNSYNC_MAPPING:                      return "broken-unsync-mapping";
    case BUG_INTEL_BROKEN_BUFFER_STORAGE:                return "intel-broken-buffer-storage";
    case BUG_BROKEN_NEGATED_BOOLEAN:                     return "broken-negated-boolean";
    case BUG_BROKEN_COPYIMAGE:                           return "broken-copyimage";
    case BUG_BROKEN_VSYNC:                               return "broken-vsync";
    case BUG_BROKEN_GEOMETRY_SHADERS:                    return "broken-geometry-shaders";
    case BUG_SLOW_GETBUFFERSUBDATA:                      return "slow-getBufferSubData";
    case BUG_BROKEN_CLIP_DISTANCE:                       return "broken-clip-distance";
    case BUG_BROKEN_DUAL_SOURCE_BLENDING:                return "broken-dual-source-blending";
    case BUG_BROKEN_BITWISE_OP_NEGATION:                 return "broken-bitwise-op-negation";
    case BUG_SHARED_CONTEXT_SHADER_COMPILATION:          return "shared-context-shader-compilation";
    case BUG_BROKEN_MSAA_CLEAR:                          return "broken-msaa-clear";
    case BUG_BROKEN_CLEAR_LOADOP_RENDERPASS:             return "broken-clear-loadop-renderpass";
    case BUG_BROKEN_D32F_CLEAR:                          return "broken-d32f-clear";
    case BUG_BROKEN_REVERSED_DEPTH_RANGE:                return "broken-reversed-depth-range";
    case BUG_SLOW_CACHED_READBACK_MEMORY:                return "slow-cached-readback-memory";
    case BUG_BROKEN_VECTOR_BITWISE_AND:                  return "broken-vector-bitwise-and";
    case BUG_BROKEN_SUBGROUP_OPS_WITH_DISCARD:           return "broken-subgroup-ops-with-discard";
    case BUG_INVERTED_IS_HELPER:                         return "inverted-is-helper";
    case BUG_BROKEN_MULTITHREADED_SHADER_PRECOMPILATION: return "broken-multithreaded-shader-precompilation";
    case BUG_BROKEN_DISCARD_WITH_EARLY_Z:                return "broken-discard-with-early-z";
    case BUG_BROKEN_DYNAMIC_SAMPLER_INDEXING:            return "broken-dynamic-sampler-indexing";
    case BUG_SLOW_OPTIMAL_IMAGE_TO_BUFFER_COPY:          return "slow-optimal-image-to-buffer-copy";
  }
  return "Unknown";
}

// clang-format on

void OverrideBug(Bug bug, bool new_value)
{
  const auto [it, added] = m_bugs.try_emplace(
      bug, BugInfo{m_api, m_os, m_vendor, m_driver, m_family, bug, -1, -1, false});
  if (it->second.m_hasbug != new_value)
  {
    DolphinAnalytics& analytics = DolphinAnalytics::Instance();
    Common::AnalyticsReportBuilder builder(analytics.BaseBuilder());
    builder.AddData("type", "gpu-bug-override");
    builder.AddData("bug", to_string(bug));
    builder.AddData("value", new_value);
    builder.AddData("gpu", m_name);
    builder.AddData("api", to_string(m_api));
    builder.AddData("driver", to_string(m_driver));
    builder.AddData("version", std::to_string(m_version));
    analytics.Send(builder);

    it->second.m_hasbug = new_value;
  }
}
}  // namespace DriverDetails
