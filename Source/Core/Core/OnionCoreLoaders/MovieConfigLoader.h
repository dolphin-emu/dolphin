// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/OnionConfig.h"
#include "Core/Movie.h"

class MovieConfigLayerLoader : public OnionConfig::ConfigLayerLoader
{
public:
  MovieConfigLayerLoader(Movie::DTMHeader* header)
      : ConfigLayerLoader(OnionConfig::LayerType::LAYER_MOVIE), m_header(header)
  {
  }

  void Load(OnionConfig::Layer* config_layer) override;
  void Save(OnionConfig::Layer* config_layer) override;

  void ChangeDTMHeader(Movie::DTMHeader* header) { m_header = header; }
private:
  Movie::DTMHeader* m_header;
};

OnionConfig::ConfigLayerLoader* GenerateMovieConfigLoader(Movie::DTMHeader* header);
