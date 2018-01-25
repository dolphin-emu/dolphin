// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <map>

#include "Common/Logging/LogManager.h"
#include "VideoCommon/DriverDetails.h"

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
const u32 m_os = OS_ALL | OS_WINDOWS;
#elif ANDROID
const u32 m_os = OS_ALL | OS_ANDROID;
#elif __APPLE__
const u32 m_os = OS_ALL | OS_OSX;
#elif __linux__
const u32 m_os = OS_ALL | OS_LINUX;
#elif __FreeBSD__
const u32 m_os = OS_ALL | OS_FREEBSD;
#elif __OpenBSD__
const u32 m_os = OS_ALL | OS_OPENBSD;
#elif __HAIKU__
const u32 m_os = OS_ALL | OS_HAIKU;
#endif

static API m_api = API_OPENGL;
static Vendor m_vendor = VENDOR_UNKNOWN;
static Driver m_driver = DRIVER_UNKNOWN;
static Family m_family = Family::UNKNOWN;
static double m_version = 0.0;

// This is a list of all known bugs for each vendor
// We use this to check if the device and driver has a issue
static BugInfo m_known_bugs[] = {
    {API_OPENGL, OS_ALL, VENDOR_QUALCOMM, DRIVER_QUALCOMM, Family::UNKNOWN,
     BUG_BROKEN_BUFFER_STREAM, -1.0, -1.0, true},
    {API_OPENGL, OS_ALL, VENDOR_QUALCOMM, DRIVER_QUALCOMM, Family::UNKNOWN,
     BUG_BROKEN_NEGATED_BOOLEAN, -1.0, -1.0, true},
    {API_OPENGL, OS_ALL, VENDOR_QUALCOMM, DRIVER_QUALCOMM, Family::UNKNOWN,
     BUG_BROKEN_EXPLICIT_FLUSH, -1.0, -1.0, true},
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
    {API_OPENGL, OS_WINDOWS, VENDOR_INTEL, DRIVER_INTEL, Family::UNKNOWN,
     BUG_INTEL_BROKEN_BUFFER_STORAGE, 101810.3907, 101810.3960, true},
    {API_OPENGL, OS_ALL, VENDOR_ATI, DRIVER_ATI, Family::UNKNOWN, BUG_SLOW_GETBUFFERSUBDATA, -1.0,
     -1.0, true},
    {API_OPENGL, OS_ALL, VENDOR_MESA, DRIVER_I965, Family::UNKNOWN, BUG_BROKEN_CLIP_DISTANCE, -1.0,
     -1.0, true},
    {API_VULKAN, OS_ALL, VENDOR_ATI, DRIVER_ATI, Family::UNKNOWN,
     BUG_BROKEN_FRAGMENT_SHADER_INDEX_DECORATION, -1.0, -1.0, true},
    {API_OPENGL, OS_WINDOWS, VENDOR_ATI, DRIVER_ATI, Family::UNKNOWN,
     BUG_BROKEN_DUAL_SOURCE_BLENDING, -1.0, -1.0, true},
    {API_OPENGL, OS_OSX, VENDOR_INTEL, DRIVER_INTEL, Family::UNKNOWN,
     BUG_BROKEN_DUAL_SOURCE_BLENDING, -1.0, -1.0, true},
    {API_OPENGL, OS_ALL, VENDOR_IMGTEC, DRIVER_IMGTEC, Family::UNKNOWN,
     BUG_BROKEN_BITWISE_OP_NEGATION, -1.0, 108.4693462, true},
    {API_VULKAN, OS_ALL, VENDOR_ATI, DRIVER_ATI, Family::UNKNOWN, BUG_PRIMITIVE_RESTART, -1.0, -1.0,
     true},
    {API_OPENGL, OS_LINUX, VENDOR_MESA, DRIVER_I965, Family::UNKNOWN,
     BUG_SHARED_CONTEXT_SHADER_COMPILATION, -1.0, -1.0, true},
    {API_OPENGL, OS_LINUX, VENDOR_MESA, DRIVER_NOUVEAU, Family::UNKNOWN,
     BUG_SHARED_CONTEXT_SHADER_COMPILATION, -1.0, -1.0, true},
    {API_VULKAN, OS_ALL, VENDOR_NVIDIA, DRIVER_NVIDIA, Family::UNKNOWN,
     BUG_BROKEN_MSAA_VKCMDCLEARATTACHMENTS, -1.0, -1.0, true}};

static std::map<Bug, BugInfo> m_bugs;

void Init(API api, Vendor vendor, Driver driver, const double version, const Family family)
{
  m_api = api;
  m_vendor = vendor;
  m_driver = driver;
  m_version = version;
  m_family = family;

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

  for (auto& bug : m_known_bugs)
  {
    if ((bug.m_api & api) && (bug.m_os & m_os) &&
        (bug.m_vendor == m_vendor || bug.m_vendor == VENDOR_ALL) &&
        (bug.m_driver == m_driver || bug.m_driver == DRIVER_ALL) &&
        (bug.m_family == m_family || bug.m_family == Family::UNKNOWN) &&
        (bug.m_versionstart <= m_version || bug.m_versionstart == -1) &&
        (bug.m_versionend > m_version || bug.m_versionend == -1))
      m_bugs.emplace(bug.m_bug, bug);
  }
}

bool HasBug(Bug bug)
{
  auto it = m_bugs.find(bug);
  if (it == m_bugs.end())
    return false;
  return it->second.m_hasbug;
}

Vendor TranslatePCIVendorID(u32 vendor_id)
{
  switch (vendor_id)
  {
  case 0x10DE:
    return VENDOR_NVIDIA;

  case 0x1002:
  case 0x1022:
    return VENDOR_ATI;

  case 0x8086:
  case 0x8087:
    return VENDOR_INTEL;

  // TODO: Is this correct for Mali?
  case 0x13B6:
    return VENDOR_ARM;

  case 0x5143:
    return VENDOR_QUALCOMM;

  default:
    return VENDOR_UNKNOWN;
  }
}
}
