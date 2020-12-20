// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>

#include "Common/Config/Config.h"

namespace InputRecorder
{
struct DITHeader;
}

namespace ConfigLoaders
{
class InputRecorderConfigLayerLoader final : public Config::ConfigLayerLoader
{
public:
  explicit InputRecorderConfigLayerLoader(InputRecorder::DITHeader* header)
      : ConfigLayerLoader(Config::LayerType::InputRecorder), m_header(header)
  {
  }

  void Load(Config::Layer* config_layer) override;
  void Save(Config::Layer* config_layer) override;

private:
  InputRecorder::DITHeader* m_header;
};

void SaveToDIT(InputRecorder::DITHeader* header);
std::unique_ptr<Config::ConfigLayerLoader>
GenerateInputRecorderConfigLoader(InputRecorder::DITHeader* header);
}  // namespace ConfigLoaders
