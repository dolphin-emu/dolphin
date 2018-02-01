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
#include "Common/Logging/Log.h"

#include "Core/Config/GraphicsSettings.h"
#include "Core/Config/MainSettings.h"
#include "Core/Config/SYSCONFSettings.h"
#include "Core/ConfigManager.h"
#include "Core/Movie.h"
#include "VideoCommon/VideoConfig.h"

namespace ConfigLoaders
{
static void LoadFromDTM(Config::Layer* config_layer, Movie::DTMHeader* dtm)
{
  config_layer->Set(Config::MAIN_SKIP_IDLE, dtm->bSkipIdle); // "SkipIdle"
  config_layer->Set(Config::MAIN_CPU_THREAD, dtm->bDualCore);
  config_layer->Set(Config::MAIN_DSP_HLE, dtm->bDSPHLE);
  config_layer->Set(Config::MAIN_FAST_DISC_SPEED, dtm->bFastDiscSpeed);
  config_layer->Set(Config::MAIN_CPU_CORE, static_cast<int>(dtm->CPUCore));
  config_layer->Set(Config::MAIN_SYNC_GPU, dtm->bSyncGPU);
  config_layer->Set(Config::MAIN_GFX_BACKEND,
                    std::string(reinterpret_cast<char*>(dtm->videoBackend)));

  config_layer->Set(Config::SYSCONF_PROGRESSIVE_SCAN, dtm->bProgressive);
  config_layer->Set(Config::SYSCONF_PAL60, dtm->bPAL60);
  if (dtm->bWii)
    config_layer->Set(Config::SYSCONF_LANGUAGE, static_cast<u32>(dtm->language));
  else
    config_layer->Set(Config::MAIN_GC_LANGUAGE, static_cast<int>(dtm->language));

  config_layer->Set(Config::GFX_USE_XFB, dtm->bUseXFB);
  config_layer->Set(Config::GFX_USE_REAL_XFB, dtm->bUseRealXFB);
  config_layer->Set(Config::GFX_HACK_EFB_ACCESS_ENABLE, dtm->bEFBAccessEnable);
  config_layer->Set(Config::GFX_HACK_SKIP_EFB_COPY_TO_RAM, dtm->bSkipEFBCopyToRam);
  config_layer->Set(Config::GFX_HACK_EFB_EMULATE_FORMAT_CHANGES, dtm->bEFBEmulateFormatChanges);
}

void SaveToDTM(Movie::DTMHeader* dtm)
{
  dtm->bDualCore = Config::Get(Config::MAIN_CPU_THREAD);
  dtm->bDSPHLE = Config::Get(Config::MAIN_DSP_HLE);
  dtm->bFastDiscSpeed = Config::Get(Config::MAIN_FAST_DISC_SPEED);
  dtm->CPUCore = Config::Get(Config::MAIN_CPU_CORE);
  dtm->bSyncGPU = Config::Get(Config::MAIN_SYNC_GPU);
  const std::string video_backend = Config::Get(Config::MAIN_GFX_BACKEND);

  dtm->bProgressive = Config::Get(Config::SYSCONF_PROGRESSIVE_SCAN);
  dtm->bPAL60 = Config::Get(Config::SYSCONF_PAL60);
  if (dtm->bWii)
    dtm->language = Config::Get(Config::SYSCONF_LANGUAGE);
  else
    dtm->language = Config::Get(Config::MAIN_GC_LANGUAGE);

  dtm->bUseXFB = Config::Get(Config::GFX_USE_XFB);
  dtm->bUseRealXFB = Config::Get(Config::GFX_USE_REAL_XFB);
  dtm->bEFBAccessEnable = Config::Get(Config::GFX_HACK_EFB_ACCESS_ENABLE);
  dtm->bSkipEFBCopyToRam = Config::Get(Config::GFX_HACK_SKIP_EFB_COPY_TO_RAM);
  dtm->bEFBEmulateFormatChanges = Config::Get(Config::GFX_HACK_EFB_EMULATE_FORMAT_CHANGES);

  // This never used the regular config
  dtm->bSkipIdle = true;
  dtm->bEFBCopyEnable = true;
  dtm->bEFBCopyCacheEnable = false;

  strncpy(reinterpret_cast<char*>(dtm->videoBackend), video_backend.c_str(),
          ArraySize(dtm->videoBackend));
}

// TODO: Future project, let this support all the configuration options.
// This will require a large break to the DTM format
void MovieConfigLayerLoader::Load(Config::Layer* config_layer)
{
  LoadFromDTM(config_layer, m_header);
}

void MovieConfigLayerLoader::Save(Config::Layer* config_layer)
{
}

// Loader generation
std::unique_ptr<Config::ConfigLayerLoader> GenerateMovieConfigLoader(Movie::DTMHeader* header)
{
  return std::make_unique<MovieConfigLayerLoader>(header);
}
}
