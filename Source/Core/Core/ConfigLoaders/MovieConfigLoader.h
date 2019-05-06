// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>

#include "Common/Config/Config.h"

namespace Movie
{
struct DTMHeader;
}

namespace ConfigLoaders
{
class MovieConfigLayerLoader final : public Config::ConfigLayerLoader
{
public:
  explicit MovieConfigLayerLoader(Movie::DTMHeader* header)
      : ConfigLayerLoader(Config::LayerType::Movie), m_header(header)
  {
  }

  void Load(Config::Layer* config_layer) override;
  void Save(Config::Layer* config_layer) override;

private:
  Movie::DTMHeader* m_header;
};

void SaveToDTM(Movie::DTMHeader* header);
std::unique_ptr<Config::ConfigLayerLoader> GenerateMovieConfigLoader(Movie::DTMHeader* header);
}  // namespace ConfigLoaders
