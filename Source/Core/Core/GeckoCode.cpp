// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include <iterator>
#include <mutex>
#include <vector>

#include "Common/CommonPaths.h"
#include "Common/FileUtil.h"
#include "Common/Thread.h"

#include "Core/ConfigManager.h"
#include "Core/GeckoCode.h"
#include "Core/HW/Memmap.h"
#include "Core/PowerPC/PowerPC.h"

namespace Gecko
{
static constexpr u32 INSTALLER_END_ADDRESS = 0x80003000;
static constexpr u32 CODE_SIZE = 8;

// return true if a code exists
bool GeckoCode::Exist(u32 address, u32 data) const
{
  return std::find_if(codes.begin(), codes.end(), [&](const Code& code) {
           return code.address == address && code.data == data;
         }) != codes.end();
}

// return true if the code is identical
bool GeckoCode::Compare(const GeckoCode& compare) const
{
  return codes.size() == compare.codes.size() &&
         std::equal(codes.begin(), codes.end(), compare.codes.begin(),
                    [](const Code& a, const Code& b) {
                      return a.address == b.address && a.data == b.data;
                    });
}

enum class Installation
{
  Uninstalled,
  Installed,
  Failed
};

static Installation s_code_handler_installed = Installation::Uninstalled;
// the currently active codes
static std::vector<GeckoCode> s_active_codes;
static std::mutex s_active_codes_lock;

void SetActiveCodes(const std::vector<GeckoCode>& gcodes)
{
  std::lock_guard<std::mutex> lk(s_active_codes_lock);

  s_active_codes.clear();
  s_active_codes.reserve(gcodes.size());
  std::copy_if(gcodes.begin(), gcodes.end(), std::back_inserter(s_active_codes),
               [](const GeckoCode& code) { return code.enabled; });
  s_active_codes.shrink_to_fit();

  s_code_handler_installed = Installation::Uninstalled;
}

// Requires s_active_codes_lock
// NOTE: Refer to "codehandleronly.s" from Gecko OS.
static Installation InstallCodeHandlerLocked()
{
  std::string data;
  if (!File::ReadFileToString(File::GetSysDirectory() + GECKO_CODE_HANDLER, data))
  {
    ERROR_LOG(ACTIONREPLAY, "Could not enable cheats because " GECKO_CODE_HANDLER " was missing.");
    return Installation::Failed;
  }

  if (data.size() > INSTALLER_END_ADDRESS - INSTALLER_BASE_ADDRESS - CODE_SIZE)
  {
    ERROR_LOG(ACTIONREPLAY, GECKO_CODE_HANDLER " is too big. The file may be corrupt.");
    return Installation::Failed;
  }

  u8 mmio_addr = 0xCC;
  if (SConfig::GetInstance().bWii)
  {
    mmio_addr = 0xCD;
  }

  // Install code handler
  for (u32 i = 0; i < data.size(); ++i)
    PowerPC::HostWrite_U8(data[i], INSTALLER_BASE_ADDRESS + i);

  // Patch the code handler to the current system type (Gamecube/Wii)
  for (unsigned int h = 0; h < data.length(); h += 4)
  {
    // Patch MMIO address
    if (PowerPC::HostRead_U32(INSTALLER_BASE_ADDRESS + h) == (0x3f000000u | ((mmio_addr ^ 1) << 8)))
    {
      NOTICE_LOG(ACTIONREPLAY, "Patching MMIO access at %08x", INSTALLER_BASE_ADDRESS + h);
      PowerPC::HostWrite_U32(0x3f000000u | mmio_addr << 8, INSTALLER_BASE_ADDRESS + h);
    }
  }

  const u32 codelist_base_address =
      INSTALLER_BASE_ADDRESS + static_cast<u32>(data.size()) - CODE_SIZE;
  const u32 codelist_end_address = INSTALLER_END_ADDRESS;

  // Write a magic value to 'gameid' (codehandleronly does not actually read this).
  // This value will be read back and modified over time by HLE_Misc::HLEGeckoCodehandler.
  PowerPC::HostWrite_U32(MAGIC_GAMEID, INSTALLER_BASE_ADDRESS);

  // Create GCT in memory
  PowerPC::HostWrite_U32(0x00d0c0de, codelist_base_address);
  PowerPC::HostWrite_U32(0x00d0c0de, codelist_base_address + 4);

  // Each code is 8 bytes (2 words) wide. There is a starter code and an end code.
  const u32 start_address = codelist_base_address + CODE_SIZE;
  const u32 end_address = codelist_end_address - CODE_SIZE;
  u32 next_address = start_address;

  // NOTE: Only active codes are in the list
  for (const GeckoCode& active_code : s_active_codes)
  {
    // If the code is not going to fit in the space we have left then we have to skip it
    if (next_address + active_code.codes.size() * CODE_SIZE > end_address)
    {
      NOTICE_LOG(ACTIONREPLAY, "Too many GeckoCodes! Ran out of storage space in Game RAM. Could "
                               "not write: \"%s\". Need %zu bytes, only %u remain.",
                 active_code.name.c_str(), active_code.codes.size() * CODE_SIZE,
                 end_address - next_address);
      continue;
    }

    for (const GeckoCode::Code& code : active_code.codes)
    {
      PowerPC::HostWrite_U32(code.address, next_address);
      PowerPC::HostWrite_U32(code.data, next_address + 4);
      next_address += CODE_SIZE;
    }
  }

  WARN_LOG(ACTIONREPLAY, "GeckoCodes: Using %u of %u bytes", next_address - start_address,
           end_address - start_address);

  // Stop code. Tells the handler that this is the end of the list.
  PowerPC::HostWrite_U32(0xF0000000, next_address);
  PowerPC::HostWrite_U32(0x00000000, next_address + 4);

  // Turn on codes
  PowerPC::HostWrite_U8(1, INSTALLER_BASE_ADDRESS + 7);

  // Invalidate the icache and any asm codes
  for (unsigned int j = 0; j < (INSTALLER_END_ADDRESS - INSTALLER_BASE_ADDRESS); j += 32)
  {
    PowerPC::ppcState.iCache.Invalidate(INSTALLER_BASE_ADDRESS + j);
  }
  return Installation::Installed;
}

void RunCodeHandler()
{
  if (!SConfig::GetInstance().bEnableCheats)
    return;

  std::lock_guard<std::mutex> codes_lock(s_active_codes_lock);
  // Don't spam retry if the install failed. The corrupt / missing disk file is not likely to be
  // fixed within 1 frame of the last error.
  if (s_active_codes.empty() || s_code_handler_installed == Installation::Failed)
    return;

  if (s_code_handler_installed != Installation::Installed)
  {
    s_code_handler_installed = InstallCodeHandlerLocked();

    // A warning was already issued for the install failing
    if (s_code_handler_installed != Installation::Installed)
      return;
  }

  // If the last block that just executed ended with a BLR instruction then we can intercept it and
  // redirect control into the Gecko Code Handler. The Code Handler will automatically BLR back to
  // the original return address (which will still be in the link register) at the end.
  if (PC == LR)
  {
    PC = NPC = ENTRY_POINT;
  }
}

}  // namespace Gecko
