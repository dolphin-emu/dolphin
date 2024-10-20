// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/IOS/VersionInfo.h"

#include <algorithm>
#include <array>

#include "Common/CommonTypes.h"
#include "Core/CommonTitles.h"
#include "Core/IOS/ES/Formats.h"

namespace IOS::HLE
{
constexpr u32 MEM1_SIZE = 0x01800000;
constexpr u32 MEM1_END = 0x81800000;
constexpr u32 MEM1_ARENA_BEGIN = 0x00000000;
constexpr u32 MEM1_ARENA_END = 0x81800000;
constexpr u32 MEM2_SIZE = 0x4000000;
constexpr u32 MEM2_ARENA_BEGIN = 0x90000800;
constexpr u32 HOLLYWOOD_REVISION = 0x00000011;
constexpr u32 PLACEHOLDER = 0xDEADBEEF;
constexpr u32 RAM_VENDOR = 0x0000FF01;
constexpr u32 RAM_VENDOR_MIOS = 0xCAFEBABE;

// These values were manually extracted from the relevant IOS binaries.
// The writes are usually contained in a single function that
// mostly writes raw literals to the relevant locations.
// e.g. IOS9, version 1034, content id 0x00000006, function at 0xffff6884
constexpr std::array<MemoryValues, 41> ios_memory_values = {
    {{
         4,          0x40003,     0x81006,          MEM1_SIZE,
         MEM1_SIZE,  MEM1_END,    MEM1_ARENA_BEGIN, MEM1_ARENA_END,
         MEM2_SIZE,  MEM2_SIZE,   0x93400000,       MEM2_ARENA_BEGIN,
         0x933E0000, 0x933E0000,  0x93400000,       HOLLYWOOD_REVISION,
         RAM_VENDOR, PLACEHOLDER, PLACEHOLDER,      0,
     },
     {
         9,          0x9040a,     0x030110,         MEM1_SIZE,
         MEM1_SIZE,  MEM1_END,    MEM1_ARENA_BEGIN, MEM1_ARENA_END,
         MEM2_SIZE,  MEM2_SIZE,   0x93400000,       MEM2_ARENA_BEGIN,
         0x933E0000, 0x933E0000,  0x93400000,       HOLLYWOOD_REVISION,
         RAM_VENDOR, PLACEHOLDER, PLACEHOLDER,      0,
     },
     {
         11,         0xb000a,     0x102506,         MEM1_SIZE,
         MEM1_SIZE,  MEM1_END,    MEM1_ARENA_BEGIN, MEM1_ARENA_END,
         MEM2_SIZE,  MEM2_SIZE,   0x93400000,       MEM2_ARENA_BEGIN,
         0x933E0000, 0x933E0000,  0x93400000,       HOLLYWOOD_REVISION,
         RAM_VENDOR, PLACEHOLDER, PLACEHOLDER,      0,
     },
     {
         12,         0xc020e,     0x030110,         MEM1_SIZE,
         MEM1_SIZE,  MEM1_END,    MEM1_ARENA_BEGIN, MEM1_ARENA_END,
         MEM2_SIZE,  MEM2_SIZE,   0x93400000,       MEM2_ARENA_BEGIN,
         0x933E0000, 0x933E0000,  0x93400000,       HOLLYWOOD_REVISION,
         RAM_VENDOR, PLACEHOLDER, PLACEHOLDER,      0,
     },
     {
         13,         0xd0408,     0x030110,         MEM1_SIZE,
         MEM1_SIZE,  MEM1_END,    MEM1_ARENA_BEGIN, MEM1_ARENA_END,
         MEM2_SIZE,  MEM2_SIZE,   0x93400000,       MEM2_ARENA_BEGIN,
         0x933E0000, 0x933E0000,  0x93400000,       HOLLYWOOD_REVISION,
         RAM_VENDOR, PLACEHOLDER, PLACEHOLDER,      0,
     },
     {
         14,         0xe0408,     0x030110,         MEM1_SIZE,
         MEM1_SIZE,  MEM1_END,    MEM1_ARENA_BEGIN, MEM1_ARENA_END,
         MEM2_SIZE,  MEM2_SIZE,   0x93400000,       MEM2_ARENA_BEGIN,
         0x933E0000, 0x933E0000,  0x93400000,       HOLLYWOOD_REVISION,
         RAM_VENDOR, PLACEHOLDER, PLACEHOLDER,      0,
     },
     {
         15,         0xf0408,     0x030110,         MEM1_SIZE,
         MEM1_SIZE,  MEM1_END,    MEM1_ARENA_BEGIN, MEM1_ARENA_END,
         MEM2_SIZE,  MEM2_SIZE,   0x93400000,       MEM2_ARENA_BEGIN,
         0x933E0000, 0x933E0000,  0x93400000,       HOLLYWOOD_REVISION,
         RAM_VENDOR, PLACEHOLDER, PLACEHOLDER,      0,
     },
     {
         17,         0x110408,    0x030110,         MEM1_SIZE,
         MEM1_SIZE,  MEM1_END,    MEM1_ARENA_BEGIN, MEM1_ARENA_END,
         MEM2_SIZE,  MEM2_SIZE,   0x93400000,       MEM2_ARENA_BEGIN,
         0x933E0000, 0x933E0000,  0x93400000,       HOLLYWOOD_REVISION,
         RAM_VENDOR, PLACEHOLDER, PLACEHOLDER,      0,
     },
     {
         20,         0x14000c,    0x102506,         MEM1_SIZE,
         MEM1_SIZE,  MEM1_END,    MEM1_ARENA_BEGIN, MEM1_ARENA_END,
         MEM2_SIZE,  MEM2_SIZE,   0x93400000,       MEM2_ARENA_BEGIN,
         0x933E0000, 0x933E0000,  0x93400000,       HOLLYWOOD_REVISION,
         RAM_VENDOR, PLACEHOLDER, PLACEHOLDER,      0,
     },
     {
         21,         0x15040f,    0x030110,         MEM1_SIZE,
         MEM1_SIZE,  MEM1_END,    MEM1_ARENA_BEGIN, MEM1_ARENA_END,
         MEM2_SIZE,  MEM2_SIZE,   0x93400000,       MEM2_ARENA_BEGIN,
         0x933E0000, 0x933E0000,  0x93400000,       HOLLYWOOD_REVISION,
         RAM_VENDOR, PLACEHOLDER, PLACEHOLDER,      0,
     },
     {
         22,         0x16050e,    0x030110,         MEM1_SIZE,
         MEM1_SIZE,  MEM1_END,    MEM1_ARENA_BEGIN, MEM1_ARENA_END,
         MEM2_SIZE,  MEM2_SIZE,   0x93400000,       MEM2_ARENA_BEGIN,
         0x933E0000, 0x933E0000,  0x93400000,       HOLLYWOOD_REVISION,
         RAM_VENDOR, PLACEHOLDER, PLACEHOLDER,      0,
     },
     {
         28,         0x1c070f,   0x030110,         MEM1_SIZE,
         MEM1_SIZE,  MEM1_END,   MEM1_ARENA_BEGIN, MEM1_ARENA_END,
         MEM2_SIZE,  MEM2_SIZE,  0x93800000,       MEM2_ARENA_BEGIN,
         0x937E0000, 0x937E0000, 0x93800000,       HOLLYWOOD_REVISION,
         RAM_VENDOR, 0x93800000, 0x93820000,       0,
     },
     {
         30,         0x1e0a10,   0x40308,          MEM1_SIZE,
         MEM1_SIZE,  MEM1_END,   MEM1_ARENA_BEGIN, MEM1_ARENA_END,
         MEM2_SIZE,  MEM2_SIZE,  0x93600000,       MEM2_ARENA_BEGIN,
         0x935E0000, 0x935E0000, 0x93600000,       HOLLYWOOD_REVISION,
         RAM_VENDOR, 0x93600000, 0x93620000,       0,
     },
     {
         31,         0x1f0e18,   0x030110,         MEM1_SIZE,
         MEM1_SIZE,  MEM1_END,   MEM1_ARENA_BEGIN, MEM1_ARENA_END,
         MEM2_SIZE,  MEM2_SIZE,  0x93600000,       MEM2_ARENA_BEGIN,
         0x935E0000, 0x935E0000, 0x93600000,       HOLLYWOOD_REVISION,
         RAM_VENDOR, 0x93600000, 0x93620000,       0,
     },
     {
         33,         0x210e18,   0x030110,         MEM1_SIZE,
         MEM1_SIZE,  MEM1_END,   MEM1_ARENA_BEGIN, MEM1_ARENA_END,
         MEM2_SIZE,  MEM2_SIZE,  0x93600000,       MEM2_ARENA_BEGIN,
         0x935E0000, 0x935E0000, 0x93600000,       HOLLYWOOD_REVISION,
         RAM_VENDOR, 0x93600000, 0x93620000,       0,
     },
     {
         34,         0x220e18,   0x030110,         MEM1_SIZE,
         MEM1_SIZE,  MEM1_END,   MEM1_ARENA_BEGIN, MEM1_ARENA_END,
         MEM2_SIZE,  MEM2_SIZE,  0x93600000,       MEM2_ARENA_BEGIN,
         0x935E0000, 0x935E0000, 0x93600000,       HOLLYWOOD_REVISION,
         RAM_VENDOR, 0x93600000, 0x93620000,       0,
     },
     {
         35,         0x230e18,   0x030110,         MEM1_SIZE,
         MEM1_SIZE,  MEM1_END,   MEM1_ARENA_BEGIN, MEM1_ARENA_END,
         MEM2_SIZE,  MEM2_SIZE,  0x93600000,       MEM2_ARENA_BEGIN,
         0x935E0000, 0x935E0000, 0x93600000,       HOLLYWOOD_REVISION,
         RAM_VENDOR, 0x93600000, 0x93620000,       0,
     },
     {
         36,         0x240e18,   0x030110,         MEM1_SIZE,
         MEM1_SIZE,  MEM1_END,   MEM1_ARENA_BEGIN, MEM1_ARENA_END,
         MEM2_SIZE,  MEM2_SIZE,  0x93600000,       MEM2_ARENA_BEGIN,
         0x935E0000, 0x935E0000, 0x93600000,       HOLLYWOOD_REVISION,
         RAM_VENDOR, 0x93600000, 0x93620000,       0,
     },
     {
         37,         0x25161f,   0x030110,         MEM1_SIZE,
         MEM1_SIZE,  MEM1_END,   MEM1_ARENA_BEGIN, MEM1_ARENA_END,
         MEM2_SIZE,  MEM2_SIZE,  0x93600000,       MEM2_ARENA_BEGIN,
         0x935E0000, 0x935E0000, 0x93600000,       HOLLYWOOD_REVISION,
         RAM_VENDOR, 0x93600000, 0x93620000,       0,
     },
     {
         38,         0x26101c,   0x030110,         MEM1_SIZE,
         MEM1_SIZE,  MEM1_END,   MEM1_ARENA_BEGIN, MEM1_ARENA_END,
         MEM2_SIZE,  MEM2_SIZE,  0x93600000,       MEM2_ARENA_BEGIN,
         0x935E0000, 0x935E0000, 0x93600000,       HOLLYWOOD_REVISION,
         RAM_VENDOR, 0x93600000, 0x93620000,       0,
     },
     {
         40,         0x280911,   0x022308,         MEM1_SIZE,
         MEM1_SIZE,  MEM1_END,   MEM1_ARENA_BEGIN, MEM1_ARENA_END,
         MEM2_SIZE,  MEM2_SIZE,  0x93600000,       MEM2_ARENA_BEGIN,
         0x935E0000, 0x935E0000, 0x93600000,       HOLLYWOOD_REVISION,
         RAM_VENDOR, 0x93600000, 0x93620000,       0,
     },
     {
         41,         0x290e17,   0x030110,         MEM1_SIZE,
         MEM1_SIZE,  MEM1_END,   MEM1_ARENA_BEGIN, MEM1_ARENA_END,
         MEM2_SIZE,  MEM2_SIZE,  0x93600000,       MEM2_ARENA_BEGIN,
         0x935E0000, 0x935E0000, 0x93600000,       HOLLYWOOD_REVISION,
         RAM_VENDOR, 0x93600000, 0x93620000,       0,
     },
     {
         43,         0x2b0e17,   0x030110,         MEM1_SIZE,
         MEM1_SIZE,  MEM1_END,   MEM1_ARENA_BEGIN, MEM1_ARENA_END,
         MEM2_SIZE,  MEM2_SIZE,  0x93600000,       MEM2_ARENA_BEGIN,
         0x935E0000, 0x935E0000, 0x93600000,       HOLLYWOOD_REVISION,
         RAM_VENDOR, 0x93600000, 0x93620000,       0,
     },
     {
         45,         0x2d0e17,   0x030110,         MEM1_SIZE,
         MEM1_SIZE,  MEM1_END,   MEM1_ARENA_BEGIN, MEM1_ARENA_END,
         MEM2_SIZE,  MEM2_SIZE,  0x93600000,       MEM2_ARENA_BEGIN,
         0x935E0000, 0x935E0000, 0x93600000,       HOLLYWOOD_REVISION,
         RAM_VENDOR, 0x93600000, 0x93620000,       0,
     },
     {
         46,         0x2e0e17,   0x030110,         MEM1_SIZE,
         MEM1_SIZE,  MEM1_END,   MEM1_ARENA_BEGIN, MEM1_ARENA_END,
         MEM2_SIZE,  MEM2_SIZE,  0x93600000,       MEM2_ARENA_BEGIN,
         0x935E0000, 0x935E0000, 0x93600000,       HOLLYWOOD_REVISION,
         RAM_VENDOR, 0x93600000, 0x93620000,       0,
     },
     {
         48,         0x30101c,   0x030110,         MEM1_SIZE,
         MEM1_SIZE,  MEM1_END,   MEM1_ARENA_BEGIN, MEM1_ARENA_END,
         MEM2_SIZE,  MEM2_SIZE,  0x93600000,       MEM2_ARENA_BEGIN,
         0x935E0000, 0x935E0000, 0x93600000,       HOLLYWOOD_REVISION,
         RAM_VENDOR, 0x93600000, 0x93620000,       0,
     },
     {
         50,         0x321319,   0x101008,         MEM1_SIZE,
         MEM1_SIZE,  MEM1_END,   MEM1_ARENA_BEGIN, MEM1_ARENA_END,
         MEM2_SIZE,  MEM2_SIZE,  0x93600000,       MEM2_ARENA_BEGIN,
         0x935E0000, 0x935E0000, 0x93600000,       HOLLYWOOD_REVISION,
         RAM_VENDOR, 0x93600000, 0x93620000,       0,
     },
     {
         51,         0x331219,   0x071108,         MEM1_SIZE,
         MEM1_SIZE,  MEM1_END,   MEM1_ARENA_BEGIN, MEM1_ARENA_END,
         MEM2_SIZE,  MEM2_SIZE,  0x93600000,       MEM2_ARENA_BEGIN,
         0x935E0000, 0x935E0000, 0x93600000,       HOLLYWOOD_REVISION,
         RAM_VENDOR, 0x93600000, 0x93620000,       0,
     },
     {
         52,         0x34161d,   0x101008,         MEM1_SIZE,
         MEM1_SIZE,  MEM1_END,   MEM1_ARENA_BEGIN, MEM1_ARENA_END,
         MEM2_SIZE,  MEM2_SIZE,  0x93600000,       MEM2_ARENA_BEGIN,
         0x935E0000, 0x935E0000, 0x93600000,       HOLLYWOOD_REVISION,
         RAM_VENDOR, 0x93600000, 0x93620000,       0,
     },
     {
         53,         0x35161f,   0x030110,         MEM1_SIZE,
         MEM1_SIZE,  MEM1_END,   MEM1_ARENA_BEGIN, MEM1_ARENA_END,
         MEM2_SIZE,  MEM2_SIZE,  0x93600000,       MEM2_ARENA_BEGIN,
         0x935E0000, 0x935E0000, 0x93600000,       HOLLYWOOD_REVISION,
         RAM_VENDOR, 0x93600000, 0x93620000,       0,
     },
     {
         55,         0x37161f,   0x030110,         MEM1_SIZE,
         MEM1_SIZE,  MEM1_END,   MEM1_ARENA_BEGIN, MEM1_ARENA_END,
         MEM2_SIZE,  MEM2_SIZE,  0x93600000,       MEM2_ARENA_BEGIN,
         0x935E0000, 0x935E0000, 0x93600000,       HOLLYWOOD_REVISION,
         RAM_VENDOR, 0x93600000, 0x93620000,       0,
     },
     {
         56,         0x38161e,   0x030110,         MEM1_SIZE,
         MEM1_SIZE,  MEM1_END,   MEM1_ARENA_BEGIN, MEM1_ARENA_END,
         MEM2_SIZE,  MEM2_SIZE,  0x93600000,       MEM2_ARENA_BEGIN,
         0x935E0000, 0x935E0000, 0x93600000,       HOLLYWOOD_REVISION,
         RAM_VENDOR, 0x93600000, 0x93620000,       0,
     },
     {
         57,         0x39171f,   0x030110,         MEM1_SIZE,
         MEM1_SIZE,  MEM1_END,   MEM1_ARENA_BEGIN, MEM1_ARENA_END,
         MEM2_SIZE,  MEM2_SIZE,  0x93600000,       MEM2_ARENA_BEGIN,
         0x935E0000, 0x935E0000, 0x93600000,       HOLLYWOOD_REVISION,
         RAM_VENDOR, 0x93600000, 0x93620000,       0,
     },
     {
         58,         0x3a1820,   0x030110,         MEM1_SIZE,
         MEM1_SIZE,  MEM1_END,   MEM1_ARENA_BEGIN, MEM1_ARENA_END,
         MEM2_SIZE,  MEM2_SIZE,  0x93600000,       MEM2_ARENA_BEGIN,
         0x935E0000, 0x935E0000, 0x93600000,       HOLLYWOOD_REVISION,
         RAM_VENDOR, 0x93600000, 0x93620000,       0,
     },
     {
         59,         0x3b2421,   0x101811,         MEM1_SIZE,
         MEM1_SIZE,  MEM1_END,   MEM1_ARENA_BEGIN, MEM1_ARENA_END,
         MEM2_SIZE,  MEM2_SIZE,  0x93600000,       MEM2_ARENA_BEGIN,
         0x935E0000, 0x935E0000, 0x93600000,       HOLLYWOOD_REVISION,
         RAM_VENDOR, 0x93600000, 0x93620000,       0,
     },
     {
         60,         0x3c181e,   0x112408,         MEM1_SIZE,
         MEM1_SIZE,  MEM1_END,   MEM1_ARENA_BEGIN, MEM1_ARENA_END,
         MEM2_SIZE,  MEM2_SIZE,  0x93600000,       MEM2_ARENA_BEGIN,
         0x935E0000, 0x935E0000, 0x93600000,       HOLLYWOOD_REVISION,
         RAM_VENDOR, 0x93600000, 0x93620000,       0,
     },
     {
         61,         0x3d161e,   0x030110,         MEM1_SIZE,
         MEM1_SIZE,  MEM1_END,   MEM1_ARENA_BEGIN, MEM1_ARENA_END,
         MEM2_SIZE,  MEM2_SIZE,  0x93600000,       MEM2_ARENA_BEGIN,
         0x935E0000, 0x935E0000, 0x93600000,       HOLLYWOOD_REVISION,
         RAM_VENDOR, 0x93600000, 0x93620000,       0,
     },
     {
         62,         0x3e191e,   0x022712,         MEM1_SIZE,
         MEM1_SIZE,  MEM1_END,   MEM1_ARENA_BEGIN, MEM1_ARENA_END,
         MEM2_SIZE,  MEM2_SIZE,  0x93600000,       MEM2_ARENA_BEGIN,
         0x935E0000, 0x935E0000, 0x93600000,       HOLLYWOOD_REVISION,
         RAM_VENDOR, 0x93600000, 0x93620000,       0,
     },
     {
         70,         0x461a1f,   0x060309,         MEM1_SIZE,
         MEM1_SIZE,  MEM1_END,   MEM1_ARENA_BEGIN, MEM1_ARENA_END,
         MEM2_SIZE,  MEM2_SIZE,  0x93600000,       MEM2_ARENA_BEGIN,
         0x935E0000, 0x935E0000, 0x93600000,       HOLLYWOOD_REVISION,
         RAM_VENDOR, 0x93600000, 0x93620000,       0,
     },
     {
         80,         0x501b20,   0x030310,         MEM1_SIZE,
         MEM1_SIZE,  MEM1_END,   MEM1_ARENA_BEGIN, MEM1_ARENA_END,
         MEM2_SIZE,  MEM2_SIZE,  0x93600000,       MEM2_ARENA_BEGIN,
         0x935E0000, 0x935E0000, 0x93600000,       HOLLYWOOD_REVISION,
         RAM_VENDOR, 0x93600000, 0x93620000,       0,
     },
     {
         257,
         0x707,
         0x82209,
         MEM1_SIZE,
         MEM1_SIZE,
         MEM1_END,
         MEM1_ARENA_BEGIN,
         MEM1_ARENA_END,
         MEM2_SIZE,
         MEM2_SIZE,
         0x93600000,
         MEM2_ARENA_BEGIN,
         0x935E0000,
         0x935E0000,
         0x93600000,
         HOLLYWOOD_REVISION,
         RAM_VENDOR_MIOS,
         PLACEHOLDER,
         PLACEHOLDER,
         PLACEHOLDER,
     }}};

const std::array<MemoryValues, 41>& GetMemoryValues()
{
  return ios_memory_values;
}

Feature GetFeatures(u32 version)
{
  // Common features that are present in most versions.
  Feature features = Feature::Core | Feature::SDIO | Feature::SO | Feature::Ethernet;

  // IOS4 is a tiny IOS that was presumably used during manufacturing. It lacks network support.
  if (version != 4)
    features |= Feature::KD | Feature::SSL | Feature::NCD | Feature::WiFi;

  if (version == 48 || (version >= 56 && version <= 62) || version == 70 || version == 80)
    features |= Feature::SDv2;

  if (version == 57 || version == 58 || version == 59)
    features |= Feature::NewUSB;
  if (version == 58 || version == 59)
    features |= Feature::EHCI;
  if (version == 59)
    features |= Feature::WFS;

  // No IOS earlier than IOS30 has USB_KBD. Any IOS with the new USB modules lacks this module.
  // TODO(IOS): it is currently unknown which other versions don't have it.
  if (version >= 30 && !HasFeature(features, Feature::NewUSB))
    features |= Feature::USB_KBD;

  // Just like KBD, USB_HIDv4 is not present on any IOS with the new USB modules
  // (since it's been replaced with USB_HIDv5 there).
  // Additionally, it appears that HIDv4 and KBD are never both present.
  // TODO(IOS): figure out which versions have HIDv4. For now we just include both KBD and HIDv4.
  if (!HasFeature(features, Feature::NewUSB))
    features |= Feature::USB_HIDv4;

  return features;
}

bool HasFeature(u32 major_version, Feature feature)
{
  return HasFeature(GetFeatures(major_version), feature);
}

bool IsEmulated(u32 major_version)
{
  if (major_version == static_cast<u32>(Titles::BC & 0xffffffff))
    return true;

  return std::ranges::any_of(ios_memory_values, [major_version](const MemoryValues& values) {
    return values.ios_number == major_version;
  });
}

bool IsEmulated(u64 title_id)
{
  const bool ios = IsTitleType(title_id, ES::TitleType::System) && title_id != Titles::SYSTEM_MENU;
  if (!ios)
    return true;
  const u32 version = static_cast<u32>(title_id);
  return IsEmulated(version);
}
}  // namespace IOS::HLE
