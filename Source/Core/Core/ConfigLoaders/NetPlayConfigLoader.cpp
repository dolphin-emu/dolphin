// Copyright 2016 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/ConfigLoaders/NetPlayConfigLoader.h"

#include <memory>

#include <fmt/format.h>

#include "Common/CommonPaths.h"
#include "Common/Config/Config.h"
#include "Common/FileUtil.h"

#include "Core/Config/GraphicsSettings.h"
#include "Core/Config/MainSettings.h"
#include "Core/Config/SYSCONFSettings.h"
#include "Core/Config/SessionSettings.h"
#include "Core/HW/EXI/EXI.h"
#include "Core/NetPlayProto.h"

namespace ConfigLoaders
{
class NetPlayConfigLayerLoader final : public Config::ConfigLayerLoader
{
public:
  explicit NetPlayConfigLayerLoader(const NetPlay::NetSettings& settings)
      : ConfigLayerLoader(Config::LayerType::Netplay), m_settings(settings)
  {
  }

  void Load(Config::Layer* layer) override
  {
    layer->Set(Config::MAIN_CPU_THREAD, m_settings.cpu_thread);
    layer->Set(Config::MAIN_CPU_CORE, m_settings.cpu_core);
    layer->Set(Config::MAIN_ENABLE_CHEATS, m_settings.enable_cheats);
    layer->Set(Config::MAIN_GC_LANGUAGE, m_settings.selected_language);
    layer->Set(Config::MAIN_OVERRIDE_REGION_SETTINGS, m_settings.override_region_settings);
    layer->Set(Config::MAIN_DSP_HLE, m_settings.dsp_hle);
    layer->Set(Config::MAIN_OVERCLOCK_ENABLE, m_settings.oc_enable);
    layer->Set(Config::MAIN_OVERCLOCK, m_settings.oc_factor);
    for (ExpansionInterface::Slot slot : ExpansionInterface::SLOTS)
      layer->Set(Config::GetInfoForEXIDevice(slot), m_settings.exi_device[slot]);
    layer->Set(Config::MAIN_MEMORY_CARD_SIZE, m_settings.memcard_size_override);
    layer->Set(Config::SESSION_SAVE_DATA_WRITABLE, m_settings.savedata_write);
    layer->Set(Config::MAIN_RAM_OVERRIDE_ENABLE, m_settings.ram_override_enable);
    layer->Set(Config::MAIN_MEM1_SIZE, m_settings.mem1_size);
    layer->Set(Config::MAIN_MEM2_SIZE, m_settings.mem2_size);
    layer->Set(Config::MAIN_FALLBACK_REGION, m_settings.fallback_region);
    layer->Set(Config::MAIN_ALLOW_SD_WRITES, m_settings.allow_sd_writes);
    layer->Set(Config::MAIN_DSP_JIT, m_settings.dsp_enable_jit);

    for (size_t i = 0; i < Config::SYSCONF_SETTINGS.size(); ++i)
    {
      std::visit(
          [&](auto* info) {
            layer->Set(*info, static_cast<decltype(info->GetDefaultValue())>(
                                  m_settings.sysconf_settings[i]));
          },
          Config::SYSCONF_SETTINGS[i].config_info);
    }

    layer->Set(Config::GFX_HACK_EFB_ACCESS_ENABLE, m_settings.efb_access_enable);
    layer->Set(Config::GFX_HACK_BBOX_ENABLE, m_settings.bbox_enable);
    layer->Set(Config::GFX_HACK_FORCE_PROGRESSIVE, m_settings.force_progressive);
    layer->Set(Config::GFX_HACK_SKIP_EFB_COPY_TO_RAM, m_settings.efb_to_texture_enable);
    layer->Set(Config::GFX_HACK_SKIP_XFB_COPY_TO_RAM, m_settings.xfb_to_texture_enable);
    layer->Set(Config::GFX_HACK_DISABLE_COPY_TO_VRAM, m_settings.disable_copy_to_vram);
    layer->Set(Config::GFX_HACK_IMMEDIATE_XFB, m_settings.immediate_xfb_enable);
    layer->Set(Config::GFX_HACK_EFB_EMULATE_FORMAT_CHANGES, m_settings.efb_emulate_format_changes);
    layer->Set(Config::GFX_SAFE_TEXTURE_CACHE_COLOR_SAMPLES,
               m_settings.safe_texture_cache_color_samples);
    layer->Set(Config::GFX_PERF_QUERIES_ENABLE, m_settings.perf_queries_enable);
    layer->Set(Config::MAIN_FLOAT_EXCEPTIONS, m_settings.float_exceptions);
    layer->Set(Config::MAIN_DIVIDE_BY_ZERO_EXCEPTIONS, m_settings.divide_by_zero_exceptions);
    layer->Set(Config::MAIN_FPRF, m_settings.fprf);
    layer->Set(Config::MAIN_ACCURATE_NANS, m_settings.accurate_nans);
    layer->Set(Config::MAIN_DISABLE_ICACHE, m_settings.disable_icache);
    layer->Set(Config::MAIN_SYNC_ON_SKIP_IDLE, m_settings.sync_on_skip_idle);
    layer->Set(Config::MAIN_SYNC_GPU, m_settings.sync_gpu);
    layer->Set(Config::MAIN_SYNC_GPU_MAX_DISTANCE, m_settings.sync_gpu_max_distance);
    layer->Set(Config::MAIN_SYNC_GPU_MIN_DISTANCE, m_settings.sync_gpu_min_distance);
    layer->Set(Config::MAIN_SYNC_GPU_OVERCLOCK, m_settings.sync_gpu_overclock);

    layer->Set(Config::MAIN_JIT_FOLLOW_BRANCH, m_settings.jit_follow_branch);
    layer->Set(Config::MAIN_FAST_DISC_SPEED, m_settings.fast_disc_speed);
    layer->Set(Config::MAIN_MMU, m_settings.mmu);
    layer->Set(Config::MAIN_FASTMEM, m_settings.fastmem);
    layer->Set(Config::MAIN_SKIP_IPL, m_settings.skip_ipl);
    layer->Set(Config::SESSION_LOAD_IPL_DUMP, m_settings.load_ipl_dump);

    layer->Set(Config::GFX_HACK_DEFER_EFB_COPIES, m_settings.defer_efb_copies);
    layer->Set(Config::GFX_HACK_EFB_ACCESS_TILE_SIZE, m_settings.efb_access_tile_size);
    layer->Set(Config::GFX_HACK_EFB_DEFER_INVALIDATION, m_settings.efb_access_defer_invalidation);

    layer->Set(Config::SESSION_USE_FMA, m_settings.use_fma);

    layer->Set(Config::MAIN_BLUETOOTH_PASSTHROUGH_ENABLED, false);

    if (m_settings.strict_settings_sync)
    {
      layer->Set(Config::GFX_HACK_VERTEX_ROUNDING, m_settings.vertex_rounding);
      layer->Set(Config::GFX_EFB_SCALE, m_settings.internal_resolution);
      layer->Set(Config::GFX_HACK_COPY_EFB_SCALED, m_settings.efb_scaled_copy);
      layer->Set(Config::GFX_FAST_DEPTH_CALC, m_settings.fast_depth_calc);
      layer->Set(Config::GFX_ENABLE_PIXEL_LIGHTING, m_settings.enable_pixel_lighting);
      layer->Set(Config::GFX_WIDESCREEN_HACK, m_settings.widescreen_hack);
      layer->Set(Config::GFX_ENHANCE_FORCE_TEXTURE_FILTERING, m_settings.force_texture_filtering);
      layer->Set(Config::GFX_ENHANCE_MAX_ANISOTROPY, m_settings.max_anisotropy);
      layer->Set(Config::GFX_ENHANCE_FORCE_TRUE_COLOR, m_settings.force_true_color);
      layer->Set(Config::GFX_ENHANCE_DISABLE_COPY_FILTER, m_settings.disable_copy_filter);
      layer->Set(Config::GFX_DISABLE_FOG, m_settings.disable_fog);
      layer->Set(Config::GFX_ENHANCE_ARBITRARY_MIPMAP_DETECTION,
                 m_settings.arbitrary_mipmap_detection);
      layer->Set(Config::GFX_ENHANCE_ARBITRARY_MIPMAP_DETECTION_THRESHOLD,
                 m_settings.arbitrary_mipmap_detection_threshold);
      layer->Set(Config::GFX_ENABLE_GPU_TEXTURE_DECODING, m_settings.enable_gpu_texture_decoding);

      // Disable AA as it isn't deterministic across GPUs
      layer->Set(Config::GFX_MSAA, 1);
      layer->Set(Config::GFX_SSAA, false);
    }

    if (m_settings.savedata_load)
    {
      if (!m_settings.is_hosting)
      {
        const std::string path = File::GetUserPath(D_GCUSER_IDX) + GC_MEMCARD_NETPLAY DIR_SEP;
        layer->Set(Config::MAIN_GCI_FOLDER_A_PATH_OVERRIDE, path + "Card A");
        layer->Set(Config::MAIN_GCI_FOLDER_B_PATH_OVERRIDE, path + "Card B");

        const auto make_memcard_path = [this](char letter) {
          return fmt::format("{}{}{}.{}.raw", File::GetUserPath(D_GCUSER_IDX), GC_MEMCARD_NETPLAY,
                             letter, m_settings.save_data_region);
        };
        layer->Set(Config::MAIN_MEMCARD_A_PATH, make_memcard_path('A'));
        layer->Set(Config::MAIN_MEMCARD_B_PATH, make_memcard_path('B'));
      }

      layer->Set(Config::SESSION_GCI_FOLDER_CURRENT_GAME_ONLY, true);
    }

#ifdef HAS_LIBMGBA
    for (size_t i = 0; i < m_settings.gba_rom_paths.size(); ++i)
    {
      layer->Set(Config::MAIN_GBA_ROM_PATHS[i], m_settings.gba_rom_paths[i]);
    }
#endif

    // Check To Override Client's Cheat Codes
    if (m_settings.sync_codes && !m_settings.is_hosting)
    {
      // Raise flag to use host's codes
      layer->Set(Config::SESSION_CODE_SYNC_OVERRIDE, true);
    }
  }

  void Save(Config::Layer* layer) override
  {
    // Do Nothing
  }

private:
  const NetPlay::NetSettings m_settings;
};

// Loader generation
std::unique_ptr<Config::ConfigLayerLoader>
GenerateNetPlayConfigLoader(const NetPlay::NetSettings& settings)
{
  return std::make_unique<NetPlayConfigLayerLoader>(settings);
}
}  // namespace ConfigLoaders
