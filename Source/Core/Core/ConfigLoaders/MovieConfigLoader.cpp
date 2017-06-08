// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/ConfigLoaders/MovieConfigLoader.h"

#include <cstring>
#include <memory>
#include <string>

#include "Common/CommonFuncs.h"
#include "Common/CommonTypes.h"
#include "Common/Config/Config.h"
#include "Common/FileUtil.h"

#include "Core/Movie.h"

namespace ConfigLoaders
{
// TODO: Future project, let this support all the configuration options.
// This will require a large break to the DTM format
void MovieConfigLayerLoader::Load(Config::Layer* config_layer)
{
  Config::Section* core = config_layer->GetOrCreateSection(Config::System::Main, "Core");
  Config::Section* display = config_layer->GetOrCreateSection(Config::System::Main, "Display");
  Config::Section* video_settings = Config::GetOrCreateSection(Config::System::GFX, "Settings");
  Config::Section* video_hacks = Config::GetOrCreateSection(Config::System::GFX, "Hacks");

  core->Set("SkipIdle", m_header->bSkipIdle);
  core->Set("CPUThread", m_header->bDualCore);
  core->Set("DSPHLE", m_header->bDSPHLE);
  core->Set("FastDiscSpeed", m_header->bFastDiscSpeed);
  core->Set("CPUCore", m_header->CPUCore);
  core->Set("SyncGPU", m_header->bSyncGPU);
  core->Set("GFXBackend", std::string(reinterpret_cast<char*>(m_header->videoBackend)));
  display->Set("ProgressiveScan", m_header->bProgressive);
  display->Set("PAL60", m_header->bPAL60);

  video_settings->Set("UseXFB", m_header->bUseXFB);
  video_settings->Set("UseRealXFB", m_header->bUseRealXFB);
  video_hacks->Set("EFBAccessEnable", m_header->bEFBAccessEnable);
  video_hacks->Set("EFBToTextureEnable", m_header->bSkipEFBCopyToRam);
  video_hacks->Set("EFBEmulateFormatChanges", m_header->bEFBEmulateFormatChanges);
}

void MovieConfigLayerLoader::Save(Config::Layer* config_layer)
{
  Config::Section* core = config_layer->GetOrCreateSection(Config::System::Main, "Core");
  Config::Section* display = config_layer->GetOrCreateSection(Config::System::Main, "Display");
  Config::Section* video_settings = Config::GetOrCreateSection(Config::System::GFX, "Settings");
  Config::Section* video_hacks = Config::GetOrCreateSection(Config::System::GFX, "Hacks");

  std::string video_backend;
  u32 cpu_core;

  core->Get("SkipIdle", &m_header->bSkipIdle);
  core->Get("CPUThread", &m_header->bDualCore);
  core->Get("DSPHLE", &m_header->bDSPHLE);
  core->Get("FastDiscSpeed", &m_header->bFastDiscSpeed);
  core->Get("CPUCore", &cpu_core);
  core->Get("SyncGPU", &m_header->bSyncGPU);
  core->Get("GFXBackend", &video_backend);
  display->Get("ProgressiveScan", &m_header->bProgressive);
  display->Get("PAL60", &m_header->bPAL60);

  video_settings->Get("UseXFB", &m_header->bUseXFB);
  video_settings->Get("UseRealXFB", &m_header->bUseRealXFB);
  video_hacks->Get("EFBAccessEnable", &m_header->bEFBAccessEnable);
  video_hacks->Get("EFBToTextureEnable", &m_header->bSkipEFBCopyToRam);
  video_hacks->Get("EFBEmulateFormatChanges", &m_header->bEFBEmulateFormatChanges);

  // This never used the regular config
  m_header->bEFBCopyEnable = true;
  m_header->bEFBCopyCacheEnable = false;

  m_header->CPUCore = cpu_core;
  strncpy(reinterpret_cast<char*>(m_header->videoBackend), video_backend.c_str(),
          ArraySize(m_header->videoBackend));
}

// Loader generation
std::unique_ptr<Config::ConfigLayerLoader> GenerateMovieConfigLoader(Movie::DTMHeader* header)
{
  return std::make_unique<MovieConfigLayerLoader>(header);
}
}
