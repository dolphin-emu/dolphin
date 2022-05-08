// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <optional>
#include <string>
#include <vector>

#include "DiscIO/Blob.h"
#include "DiscIO/WIABlob.h"
#include "DolphinTool/Command.h"

namespace DolphinTool
{
class ConvertCommand final : public Command
{
public:
  int Main(const std::vector<std::string>& args) override;

private:
  std::optional<DiscIO::WIARVZCompressionType>
  ParseCompressionTypeString(const std::string& compression_str);
  std::optional<DiscIO::BlobType> ParseFormatString(const std::string& format_str);
};

}  // namespace DolphinTool
