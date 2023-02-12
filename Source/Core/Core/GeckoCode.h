// Copyright 2010 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <span>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"

class PointerWrap;

namespace Core
{
class CPUThreadGuard;
};

namespace Gecko
{
class GeckoCode
{
public:
  GeckoCode() = default;
  struct Code
  {
    u32 address = 0;
    u32 data = 0;
    std::string original_line;
  };

  std::vector<Code> codes;
  std::string name, creator;
  std::vector<std::string> notes;

  bool enabled = false;
  bool default_enabled = false;
  bool user_defined = false;

  bool Exist(u32 address, u32 data) const;
};

bool operator==(const GeckoCode& lhs, const GeckoCode& rhs);
bool operator!=(const GeckoCode& lhs, const GeckoCode& rhs);

bool operator==(const GeckoCode::Code& lhs, const GeckoCode::Code& rhs);
bool operator!=(const GeckoCode::Code& lhs, const GeckoCode::Code& rhs);

// Installation address for codehandler.bin in the Game's RAM
constexpr u32 INSTALLER_BASE_ADDRESS = 0x80001800;
constexpr u32 INSTALLER_END_ADDRESS = 0x80003000;
constexpr u32 ENTRY_POINT = INSTALLER_BASE_ADDRESS + 0xA8;
// If the GCT is max-length then this is the second word of the End code (0xF0000000 0x00000000)
// If the table is shorter than the max-length then this address is unused / contains trash.
constexpr u32 HLE_TRAMPOLINE_ADDRESS = INSTALLER_END_ADDRESS - 4;

// This forms part of a communication protocol with HLE_Misc::GeckoCodeHandlerICacheFlush.
// Basically, codehandleronly.s doesn't use ICBI like it's supposed to when patching the
// game's code. This results in the JIT happily ignoring all code patches for blocks that
// are already compiled. The hack for getting around that is that the first 5 frames after
// the handler is installed (0xD01F1BAD -> +5 -> 0xD01F1BB2) cause full ICache resets.
//
// GeckoCodeHandlerICacheFlush will increment this value 5 times then cease flushing the ICache to
// preserve the emulation performance.
constexpr u32 MAGIC_GAMEID = 0xD01F1BAD;

void SetActiveCodes(std::span<const GeckoCode> gcodes);
void SetSyncedCodesAsActive();
void UpdateSyncedCodes(std::span<const GeckoCode> gcodes);
std::vector<GeckoCode> SetAndReturnActiveCodes(std::span<const GeckoCode> gcodes);
void RunCodeHandler(const Core::CPUThreadGuard& guard);
void Shutdown();
void DoState(PointerWrap&);

}  // namespace Gecko
