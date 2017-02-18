// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/Config.h"
#include "Core/Movie.h"

class MovieConfigLayerLoader : public Config::ConfigLayerLoader
{
public:
  MovieConfigLayerLoader(Movie::DTMHeader* header)
      : ConfigLayerLoader(Config::LayerType::Movie), m_header(header)
  {
  }

  void Load(Config::Layer* config_layer) override;
  void Save(Config::Layer* config_layer) override;

  void ChangeDTMHeader(Movie::DTMHeader* header) { m_header = header; }
private:
  Movie::DTMHeader* m_header;
};

Config::ConfigLayerLoader* GenerateMovieConfigLoader(Movie::DTMHeader* header);
