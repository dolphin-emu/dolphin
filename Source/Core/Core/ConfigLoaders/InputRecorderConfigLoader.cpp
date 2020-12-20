// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/ConfigLoaders/InputRecorderConfigLoader.h"

#include <cstring>
#include <memory>
#include <string>

#include "Common/CommonTypes.h"
#include "Common/Config/Config.h"

#include "Core/Config/GraphicsSettings.h"
#include "Core/Config/MainSettings.h"
#include "Core/Config/SYSCONFSettings.h"
#include "Core/ConfigManager.h"
#include "Core/InputRecorder.h"
#include "VideoCommon/VideoConfig.h"

namespace PowerPC
{
enum class CPUCore;
}

namespace ConfigLoaders
{
static void LoadFromDIT(Config::Layer* config_layer, InputRecorder::DITHeader* dit)
{
  config_layer->Set(Config::MAIN_CPU_THREAD, dit->bDualCore);
  config_layer->Set(Config::MAIN_DSP_HLE, dit->bDSPHLE);
  config_layer->Set(Config::MAIN_FAST_DISC_SPEED, dit->bFastDiscSpeed);
  config_layer->Set(Config::MAIN_CPU_CORE, static_cast<PowerPC::CPUCore>(dit->CPUCore));
  config_layer->Set(Config::MAIN_SYNC_GPU, dit->bSyncGPU);
  config_layer->Set(Config::MAIN_GFX_BACKEND, dit->videoBackend.data());

  config_layer->Set(Config::SYSCONF_PROGRESSIVE_SCAN, dit->bProgressive);
  config_layer->Set(Config::SYSCONF_PAL60, dit->bPAL60);
  if (dit->bWii)
    config_layer->Set(Config::SYSCONF_LANGUAGE, static_cast<u32>(dit->language));
  else
    config_layer->Set(Config::MAIN_GC_LANGUAGE, static_cast<int>(dit->language));

  config_layer->Set(Config::GFX_HACK_EFB_ACCESS_ENABLE, dit->bEFBAccessEnable);
  config_layer->Set(Config::GFX_HACK_SKIP_EFB_COPY_TO_RAM, dit->bSkipEFBCopyToRam);
  config_layer->Set(Config::GFX_HACK_EFB_EMULATE_FORMAT_CHANGES, dit->bEFBEmulateFormatChanges);
  config_layer->Set(Config::GFX_HACK_IMMEDIATE_XFB, dit->bImmediateXFB);
  config_layer->Set(Config::GFX_HACK_SKIP_XFB_COPY_TO_RAM, dit->bSkipXFBCopyToRam);
}

void SaveToDIT(InputRecorder::DITHeader* dit)
{
  dit->bDualCore = Config::Get(Config::MAIN_CPU_THREAD);
  dit->bDSPHLE = Config::Get(Config::MAIN_DSP_HLE);
  dit->bFastDiscSpeed = Config::Get(Config::MAIN_FAST_DISC_SPEED);
  dit->CPUCore = static_cast<u8>(Config::Get(Config::MAIN_CPU_CORE));
  dit->bSyncGPU = Config::Get(Config::MAIN_SYNC_GPU);
  const std::string video_backend = Config::Get(Config::MAIN_GFX_BACKEND);

  dit->bProgressive = Config::Get(Config::SYSCONF_PROGRESSIVE_SCAN);
  dit->bPAL60 = Config::Get(Config::SYSCONF_PAL60);
  if (dit->bWii)
    dit->language = Config::Get(Config::SYSCONF_LANGUAGE);
  else
    dit->language = Config::Get(Config::MAIN_GC_LANGUAGE);

  dit->bEFBAccessEnable = Config::Get(Config::GFX_HACK_EFB_ACCESS_ENABLE);
  dit->bSkipEFBCopyToRam = Config::Get(Config::GFX_HACK_SKIP_EFB_COPY_TO_RAM);
  dit->bEFBEmulateFormatChanges = Config::Get(Config::GFX_HACK_EFB_EMULATE_FORMAT_CHANGES);
  dit->bImmediateXFB = Config::Get(Config::GFX_HACK_IMMEDIATE_XFB);
  dit->bSkipXFBCopyToRam = Config::Get(Config::GFX_HACK_SKIP_XFB_COPY_TO_RAM);

  // This never used the regular config
  dit->bSkipIdle = true;
  dit->bEFBCopyEnable = true;
  dit->bEFBCopyCacheEnable = false;

  strncpy(dit->videoBackend.data(), video_backend.c_str(), dit->videoBackend.size());
}

// TODO: Future project, let this support all the configuration options.
// This will require a large break to the DIT format
void InputRecorderConfigLayerLoader::Load(Config::Layer* config_layer)
{
  LoadFromDIT(config_layer, m_header);
}

void InputRecorderConfigLayerLoader::Save(Config::Layer* config_layer)
{
}

// Loader generation
std::unique_ptr<Config::ConfigLayerLoader>
GenerateInputRecorderConfigLoader(InputRecorder::DITHeader* header)
{
  return std::make_unique<InputRecorderConfigLayerLoader>(header);
}
}  // namespace ConfigLoaders
