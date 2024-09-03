// Copyright 2010 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/TextureCacheBase.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#if defined(_M_X86_64)
#include <pmmintrin.h>
#endif

#include <fmt/format.h>

#include "Common/Align.h"
#include "Common/Assert.h"
#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/Hash.h"
#include "Common/Logging/Log.h"
#include "Common/MathUtil.h"
#include "Common/MemoryUtil.h"

#include "Core/Config/GraphicsSettings.h"
#include "Core/ConfigManager.h"
#include "Core/FifoPlayer/FifoPlayer.h"
#include "Core/FifoPlayer/FifoRecorder.h"
#include "Core/HW/Memmap.h"
#include "Core/System.h"

#include "VideoCommon/AbstractFramebuffer.h"
#include "VideoCommon/AbstractGfx.h"
#include "VideoCommon/AbstractStagingTexture.h"
#include "VideoCommon/Assets/CustomTextureData.h"
#include "VideoCommon/BPMemory.h"
#include "VideoCommon/FramebufferManager.h"
#include "VideoCommon/GraphicsModSystem/Runtime/FBInfo.h"
#include "VideoCommon/GraphicsModSystem/Runtime/GraphicsModActionData.h"
#include "VideoCommon/GraphicsModSystem/Runtime/GraphicsModManager.h"
#include "VideoCommon/HiresTextures.h"
#include "VideoCommon/OpcodeDecoding.h"
#include "VideoCommon/PixelShaderManager.h"
#include "VideoCommon/Present.h"
#include "VideoCommon/ShaderCache.h"
#include "VideoCommon/Statistics.h"
#include "VideoCommon/TMEM.h"
#include "VideoCommon/TextureConversionShader.h"
#include "VideoCommon/TextureConverterShaderGen.h"
#include "VideoCommon/TextureDecoder.h"
#include "VideoCommon/VertexManagerBase.h"
#include "VideoCommon/VideoCommon.h"
#include "VideoCommon/VideoConfig.h"

static const u64 TEXHASH_INVALID = 0;
// Sonic the Fighters (inside Sonic Gems Collection) loops a 64 frames animation
static const int TEXTURE_KILL_THRESHOLD = 64;
static const int TEXTURE_POOL_KILL_THRESHOLD = 3;

static int xfb_count = 0;

std::unique_ptr<TextureCacheBase> g_texture_cache;

TCacheEntry::TCacheEntry(std::unique_ptr<AbstractTexture> tex,
                         std::unique_ptr<AbstractFramebuffer> fb)
    : texture(std::move(tex)), framebuffer(std::move(fb))
{
}

TCacheEntry::~TCacheEntry()
{
  for (auto& reference : references)
    reference->references.erase(this);
  ASSERT_MSG(VIDEO, g_texture_cache, "Texture cache destroyed before TCacheEntry was destroyed");
  g_texture_cache->ReleaseToPool(this);
}

void TextureCacheBase::CheckTempSize(size_t required_size)
{
  if (required_size <= m_temp_size)
    return;

  m_temp_size = required_size;
  Common::FreeAlignedMemory(m_temp);
  m_temp = static_cast<u8*>(Common::AllocateAlignedMemory(m_temp_size, 16));
}

TextureCacheBase::TextureCacheBase()
{
  SetBackupConfig(g_ActiveConfig);

  m_temp_size = 2048 * 2048 * 4;
  m_temp = static_cast<u8*>(Common::AllocateAlignedMemory(m_temp_size, 16));

  TexDecoder_SetTexFmtOverlayOptions(m_backup_config.texfmt_overlay,
                                     m_backup_config.texfmt_overlay_center);

  HiresTexture::Init();

  TMEM::InvalidateAll();
}

void TextureCacheBase::Shutdown()
{
  // Clear pending EFB copies first, so we don't try to flush them.
  m_pending_efb_copies.clear();

  HiresTexture::Shutdown();

  // For correctness, we need to invalidate textures before the gpu context starts shutting down.
  Invalidate();
}

TextureCacheBase::~TextureCacheBase()
{
  Common::FreeAlignedMemory(m_temp);
  m_temp = nullptr;
}

bool TextureCacheBase::Initialize()
{
  if (!CreateUtilityTextures())
  {
    PanicAlertFmt("Failed to create utility textures.");
    return false;
  }

  return true;
}

void TextureCacheBase::Invalidate()
{
  FlushEFBCopies();
  TMEM::InvalidateAll();

  for (auto& bind : m_bound_textures)
    bind.reset();
  m_textures_by_hash.clear();
  m_textures_by_address.clear();

  m_texture_pool.clear();
}

void TextureCacheBase::OnConfigChanged(const VideoConfig& config)
{
  if (config.bHiresTextures != m_backup_config.hires_textures ||
      config.bCacheHiresTextures != m_backup_config.cache_hires_textures)
  {
    HiresTexture::Update();
  }

  const u32 change_count =
      config.graphics_mod_config ? config.graphics_mod_config->GetChangeCount() : 0;

  // TODO: Invalidating texcache is really stupid in some of these cases
  if (config.iSafeTextureCache_ColorSamples != m_backup_config.color_samples ||
      config.bTexFmtOverlayEnable != m_backup_config.texfmt_overlay ||
      config.bTexFmtOverlayCenter != m_backup_config.texfmt_overlay_center ||
      config.bHiresTextures != m_backup_config.hires_textures ||
      config.bEnableGPUTextureDecoding != m_backup_config.gpu_texture_decoding ||
      config.bDisableCopyToVRAM != m_backup_config.disable_vram_copies ||
      config.bArbitraryMipmapDetection != m_backup_config.arbitrary_mipmap_detection ||
      config.bGraphicMods != m_backup_config.graphics_mods ||
      change_count != m_backup_config.graphics_mod_change_count)
  {
    Invalidate();
    TexDecoder_SetTexFmtOverlayOptions(config.bTexFmtOverlayEnable, config.bTexFmtOverlayCenter);
  }

  SetBackupConfig(config);
}

void TextureCacheBase::Cleanup(int _frameCount)
{
  TexAddrCache::iterator iter = m_textures_by_address.begin();
  TexAddrCache::iterator tcend = m_textures_by_address.end();
  while (iter != tcend)
  {
    if (iter->second->frameCount == FRAMECOUNT_INVALID)
    {
      iter->second->frameCount = _frameCount;
      ++iter;
    }
    else if (_frameCount > TEXTURE_KILL_THRESHOLD + iter->second->frameCount)
    {
      if (iter->second->IsCopy())
      {
        // Only remove EFB copies when they wouldn't be used anymore(changed hash), because EFB
        // copies living on the
        // host GPU are unrecoverable. Perform this check only every TEXTURE_KILL_THRESHOLD for
        // performance reasons
        if ((_frameCount - iter->second->frameCount) % TEXTURE_KILL_THRESHOLD == 1 &&
            iter->second->hash != iter->second->CalculateHash())
        {
          iter = InvalidateTexture(iter);
        }
        else
        {
          ++iter;
        }
      }
      else
      {
        iter = InvalidateTexture(iter);
      }
    }
    else
    {
      ++iter;
    }
  }

  TexPool::iterator iter2 = m_texture_pool.begin();
  TexPool::iterator tcend2 = m_texture_pool.end();
  while (iter2 != tcend2)
  {
    if (iter2->second.frameCount == FRAMECOUNT_INVALID)
    {
      iter2->second.frameCount = _frameCount;
    }
    if (_frameCount > TEXTURE_POOL_KILL_THRESHOLD + iter2->second.frameCount)
    {
      iter2 = m_texture_pool.erase(iter2);
    }
    else
    {
      ++iter2;
    }
  }
}

bool TCacheEntry::OverlapsMemoryRange(u32 range_address, u32 range_size) const
{
  if (addr + size_in_bytes <= range_address)
    return false;

  if (addr >= range_address + range_size)
    return false;

  return true;
}

void TextureCacheBase::SetBackupConfig(const VideoConfig& config)
{
  m_backup_config.color_samples = config.iSafeTextureCache_ColorSamples;
  m_backup_config.texfmt_overlay = config.bTexFmtOverlayEnable;
  m_backup_config.texfmt_overlay_center = config.bTexFmtOverlayCenter;
  m_backup_config.hires_textures = config.bHiresTextures;
  m_backup_config.cache_hires_textures = config.bCacheHiresTextures;
  m_backup_config.stereo_3d = config.stereo_mode != StereoMode::Off;
  m_backup_config.efb_mono_depth = config.bStereoEFBMonoDepth;
  m_backup_config.gpu_texture_decoding = config.bEnableGPUTextureDecoding;
  m_backup_config.disable_vram_copies = config.bDisableCopyToVRAM;
  m_backup_config.arbitrary_mipmap_detection = config.bArbitraryMipmapDetection;
  m_backup_config.graphics_mods = config.bGraphicMods;
  m_backup_config.graphics_mod_change_count =
      config.graphics_mod_config ? config.graphics_mod_config->GetChangeCount() : 0;
}

bool TextureCacheBase::DidLinkedAssetsChange(const TCacheEntry& entry)
{
  for (const auto& cached_asset : entry.linked_game_texture_assets)
  {
    if (cached_asset.m_asset)
    {
      if (cached_asset.m_asset->GetLastLoadedTime() > cached_asset.m_cached_write_time)
        return true;
    }
  }

  for (const auto& cached_asset : entry.linked_asset_dependencies)
  {
    if (cached_asset.m_asset)
    {
      if (cached_asset.m_asset->GetLastLoadedTime() > cached_asset.m_cached_write_time)
        return true;
    }
  }

  return false;
}

RcTcacheEntry TextureCacheBase::ApplyPaletteToEntry(RcTcacheEntry& entry, const u8* palette,
                                                    TLUTFormat tlutfmt)
{
  DEBUG_ASSERT(g_ActiveConfig.backend_info.bSupportsPaletteConversion);

  const AbstractPipeline* pipeline = g_shader_cache->GetPaletteConversionPipeline(tlutfmt);
  if (!pipeline)
  {
    ERROR_LOG_FMT(VIDEO, "Failed to get conversion pipeline for format {}", tlutfmt);
    return {};
  }

  TextureConfig new_config = entry->texture->GetConfig();
  new_config.levels = 1;
  new_config.flags |= AbstractTextureFlag_RenderTarget;

  RcTcacheEntry decoded_entry = AllocateCacheEntry(new_config);
  if (!decoded_entry)
    return decoded_entry;

  decoded_entry->SetGeneralParameters(entry->addr, entry->size_in_bytes, entry->format,
                                      entry->should_force_safe_hashing);
  decoded_entry->SetDimensions(entry->native_width, entry->native_height, 1);
  decoded_entry->SetHashes(entry->base_hash, entry->hash);
  decoded_entry->frameCount = FRAMECOUNT_INVALID;
  decoded_entry->should_force_safe_hashing = false;
  decoded_entry->SetNotCopy();
  decoded_entry->may_have_overlapping_textures = entry->may_have_overlapping_textures;

  g_gfx->BeginUtilityDrawing();

  const u32 palette_size = entry->format == TextureFormat::I4 ? 32 : 512;
  u32 texel_buffer_offset;
  if (g_vertex_manager->UploadTexelBuffer(palette, palette_size,
                                          TexelBufferFormat::TEXEL_BUFFER_FORMAT_R16_UINT,
                                          &texel_buffer_offset))
  {
    struct Uniforms
    {
      float multiplier;
      u32 texel_buffer_offset;
      u32 pad[2];
    };
    static_assert(std::is_standard_layout<Uniforms>::value);
    Uniforms uniforms = {};
    uniforms.multiplier = entry->format == TextureFormat::I4 ? 15.0f : 255.0f;
    uniforms.texel_buffer_offset = texel_buffer_offset;
    g_vertex_manager->UploadUtilityUniforms(&uniforms, sizeof(uniforms));

    g_gfx->SetAndDiscardFramebuffer(decoded_entry->framebuffer.get());
    g_gfx->SetViewportAndScissor(decoded_entry->texture->GetRect());
    g_gfx->SetPipeline(pipeline);
    g_gfx->SetTexture(1, entry->texture.get());
    g_gfx->SetSamplerState(1, RenderState::GetPointSamplerState());
    g_gfx->Draw(0, 3);
    g_gfx->EndUtilityDrawing();
    decoded_entry->texture->FinishedRendering();
  }
  else
  {
    ERROR_LOG_FMT(VIDEO, "Texel buffer upload of {} bytes failed", palette_size);
    g_gfx->EndUtilityDrawing();
  }

  m_textures_by_address.emplace(decoded_entry->addr, decoded_entry);

  return decoded_entry;
}

RcTcacheEntry TextureCacheBase::ReinterpretEntry(const RcTcacheEntry& existing_entry,
                                                 TextureFormat new_format)
{
  const AbstractPipeline* pipeline =
      g_shader_cache->GetTextureReinterpretPipeline(existing_entry->format.texfmt, new_format);
  if (!pipeline)
  {
    ERROR_LOG_FMT(VIDEO, "Failed to obtain texture reinterpreting pipeline from format {} to {}",
                  existing_entry->format.texfmt, new_format);
    return {};
  }

  TextureConfig new_config = existing_entry->texture->GetConfig();
  new_config.levels = 1;
  new_config.flags |= AbstractTextureFlag_RenderTarget;

  RcTcacheEntry reinterpreted_entry = AllocateCacheEntry(new_config);
  if (!reinterpreted_entry)
    return {};

  reinterpreted_entry->SetGeneralParameters(existing_entry->addr, existing_entry->size_in_bytes,
                                            new_format, existing_entry->should_force_safe_hashing);
  reinterpreted_entry->SetDimensions(existing_entry->native_width, existing_entry->native_height,
                                     1);
  reinterpreted_entry->SetHashes(existing_entry->base_hash, existing_entry->hash);
  reinterpreted_entry->frameCount = existing_entry->frameCount;
  reinterpreted_entry->SetNotCopy();
  reinterpreted_entry->is_efb_copy = existing_entry->is_efb_copy;
  reinterpreted_entry->may_have_overlapping_textures =
      existing_entry->may_have_overlapping_textures;

  g_gfx->BeginUtilityDrawing();
  g_gfx->SetAndDiscardFramebuffer(reinterpreted_entry->framebuffer.get());
  g_gfx->SetViewportAndScissor(reinterpreted_entry->texture->GetRect());
  g_gfx->SetPipeline(pipeline);
  g_gfx->SetTexture(0, existing_entry->texture.get());
  g_gfx->SetSamplerState(1, RenderState::GetPointSamplerState());
  g_gfx->Draw(0, 3);
  g_gfx->EndUtilityDrawing();
  reinterpreted_entry->texture->FinishedRendering();

  m_textures_by_address.emplace(reinterpreted_entry->addr, reinterpreted_entry);

  return reinterpreted_entry;
}

void TextureCacheBase::ScaleTextureCacheEntryTo(RcTcacheEntry& entry, u32 new_width, u32 new_height)
{
  if (entry->GetWidth() == new_width && entry->GetHeight() == new_height)
  {
    return;
  }

  const u32 max = g_ActiveConfig.backend_info.MaxTextureSize;
  if (max < new_width || max < new_height)
  {
    ERROR_LOG_FMT(VIDEO, "Texture too big, width = {}, height = {}", new_width, new_height);
    return;
  }

  const TextureConfig newconfig(new_width, new_height, 1, entry->GetNumLayers(), 1,
                                AbstractTextureFormat::RGBA8, AbstractTextureFlag_RenderTarget,
                                AbstractTextureType::Texture_2DArray);
  std::optional<TexPoolEntry> new_texture = AllocateTexture(newconfig);
  if (!new_texture)
  {
    ERROR_LOG_FMT(VIDEO, "Scaling failed due to texture allocation failure");
    return;
  }

  // No need to convert the coordinates here since they'll be the same.
  g_gfx->ScaleTexture(new_texture->framebuffer.get(), new_texture->texture->GetConfig().GetRect(),
                      entry->texture.get(), entry->texture->GetConfig().GetRect());
  entry->texture.swap(new_texture->texture);
  entry->framebuffer.swap(new_texture->framebuffer);

  // At this point new_texture has the old texture in it,
  // we can potentially reuse this, so let's move it back to the pool
  auto config = new_texture->texture->GetConfig();
  m_texture_pool.emplace(
      config, TexPoolEntry(std::move(new_texture->texture), std::move(new_texture->framebuffer)));
}

bool TextureCacheBase::CheckReadbackTexture(u32 width, u32 height, AbstractTextureFormat format)
{
  if (m_readback_texture && m_readback_texture->GetConfig().width >= width &&
      m_readback_texture->GetConfig().height >= height &&
      m_readback_texture->GetConfig().format == format)
  {
    return true;
  }

  TextureConfig staging_config(std::max(width, 128u), std::max(height, 128u), 1, 1, 1, format, 0,
                               AbstractTextureType::Texture_2DArray);
  m_readback_texture.reset();
  m_readback_texture = g_gfx->CreateStagingTexture(StagingTextureType::Readback, staging_config);
  return m_readback_texture != nullptr;
}

void TextureCacheBase::SerializeTexture(AbstractTexture* tex, const TextureConfig& config,
                                        PointerWrap& p)
{
  // If we're in measure mode, skip the actual readback to save some time.
  const bool skip_readback = p.IsMeasureMode();
  p.Do(config);

  if (skip_readback || CheckReadbackTexture(config.width, config.height, config.format))
  {
    // First, measure the amount of memory needed.
    u32 total_size = 0;
    for (u32 layer = 0; layer < config.layers; layer++)
    {
      for (u32 level = 0; level < config.levels; level++)
      {
        u32 level_width = std::max(config.width >> level, 1u);
        u32 level_height = std::max(config.height >> level, 1u);

        u32 stride = AbstractTexture::CalculateStrideForFormat(config.format, level_width);
        u32 size = stride * level_height;

        total_size += size;
      }
    }

    // Set aside total_size bytes of space for the textures.
    // When measuring, this will be set aside and not written to,
    // but when writing we'll use this pointer directly to avoid
    // needing to allocate/free an extra buffer.
    u8* texture_data = p.DoExternal(total_size);

    if (!skip_readback && p.IsMeasureMode())
    {
      ERROR_LOG_FMT(VIDEO, "Couldn't acquire {} bytes for serializing texture.", total_size);
      return;
    }

    if (!skip_readback)
    {
      // Save out each layer of the texture to the pointer.
      for (u32 layer = 0; layer < config.layers; layer++)
      {
        for (u32 level = 0; level < config.levels; level++)
        {
          u32 level_width = std::max(config.width >> level, 1u);
          u32 level_height = std::max(config.height >> level, 1u);
          auto rect = tex->GetConfig().GetMipRect(level);
          m_readback_texture->CopyFromTexture(tex, rect, layer, level, rect);

          u32 stride = AbstractTexture::CalculateStrideForFormat(config.format, level_width);
          u32 size = stride * level_height;
          m_readback_texture->ReadTexels(rect, texture_data, stride);

          texture_data += size;
        }
      }
    }
  }
  else
  {
    PanicAlertFmt("Failed to create staging texture for serialization");
  }
}

std::optional<TextureCacheBase::TexPoolEntry> TextureCacheBase::DeserializeTexture(PointerWrap& p)
{
  TextureConfig config;
  p.Do(config);

  // Read in the size from the save state, then texture data will point to
  // a region of size total_size where textures are stored.
  u32 total_size = 0;
  u8* texture_data = p.DoExternal(total_size);

  if (!p.IsReadMode() || total_size == 0)
    return std::nullopt;

  auto tex = AllocateTexture(config);
  if (!tex)
  {
    PanicAlertFmt("Failed to create texture for deserialization");
    return std::nullopt;
  }

  size_t start = 0;
  for (u32 layer = 0; layer < config.layers; layer++)
  {
    for (u32 level = 0; level < config.levels; level++)
    {
      const u32 level_width = std::max(config.width >> level, 1u);
      const u32 level_height = std::max(config.height >> level, 1u);
      const size_t stride = AbstractTexture::CalculateStrideForFormat(config.format, level_width);
      const size_t size = stride * level_height;
      if ((start + size) > total_size)
      {
        ERROR_LOG_FMT(VIDEO, "Insufficient texture data for layer {} level {}", layer, level);
        return tex;
      }

      tex->texture->Load(level, level_width, level_height, level_width, &texture_data[start], size);
      start += size;
    }
  }

  return tex;
}

void TextureCacheBase::DoState(PointerWrap& p)
{
  // Flush all pending XFB copies before either loading or saving.
  FlushEFBCopies();

  p.Do(m_last_entry_id);

  if (p.IsWriteMode() || p.IsMeasureMode())
    DoSaveState(p);
  else
    DoLoadState(p);
}

void TextureCacheBase::DoSaveState(PointerWrap& p)
{
  // Flush all stale binds
  FlushStaleBinds();

  std::map<const TCacheEntry*, u32> entry_map;
  std::vector<TCacheEntry*> entries_to_save;
  auto ShouldSaveEntry = [](const RcTcacheEntry& entry) {
    // We skip non-copies as they can be decoded from RAM when the state is loaded.
    // Storing them would duplicate data in the save state file, adding to decompression time.
    // We also need to store invalidated entires, as they can't be restored from RAM.
    return entry->IsCopy() || entry->invalidated;
  };
  auto AddCacheEntryToMap = [&entry_map, &entries_to_save](const RcTcacheEntry& entry) -> u32 {
    auto iter = entry_map.find(entry.get());
    if (iter != entry_map.end())
      return iter->second;

    // Since we are sequentially allocating texture entries, we need to save the textures in the
    // same order they were collected. This is because of iterating both the address and hash maps.
    // Therefore, the map is used for fast lookup, and the vector for ordering.
    u32 id = static_cast<u32>(entry_map.size());
    entry_map.emplace(entry.get(), id);
    entries_to_save.push_back(entry.get());
    return id;
  };
  auto GetCacheEntryId = [&entry_map](const TCacheEntry* entry) -> std::optional<u32> {
    auto iter = entry_map.find(entry);
    return iter != entry_map.end() ? std::make_optional(iter->second) : std::nullopt;
  };

  // Transform the m_textures_by_address and m_textures_by_hash maps to a mapping
  // of address/hash to entry ID.
  std::vector<std::pair<u32, u32>> textures_by_address_list;
  std::vector<std::pair<u64, u32>> textures_by_hash_list;
  std::vector<std::pair<u32, u32>> bound_textures_list;
  if (Config::Get(Config::GFX_SAVE_TEXTURE_CACHE_TO_STATE))
  {
    for (const auto& it : m_textures_by_address)
    {
      if (ShouldSaveEntry(it.second))
      {
        const u32 id = AddCacheEntryToMap(it.second);
        textures_by_address_list.emplace_back(it.first, id);
      }
    }
    for (const auto& it : m_textures_by_hash)
    {
      if (ShouldSaveEntry(it.second))
      {
        const u32 id = AddCacheEntryToMap(it.second);
        textures_by_hash_list.emplace_back(it.first, id);
      }
    }
    for (u32 i = 0; i < m_bound_textures.size(); i++)
    {
      const auto& tentry = m_bound_textures[i];
      if (m_bound_textures[i] && ShouldSaveEntry(tentry))
      {
        const u32 id = AddCacheEntryToMap(tentry);
        bound_textures_list.emplace_back(i, id);
      }
    }
  }

  // Save the texture cache entries out in the order the were referenced.
  u32 size = static_cast<u32>(entries_to_save.size());
  p.Do(size);
  for (TCacheEntry* entry : entries_to_save)
  {
    SerializeTexture(entry->texture.get(), entry->texture->GetConfig(), p);
    entry->DoState(p);
  }
  p.DoMarker("TextureCacheEntries");

  // Save references for each cache entry.
  // As references are circular, we need to have everything created before linking entries.
  std::set<std::pair<u32, u32>> reference_pairs;
  for (const auto& it : entry_map)
  {
    const TCacheEntry* entry = it.first;
    auto id1 = GetCacheEntryId(entry);
    if (!id1)
      continue;

    for (const TCacheEntry* referenced_entry : entry->references)
    {
      auto id2 = GetCacheEntryId(referenced_entry);
      if (!id2)
        continue;

      auto refpair1 = std::make_pair(*id1, *id2);
      auto refpair2 = std::make_pair(*id2, *id1);
      if (!reference_pairs.contains(refpair1) && !reference_pairs.contains(refpair2))
        reference_pairs.insert(refpair1);
    }
  }

  auto doList = [&p](auto list) {
    u32 list_size = static_cast<u32>(list.size());
    p.Do(list_size);
    for (const auto& it : list)
    {
      p.Do(it.first);
      p.Do(it.second);
    }
  };

  doList(reference_pairs);
  doList(textures_by_address_list);
  doList(textures_by_hash_list);
  doList(bound_textures_list);

  // Free the readback texture to potentially save host-mapped GPU memory, depending on where
  // the driver mapped the staging buffer.
  m_readback_texture.reset();
}

void TextureCacheBase::DoLoadState(PointerWrap& p)
{
  // Helper for getting a cache entry from an ID.
  std::map<u32, RcTcacheEntry> id_map;
  RcTcacheEntry null_entry;
  auto GetEntry = [&id_map, &null_entry](u32 id) -> RcTcacheEntry& {
    auto iter = id_map.find(id);
    return iter == id_map.end() ? null_entry : iter->second;
  };

  // Only clear out state when actually restoring/loading.
  // Since we throw away entries when not in loading mode now, we don't need to check
  // before inserting entries into the cache, as GetEntry will always return null.
  const bool commit_state = p.IsReadMode();
  if (commit_state)
    Invalidate();

  // Preload all cache entries.
  u32 size = 0;
  p.Do(size);
  for (u32 i = 0; i < size; i++)
  {
    // Even if the texture isn't valid, we still need to create the cache entry object
    // to update the point in the state state. We'll just throw it away if it's invalid.
    auto tex = DeserializeTexture(p);
    auto entry =
        std::make_shared<TCacheEntry>(std::move(tex->texture), std::move(tex->framebuffer));
    entry->textures_by_hash_iter = m_textures_by_hash.end();
    entry->DoState(p);
    if (entry->texture && commit_state)
      id_map.emplace(i, entry);
  }
  p.DoMarker("TextureCacheEntries");

  // Link all cache entry references.
  p.Do(size);
  for (u32 i = 0; i < size; i++)
  {
    u32 id1 = 0, id2 = 0;
    p.Do(id1);
    p.Do(id2);
    auto e1 = GetEntry(id1);
    auto e2 = GetEntry(id2);
    if (e1 && e2)
      e1->CreateReference(e2.get());
  }

  // Fill in address map.
  p.Do(size);
  for (u32 i = 0; i < size; i++)
  {
    u32 addr = 0;
    u32 id = 0;
    p.Do(addr);
    p.Do(id);

    auto& entry = GetEntry(id);
    if (entry)
      m_textures_by_address.emplace(addr, entry);
  }

  // Fill in hash map.
  p.Do(size);
  for (u32 i = 0; i < size; i++)
  {
    u64 hash = 0;
    u32 id = 0;
    p.Do(hash);
    p.Do(id);

    auto& entry = GetEntry(id);
    if (entry)
      entry->textures_by_hash_iter = m_textures_by_hash.emplace(hash, entry);
  }

  // Clear bound textures
  for (u32 i = 0; i < m_bound_textures.size(); i++)
    m_bound_textures[i].reset();

  // Fill in bound textures
  p.Do(size);
  for (u32 i = 0; i < size; i++)
  {
    u32 index = 0;
    u32 id = 0;
    p.Do(index);
    p.Do(id);

    auto& entry = GetEntry(id);
    if (entry)
      m_bound_textures[index] = entry;
  }
}

void TextureCacheBase::OnFrameEnd()
{
  // Flush any outstanding EFB copies to RAM, in case the game is running at an uncapped frame
  // rate and not waiting for vblank. Otherwise, we'd end up with a huge list of pending
  // copies.
  FlushEFBCopies();

  Cleanup(g_presenter->FrameCount());
}

void TCacheEntry::DoState(PointerWrap& p)
{
  p.Do(addr);
  p.Do(size_in_bytes);
  p.Do(base_hash);
  p.Do(hash);
  p.Do(format);
  p.Do(memory_stride);
  p.Do(is_efb_copy);
  p.Do(is_custom_tex);
  p.Do(may_have_overlapping_textures);
  p.Do(invalidated);
  p.Do(has_arbitrary_mips);
  p.Do(should_force_safe_hashing);
  p.Do(is_xfb_copy);
  p.Do(is_xfb_container);
  p.Do(id);
  p.Do(reference_changed);
  p.Do(native_width);
  p.Do(native_height);
  p.Do(native_levels);
  p.Do(frameCount);
}

RcTcacheEntry TextureCacheBase::DoPartialTextureUpdates(RcTcacheEntry& entry_to_update,
                                                        const u8* palette, TLUTFormat tlutfmt)
{
  // If the flag may_have_overlapping_textures is cleared, there are no overlapping EFB copies,
  // which aren't applied already. It is set for new textures, and for the affected range
  // on each EFB copy.
  if (!entry_to_update->may_have_overlapping_textures)
    return entry_to_update;
  entry_to_update->may_have_overlapping_textures = false;

  const bool isPaletteTexture = IsColorIndexed(entry_to_update->format.texfmt);

  // EFB copies are excluded from these updates, until there's an example where a game would
  // benefit from updating. This would require more work to be done.
  if (entry_to_update->IsCopy())
    return entry_to_update;

  if (entry_to_update->IsLocked())
  {
    // TODO: Shouldn't be too hard, just need to clone the texture entry + texture contents.
    PanicAlertFmt("TextureCache: PartialTextureUpdates of locked textures is not implemented");
    return {};
  }

  u32 block_width = TexDecoder_GetBlockWidthInTexels(entry_to_update->format.texfmt);
  u32 block_height = TexDecoder_GetBlockHeightInTexels(entry_to_update->format.texfmt);
  u32 block_size = block_width * block_height *
                   TexDecoder_GetTexelSizeInNibbles(entry_to_update->format.texfmt) / 2;

  u32 numBlocksX = (entry_to_update->native_width + block_width - 1) / block_width;

  auto iter = FindOverlappingTextures(entry_to_update->addr, entry_to_update->size_in_bytes);
  while (iter.first != iter.second)
  {
    auto& entry = iter.first->second;
    if (entry != entry_to_update && entry->IsCopy() &&
        !entry->references.contains(entry_to_update.get()) &&
        entry->OverlapsMemoryRange(entry_to_update->addr, entry_to_update->size_in_bytes) &&
        entry->memory_stride == numBlocksX * block_size)
    {
      if (entry->hash == entry->CalculateHash())
      {
        // If the texture formats are not compatible or convertible, skip it.
        if (!IsCompatibleTextureFormat(entry_to_update->format.texfmt, entry->format.texfmt))
        {
          if (!CanReinterpretTextureOnGPU(entry_to_update->format.texfmt, entry->format.texfmt))
          {
            ++iter.first;
            continue;
          }

          auto reinterpreted_entry = ReinterpretEntry(entry, entry_to_update->format.texfmt);
          if (reinterpreted_entry)
            entry = reinterpreted_entry;
        }

        if (isPaletteTexture)
        {
          auto decoded_entry = ApplyPaletteToEntry(entry, palette, tlutfmt);
          if (decoded_entry)
          {
            // Link the efb copy with the partially updated texture, so we won't apply this partial
            // update again
            entry->CreateReference(entry_to_update.get());
            // Mark the texture update as used, as if it was loaded directly
            entry->frameCount = FRAMECOUNT_INVALID;
            entry = decoded_entry;
          }
          else
          {
            ++iter.first;
            continue;
          }
        }

        u32 src_x, src_y, dst_x, dst_y;

        // Note for understanding the math:
        // Normal textures can't be strided, so the 2 missing cases with src_x > 0 don't exist
        if (entry->addr >= entry_to_update->addr)
        {
          u32 block_offset = (entry->addr - entry_to_update->addr) / block_size;
          u32 block_x = block_offset % numBlocksX;
          u32 block_y = block_offset / numBlocksX;
          src_x = 0;
          src_y = 0;
          dst_x = block_x * block_width;
          dst_y = block_y * block_height;
        }
        else
        {
          u32 block_offset = (entry_to_update->addr - entry->addr) / block_size;
          u32 block_x = (~block_offset + 1) % numBlocksX;
          u32 block_y = (block_offset + block_x) / numBlocksX;
          src_x = 0;
          src_y = block_y * block_height;
          dst_x = block_x * block_width;
          dst_y = 0;
        }

        u32 copy_width =
            std::min(entry->native_width - src_x, entry_to_update->native_width - dst_x);
        u32 copy_height =
            std::min(entry->native_height - src_y, entry_to_update->native_height - dst_y);

        // If one of the textures is scaled, scale both with the current efb scaling factor
        if (entry_to_update->native_width != entry_to_update->GetWidth() ||
            entry_to_update->native_height != entry_to_update->GetHeight() ||
            entry->native_width != entry->GetWidth() || entry->native_height != entry->GetHeight())
        {
          ScaleTextureCacheEntryTo(
              entry_to_update, g_framebuffer_manager->EFBToScaledX(entry_to_update->native_width),
              g_framebuffer_manager->EFBToScaledY(entry_to_update->native_height));
          ScaleTextureCacheEntryTo(entry, g_framebuffer_manager->EFBToScaledX(entry->native_width),
                                   g_framebuffer_manager->EFBToScaledY(entry->native_height));

          src_x = g_framebuffer_manager->EFBToScaledX(src_x);
          src_y = g_framebuffer_manager->EFBToScaledY(src_y);
          dst_x = g_framebuffer_manager->EFBToScaledX(dst_x);
          dst_y = g_framebuffer_manager->EFBToScaledY(dst_y);
          copy_width = g_framebuffer_manager->EFBToScaledX(copy_width);
          copy_height = g_framebuffer_manager->EFBToScaledY(copy_height);
        }

        // If the source rectangle is outside of what we actually have in VRAM, skip the copy.
        // The backend doesn't do any clamping, so if we don't, we'd pass out-of-range coordinates
        // to the graphics driver, which can cause GPU resets.
        if (static_cast<u32>(src_x + copy_width) > entry->GetWidth() ||
            static_cast<u32>(src_y + copy_height) > entry->GetHeight() ||
            static_cast<u32>(dst_x + copy_width) > entry_to_update->GetWidth() ||
            static_cast<u32>(dst_y + copy_height) > entry_to_update->GetHeight())
        {
          ++iter.first;
          continue;
        }

        MathUtil::Rectangle<int> srcrect, dstrect;
        srcrect.left = src_x;
        srcrect.top = src_y;
        srcrect.right = (src_x + copy_width);
        srcrect.bottom = (src_y + copy_height);
        dstrect.left = dst_x;
        dstrect.top = dst_y;
        dstrect.right = (dst_x + copy_width);
        dstrect.bottom = (dst_y + copy_height);

        // If one copy is stereo, and the other isn't... not much we can do here :/
        const u32 layers_to_copy = std::min(entry->GetNumLayers(), entry_to_update->GetNumLayers());
        for (u32 layer = 0; layer < layers_to_copy; layer++)
        {
          entry_to_update->texture->CopyRectangleFromTexture(entry->texture.get(), srcrect, layer,
                                                             0, dstrect, layer, 0);
        }

        if (isPaletteTexture)
        {
          // Remove the temporary converted texture, it won't be used anywhere else
          // TODO: It would be nice to convert and copy in one step, but this code path isn't common
          iter.first = InvalidateTexture(iter.first);
          continue;
        }
        else
        {
          // Link the two textures together, so we won't apply this partial update again
          entry->CreateReference(entry_to_update.get());
          // Mark the texture update as used, as if it was loaded directly
          entry->frameCount = FRAMECOUNT_INVALID;
        }
      }
      else
      {
        // If the hash does not match, this EFB copy will not be used for anything, so remove it
        iter.first = InvalidateTexture(iter.first);
        continue;
      }
    }
    ++iter.first;
  }

  return entry_to_update;
}

// Helper for checking if a BPMemory TexMode0 register is set to Point
// Filtering modes. This is used to decide whether Anisotropic enhancements
// are (mostly) safe in the VideoBackends.
// If both the minification and magnification filters are set to POINT modes
// then applying anisotropic filtering is equivalent to forced filtering. Point
// mode textures are usually some sort of 2D UI billboard which will end up
// misaligned from the correct pixels when filtered anisotropically.
static bool IsAnisostropicEnhancementSafe(const TexMode0& tm0)
{
  return !(tm0.min_filter == FilterMode::Near && tm0.mag_filter == FilterMode::Near);
}

SamplerState TextureCacheBase::GetSamplerState(u32 index, float custom_tex_scale, bool custom_tex,
                                               bool has_arbitrary_mips)
{
  const TexMode0& tm0 = bpmem.tex.GetUnit(index).texMode0;

  SamplerState state = {};
  state.Generate(bpmem, index);

  // Force texture filtering config option.
  if (g_ActiveConfig.texture_filtering_mode == TextureFilteringMode::Nearest)
  {
    state.tm0.min_filter = FilterMode::Near;
    state.tm0.mag_filter = FilterMode::Near;
    state.tm0.mipmap_filter = FilterMode::Near;
  }
  else if (g_ActiveConfig.texture_filtering_mode == TextureFilteringMode::Linear)
  {
    state.tm0.min_filter = FilterMode::Linear;
    state.tm0.mag_filter = FilterMode::Linear;
    state.tm0.mipmap_filter =
        tm0.mipmap_filter != MipMode::None ? FilterMode::Linear : FilterMode::Near;
  }

  // Custom textures may have a greater number of mips
  if (custom_tex)
    state.tm1.max_lod = 255;

  // Anisotropic filtering option.
  if (g_ActiveConfig.iMaxAnisotropy != 0 && IsAnisostropicEnhancementSafe(tm0))
  {
    // https://www.opengl.org/registry/specs/EXT/texture_filter_anisotropic.txt
    // For predictable results on all hardware/drivers, only use one of:
    //	GL_LINEAR + GL_LINEAR (No Mipmaps [Bilinear])
    //	GL_LINEAR + GL_LINEAR_MIPMAP_LINEAR (w/ Mipmaps [Trilinear])
    // Letting the game set other combinations will have varying arbitrary results;
    // possibly being interpreted as equal to bilinear/trilinear, implicitly
    // disabling anisotropy, or changing the anisotropic algorithm employed.
    state.tm0.min_filter = FilterMode::Linear;
    state.tm0.mag_filter = FilterMode::Linear;
    if (tm0.mipmap_filter != MipMode::None)
      state.tm0.mipmap_filter = FilterMode::Linear;
    state.tm0.anisotropic_filtering = true;
  }
  else
  {
    state.tm0.anisotropic_filtering = false;
  }

  if (has_arbitrary_mips && tm0.mipmap_filter != MipMode::None)
  {
    // Apply a secondary bias calculated from the IR scale to pull inwards mipmaps
    // that have arbitrary contents, eg. are used for fog effects where the
    // distance they kick in at is important to preserve at any resolution.
    // Correct this with the upscaling factor of custom textures.
    s32 lod_offset = std::log2(g_framebuffer_manager->GetEFBScale() / custom_tex_scale) * 256.f;
    state.tm0.lod_bias = std::clamp<s32>(state.tm0.lod_bias + lod_offset, -32768, 32767);

    // Anisotropic also pushes mips farther away so it cannot be used either
    state.tm0.anisotropic_filtering = false;
  }

  return state;
}

void TextureCacheBase::BindTextures(BitSet32 used_textures,
                                    const std::array<SamplerState, 8>& samplers)
{
  auto& system = Core::System::GetInstance();
  auto& pixel_shader_manager = system.GetPixelShaderManager();
  for (u32 i = 0; i < m_bound_textures.size(); i++)
  {
    const RcTcacheEntry& tentry = m_bound_textures[i];
    if (used_textures[i] && tentry)
    {
      g_gfx->SetTexture(i, tentry->texture.get());
      pixel_shader_manager.SetTexDims(i, tentry->native_width, tentry->native_height);

      auto& state = samplers[i];
      g_gfx->SetSamplerState(i, state);
      pixel_shader_manager.SetSamplerState(i, state.tm0.hex, state.tm1.hex);
    }
  }

  TMEM::FinalizeBinds(used_textures);
}

class ArbitraryMipmapDetector
{
private:
  using PixelRGBAf = std::array<float, 4>;
  using PixelRGBAu8 = std::array<u8, 4>;

public:
  explicit ArbitraryMipmapDetector() = default;

  void AddLevel(u32 width, u32 height, u32 row_length, const u8* buffer)
  {
    levels.push_back({{width, height, row_length}, buffer});
  }

  bool HasArbitraryMipmaps(u8* downsample_buffer) const
  {
    if (levels.size() < 2)
      return false;

    if (!g_ActiveConfig.bArbitraryMipmapDetection)
      return false;

    // This is the average per-pixel, per-channel difference in percent between what we
    // expect a normal blurred mipmap to look like and what we actually received
    // 4.5% was chosen because it's just below the lowest clearly-arbitrary texture
    // I found in my tests, the background clouds in Mario Galaxy's Observatory lobby.
    const auto threshold = g_ActiveConfig.fArbitraryMipmapDetectionThreshold;

    auto* src = downsample_buffer;
    auto* dst = downsample_buffer + levels[1].shape.row_length * levels[1].shape.height * 4;

    float total_diff = 0.f;

    for (std::size_t i = 0; i < levels.size() - 1; ++i)
    {
      const auto& level = levels[i];
      const auto& mip = levels[i + 1];

      u64 level_pixel_count = level.shape.width;
      level_pixel_count *= level.shape.height;

      // AverageDiff stores the difference sum in a u64, so make sure we can't overflow
      ASSERT(level_pixel_count < (std::numeric_limits<u64>::max() / (255 * 255 * 4)));

      // Manually downsample the past downsample with a simple box blur
      // This is not necessarily close to whatever the original artists used, however
      // It should still be closer than a thing that's not a downscale at all
      Level::Downsample(i ? src : level.pixels, level.shape, dst, mip.shape);

      // Find the average difference between pixels in this level but downsampled
      // and the next level
      auto diff = mip.AverageDiff(dst);
      total_diff += diff;

      std::swap(src, dst);
    }

    auto all_levels = total_diff / (levels.size() - 1);
    return all_levels > threshold;
  }

private:
  struct Shape
  {
    u32 width;
    u32 height;
    u32 row_length;
  };

  struct Level
  {
    Shape shape;
    const u8* pixels;

    static PixelRGBAu8 SampleLinear(const u8* src, const Shape& src_shape, u32 x, u32 y)
    {
      const auto* p = src + (x + y * src_shape.row_length) * 4;
      return {{p[0], p[1], p[2], p[3]}};
    }

    // Puts a downsampled image in dst. dst must be at least width*height*4
    static void Downsample(const u8* src, const Shape& src_shape, u8* dst, const Shape& dst_shape)
    {
      for (u32 i = 0; i < dst_shape.height; ++i)
      {
        for (u32 j = 0; j < dst_shape.width; ++j)
        {
          auto x = j * 2;
          auto y = i * 2;
          const std::array<PixelRGBAu8, 4> samples{{
              SampleLinear(src, src_shape, x, y),
              SampleLinear(src, src_shape, x + 1, y),
              SampleLinear(src, src_shape, x, y + 1),
              SampleLinear(src, src_shape, x + 1, y + 1),
          }};

          auto* dst_pixel = dst + (j + i * dst_shape.row_length) * 4;
          for (int channel = 0; channel < 4; channel++)
          {
            uint32_t channel_value = samples[0][channel] + samples[1][channel] +
                                     samples[2][channel] + samples[3][channel];
            dst_pixel[channel] = (channel_value + 2) / 4;
          }
        }
      }
    }

    float AverageDiff(const u8* other) const
    {
      // As textures are stored in (at most) 8 bit precision, each channel can
      // have a max diff of (2^8)^2, multiply by 4 channels = 2^18 per pixel.
      // That means to overflow, we must have a texture with more than 2^46
      // pixels - which is way beyond anything the original hardware could do,
      // and likely a sane assumption going forward for some significant time.
      u64 current_diff_sum = 0;
      const auto* ptr1 = pixels;
      const auto* ptr2 = other;
      for (u32 i = 0; i < shape.height; ++i)
      {
        const auto* row1 = ptr1;
        const auto* row2 = ptr2;
        for (u32 j = 0; j < shape.width; ++j, row1 += 4, row2 += 4)
        {
          int pixel_diff = 0;
          for (int channel = 0; channel < 4; channel++)
          {
            const int diff = static_cast<int>(row1[channel]) - static_cast<int>(row2[channel]);
            const int diff_squared = diff * diff;
            pixel_diff += diff_squared;
          }
          current_diff_sum += pixel_diff;
        }
        ptr1 += shape.row_length;
        ptr2 += shape.row_length;
      }
      // calculate the MSE over all pixels, divide by 2.56 to make it a percent
      // (IE scale to 0..100 instead of 0..256)

      return std::sqrt(static_cast<float>(current_diff_sum) / (shape.width * shape.height * 4)) /
             2.56f;
    }
  };
  std::vector<Level> levels;
};

TCacheEntry* TextureCacheBase::Load(const TextureInfo& texture_info)
{
  if (auto entry = LoadImpl(texture_info, false))
  {
    if (!DidLinkedAssetsChange(*entry))
    {
      return entry;
    }

    InvalidateTexture(GetTexCacheIter(entry));
    return LoadImpl(texture_info, true);
  }

  return nullptr;
}

TCacheEntry* TextureCacheBase::LoadImpl(const TextureInfo& texture_info, bool force_reload)
{
  // if this stage was not invalidated by changes to texture registers, keep the current texture
  if (!force_reload && TMEM::IsValid(texture_info.GetStage()) &&
      m_bound_textures[texture_info.GetStage()])
  {
    TCacheEntry* entry = m_bound_textures[texture_info.GetStage()].get();
    // If the TMEM configuration is such that this texture is more or less guaranteed to still
    // be in TMEM, then we know we can reuse the old entry without even hashing the memory
    //
    // It's possible this texture has already been overwritten in emulated memory and therfore
    // invalidated from our texture cache, but we want to use it anyway to approximate the
    // result of the game using an overwritten texture cached in TMEM.
    //
    // Spyro: A Hero's Tail is known for (deliberately?) using such overwritten textures
    // in it's bloom effect, which breaks without giving it the invalidated texture.
    if (TMEM::IsCached(texture_info.GetStage()))
    {
      return entry;
    }

    // Otherwise, hash the backing memory and check it's unchanged.
    // FIXME: this doesn't correctly handle textures from tmem.
    if (!entry->invalidated && entry->base_hash == entry->CalculateHash())
    {
      return entry;
    }
  }

  auto entry = GetTexture(g_ActiveConfig.iSafeTextureCache_ColorSamples, texture_info);

  if (!entry)
    return nullptr;

  entry->frameCount = FRAMECOUNT_INVALID;
  if (entry->texture_info_name.empty() && g_ActiveConfig.bGraphicMods)
  {
    entry->texture_info_name = texture_info.CalculateTextureName().GetFullName();

    GraphicsModActionData::TextureLoad texture_load{entry->texture_info_name};
    for (const auto& action :
         g_graphics_mod_manager->GetTextureLoadActions(entry->texture_info_name))
    {
      action->OnTextureLoad(&texture_load);
    }
  }
  m_bound_textures[texture_info.GetStage()] = entry;

  // We need to keep track of invalided textures until they have actually been replaced or
  // re-loaded
  TMEM::Bind(texture_info.GetStage(), entry->NumBlocksX(), entry->NumBlocksY(),
             entry->GetNumLevels() > 1, entry->format == TextureFormat::RGBA8);

  return entry.get();
}

RcTcacheEntry TextureCacheBase::GetTexture(const int textureCacheSafetyColorSampleSize,
                                           const TextureInfo& texture_info)
{
  if (!texture_info.IsDataValid())
    return {};

  // Hash assigned to texcache entry (also used to generate filenames used for texture dumping and
  // custom texture lookup)
  u64 base_hash = TEXHASH_INVALID;
  u64 full_hash = TEXHASH_INVALID;

  TextureAndTLUTFormat full_format(texture_info.GetTextureFormat(), texture_info.GetTlutFormat());

  // Reject invalid tlut format.
  if (texture_info.GetPaletteSize() && !IsValidTLUTFormat(texture_info.GetTlutFormat()))
    return {};

  u32 bytes_per_block = (texture_info.GetBlockWidth() * texture_info.GetBlockHeight() *
                         TexDecoder_GetTexelSizeInNibbles(texture_info.GetTextureFormat())) /
                        2;

  // TODO: the texture cache lookup is based on address, but a texture from tmem has no reason
  //       to have a unique and valid address. This could result in a regular texture and a tmem
  //       texture aliasing onto the same texture cache entry.

  // If we are recording a FifoLog, keep track of what memory we read. FifoRecorder does
  // its own memory modification tracking independent of the texture hashing below.
  if (OpcodeDecoder::g_record_fifo_data && !texture_info.IsFromTmem())
  {
    Core::System::GetInstance().GetFifoRecorder().UseMemory(texture_info.GetRawAddress(),
                                                            texture_info.GetFullLevelSize(),
                                                            MemoryUpdate::Type::TextureMap);
  }

  // TODO: This doesn't hash GB tiles for preloaded RGBA8 textures (instead, it's hashing more data
  // from the low tmem bank than it should)
  base_hash = Common::GetHash64(texture_info.GetData(), texture_info.GetTextureSize(),
                                textureCacheSafetyColorSampleSize);
  u32 palette_size = 0;
  if (texture_info.GetPaletteSize())
  {
    palette_size = *texture_info.GetPaletteSize();
    full_hash =
        base_hash ^ Common::GetHash64(texture_info.GetTlutAddress(), *texture_info.GetPaletteSize(),
                                      textureCacheSafetyColorSampleSize);
  }
  else
  {
    full_hash = base_hash;
  }

  // Search the texture cache for textures by address
  //
  // Find all texture cache entries for the current texture address, and decide whether to use one
  // of them, or to create a new one
  //
  // In most cases, the fastest way is to use only one texture cache entry for the same address.
  // Usually, when a texture changes, the old version of the texture is unlikely to be used again.
  // If there were new cache entries created for normal texture updates, there would be a slowdown
  // due to a huge amount of unused cache entries. Also thanks to texture pooling, overwriting an
  // existing cache entry is faster than creating a new one from scratch.
  //
  // Some games use the same address for different textures though. If the same cache entry was used
  // in this case, it would be constantly overwritten, and effectively there wouldn't be any caching
  // for those textures. Examples for this are Metroid Prime and Castlevania 3. Metroid Prime has
  // multiple sets of fonts on each other stored in a single texture and uses the palette to make
  // different characters visible or invisible. In Castlevania 3 some textures are used for 2
  // different things or at least in 2 different ways (size 1024x1024 vs 1024x256).
  //
  // To determine whether to use multiple cache entries or a single entry, use the following
  // heuristic: If the same texture address is used several times during the same frame, assume the
  // address is used for different purposes and allow creating an additional cache entry. If there's
  // at least one entry that hasn't been used for the same frame, then overwrite it, in order to
  // keep the cache as small as possible. If the current texture is found in the cache, use that
  // entry.
  //
  // For efb copies, the entry created in CopyRenderTargetToTexture always has to be used, or else
  // it was done in vain.
  auto iter_range = m_textures_by_address.equal_range(texture_info.GetRawAddress());
  TexAddrCache::iterator iter = iter_range.first;
  TexAddrCache::iterator oldest_entry = iter;
  int temp_frameCount = 0x7fffffff;
  TexAddrCache::iterator unconverted_copy = m_textures_by_address.end();
  TexAddrCache::iterator unreinterpreted_copy = m_textures_by_address.end();

  while (iter != iter_range.second)
  {
    RcTcacheEntry& entry = iter->second;

    // TODO: Some games (Rogue Squadron 3, Twin Snakes) seem to load a previously made XFB
    // copy as a regular texture. You can see this particularly well in RS3 whenever the
    // game freezes the image and fades it out to black on screen transitions, which fades
    // out a purple screen in XFB2Tex. Check for this here and convert them if necessary.

    // Do not load strided EFB copies, they are not meant to be used directly.
    // Also do not directly load EFB copies, which were partly overwritten.
    if (entry->IsEfbCopy() && entry->native_width == texture_info.GetRawWidth() &&
        entry->native_height == texture_info.GetRawHeight() &&
        entry->memory_stride == entry->BytesPerRow() && !entry->may_have_overlapping_textures)
    {
      // EFB copies have slightly different rules as EFB copy formats have different
      // meanings from texture formats.
      if ((base_hash == entry->hash &&
           (!texture_info.GetPaletteSize() || g_Config.backend_info.bSupportsPaletteConversion)) ||
          IsPlayingBackFifologWithBrokenEFBCopies)
      {
        // The texture format in VRAM must match the format that the copy was created with. Some
        // formats are inherently compatible, as the channel and bit layout is identical (e.g.
        // I8/C8). Others have the same number of bits per texel, and can be reinterpreted on the
        // GPU (e.g. IA4 and I8 or RGB565 and RGBA5). The only known game which reinteprets texels
        // in this manner is Spiderman Shattered Dimensions, where it creates a copy in B8 format,
        // and sets it up as a IA4 texture.
        if (!IsCompatibleTextureFormat(entry->format.texfmt, texture_info.GetTextureFormat()))
        {
          // Can we reinterpret this in VRAM?
          if (CanReinterpretTextureOnGPU(entry->format.texfmt, texture_info.GetTextureFormat()))
          {
            // Delay the conversion until afterwards, it's possible this texture has already been
            // converted.
            unreinterpreted_copy = iter++;
            continue;
          }
          else
          {
            // If the EFB copies are in a different format and are not reinterpretable, use the RAM
            // copy.
            ++iter;
            continue;
          }
        }
        else
        {
          // Prefer the already-converted copy.
          unconverted_copy = m_textures_by_address.end();
        }

        // TODO: We should check width/height/levels for EFB copies. I'm not sure what effect
        // checking width/height/levels would have.
        if (!texture_info.GetPaletteSize() || !g_Config.backend_info.bSupportsPaletteConversion)
          return entry;

        // Note that we found an unconverted EFB copy, then continue.  We'll
        // perform the conversion later.  Currently, we only convert EFB copies to
        // palette textures; we could do other conversions if it proved to be
        // beneficial.
        unconverted_copy = iter;
      }
      else
      {
        // Aggressively prune EFB copies: if it isn't useful here, it will probably
        // never be useful again.  It's theoretically possible for a game to do
        // something weird where the copy could become useful in the future, but in
        // practice it doesn't happen.
        iter = InvalidateTexture(iter);
        continue;
      }
    }
    else
    {
      // For normal textures, all texture parameters need to match
      if (!entry->IsEfbCopy() && entry->hash == full_hash && entry->format == full_format &&
          entry->native_levels >= texture_info.GetLevelCount() &&
          entry->native_width == texture_info.GetRawWidth() &&
          entry->native_height == texture_info.GetRawHeight())
      {
        entry = DoPartialTextureUpdates(iter->second, texture_info.GetTlutAddress(),
                                        texture_info.GetTlutFormat());
        if (entry)
        {
          entry->texture->FinishedRendering();
          return entry;
        }
      }
    }

    // Find the texture which hasn't been used for the longest time. Count paletted
    // textures as the same texture here, when the texture itself is the same. This
    // improves the performance a lot in some games that use paletted textures.
    // Example: Sonic the Fighters (inside Sonic Gems Collection)
    // Skip EFB copies here, so they can be used for partial texture updates
    // Also skip XFB copies, we might need to still scan them out
    // or load them as regular textures later.
    if (entry->frameCount != FRAMECOUNT_INVALID && entry->frameCount < temp_frameCount &&
        !entry->IsCopy() && !(texture_info.GetPaletteSize() && entry->base_hash == base_hash))
    {
      temp_frameCount = entry->frameCount;
      oldest_entry = iter;
    }
    ++iter;
  }

  if (unreinterpreted_copy != m_textures_by_address.end())
  {
    auto decoded_entry =
        ReinterpretEntry(unreinterpreted_copy->second, texture_info.GetTextureFormat());

    // It's possible to combine reinterpreted textures + palettes.
    if (unreinterpreted_copy == unconverted_copy && decoded_entry)
      decoded_entry = ApplyPaletteToEntry(decoded_entry, texture_info.GetTlutAddress(),
                                          texture_info.GetTlutFormat());

    if (decoded_entry)
      return decoded_entry;
  }

  if (unconverted_copy != m_textures_by_address.end())
  {
    auto decoded_entry = ApplyPaletteToEntry(
        unconverted_copy->second, texture_info.GetTlutAddress(), texture_info.GetTlutFormat());

    if (decoded_entry)
    {
      return decoded_entry;
    }
  }

  // Search the texture cache for normal textures by hash
  //
  // If the texture was fully hashed, the address does not need to match. Identical duplicate
  // textures cause unnecessary slowdowns
  // Example: Tales of Symphonia (GC) uses over 500 small textures in menus, but only around 70
  // different ones
  if (textureCacheSafetyColorSampleSize == 0 ||
      std::max(texture_info.GetTextureSize(), palette_size) <=
          (u32)textureCacheSafetyColorSampleSize * 8)
  {
    auto hash_range = m_textures_by_hash.equal_range(full_hash);
    TexHashCache::iterator hash_iter = hash_range.first;
    while (hash_iter != hash_range.second)
    {
      RcTcacheEntry& entry = hash_iter->second;
      // All parameters, except the address, need to match here
      if (entry->format == full_format && entry->native_levels >= texture_info.GetLevelCount() &&
          entry->native_width == texture_info.GetRawWidth() &&
          entry->native_height == texture_info.GetRawHeight())
      {
        entry = DoPartialTextureUpdates(hash_iter->second, texture_info.GetTlutAddress(),
                                        texture_info.GetTlutFormat());
        if (entry)
        {
          entry->texture->FinishedRendering();
          return entry;
        }
      }
      ++hash_iter;
    }
  }

  // If at least one entry was not used for the same frame, overwrite the oldest one
  if (temp_frameCount != 0x7fffffff)
  {
    // pool this texture and make a new one later
    InvalidateTexture(oldest_entry);
  }

  std::vector<VideoCommon::CachedAsset<VideoCommon::GameTextureAsset>> cached_game_assets;
  std::vector<std::shared_ptr<VideoCommon::TextureData>> data_for_assets;
  bool has_arbitrary_mipmaps = false;
  bool skip_texture_dump = false;
  std::shared_ptr<HiresTexture> hires_texture;
  if (g_ActiveConfig.bHiresTextures)
  {
    hires_texture = HiresTexture::Search(texture_info);
    if (hires_texture)
    {
      auto asset = hires_texture->GetAsset();
      const auto loaded_time = asset->GetLastLoadedTime();
      cached_game_assets.push_back(
          VideoCommon::CachedAsset<VideoCommon::GameTextureAsset>{std::move(asset), loaded_time});
      has_arbitrary_mipmaps = hires_texture->HasArbitraryMipmaps();
      skip_texture_dump = true;
    }
  }

  std::vector<VideoCommon::CachedAsset<VideoCommon::CustomAsset>> additional_dependencies;

  std::string texture_name = "";

  if (g_ActiveConfig.bGraphicMods)
  {
    u32 height = texture_info.GetRawHeight();
    u32 width = texture_info.GetRawWidth();
    if (hires_texture)
    {
      auto asset = hires_texture->GetAsset();
      if (asset)
      {
        auto data = asset->GetData();
        if (data)
        {
          if (!data->m_texture.m_slices.empty())
          {
            if (!data->m_texture.m_slices[0].m_levels.empty())
            {
              height = data->m_texture.m_slices[0].m_levels[0].height;
              width = data->m_texture.m_slices[0].m_levels[0].width;
            }
          }
        }
      }
    }
    texture_name = texture_info.CalculateTextureName().GetFullName();
    GraphicsModActionData::TextureCreate texture_create{
        texture_name, width, height, &cached_game_assets, &additional_dependencies};
    for (const auto& action : g_graphics_mod_manager->GetTextureCreateActions(texture_name))
    {
      action->OnTextureCreate(&texture_create);
    }
  }

  data_for_assets.reserve(cached_game_assets.size());
  for (auto& cached_asset : cached_game_assets)
  {
    auto data = cached_asset.m_asset->GetData();
    if (data)
    {
      if (cached_asset.m_asset->Validate(texture_info.GetRawWidth(), texture_info.GetRawHeight()))
      {
        data_for_assets.push_back(data);
      }
    }
  }

  auto entry =
      CreateTextureEntry(TextureCreationInfo{base_hash, full_hash, bytes_per_block, palette_size},
                         texture_info, textureCacheSafetyColorSampleSize,
                         std::move(data_for_assets), has_arbitrary_mipmaps, skip_texture_dump);
  entry->linked_game_texture_assets = std::move(cached_game_assets);
  entry->linked_asset_dependencies = std::move(additional_dependencies);
  entry->texture_info_name = std::move(texture_name);
  return entry;
}

// Note: the following function assumes all CustomTextureData has a single slice.  This is verified
// with the 'GameTexture::Validate' function after the data is loaded. Only a single slice is
// expected because each texture is loaded into a texture array
RcTcacheEntry TextureCacheBase::CreateTextureEntry(
    const TextureCreationInfo& creation_info, const TextureInfo& texture_info,
    const int safety_color_sample_size,
    std::vector<std::shared_ptr<VideoCommon::TextureData>> assets_data,
    const bool custom_arbitrary_mipmaps, bool skip_texture_dump)
{
#ifdef __APPLE__
  const bool no_mips = g_ActiveConfig.bNoMipmapping;
#else
  const bool no_mips = false;
#endif

  RcTcacheEntry entry;
  if (!assets_data.empty())
  {
    const auto calculate_max_levels = [&]() {
      const auto max_element = std::max_element(
          assets_data.begin(), assets_data.end(), [](const auto& lhs, const auto& rhs) {
            return lhs->m_texture.m_slices[0].m_levels.size() <
                   rhs->m_texture.m_slices[0].m_levels.size();
          });
      return (*max_element)->m_texture.m_slices[0].m_levels.size();
    };
    const u32 texLevels = no_mips ? 1 : (u32)calculate_max_levels();
    const auto& first_level = assets_data[0]->m_texture.m_slices[0].m_levels[0];
    const TextureConfig config(first_level.width, first_level.height, texLevels,
                               static_cast<u32>(assets_data.size()), 1, first_level.format, 0,
                               AbstractTextureType::Texture_2DArray);
    entry = AllocateCacheEntry(config);
    if (!entry) [[unlikely]]
      return entry;
    for (u32 data_index = 0; data_index < static_cast<u32>(assets_data.size()); data_index++)
    {
      const auto& asset = assets_data[data_index];
      const auto& slice = asset->m_texture.m_slices[0];
      for (u32 level_index = 0;
           level_index < std::min(texLevels, static_cast<u32>(slice.m_levels.size()));
           ++level_index)
      {
        const auto& level = slice.m_levels[level_index];
        entry->texture->Load(level_index, level.width, level.height, level.row_length,
                             level.data.data(), level.data.size(), data_index);
      }
    }

    entry->has_arbitrary_mips = custom_arbitrary_mipmaps;
    entry->is_custom_tex = true;
  }
  else
  {
    const u32 texLevels = no_mips ? 1 : texture_info.GetLevelCount();
    const u32 expanded_width = texture_info.GetExpandedWidth();
    const u32 expanded_height = texture_info.GetExpandedHeight();

    const u32 width = texture_info.GetRawWidth();
    const u32 height = texture_info.GetRawHeight();

    const TextureConfig config(width, height, texLevels, 1, 1, AbstractTextureFormat::RGBA8, 0,
                               AbstractTextureType::Texture_2DArray);
    entry = AllocateCacheEntry(config);
    if (!entry) [[unlikely]]
      return entry;

    // We can decode on the GPU if it is a supported format and the flag is enabled.
    // Currently we don't decode RGBA8 textures from TMEM, as that would require copying from both
    // banks, and if we're doing an copy we may as well just do the whole thing on the CPU, since
    // there's no conversion between formats. In the future this could be extended with a separate
    // shader, however.
    const bool decode_on_gpu =
        g_ActiveConfig.UseGPUTextureDecoding() &&
        !(texture_info.IsFromTmem() && texture_info.GetTextureFormat() == TextureFormat::RGBA8);

    ArbitraryMipmapDetector arbitrary_mip_detector;

    // Initialized to null because only software loading uses this buffer
    u8* dst_buffer = nullptr;

    if (!decode_on_gpu ||
        !DecodeTextureOnGPU(
            entry, 0, texture_info.GetData(), texture_info.GetTextureSize(),
            texture_info.GetTextureFormat(), width, height, expanded_width, expanded_height,
            creation_info.bytes_per_block * (expanded_width / texture_info.GetBlockWidth()),
            texture_info.GetTlutAddress(), texture_info.GetTlutFormat()))
    {
      size_t decoded_texture_size = expanded_width * sizeof(u32) * expanded_height;

      // Allocate memory for all levels at once
      size_t total_texture_size = decoded_texture_size;

      // For the downsample, we need 2 buffers; 1 is 1/4 of the original texture, the other 1/16
      size_t mip_downsample_buffer_size = decoded_texture_size * 5 / 16;

      size_t prev_level_size = decoded_texture_size;
      for (u32 i = 1; i < texture_info.GetLevelCount(); ++i)
      {
        prev_level_size /= 4;
        total_texture_size += prev_level_size;
      }

      // Add space for the downsampling at the end
      total_texture_size += mip_downsample_buffer_size;

      CheckTempSize(total_texture_size);
      dst_buffer = m_temp;
      if (!(texture_info.GetTextureFormat() == TextureFormat::RGBA8 && texture_info.IsFromTmem()))
      {
        TexDecoder_Decode(dst_buffer, texture_info.GetData(), expanded_width, expanded_height,
                          texture_info.GetTextureFormat(), texture_info.GetTlutAddress(),
                          texture_info.GetTlutFormat());
      }
      else
      {
        TexDecoder_DecodeRGBA8FromTmem(dst_buffer, texture_info.GetData(),
                                       texture_info.GetTmemOddAddress(), expanded_width,
                                       expanded_height);
      }

      entry->texture->Load(0, width, height, expanded_width, dst_buffer, decoded_texture_size);

      arbitrary_mip_detector.AddLevel(width, height, expanded_width, dst_buffer);

      dst_buffer += decoded_texture_size;
    }

    for (u32 level = 1; level != texLevels; ++level)
    {
      auto mip_level = texture_info.GetMipMapLevel(level - 1);
      if (!mip_level)
        continue;

      if (!decode_on_gpu ||
          !DecodeTextureOnGPU(entry, level, mip_level->GetData(), mip_level->GetTextureSize(),
                              texture_info.GetTextureFormat(), mip_level->GetRawWidth(),
                              mip_level->GetRawHeight(), mip_level->GetExpandedWidth(),
                              mip_level->GetExpandedHeight(),
                              creation_info.bytes_per_block *
                                  (mip_level->GetExpandedWidth() / texture_info.GetBlockWidth()),
                              texture_info.GetTlutAddress(), texture_info.GetTlutFormat()))
      {
        // No need to call CheckTempSize here, as the whole buffer is preallocated at the beginning
        const u32 decoded_mip_size =
            mip_level->GetExpandedWidth() * sizeof(u32) * mip_level->GetExpandedHeight();
        TexDecoder_Decode(dst_buffer, mip_level->GetData(), mip_level->GetExpandedWidth(),
                          mip_level->GetExpandedHeight(), texture_info.GetTextureFormat(),
                          texture_info.GetTlutAddress(), texture_info.GetTlutFormat());
        entry->texture->Load(level, mip_level->GetRawWidth(), mip_level->GetRawHeight(),
                             mip_level->GetExpandedWidth(), dst_buffer, decoded_mip_size);

        arbitrary_mip_detector.AddLevel(mip_level->GetRawWidth(), mip_level->GetRawHeight(),
                                        mip_level->GetExpandedWidth(), dst_buffer);

        dst_buffer += decoded_mip_size;
      }
    }

    entry->has_arbitrary_mips = arbitrary_mip_detector.HasArbitraryMipmaps(dst_buffer);

    if (g_ActiveConfig.bDumpTextures && !skip_texture_dump && texLevels > 0)
    {
      const std::string basename = texture_info.CalculateTextureName().GetFullName();
      if (g_ActiveConfig.bDumpBaseTextures)
      {
        m_texture_dumper.DumpTexture(*entry->texture, basename, 0, entry->has_arbitrary_mips);
      }
      if (g_ActiveConfig.bDumpMipmapTextures)
      {
        for (u32 level = 1; level < texLevels; ++level)
        {
          m_texture_dumper.DumpTexture(*entry->texture, basename, level, entry->has_arbitrary_mips);
        }
      }
    }
  }

  const auto iter = m_textures_by_address.emplace(texture_info.GetRawAddress(), entry);
  if (safety_color_sample_size == 0 ||
      std::max(texture_info.GetTextureSize(), creation_info.palette_size) <=
          (u32)safety_color_sample_size * 8)
  {
    entry->textures_by_hash_iter = m_textures_by_hash.emplace(creation_info.full_hash, entry);
  }

  const TextureAndTLUTFormat full_format(texture_info.GetTextureFormat(),
                                         texture_info.GetTlutFormat());
  entry->SetGeneralParameters(texture_info.GetRawAddress(), texture_info.GetTextureSize(),
                              full_format, false);
  entry->SetDimensions(texture_info.GetRawWidth(), texture_info.GetRawHeight(),
                       texture_info.GetLevelCount());
  entry->SetHashes(creation_info.base_hash, creation_info.full_hash);
  entry->memory_stride = entry->BytesPerRow();
  entry->SetNotCopy();

  INCSTAT(g_stats.num_textures_uploaded);
  SETSTAT(g_stats.num_textures_alive, static_cast<int>(m_textures_by_address.size()));

  entry = DoPartialTextureUpdates(iter->second, texture_info.GetTlutAddress(),
                                  texture_info.GetTlutFormat());

  // This should only be needed if the texture was updated, or used GPU decoding.
  entry->texture->FinishedRendering();

  return entry;
}

static void GetDisplayRectForXFBEntry(TCacheEntry* entry, u32 width, u32 height,
                                      MathUtil::Rectangle<int>* display_rect)
{
  // Scale the sub-rectangle to the full resolution of the texture.
  display_rect->left = 0;
  display_rect->top = 0;
  display_rect->right = static_cast<int>(width * entry->GetWidth() / entry->native_width);
  display_rect->bottom = static_cast<int>(height * entry->GetHeight() / entry->native_height);
}

RcTcacheEntry TextureCacheBase::GetXFBTexture(u32 address, u32 width, u32 height, u32 stride,
                                              MathUtil::Rectangle<int>* display_rect)
{
  // Compute total texture size. XFB textures aren't tiled, so this is simple.
  const u32 total_size = height * stride;

  auto& system = Core::System::GetInstance();
  auto& memory = system.GetMemory();
  const u8* src_data = memory.GetPointerForRange(address, total_size);
  if (!src_data)
  {
    ERROR_LOG_FMT(VIDEO, "Trying to load XFB texture from invalid address {:#010x}", address);
    return {};
  }

  // Do we currently have a mutable version of this XFB copy in VRAM?
  RcTcacheEntry entry = GetXFBFromCache(address, width, height, stride);
  if (entry && !entry->IsLocked())
  {
    if (entry->is_xfb_container)
    {
      StitchXFBCopy(entry);
      entry->texture->FinishedRendering();
    }

    GetDisplayRectForXFBEntry(entry.get(), width, height, display_rect);
    return entry;
  }

  // Create a new VRAM texture, and fill it with the data from guest RAM.
  entry = AllocateCacheEntry(TextureConfig(width, height, 1, 1, 1, AbstractTextureFormat::RGBA8,
                                           AbstractTextureFlag_RenderTarget,
                                           AbstractTextureType::Texture_2DArray));

  entry->SetGeneralParameters(address, total_size,
                              TextureAndTLUTFormat(TextureFormat::XFB, TLUTFormat::IA8), true);
  entry->SetDimensions(width, height, 1);
  entry->SetXfbCopy(stride);

  const u64 hash = entry->CalculateHash();
  entry->SetHashes(hash, hash);
  entry->is_xfb_container = true;
  entry->is_custom_tex = false;
  entry->may_have_overlapping_textures = false;
  entry->frameCount = FRAMECOUNT_INVALID;
  if (!g_ActiveConfig.UseGPUTextureDecoding() ||
      !DecodeTextureOnGPU(entry, 0, src_data, total_size, entry->format.texfmt, width, height,
                          width, height, stride, s_tex_mem.data(), entry->format.tlutfmt))
  {
    const u32 decoded_size = width * height * sizeof(u32);
    CheckTempSize(decoded_size);
    TexDecoder_DecodeXFB(m_temp, src_data, width, height, stride);
    entry->texture->Load(0, width, height, width, m_temp, decoded_size);
  }

  // Stitch any VRAM copies into the new RAM copy.
  StitchXFBCopy(entry);
  entry->texture->FinishedRendering();

  // Insert into the texture cache so we can re-use it next frame, if needed.
  m_textures_by_address.emplace(entry->addr, entry);
  SETSTAT(g_stats.num_textures_alive, static_cast<int>(m_textures_by_address.size()));
  INCSTAT(g_stats.num_textures_uploaded);

  if (g_ActiveConfig.bDumpXFBTarget || g_ActiveConfig.bGraphicMods)
  {
    const std::string id = fmt::format("{}x{}", width, height);
    if (g_ActiveConfig.bGraphicMods)
    {
      entry->texture_info_name = fmt::format("{}_{}", XFB_DUMP_PREFIX, id);
    }

    if (g_ActiveConfig.bDumpXFBTarget)
    {
      entry->texture->Save(fmt::format("{}{}_n{:06}_{}.png", File::GetUserPath(D_DUMPTEXTURES_IDX),
                                       XFB_DUMP_PREFIX, xfb_count++, id),
                           0);
    }
  }

  GetDisplayRectForXFBEntry(entry.get(), width, height, display_rect);
  return entry;
}

RcTcacheEntry TextureCacheBase::GetXFBFromCache(u32 address, u32 width, u32 height, u32 stride)
{
  auto iter_range = m_textures_by_address.equal_range(address);
  TexAddrCache::iterator iter = iter_range.first;

  while (iter != iter_range.second)
  {
    auto& entry = iter->second;

    // The only thing which has to match exactly is the stride. We can use a partial rectangle if
    // the VI width/height differs from that of the XFB copy.
    if (entry->is_xfb_copy && entry->memory_stride == stride && entry->native_width >= width &&
        entry->native_height >= height && !entry->may_have_overlapping_textures)
    {
      if (entry->hash == entry->CalculateHash() && !entry->reference_changed)
      {
        return entry;
      }
      else
      {
        // At this point, we either have an xfb copy that has changed its hash
        // or an xfb created by stitching or from memory that has been changed
        // we are safe to invalidate this
        iter = InvalidateTexture(iter);
        continue;
      }
    }

    ++iter;
  }

  return {};
}

void TextureCacheBase::StitchXFBCopy(RcTcacheEntry& stitched_entry)
{
  // It is possible that some of the overlapping textures overlap each other. This behavior has been
  // seen with XFB copies in Rogue Leader. To get the correct result, we apply the texture updates
  // in the order the textures were originally loaded. This ensures that the parts of the texture
  // that would have been overwritten in memory on real hardware get overwritten the same way here
  // too. This should work, but it may be a better idea to keep track of partial XFB copy
  // invalidations instead, which would reduce the amount of copying work here.
  std::vector<TCacheEntry*> candidates;
  bool create_upscaled_copy = false;

  auto iter = FindOverlappingTextures(stitched_entry->addr, stitched_entry->size_in_bytes);
  while (iter.first != iter.second)
  {
    // Currently, this checks the stride of the VRAM copy against the VI request. Therefore, for
    // interlaced modes, VRAM copies won't be considered candidates. This is okay for now, because
    // our force progressive hack means that an XFB copy should always have a matching stride. If
    // the hack is disabled, XFB2RAM should also be enabled. Should we wish to implement interlaced
    // stitching in the future, this would require a shader which grabs every second line.
    auto& entry = iter.first->second;
    if (entry != stitched_entry && entry->IsCopy() &&
        entry->OverlapsMemoryRange(stitched_entry->addr, stitched_entry->size_in_bytes) &&
        entry->memory_stride == stitched_entry->memory_stride)
    {
      if (entry->hash == entry->CalculateHash())
      {
        // Can't check the height here because of Y scaling.
        if (entry->native_width != entry->GetWidth())
          create_upscaled_copy = true;

        candidates.emplace_back(entry.get());
      }
      else
      {
        // If the hash does not match, this EFB copy will not be used for anything, so remove it
        iter.first = InvalidateTexture(iter.first);
        continue;
      }
    }
    ++iter.first;
  }

  if (candidates.empty())
    return;

  std::sort(candidates.begin(), candidates.end(),
            [](const TCacheEntry* a, const TCacheEntry* b) { return a->id < b->id; });

  // We only upscale when necessary to preserve resolution. i.e. when there are upscaled partial
  // copies to be stitched together.
  if (create_upscaled_copy)
  {
    ScaleTextureCacheEntryTo(stitched_entry,
                             g_framebuffer_manager->EFBToScaledX(stitched_entry->native_width),
                             g_framebuffer_manager->EFBToScaledY(stitched_entry->native_height));
  }

  for (TCacheEntry* entry : candidates)
  {
    int src_x, src_y, dst_x, dst_y;
    if (entry->addr >= stitched_entry->addr)
    {
      int pixel_offset = (entry->addr - stitched_entry->addr) / 2;
      src_x = 0;
      src_y = 0;
      dst_x = pixel_offset % stitched_entry->native_width;
      dst_y = pixel_offset / stitched_entry->native_width;
    }
    else
    {
      int pixel_offset = (stitched_entry->addr - entry->addr) / 2;
      src_x = pixel_offset % entry->native_width;
      src_y = pixel_offset / entry->native_width;
      dst_x = 0;
      dst_y = 0;
    }

    const int native_width =
        std::min(entry->native_width - src_x, stitched_entry->native_width - dst_x);
    const int native_height =
        std::min(entry->native_height - src_y, stitched_entry->native_height - dst_y);
    int src_width = native_width;
    int src_height = native_height;
    int dst_width = native_width;
    int dst_height = native_height;

    // Scale to internal resolution.
    if (entry->native_width != entry->GetWidth())
    {
      src_x = g_framebuffer_manager->EFBToScaledX(src_x);
      src_y = g_framebuffer_manager->EFBToScaledY(src_y);
      src_width = g_framebuffer_manager->EFBToScaledX(src_width);
      src_height = g_framebuffer_manager->EFBToScaledY(src_height);
    }
    if (create_upscaled_copy)
    {
      dst_x = g_framebuffer_manager->EFBToScaledX(dst_x);
      dst_y = g_framebuffer_manager->EFBToScaledY(dst_y);
      dst_width = g_framebuffer_manager->EFBToScaledX(dst_width);
      dst_height = g_framebuffer_manager->EFBToScaledY(dst_height);
    }

    // If the source rectangle is outside of what we actually have in VRAM, skip the copy.
    // The backend doesn't do any clamping, so if we don't, we'd pass out-of-range coordinates
    // to the graphics driver, which can cause GPU resets.
    if (static_cast<u32>(src_x + src_width) > entry->GetWidth() ||
        static_cast<u32>(src_y + src_height) > entry->GetHeight() ||
        static_cast<u32>(dst_x + dst_width) > stitched_entry->GetWidth() ||
        static_cast<u32>(dst_y + dst_height) > stitched_entry->GetHeight())
    {
      continue;
    }

    MathUtil::Rectangle<int> srcrect, dstrect;
    srcrect.left = src_x;
    srcrect.top = src_y;
    srcrect.right = (src_x + src_width);
    srcrect.bottom = (src_y + src_height);
    dstrect.left = dst_x;
    dstrect.top = dst_y;
    dstrect.right = (dst_x + dst_width);
    dstrect.bottom = (dst_y + dst_height);

    // We may have to scale if one of the copies is not internal resolution.
    if (srcrect.GetWidth() != dstrect.GetWidth() || srcrect.GetHeight() != dstrect.GetHeight())
    {
      g_gfx->ScaleTexture(stitched_entry->framebuffer.get(), dstrect, entry->texture.get(),
                          srcrect);
    }
    else
    {
      // If one copy is stereo, and the other isn't... not much we can do here :/
      const u32 layers_to_copy = std::min(entry->GetNumLayers(), stitched_entry->GetNumLayers());
      for (u32 layer = 0; layer < layers_to_copy; layer++)
      {
        stitched_entry->texture->CopyRectangleFromTexture(entry->texture.get(), srcrect, layer, 0,
                                                          dstrect, layer, 0);
      }
    }

    // Link the two textures together, so we won't apply this partial update again
    entry->CreateReference(stitched_entry.get());

    // Mark the texture update as used, as if it was loaded directly
    entry->frameCount = FRAMECOUNT_INVALID;
  }
}

std::array<u32, 3>
TextureCacheBase::GetRAMCopyFilterCoefficients(const CopyFilterCoefficients::Values& coefficients)
{
  // To simplify the backend, we precalculate the three coefficients in common. Coefficients 0, 1
  // are for the row above, 2, 3, 4 are for the current pixel, and 5, 6 are for the row below.
  return {
      static_cast<u32>(coefficients[0]) + static_cast<u32>(coefficients[1]),
      static_cast<u32>(coefficients[2]) + static_cast<u32>(coefficients[3]) +
          static_cast<u32>(coefficients[4]),
      static_cast<u32>(coefficients[5]) + static_cast<u32>(coefficients[6]),
  };
}

std::array<u32, 3>
TextureCacheBase::GetVRAMCopyFilterCoefficients(const CopyFilterCoefficients::Values& coefficients)
{
  // If the user disables the copy filter, only apply it to the VRAM copy.
  // This way games which are sensitive to changes to the RAM copy of the XFB will be unaffected.
  std::array<u32, 3> res = GetRAMCopyFilterCoefficients(coefficients);
  if (!g_ActiveConfig.bDisableCopyFilter)
    return res;

  // Disabling the copy filter in options should not ignore the values the game sets completely,
  // as some games use the filter coefficients to control the brightness of the screen. Instead,
  // add all coefficients to the middle sample, so the deflicker/vertical filter has no effect.
  res[1] = res[0] + res[1] + res[2];
  res[0] = 0;
  res[2] = 0;
  return res;
}

bool TextureCacheBase::AllCopyFilterCoefsNeeded(const std::array<u32, 3>& coefficients)
{
  // If the top/bottom coefficients are zero, no point sampling/blending from these rows.
  return coefficients[0] != 0 || coefficients[2] != 0;
}

bool TextureCacheBase::CopyFilterCanOverflow(const std::array<u32, 3>& coefficients)
{
  // Normally, the copy filter coefficients will sum to at most 64.  If the sum is higher than that,
  // colors are clamped to the range [0, 255], but if the sum is higher than 128, that clamping
  // breaks (as colors end up >= 512, which wraps back to 0).
  return coefficients[0] + coefficients[1] + coefficients[2] >= 128;
}

void TextureCacheBase::CopyRenderTargetToTexture(
    u32 dstAddr, EFBCopyFormat dstFormat, u32 width, u32 height, u32 dstStride, bool is_depth_copy,
    const MathUtil::Rectangle<int>& srcRect, bool isIntensity, bool scaleByHalf, float y_scale,
    float gamma, bool clamp_top, bool clamp_bottom,
    const CopyFilterCoefficients::Values& filter_coefficients)
{
  // Emulation methods:
  //
  // - EFB to RAM:
  //      Encodes the requested EFB data at its native resolution to the emulated RAM using shaders.
  //      Load() decodes the data from there again (using TextureDecoder) if the EFB copy is being
  //      used as a texture again.
  //      Advantage: CPU can read data from the EFB copy and we don't lose any important updates to
  //      the texture
  //      Disadvantage: Encoding+decoding steps often are redundant because only some games read or
  //      modify EFB copies before using them as textures.
  //
  // - EFB to texture:
  //      Copies the requested EFB data to a texture object in VRAM, performing any color conversion
  //      using shaders.
  //      Advantage: Works for many games, since in most cases EFB copies aren't read or modified at
  //      all before being used as a texture again.
  //                 Since we don't do any further encoding or decoding here, this method is much
  //                 faster.
  //                 It also allows enhancing the visual quality by doing scaled EFB copies.
  //
  // - Hybrid EFB copies:
  //      1a) Whenever this function gets called, encode the requested EFB data to RAM (like EFB to
  //      RAM)
  //      1b) Set type to TCET_EC_DYNAMIC for all texture cache entries in the destination address
  //      range.
  //          If EFB copy caching is enabled, further checks will (try to) prevent redundant EFB
  //          copies.
  //      2) Check if a texture cache entry for the specified dstAddr already exists (i.e. if an EFB
  //      copy was triggered to that address before):
  //      2a) Entry doesn't exist:
  //          - Also copy the requested EFB data to a texture object in VRAM (like EFB to texture)
  //          - Create a texture cache entry for the target (type = TCET_EC_VRAM)
  //          - Store a hash of the encoded RAM data in the texcache entry.
  //      2b) Entry exists AND type is TCET_EC_VRAM:
  //          - Like case 2a, but reuse the old texcache entry instead of creating a new one.
  //      2c) Entry exists AND type is TCET_EC_DYNAMIC:
  //          - Only encode the texture to RAM (like EFB to RAM) and store a hash of the encoded
  //          data in the existing texcache entry.
  //          - Do NOT copy the requested EFB data to a VRAM object. Reason: the texture is dynamic,
  //          i.e. the CPU is modifying it. Storing a VRAM copy is useless, because we'd always end
  //          up deleting it and reloading the data from RAM anyway.
  //      3) If the EFB copy gets used as a texture, compare the source RAM hash with the hash you
  //      stored when encoding the EFB data to RAM.
  //      3a) If the two hashes match AND type is TCET_EC_VRAM, reuse the VRAM copy you created
  //      3b) If the two hashes differ AND type is TCET_EC_VRAM, screw your existing VRAM copy. Set
  //      type to TCET_EC_DYNAMIC.
  //          Redecode the source RAM data to a VRAM object. The entry basically behaves like a
  //          normal texture now.
  //      3c) If type is TCET_EC_DYNAMIC, treat the EFB copy like a normal texture.
  //      Advantage: Non-dynamic EFB copies can be visually enhanced like with EFB to texture.
  //                 Compatibility is as good as EFB to RAM.
  //      Disadvantage: Slower than EFB to texture and often even slower than EFB to RAM.
  //                    EFB copy cache depends on accurate texture hashing being enabled. However,
  //                    with accurate hashing you end up being as slow as without a copy cache
  //                    anyway.
  //
  // Disadvantage of all methods: Calling this function requires the GPU to perform a pipeline flush
  // which stalls any further CPU processing.
  const bool is_xfb_copy = !is_depth_copy && !isIntensity && dstFormat == EFBCopyFormat::XFB;
  bool copy_to_vram =
      g_ActiveConfig.backend_info.bSupportsCopyToVram && !g_ActiveConfig.bDisableCopyToVRAM;
  bool copy_to_ram =
      !(is_xfb_copy ? g_ActiveConfig.bSkipXFBCopyToRam : g_ActiveConfig.bSkipEFBCopyToRam) ||
      !copy_to_vram;

  // tex_w and tex_h are the native size of the texture in the GC memory.
  // The size scaled_* represents the emulated texture. Those differ
  // because of upscaling and because of yscaling of XFB copies.
  // For the latter, we keep the EFB resolution for the virtual XFB blit.
  u32 tex_w = width;
  u32 tex_h = height;
  u32 scaled_tex_w = g_framebuffer_manager->EFBToScaledX(width);
  u32 scaled_tex_h = g_framebuffer_manager->EFBToScaledY(height);

  if (scaleByHalf)
  {
    tex_w /= 2;
    tex_h /= 2;
    scaled_tex_w /= 2;
    scaled_tex_h /= 2;
  }

  if (!is_xfb_copy && !g_ActiveConfig.bCopyEFBScaled)
  {
    // No upscaling
    scaled_tex_w = tex_w;
    scaled_tex_h = tex_h;
  }

  // Get the base (in memory) format of this efb copy.
  TextureFormat baseFormat = TexDecoder_GetEFBCopyBaseFormat(dstFormat);

  u32 blockH = TexDecoder_GetBlockHeightInTexels(baseFormat);
  const u32 blockW = TexDecoder_GetBlockWidthInTexels(baseFormat);

  // Round up source height to multiple of block size
  u32 actualHeight = Common::AlignUp(tex_h, blockH);
  const u32 actualWidth = Common::AlignUp(tex_w, blockW);

  u32 num_blocks_y = actualHeight / blockH;
  const u32 num_blocks_x = actualWidth / blockW;

  // RGBA takes two cache lines per block; all others take one
  const u32 bytes_per_block = baseFormat == TextureFormat::RGBA8 ? 64 : 32;

  const u32 bytes_per_row = num_blocks_x * bytes_per_block;
  const u32 covered_range = num_blocks_y * dstStride;

  auto& system = Core::System::GetInstance();
  auto& memory = system.GetMemory();
  u8* dst = memory.GetPointerForRange(dstAddr, covered_range);
  if (dst == nullptr)
  {
    ERROR_LOG_FMT(VIDEO, "Trying to copy from EFB to invalid address {:#010x}", dstAddr);
    return;
  }

  if (g_ActiveConfig.bGraphicMods)
  {
    FBInfo info;
    info.m_width = tex_w;
    info.m_height = tex_h;
    info.m_texture_format = baseFormat;
    if (is_xfb_copy)
    {
      for (const auto& action : g_graphics_mod_manager->GetXFBActions(info))
      {
        action->OnXFB();
      }
    }
    else
    {
      bool skip = false;
      GraphicsModActionData::EFB efb{tex_w, tex_h, &skip, &scaled_tex_w, &scaled_tex_h};
      for (const auto& action : g_graphics_mod_manager->GetEFBActions(info))
      {
        action->OnEFB(&efb);
      }
      if (skip == true)
      {
        if (copy_to_ram)
          UninitializeEFBMemory(dst, dstStride, bytes_per_row, num_blocks_y);
        return;
      }
    }
  }

  if (dstStride < bytes_per_row)
  {
    // This kind of efb copy results in a scrambled image.
    // I'm pretty sure no game actually wants to do this, it might be caused by a
    // programming bug in the game, or a CPU/Bounding box emulation issue with dolphin.
    // The copy_to_ram code path above handles this "correctly" and scrambles the image
    // but the copy_to_vram code path just saves and uses unscrambled texture instead.

    // To avoid a "incorrect" result, we simply skip doing the copy_to_vram code path
    // so if the game does try to use the scrambled texture, dolphin will grab the scrambled
    // texture (or black if copy_to_ram is also disabled) out of ram.
    ERROR_LOG_FMT(VIDEO, "Memory stride too small ({} < {})", dstStride, bytes_per_row);
    copy_to_vram = false;
  }

  // We also linear filtering for both box filtering and downsampling higher resolutions to 1x.
  // TODO: This only produces perfect downsampling for 2x IR, other resolutions will need more
  //       complex down filtering to average all pixels and produce the correct result.
  const bool linear_filter =
      !is_depth_copy &&
      (scaleByHalf || g_framebuffer_manager->GetEFBScale() != 1 || y_scale > 1.0f);

  RcTcacheEntry entry;
  if (copy_to_vram)
  {
    // create the texture
    const TextureConfig config(scaled_tex_w, scaled_tex_h, 1, g_framebuffer_manager->GetEFBLayers(),
                               1, AbstractTextureFormat::RGBA8, AbstractTextureFlag_RenderTarget,
                               AbstractTextureType::Texture_2DArray);
    entry = AllocateCacheEntry(config);
    if (entry)
    {
      entry->SetGeneralParameters(dstAddr, 0, baseFormat, is_xfb_copy);
      entry->SetDimensions(tex_w, tex_h, 1);
      entry->frameCount = FRAMECOUNT_INVALID;
      if (is_xfb_copy)
      {
        entry->should_force_safe_hashing = is_xfb_copy;
        entry->SetXfbCopy(dstStride);
      }
      else
      {
        entry->SetEfbCopy(dstStride);
      }
      entry->may_have_overlapping_textures = false;
      entry->is_custom_tex = false;

      CopyEFBToCacheEntry(entry, is_depth_copy, srcRect, scaleByHalf, linear_filter, dstFormat,
                          isIntensity, gamma, clamp_top, clamp_bottom,
                          GetVRAMCopyFilterCoefficients(filter_coefficients));

      if (is_xfb_copy && (g_ActiveConfig.bDumpXFBTarget || g_ActiveConfig.bGraphicMods))
      {
        const std::string id = fmt::format("{}x{}", tex_w, tex_h);
        if (g_ActiveConfig.bGraphicMods)
        {
          entry->texture_info_name = fmt::format("{}_{}", XFB_DUMP_PREFIX, id);
        }

        if (g_ActiveConfig.bDumpXFBTarget)
        {
          entry->texture->Save(fmt::format("{}{}_n{:06}_{}.png",
                                           File::GetUserPath(D_DUMPTEXTURES_IDX), XFB_DUMP_PREFIX,
                                           xfb_count++, id),
                               0);
        }
      }
      else if (g_ActiveConfig.bDumpEFBTarget || g_ActiveConfig.bGraphicMods)
      {
        const std::string id = fmt::format("{}x{}_{}", tex_w, tex_h, static_cast<int>(baseFormat));
        if (g_ActiveConfig.bGraphicMods)
        {
          entry->texture_info_name = fmt::format("{}_{}", EFB_DUMP_PREFIX, id);
        }

        if (g_ActiveConfig.bDumpEFBTarget)
        {
          static int efb_count = 0;
          entry->texture->Save(fmt::format("{}{}_n{:06}_{}.png",
                                           File::GetUserPath(D_DUMPTEXTURES_IDX), EFB_DUMP_PREFIX,
                                           efb_count++, id),
                               0);
        }
      }
    }
  }

  if (copy_to_ram)
  {
    const std::array<u32, 3> coefficients = GetRAMCopyFilterCoefficients(filter_coefficients);
    PixelFormat srcFormat = bpmem.zcontrol.pixel_format;
    EFBCopyParams format(srcFormat, dstFormat, is_depth_copy, isIntensity,
                         AllCopyFilterCoefsNeeded(coefficients),
                         CopyFilterCanOverflow(coefficients), gamma != 1.0);

    std::unique_ptr<AbstractStagingTexture> staging_texture = GetEFBCopyStagingTexture();
    if (staging_texture)
    {
      CopyEFB(staging_texture.get(), format, tex_w, bytes_per_row, num_blocks_y, dstStride, srcRect,
              scaleByHalf, linear_filter, y_scale, gamma, clamp_top, clamp_bottom, coefficients);

      // We can't defer if there is no VRAM copy (since we need to update the hash).
      if (!copy_to_vram || !g_ActiveConfig.bDeferEFBCopies)
      {
        // Immediately flush it.
        WriteEFBCopyToRAM(dst, bytes_per_row / sizeof(u32), num_blocks_y, dstStride,
                          std::move(staging_texture));
      }
      else
      {
        // Defer the flush until later.
        entry->pending_efb_copy = std::move(staging_texture);
        entry->pending_efb_copy_width = bytes_per_row / sizeof(u32);
        entry->pending_efb_copy_height = num_blocks_y;
        m_pending_efb_copies.push_back(entry);
      }
    }
  }
  else
  {
    if (is_xfb_copy)
    {
      UninitializeXFBMemory(dst, dstStride, bytes_per_row, num_blocks_y);
    }
    else
    {
      UninitializeEFBMemory(dst, dstStride, bytes_per_row, num_blocks_y);
    }
  }

  // Invalidate all textures, if they are either fully overwritten by our efb copy, or if they
  // have a different stride than our efb copy. Partly overwritten textures with the same stride
  // as our efb copy are marked to check them for partial texture updates.
  // TODO: The logic to detect overlapping strided efb copies is not 100% accurate.
  bool strided_efb_copy = dstStride != bytes_per_row;
  auto iter = FindOverlappingTextures(dstAddr, covered_range);
  while (iter.first != iter.second)
  {
    RcTcacheEntry& overlapping_entry = iter.first->second;

    if (overlapping_entry->addr == dstAddr && overlapping_entry->is_xfb_copy)
    {
      for (auto& reference : overlapping_entry->references)
      {
        reference->reference_changed = true;
      }
    }

    if (overlapping_entry->OverlapsMemoryRange(dstAddr, covered_range))
    {
      u32 overlap_range = std::min(overlapping_entry->addr + overlapping_entry->size_in_bytes,
                                   dstAddr + covered_range) -
                          std::max(overlapping_entry->addr, dstAddr);
      if (!copy_to_vram || overlapping_entry->memory_stride != dstStride ||
          (!strided_efb_copy && overlapping_entry->size_in_bytes == overlap_range) ||
          (strided_efb_copy && overlapping_entry->size_in_bytes == overlap_range &&
           overlapping_entry->addr == dstAddr))
      {
        // Pending EFB copies which are completely covered by this new copy can simply be tossed,
        // instead of having to flush them later on, since this copy will write over everything.
        iter.first = InvalidateTexture(iter.first, true);
        continue;
      }

      // We don't want to change the may_have_overlapping_textures flag on XFB container entries
      // because otherwise they can't be re-used/updated, leaking textures for several frames.
      if (!overlapping_entry->is_xfb_container)
        overlapping_entry->may_have_overlapping_textures = true;

      // There are cases (Rogue Squadron 2 / Texas Holdem on Wiiware) where
      // for xfb copies the textures overlap which causes the hash of the first copy
      // to be different (from when it was originally created).  This has no implications
      // for XFB2Tex because the underlying memory doesn't change (dummy values) but
      // can affect XFB2Ram when we compare the texture cache copy hash with the
      // newly computed hash
      // By calculating the hash when we receive overlapping xfbs, we are able
      // to mitigate this
      if (overlapping_entry->is_xfb_copy && copy_to_ram)
      {
        overlapping_entry->hash = overlapping_entry->CalculateHash();
      }

      // Do not load textures by hash, if they were at least partly overwritten by an efb copy.
      // In this case, comparing the hash is not enough to check, if two textures are identical.
      if (overlapping_entry->textures_by_hash_iter != m_textures_by_hash.end())
      {
        m_textures_by_hash.erase(overlapping_entry->textures_by_hash_iter);
        overlapping_entry->textures_by_hash_iter = m_textures_by_hash.end();
      }
    }
    ++iter.first;
  }

  if (OpcodeDecoder::g_record_fifo_data)
  {
    // Mark the memory behind this efb copy as dynamicly generated for the Fifo log
    u32 address = dstAddr;
    for (u32 i = 0; i < num_blocks_y; i++)
    {
      Core::System::GetInstance().GetFifoRecorder().UseMemory(address, bytes_per_row,
                                                              MemoryUpdate::Type::TextureMap, true);
      address += dstStride;
    }
  }

  // Even if the copy is deferred, still compute the hash. This way if the copy is used as a texture
  // in a subsequent draw before it is flushed, it will have the same hash.
  if (entry)
  {
    const u64 hash = entry->CalculateHash();
    entry->SetHashes(hash, hash);
    m_textures_by_address.emplace(dstAddr, std::move(entry));
  }
}

void TextureCacheBase::FlushEFBCopies()
{
  if (m_pending_efb_copies.empty())
    return;

  for (auto& entry : m_pending_efb_copies)
    FlushEFBCopy(entry.get());
  m_pending_efb_copies.clear();
}

void TextureCacheBase::FlushStaleBinds()
{
  for (u32 i = 0; i < m_bound_textures.size(); i++)
  {
    if (!TMEM::IsCached(i))
      m_bound_textures[i].reset();
  }
}

void TextureCacheBase::WriteEFBCopyToRAM(u8* dst_ptr, u32 width, u32 height, u32 stride,
                                         std::unique_ptr<AbstractStagingTexture> staging_texture)
{
  MathUtil::Rectangle<int> copy_rect(0, 0, static_cast<int>(width), static_cast<int>(height));
  staging_texture->ReadTexels(copy_rect, dst_ptr, stride);
  ReleaseEFBCopyStagingTexture(std::move(staging_texture));
}

void TextureCacheBase::FlushEFBCopy(TCacheEntry* entry)
{
  const u32 covered_range = entry->pending_efb_copy_height * entry->memory_stride;

  // Copy from texture -> guest memory.
  auto& system = Core::System::GetInstance();
  auto& memory = system.GetMemory();
  u8* const dst = memory.GetPointerForRange(entry->addr, covered_range);
  WriteEFBCopyToRAM(dst, entry->pending_efb_copy_width, entry->pending_efb_copy_height,
                    entry->memory_stride, std::move(entry->pending_efb_copy));

  // If the EFB copy was invalidated (e.g. the bloom case mentioned in InvalidateTexture), we don't
  // need to do anything more. The entry will be automatically deleted by smart pointers
  if (entry->invalidated)
    return;

  // Re-hash the texture now that the guest memory is populated.
  // This should be safe because we'll catch any writes before the game can modify it.
  const u64 hash = entry->CalculateHash();
  entry->SetHashes(hash, hash);

  // Check for any overlapping XFB copies which now need the hash recomputed.
  // See the comment above regarding Rogue Squadron 2.
  if (entry->is_xfb_copy)
  {
    auto range = FindOverlappingTextures(entry->addr, covered_range);
    for (auto iter = range.first; iter != range.second; ++iter)
    {
      auto& overlapping_entry = iter->second;
      if (overlapping_entry->may_have_overlapping_textures && overlapping_entry->is_xfb_copy &&
          overlapping_entry->OverlapsMemoryRange(entry->addr, covered_range))
      {
        const u64 overlapping_hash = overlapping_entry->CalculateHash();
        entry->SetHashes(overlapping_hash, overlapping_hash);
      }
    }
  }
}

std::unique_ptr<AbstractStagingTexture> TextureCacheBase::GetEFBCopyStagingTexture()
{
  // Pull off the back first to re-use the most frequently used textures.
  if (!m_efb_copy_staging_texture_pool.empty())
  {
    auto ptr = std::move(m_efb_copy_staging_texture_pool.back());
    m_efb_copy_staging_texture_pool.pop_back();
    return ptr;
  }

  std::unique_ptr<AbstractStagingTexture> tex = g_gfx->CreateStagingTexture(
      StagingTextureType::Readback, m_efb_encoding_texture->GetConfig());
  if (!tex)
    WARN_LOG_FMT(VIDEO, "Failed to create EFB copy staging texture");

  return tex;
}

void TextureCacheBase::ReleaseEFBCopyStagingTexture(std::unique_ptr<AbstractStagingTexture> tex)
{
  m_efb_copy_staging_texture_pool.push_back(std::move(tex));
}

void TextureCacheBase::UninitializeEFBMemory(u8* dst, u32 stride, u32 bytes_per_row,
                                             u32 num_blocks_y)
{
  // Hack: Most games don't actually need the correct texture data in RAM
  //       and we can just keep a copy in VRAM. We zero the memory so we
  //       can check it hasn't changed before using our copy in VRAM.
  u8* ptr = dst;
  for (u32 i = 0; i < num_blocks_y; i++)
  {
    std::memset(ptr, 0, bytes_per_row);
    ptr += stride;
  }
}

void TextureCacheBase::UninitializeXFBMemory(u8* dst, u32 stride, u32 bytes_per_row,
                                             u32 num_blocks_y)
{
  // Originally, we planned on using a 'key color'
  // for alpha to address partial xfbs (Mario Strikers / Chicken Little).
  // This work was removed since it was unfinished but there
  // was still a desire to differentiate between the old and the new approach
  // which is why we still set uninitialized xfb memory to fuchsia
  // (Y=1,U=254,V=254) instead of dark green (Y=0,U=0,V=0) in YUV
  // like is done in the EFB path.

#if defined(_M_X86_64)
  __m128i sixteenBytes = _mm_set1_epi16((s16)(u16)0xFE01);
#endif

  for (u32 i = 0; i < num_blocks_y; i++)
  {
    u32 size = bytes_per_row;
    u8* rowdst = dst;
#if defined(_M_X86_64)
    while (size >= 16)
    {
      _mm_storeu_si128((__m128i*)rowdst, sixteenBytes);
      size -= 16;
      rowdst += 16;
    }
#endif
    for (u32 offset = 0; offset < size; offset++)
    {
      if (offset & 1)
      {
        rowdst[offset] = 254;
      }
      else
      {
        rowdst[offset] = 1;
      }
    }
    dst += stride;
  }
}

RcTcacheEntry TextureCacheBase::AllocateCacheEntry(const TextureConfig& config)
{
  std::optional<TexPoolEntry> alloc = AllocateTexture(config);
  if (!alloc)
    return {};

  auto cacheEntry =
      std::make_shared<TCacheEntry>(std::move(alloc->texture), std::move(alloc->framebuffer));
  cacheEntry->textures_by_hash_iter = m_textures_by_hash.end();
  cacheEntry->id = m_last_entry_id++;
  return cacheEntry;
}

std::optional<TextureCacheBase::TexPoolEntry>
TextureCacheBase::AllocateTexture(const TextureConfig& config)
{
  TexPool::iterator iter = FindMatchingTextureFromPool(config);
  if (iter != m_texture_pool.end())
  {
    auto entry = std::move(iter->second);
    m_texture_pool.erase(iter);
    return std::move(entry);
  }

  std::unique_ptr<AbstractTexture> texture = g_gfx->CreateTexture(config);
  if (!texture)
  {
    WARN_LOG_FMT(VIDEO, "Failed to allocate a {}x{}x{} texture", config.width, config.height,
                 config.layers);
    return {};
  }

  std::unique_ptr<AbstractFramebuffer> framebuffer;
  if (config.IsRenderTarget())
  {
    framebuffer = g_gfx->CreateFramebuffer(texture.get(), nullptr);
    if (!framebuffer)
    {
      WARN_LOG_FMT(VIDEO, "Failed to allocate a {}x{}x{} framebuffer", config.width, config.height,
                   config.layers);
      return {};
    }
  }

  INCSTAT(g_stats.num_textures_created);
  return TexPoolEntry(std::move(texture), std::move(framebuffer));
}

TextureCacheBase::TexPool::iterator
TextureCacheBase::FindMatchingTextureFromPool(const TextureConfig& config)
{
  // Find a texture from the pool that does not have a frameCount of FRAMECOUNT_INVALID.
  // This prevents a texture from being used twice in a single frame with different data,
  // which potentially means that a driver has to maintain two copies of the texture anyway.
  // Render-target textures are fine through, as they have to be generated in a seperated pass.
  // As non-render-target textures are usually static, this should not matter much.
  auto range = m_texture_pool.equal_range(config);
  auto matching_iter = std::find_if(range.first, range.second, [](const auto& iter) {
    return iter.first.IsRenderTarget() || iter.second.frameCount != FRAMECOUNT_INVALID;
  });
  return matching_iter != range.second ? matching_iter : m_texture_pool.end();
}

TextureCacheBase::TexAddrCache::iterator TextureCacheBase::GetTexCacheIter(TCacheEntry* entry)
{
  auto iter_range = m_textures_by_address.equal_range(entry->addr);
  TexAddrCache::iterator iter = iter_range.first;
  while (iter != iter_range.second)
  {
    if (iter->second.get() == entry)
    {
      return iter;
    }
    ++iter;
  }
  return m_textures_by_address.end();
}

std::pair<TextureCacheBase::TexAddrCache::iterator, TextureCacheBase::TexAddrCache::iterator>
TextureCacheBase::FindOverlappingTextures(u32 addr, u32 size_in_bytes)
{
  // We index by the starting address only, so there is no way to query all textures
  // which end after the given addr. But the GC textures have a limited size, so we
  // look for all textures which have a start address bigger than addr minus the maximal
  // texture size. But this yields false-positives which must be checked later on.

  // 1024 x 1024 texel times 8 nibbles per texel
  constexpr u32 max_texture_size = 1024 * 1024 * 4;
  u32 lower_addr = addr > max_texture_size ? addr - max_texture_size : 0;
  auto begin = m_textures_by_address.lower_bound(lower_addr);
  auto end = m_textures_by_address.upper_bound(addr + size_in_bytes);

  return std::make_pair(begin, end);
}

TextureCacheBase::TexAddrCache::iterator
TextureCacheBase::InvalidateTexture(TexAddrCache::iterator iter, bool discard_pending_efb_copy)
{
  if (iter == m_textures_by_address.end())
    return m_textures_by_address.end();

  RcTcacheEntry& entry = iter->second;

  if (entry->textures_by_hash_iter != m_textures_by_hash.end())
  {
    m_textures_by_hash.erase(entry->textures_by_hash_iter);
    entry->textures_by_hash_iter = m_textures_by_hash.end();
  }

  // If this is a pending EFB copy, we don't want to flush it here.
  // Why? Because let's say a game is rendering a bloom-type effect, using EFB copies to essentially
  // downscale the framebuffer. Copy from EFB->Texture, draw texture to EFB, copy EFB->Texture,
  // draw, repeat. The second copy will invalidate the first, forcing a flush. Which means we lose
  // any benefit of EFB copy batching. So instead, let's just leave the EFB copy pending, but remove
  // it from the texture cache. This way we don't use the old VRAM copy. When the EFB copies are
  // eventually flushed, they will overwrite each other, and the end result should be the same.
  if (entry->pending_efb_copy)
  {
    if (discard_pending_efb_copy)
    {
      // If the RAM copy is being completely overwritten by a new EFB copy, we can discard the
      // existing pending copy, and not bother waiting for it in the future. This happens in
      // Xenoblade's sunset scene, where 35 copies are done per frame, and 25 of them are
      // copied to the same address, and can be skipped.
      ReleaseEFBCopyStagingTexture(std::move(entry->pending_efb_copy));
      auto pending_it = std::find(m_pending_efb_copies.begin(), m_pending_efb_copies.end(), entry);
      if (pending_it != m_pending_efb_copies.end())
        m_pending_efb_copies.erase(pending_it);
    }
    else
    {
      // The texture data has already been copied into the staging texture, so it's valid to
      // optimistically release the texture data. Will slightly lower VRAM usage.
      if (!entry->IsLocked())
        ReleaseToPool(entry.get());
    }
  }
  entry->invalidated = true;

  return m_textures_by_address.erase(iter);
}

void TextureCacheBase::ReleaseToPool(TCacheEntry* entry)
{
  if (!entry->texture)
    return;
  auto config = entry->texture->GetConfig();
  m_texture_pool.emplace(config,
                         TexPoolEntry(std::move(entry->texture), std::move(entry->framebuffer)));
}

bool TextureCacheBase::CreateUtilityTextures()
{
  constexpr TextureConfig encoding_texture_config(
      EFB_WIDTH * 4, 1024, 1, 1, 1, AbstractTextureFormat::BGRA8, AbstractTextureFlag_RenderTarget,
      AbstractTextureType::Texture_2DArray);
  m_efb_encoding_texture = g_gfx->CreateTexture(encoding_texture_config, "EFB encoding texture");
  if (!m_efb_encoding_texture)
    return false;

  m_efb_encoding_framebuffer = g_gfx->CreateFramebuffer(m_efb_encoding_texture.get(), nullptr);
  if (!m_efb_encoding_framebuffer)
    return false;

  if (g_ActiveConfig.backend_info.bSupportsGPUTextureDecoding)
  {
    constexpr TextureConfig decoding_texture_config(
        1024, 1024, 1, 1, 1, AbstractTextureFormat::RGBA8, AbstractTextureFlag_ComputeImage,
        AbstractTextureType::Texture_2DArray);
    m_decoding_texture =
        g_gfx->CreateTexture(decoding_texture_config, "GPU texture decoding texture");
    if (!m_decoding_texture)
      return false;
  }

  return true;
}

void TextureCacheBase::CopyEFBToCacheEntry(RcTcacheEntry& entry, bool is_depth_copy,
                                           const MathUtil::Rectangle<int>& src_rect,
                                           bool scale_by_half, bool linear_filter,
                                           EFBCopyFormat dst_format, bool is_intensity, float gamma,
                                           bool clamp_top, bool clamp_bottom,
                                           const std::array<u32, 3>& filter_coefficients)
{
  // Flush EFB pokes first, as they're expected to be included.
  g_framebuffer_manager->FlushEFBPokes();

  // Get the pipeline which we will be using. If the compilation failed, this will be null.
  const AbstractPipeline* copy_pipeline = g_shader_cache->GetEFBCopyToVRAMPipeline(
      TextureConversionShaderGen::GetShaderUid(dst_format, is_depth_copy, is_intensity,
                                               scale_by_half, 1.0f / gamma, filter_coefficients));
  if (!copy_pipeline)
  {
    WARN_LOG_FMT(VIDEO, "Skipping EFB copy to VRAM due to missing pipeline.");
    return;
  }

  const auto scaled_src_rect = g_framebuffer_manager->ConvertEFBRectangle(src_rect);
  const auto framebuffer_rect = g_gfx->ConvertFramebufferRectangle(
      scaled_src_rect, g_framebuffer_manager->GetEFBFramebuffer());
  AbstractTexture* src_texture =
      is_depth_copy ? g_framebuffer_manager->ResolveEFBDepthTexture(framebuffer_rect) :
                      g_framebuffer_manager->ResolveEFBColorTexture(framebuffer_rect);

  src_texture->FinishedRendering();
  g_gfx->BeginUtilityDrawing();

  // Fill uniform buffer.
  struct Uniforms
  {
    float src_left, src_top, src_width, src_height;
    std::array<u32, 3> filter_coefficients;
    float gamma_rcp;
    float clamp_top;
    float clamp_bottom;
    float pixel_height;
    float rcp_pixel_width;
    float rcp_pixel_height;
    float efb_scale;
    u32 efb_copy_scaled;
    u32 linear_filter;
    u32 padding;
  };
  Uniforms uniforms;
  const float rcp_efb_width = 1.0f / static_cast<float>(g_framebuffer_manager->GetEFBWidth());
  const u32 efb_height = g_framebuffer_manager->GetEFBHeight();
  const float rcp_efb_height = 1.0f / static_cast<float>(efb_height);
  uniforms.src_left = framebuffer_rect.left * rcp_efb_width;
  uniforms.src_top = framebuffer_rect.top * rcp_efb_height;
  uniforms.src_width = framebuffer_rect.GetWidth() * rcp_efb_width;
  uniforms.src_height = framebuffer_rect.GetHeight() * rcp_efb_height;
  uniforms.filter_coefficients = filter_coefficients;
  uniforms.gamma_rcp = 1.0f / gamma;
  //   NOTE: when the clamp bits aren't set, the hardware will happily read beyond the EFB,
  //         which returns random garbage from the empty bus (confirmed by hardware tests).
  //
  //         In our implementation, the garbage just so happens to be the top or bottom row.
  //         Statistically, that could happen.
  const u32 top_coord = clamp_top ? framebuffer_rect.top : 0;
  uniforms.clamp_top = (static_cast<float>(top_coord) + .5f) * rcp_efb_height;
  uniforms.clamp_top -= uniforms.src_top;
  const u32 bottom_coord = (clamp_bottom ? framebuffer_rect.bottom : efb_height) - 1;
  uniforms.clamp_bottom = (uniforms.src_top + uniforms.src_height);
  uniforms.clamp_bottom -= (static_cast<float>(bottom_coord) + .5f) * rcp_efb_height;
  uniforms.pixel_height = g_ActiveConfig.bCopyEFBScaled ? rcp_efb_height : 1.0f / EFB_HEIGHT;
  uniforms.rcp_pixel_width = rcp_efb_width;
  uniforms.rcp_pixel_height = rcp_efb_height;
  uniforms.efb_scale = static_cast<float>(g_framebuffer_manager->GetEFBScale());
  uniforms.efb_copy_scaled = g_ActiveConfig.bCopyEFBScaled ? 1u : 0u;
  uniforms.linear_filter = linear_filter ? 1u : 0u;
  uniforms.padding = 0;
  g_vertex_manager->UploadUtilityUniforms(&uniforms, sizeof(uniforms));

  // Use the copy pipeline to render the VRAM copy.
  g_gfx->SetAndDiscardFramebuffer(entry->framebuffer.get());
  g_gfx->SetViewportAndScissor(entry->framebuffer->GetRect());
  g_gfx->SetPipeline(copy_pipeline);
  g_gfx->SetTexture(0, src_texture);
  g_gfx->SetSamplerState(0, linear_filter ? RenderState::GetLinearSamplerState() :
                                            RenderState::GetPointSamplerState());
  g_gfx->Draw(0, 3);
  g_gfx->EndUtilityDrawing();
  entry->texture->FinishedRendering();
}

void TextureCacheBase::CopyEFB(AbstractStagingTexture* dst, const EFBCopyParams& params,
                               u32 native_width, u32 bytes_per_row, u32 num_blocks_y,
                               u32 memory_stride, const MathUtil::Rectangle<int>& src_rect,
                               bool scale_by_half, bool linear_filter, float y_scale, float gamma,
                               bool clamp_top, bool clamp_bottom,
                               const std::array<u32, 3>& filter_coefficients)
{
  // Flush EFB pokes first, as they're expected to be included.
  g_framebuffer_manager->FlushEFBPokes();

  // Get the pipeline which we will be using. If the compilation failed, this will be null.
  const AbstractPipeline* copy_pipeline = g_shader_cache->GetEFBCopyToRAMPipeline(params);
  if (!copy_pipeline)
  {
    WARN_LOG_FMT(VIDEO, "Skipping EFB copy to VRAM due to missing pipeline.");
    return;
  }

  const auto scaled_src_rect = g_framebuffer_manager->ConvertEFBRectangle(src_rect);
  const auto framebuffer_rect = g_gfx->ConvertFramebufferRectangle(
      scaled_src_rect, g_framebuffer_manager->GetEFBFramebuffer());
  AbstractTexture* src_texture =
      params.depth ? g_framebuffer_manager->ResolveEFBDepthTexture(framebuffer_rect) :
                     g_framebuffer_manager->ResolveEFBColorTexture(framebuffer_rect);

  src_texture->FinishedRendering();
  g_gfx->BeginUtilityDrawing();

  // Fill uniform buffer.
  struct Uniforms
  {
    float src_left, src_top, src_width, src_height;
    std::array<s32, 4> position_uniform;
    float y_scale;
    float gamma_rcp;
    float clamp_top;
    float clamp_bottom;
    std::array<u32, 3> filter_coefficients;
    u32 padding;
  };
  Uniforms encoder_params;
  const float rcp_efb_width = 1.0f / static_cast<float>(g_framebuffer_manager->GetEFBWidth());
  const u32 efb_height = g_framebuffer_manager->GetEFBHeight();
  const float rcp_efb_height = 1.0f / static_cast<float>(efb_height);
  encoder_params.src_left = framebuffer_rect.left * rcp_efb_width;
  encoder_params.src_top = framebuffer_rect.top * rcp_efb_height;
  encoder_params.src_width = framebuffer_rect.GetWidth() * rcp_efb_width;
  encoder_params.src_height = framebuffer_rect.GetHeight() * rcp_efb_height;
  encoder_params.position_uniform[0] = src_rect.left;
  encoder_params.position_uniform[1] = src_rect.top;
  encoder_params.position_uniform[2] = static_cast<s32>(native_width);
  encoder_params.position_uniform[3] = scale_by_half ? 2 : 1;
  encoder_params.y_scale = y_scale;
  encoder_params.gamma_rcp = 1.0f / gamma;
  //   NOTE: when the clamp bits aren't set, the hardware will happily read beyond the EFB,
  //         which returns random garbage from the empty bus (confirmed by hardware tests).
  //
  //         In our implementation, the garbage just so happens to be the top or bottom row.
  //         Statistically, that could happen.
  const u32 top_coord = clamp_top ? framebuffer_rect.top : 0;
  encoder_params.clamp_top = (static_cast<float>(top_coord) + .5f) * rcp_efb_height;
  encoder_params.clamp_top -= encoder_params.src_top;
  const u32 bottom_coord = (clamp_bottom ? framebuffer_rect.bottom : efb_height) - 1;
  encoder_params.clamp_bottom = (encoder_params.src_top + encoder_params.src_height);
  encoder_params.clamp_bottom -= (static_cast<float>(bottom_coord) + .5f) * rcp_efb_height;
  encoder_params.filter_coefficients = filter_coefficients;
  g_vertex_manager->UploadUtilityUniforms(&encoder_params, sizeof(encoder_params));

  // Because the shader uses gl_FragCoord and we read it back, we must render to the lower-left.
  const u32 render_width = bytes_per_row / sizeof(u32);
  const u32 render_height = num_blocks_y;
  const auto encode_rect = MathUtil::Rectangle<int>(0, 0, render_width, render_height);

  // Render to GPU texture, and then copy to CPU-accessible texture.
  g_gfx->SetAndDiscardFramebuffer(m_efb_encoding_framebuffer.get());
  g_gfx->SetViewportAndScissor(encode_rect);
  g_gfx->SetPipeline(copy_pipeline);
  g_gfx->SetTexture(0, src_texture);
  g_gfx->SetSamplerState(0, linear_filter ? RenderState::GetLinearSamplerState() :
                                            RenderState::GetPointSamplerState());
  g_gfx->Draw(0, 3);
  dst->CopyFromTexture(m_efb_encoding_texture.get(), encode_rect, 0, 0, encode_rect);
  g_gfx->EndUtilityDrawing();

  // Flush if there's sufficient draws between this copy and the last.
  g_vertex_manager->OnEFBCopyToRAM();
}

bool TextureCacheBase::DecodeTextureOnGPU(RcTcacheEntry& entry, u32 dst_level, const u8* data,
                                          u32 data_size, TextureFormat format, u32 width,
                                          u32 height, u32 aligned_width, u32 aligned_height,
                                          u32 row_stride, const u8* palette,
                                          TLUTFormat palette_format)
{
  const auto* info = TextureConversionShaderTiled::GetDecodingShaderInfo(format);
  if (!info)
    return false;

  const AbstractShader* shader = g_shader_cache->GetTextureDecodingShader(
      format, info->palette_size != 0 ? std::make_optional(palette_format) : std::nullopt);
  if (!shader)
    return false;

  // Copy to GPU-visible buffer, aligned to the data type.
  const u32 bytes_per_buffer_elem =
      VertexManagerBase::GetTexelBufferElementSize(info->buffer_format);

  // Allocate space in stream buffer, and copy texture + palette across.
  u32 src_offset = 0, palette_offset = 0;
  if (info->palette_size > 0)
  {
    if (!g_vertex_manager->UploadTexelBuffer(data, data_size, info->buffer_format, &src_offset,
                                             palette, info->palette_size,
                                             TEXEL_BUFFER_FORMAT_R16_UINT, &palette_offset))
    {
      return false;
    }
  }
  else
  {
    if (!g_vertex_manager->UploadTexelBuffer(data, data_size, info->buffer_format, &src_offset))
      return false;
  }

  // Set up uniforms.
  struct Uniforms
  {
    u32 dst_width, dst_height;
    u32 src_width, src_height;
    u32 src_offset, src_row_stride;
    u32 palette_offset, unused;
  } uniforms = {width,          height,     aligned_width,
                aligned_height, src_offset, row_stride / bytes_per_buffer_elem,
                palette_offset};
  g_vertex_manager->UploadUtilityUniforms(&uniforms, sizeof(uniforms));
  g_gfx->SetComputeImageTexture(0, m_decoding_texture.get(), false, true);

  auto dispatch_groups =
      TextureConversionShaderTiled::GetDispatchCount(info, aligned_width, aligned_height);
  g_gfx->DispatchComputeShader(shader, info->group_size_x, info->group_size_y, 1,
                               dispatch_groups.first, dispatch_groups.second, 1);

  // Copy from decoding texture -> final texture
  // This is because we don't want to have to create compute view for every layer
  const auto copy_rect = entry->texture->GetConfig().GetMipRect(dst_level);
  entry->texture->CopyRectangleFromTexture(m_decoding_texture.get(), copy_rect, 0, 0, copy_rect, 0,
                                           dst_level);
  entry->texture->FinishedRendering();
  return true;
}

u32 TCacheEntry::BytesPerRow() const
{
  // RGBA takes two cache lines per block; all others take one
  const u32 bytes_per_block = format == TextureFormat::RGBA8 ? 64 : 32;

  return NumBlocksX() * bytes_per_block;
}

u32 TCacheEntry::NumBlocksX() const
{
  const u32 blockW = TexDecoder_GetBlockWidthInTexels(format.texfmt);

  // Round up source height to multiple of block size
  const u32 actualWidth = Common::AlignUp(native_width, blockW);

  return actualWidth / blockW;
}

u32 TCacheEntry::NumBlocksY() const
{
  u32 blockH = TexDecoder_GetBlockHeightInTexels(format.texfmt);
  // Round up source height to multiple of block size
  u32 actualHeight = Common::AlignUp(native_height, blockH);

  return actualHeight / blockH;
}

void TCacheEntry::SetXfbCopy(u32 stride)
{
  is_efb_copy = false;
  is_xfb_copy = true;
  is_xfb_container = false;
  memory_stride = stride;

  ASSERT_MSG(VIDEO, memory_stride >= BytesPerRow(), "Memory stride is too small");

  size_in_bytes = memory_stride * NumBlocksY();
}

void TCacheEntry::SetEfbCopy(u32 stride)
{
  is_efb_copy = true;
  is_xfb_copy = false;
  is_xfb_container = false;
  memory_stride = stride;

  ASSERT_MSG(VIDEO, memory_stride >= BytesPerRow(), "Memory stride is too small");

  size_in_bytes = memory_stride * NumBlocksY();
}

void TCacheEntry::SetNotCopy()
{
  is_efb_copy = false;
  is_xfb_copy = false;
  is_xfb_container = false;
}

int TCacheEntry::HashSampleSize() const
{
  if (should_force_safe_hashing)
  {
    return 0;
  }

  return g_ActiveConfig.iSafeTextureCache_ColorSamples;
}

u64 TCacheEntry::CalculateHash() const
{
  const u32 bytes_per_row = BytesPerRow();
  const u32 hash_sample_size = HashSampleSize();

  // FIXME: textures from tmem won't get the correct hash.
  auto& system = Core::System::GetInstance();
  auto& memory = system.GetMemory();
  u8* ptr = memory.GetPointerForRange(addr, size_in_bytes);
  if (memory_stride == bytes_per_row)
  {
    return Common::GetHash64(ptr, size_in_bytes, hash_sample_size);
  }
  else
  {
    const u32 num_blocks_y = NumBlocksY();
    u64 temp_hash = size_in_bytes;

    u32 samples_per_row = 0;
    if (hash_sample_size != 0)
    {
      // Hash at least 4 samples per row to avoid hashing in a bad pattern, like just on the left
      // side of the efb copy
      samples_per_row = std::max(hash_sample_size / num_blocks_y, 4u);
    }

    for (u32 i = 0; i < num_blocks_y; i++)
    {
      // Multiply by a prime number to mix the hash up a bit. This prevents identical blocks from
      // canceling each other out
      temp_hash = (temp_hash * 397) ^ Common::GetHash64(ptr, bytes_per_row, samples_per_row);
      ptr += memory_stride;
    }
    return temp_hash;
  }
}

TextureCacheBase::TexPoolEntry::TexPoolEntry(std::unique_ptr<AbstractTexture> tex,
                                             std::unique_ptr<AbstractFramebuffer> fb)
    : texture(std::move(tex)), framebuffer(std::move(fb))
{
}
