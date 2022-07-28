// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <iostream>
#include <string>
#include <vector>

#include "DiscIO/Volume.h"
#include "DiscIO/VolumeDisc.h"
#include "DiscIO/VolumeVerifier.h"
#include "DolphinTool/Command.h"

namespace DolphinTool
{
class VerifyCommand final : public Command
{
public:
  int Main(const std::vector<std::string>& args) override;

private:
  void PrintFullReport(const std::optional<DiscIO::VolumeVerifier::Result> result);

  std::optional<DiscIO::VolumeVerifier::Result>
  VerifyVolume(std::shared_ptr<DiscIO::VolumeDisc> volume,
               const DiscIO::Hashes<bool>& hashes_to_calculate);

  std::string HashToHexString(const std::vector<u8>& hash);
};

}  // namespace DolphinTool
