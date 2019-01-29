// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#if defined(_M_X86) || defined(_M_X86_64)
#include <pmmintrin.h>
#endif

#include "Common/Align.h"
#include "Common/Assert.h"
#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/Hash.h"
#include "Common/Logging/Log.h"
#include "Common/MathUtil.h"
#include "Common/MemoryUtil.h"
#include "Common/StringUtil.h"

#include "Core/ConfigManager.h"
#include "Core/FifoPlayer/FifoPlayer.h"
#include "Core/FifoPlayer/FifoRecorder.h"
#include "Core/HW/Memmap.h"

#include "VideoCommon/AbstractStagingTexture.h"
#include "VideoCommon/BPMemory.h"
#include "VideoCommon/Debugger.h"
#include "VideoCommon/FramebufferManagerBase.h"
#include "VideoCommon/HiresTextures.h"
#include "VideoCommon/PixelShaderManager.h"
#include "VideoCommon/RenderBase.h"
#include "VideoCommon/SamplerCommon.h"
#include "VideoCommon/Statistics.h"
#include "VideoCommon/TextureCacheBase.h"
#include "VideoCommon/TextureDecoder.h"
#include "VideoCommon/VideoCommon.h"
#include "VideoCommon/VideoConfig.h"

static const u64 TEXHASH_INVALID = 0;
// Sonic the Fighters (inside Sonic Gems Collection) loops a 64 frames animation
static const int TEXTURE_KILL_THRESHOLD = 64;
static const int TEXTURE_POOL_KILL_THRESHOLD = 3;

std::unique_ptr<TextureCacheBase> g_texture_cache;

std::bitset<8> TextureCacheBase::valid_bind_points;

TextureCacheBase::TCacheEntry::TCacheEntry(std::unique_ptr<AbstractTexture> tex)
    : texture(std::move(tex))
{
}

TextureCacheBase::TCacheEntry::~TCacheEntry()
{
  for (auto& reference : references)
    reference->references.erase(this);
}

void TextureCacheBase::CheckTempSize(size_t required_size)
{
  if (required_size <= temp_size)
    return;

  temp_size = required_size;
  Common::FreeAlignedMemory(temp);
  temp = static_cast<u8*>(Common::AllocateAlignedMemory(temp_size, 16));
}

TextureCacheBase::TextureCacheBase()
{
  SetBackupConfig(g_ActiveConfig);

  temp_size = 2048 * 2048 * 4;
  temp = static_cast<u8*>(Common::AllocateAlignedMemory(temp_size, 16));

  TexDecoder_SetTexFmtOverlayOptions(backup_config.texfmt_overlay,
                                     backup_config.texfmt_overlay_center);

  HiresTexture::Init();

  Common::SetHash64Function();

  InvalidateAllBindPoints();
}

void TextureCacheBase::Invalidate()
{
  FlushEFBCopies();
  InvalidateAllBindPoints();
  for (size_t i = 0; i < bound_textures.size(); ++i)
  {
    bound_textures[i] = nullptr;
  }

  for (auto& tex : textures_by_address)
  {
    delete tex.second;
  }
  textures_by_address.clear();
  textures_by_hash.clear();

  texture_pool.clear();
}

TextureCacheBase::~TextureCacheBase()
{
  HiresTexture::Shutdown();
  Invalidate();
  Common::FreeAlignedMemory(temp);
  temp = nullptr;
}

void TextureCacheBase::OnConfigChanged(VideoConfig& config)
{
  if (config.bHiresTextures != backup_config.hires_textures ||
      config.bCacheHiresTextures != backup_config.cache_hires_textures)
  {
    HiresTexture::Update();
  }

  // TODO: Invalidating texcache is really stupid in some of these cases
  if (config.iSafeTextureCache_ColorSamples != backup_config.color_samples ||
      config.bTexFmtOverlayEnable != backup_config.texfmt_overlay ||
      config.bTexFmtOverlayCenter != backup_config.texfmt_overlay_center ||
      config.bHiresTextures != backup_config.hires_textures ||
      config.bEnableGPUTextureDecoding != backup_config.gpu_texture_decoding ||
      config.bDisableCopyToVRAM != backup_config.disable_vram_copies ||
      config.bArbitraryMipmapDetection != backup_config.arbitrary_mipmap_detection)
  {
    Invalidate();

    TexDecoder_SetTexFmtOverlayOptions(g_ActiveConfig.bTexFmtOverlayEnable,
                                       g_ActiveConfig.bTexFmtOverlayCenter);
  }

  if ((config.stereo_mode != StereoMode::Off) != backup_config.stereo_3d ||
      config.bStereoEFBMonoDepth != backup_config.efb_mono_depth)
  {
    g_texture_cache->DeleteShaders();
    if (!g_texture_cache->CompileShaders())
      PanicAlert("Failed to recompile one or more texture conversion shaders.");
  }

  SetBackupConfig(config);
}

void TextureCacheBase::Cleanup(int _frameCount)
{
  TexAddrCache::iterator iter = textures_by_address.begin();
  TexAddrCache::iterator tcend = textures_by_address.end();
  while (iter != tcend)
  {
    if (iter->second->tmem_only)
    {
      iter = InvalidateTexture(iter);
    }
    else if (iter->second->frameCount == FRAMECOUNT_INVALID)
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

  TexPool::iterator iter2 = texture_pool.begin();
  TexPool::iterator tcend2 = texture_pool.end();
  while (iter2 != tcend2)
  {
    if (iter2->second.frameCount == FRAMECOUNT_INVALID)
    {
      iter2->second.frameCount = _frameCount;
    }
    if (_frameCount > TEXTURE_POOL_KILL_THRESHOLD + iter2->second.frameCount)
    {
      iter2 = texture_pool.erase(iter2);
    }
    else
    {
      ++iter2;
    }
  }
}

bool TextureCacheBase::TCacheEntry::OverlapsMemoryRange(u32 range_address, u32 range_size) const
{
  if (addr + size_in_bytes <= range_address)
    return false;

  if (addr >= range_address + range_size)
    return false;

  return true;
}

void TextureCacheBase::SetBackupConfig(const VideoConfig& config)
{
  backup_config.color_samples = config.iSafeTextureCache_ColorSamples;
  backup_config.texfmt_overlay = config.bTexFmtOverlayEnable;
  backup_config.texfmt_overlay_center = config.bTexFmtOverlayCenter;
  backup_config.hires_textures = config.bHiresTextures;
  backup_config.cache_hires_textures = config.bCacheHiresTextures;
  backup_config.stereo_3d = config.stereo_mode != StereoMode::Off;
  backup_config.efb_mono_depth = config.bStereoEFBMonoDepth;
  backup_config.gpu_texture_decoding = config.bEnableGPUTextureDecoding;
  backup_config.disable_vram_copies = config.bDisableCopyToVRAM;
  backup_config.arbitrary_mipmap_detection = config.bArbitraryMipmapDetection;
}

TextureCacheBase::TCacheEntry*
TextureCacheBase::ApplyPaletteToEntry(TCacheEntry* entry, u8* palette, TLUTFormat tlutfmt)
{
  TextureConfig new_config = entry->texture->GetConfig();
  new_config.levels = 1;
  new_config.rendertarget = true;

  TCacheEntry* decoded_entry = AllocateCacheEntry(new_config);
  if (!decoded_entry)
    return nullptr;

  decoded_entry->SetGeneralParameters(entry->addr, entry->size_in_bytes, entry->format,
                                      entry->should_force_safe_hashing);
  decoded_entry->SetDimensions(entry->native_width, entry->native_height, 1);
  decoded_entry->SetHashes(entry->base_hash, entry->hash);
  decoded_entry->frameCount = FRAMECOUNT_INVALID;
  decoded_entry->should_force_safe_hashing = false;
  decoded_entry->SetNotCopy();
  decoded_entry->may_have_overlapping_textures = entry->may_have_overlapping_textures;

  ConvertTexture(decoded_entry, entry, palette, tlutfmt);
  textures_by_address.emplace(entry->addr, decoded_entry);

  return decoded_entry;
}

void TextureCacheBase::ScaleTextureCacheEntryTo(TextureCacheBase::TCacheEntry* entry, u32 new_width,
                                                u32 new_height)
{
  if (entry->GetWidth() == new_width && entry->GetHeight() == new_height)
  {
    return;
  }

  const u32 max = g_ActiveConfig.backend_info.MaxTextureSize;
  if (max < new_width || max < new_height)
  {
    ERROR_LOG(VIDEO, "Texture too big, width = %d, height = %d", new_width, new_height);
    return;
  }

  TextureConfig newconfig;
  newconfig.width = new_width;
  newconfig.height = new_height;
  newconfig.layers = entry->GetNumLayers();
  newconfig.rendertarget = true;

  std::unique_ptr<AbstractTexture> new_texture = AllocateTexture(newconfig);
  if (new_texture)
  {
    new_texture->ScaleRectangleFromTexture(entry->texture.get(),
                                           entry->texture->GetConfig().GetRect(),
                                           new_texture->GetConfig().GetRect());
    entry->texture.swap(new_texture);

    auto config = new_texture->GetConfig();
    // At this point new_texture has the old texture in it,
    // we can potentially reuse this, so let's move it back to the pool
    texture_pool.emplace(config, TexPoolEntry(std::move(new_texture)));
  }
  else
  {
    ERROR_LOG(VIDEO, "Scaling failed");
  }
}

TextureCacheBase::TCacheEntry*
TextureCacheBase::DoPartialTextureUpdates(TCacheEntry* entry_to_update, u8* palette,
                                          TLUTFormat tlutfmt)
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

  u32 block_width = TexDecoder_GetBlockWidthInTexels(entry_to_update->format.texfmt);
  u32 block_height = TexDecoder_GetBlockHeightInTexels(entry_to_update->format.texfmt);
  u32 block_size = block_width * block_height *
                   TexDecoder_GetTexelSizeInNibbles(entry_to_update->format.texfmt) / 2;

  u32 numBlocksX = (entry_to_update->native_width + block_width - 1) / block_width;

  auto iter = FindOverlappingTextures(entry_to_update->addr, entry_to_update->size_in_bytes);
  while (iter.first != iter.second)
  {
    TCacheEntry* entry = iter.first->second;
    if (entry != entry_to_update && entry->IsCopy() && !entry->tmem_only &&
        entry->references.count(entry_to_update) == 0 &&
        entry->OverlapsMemoryRange(entry_to_update->addr, entry_to_update->size_in_bytes) &&
        entry->memory_stride == numBlocksX * block_size)
    {
      if (entry->hash == entry->CalculateHash())
      {
        if (isPaletteTexture)
        {
          TCacheEntry* decoded_entry = ApplyPaletteToEntry(entry, palette, tlutfmt);
          if (decoded_entry)
          {
            // Link the efb copy with the partially updated texture, so we won't apply this partial
            // update again
            entry->CreateReference(entry_to_update);
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
          ScaleTextureCacheEntryTo(entry_to_update,
                                   g_renderer->EFBToScaledX(entry_to_update->native_width),
                                   g_renderer->EFBToScaledY(entry_to_update->native_height));
          ScaleTextureCacheEntryTo(entry, g_renderer->EFBToScaledX(entry->native_width),
                                   g_renderer->EFBToScaledY(entry->native_height));

          src_x = g_renderer->EFBToScaledX(src_x);
          src_y = g_renderer->EFBToScaledY(src_y);
          dst_x = g_renderer->EFBToScaledX(dst_x);
          dst_y = g_renderer->EFBToScaledY(dst_y);
          copy_width = g_renderer->EFBToScaledX(copy_width);
          copy_height = g_renderer->EFBToScaledY(copy_height);
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
          entry->CreateReference(entry_to_update);
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

void TextureCacheBase::DumpTexture(TCacheEntry* entry, std::string basename, unsigned int level,
                                   bool is_arbitrary)
{
  std::string szDir = File::GetUserPath(D_DUMPTEXTURES_IDX) + SConfig::GetInstance().GetGameID();

  // make sure that the directory exists
  if (!File::IsDirectory(szDir))
    File::CreateDir(szDir);

  if (is_arbitrary)
  {
    basename += "_arb";
  }

  if (level > 0)
  {
    basename += StringFromFormat("_mip%i", level);
  }

  std::string filename = szDir + "/" + basename + ".png";

  if (!File::Exists(filename))
    entry->texture->Save(filename, level);
}

static u32 CalculateLevelSize(u32 level_0_size, u32 level)
{
  return std::max(level_0_size >> level, 1u);
}

static void SetSamplerState(u32 index, float custom_tex_scale, bool custom_tex,
                            bool has_arbitrary_mips)
{
  const FourTexUnits& tex = bpmem.tex[index / 4];
  const TexMode0& tm0 = tex.texMode0[index % 4];

  SamplerState state = {};
  state.Generate(bpmem, index);

  // Force texture filtering config option.
  if (g_ActiveConfig.bForceFiltering)
  {
    state.min_filter = SamplerState::Filter::Linear;
    state.mag_filter = SamplerState::Filter::Linear;
    state.mipmap_filter = SamplerCommon::AreBpTexMode0MipmapsEnabled(tm0) ?
                              SamplerState::Filter::Linear :
                              SamplerState::Filter::Point;
  }

  // Custom textures may have a greater number of mips
  if (custom_tex)
    state.max_lod = 255;

  // Anisotropic filtering option.
  if (g_ActiveConfig.iMaxAnisotropy != 0 && !SamplerCommon::IsBpTexMode0PointFiltering(tm0))
  {
    // https://www.opengl.org/registry/specs/EXT/texture_filter_anisotropic.txt
    // For predictable results on all hardware/drivers, only use one of:
    //	GL_LINEAR + GL_LINEAR (No Mipmaps [Bilinear])
    //	GL_LINEAR + GL_LINEAR_MIPMAP_LINEAR (w/ Mipmaps [Trilinear])
    // Letting the game set other combinations will have varying arbitrary results;
    // possibly being interpreted as equal to bilinear/trilinear, implicitly
    // disabling anisotropy, or changing the anisotropic algorithm employed.
    state.min_filter = SamplerState::Filter::Linear;
    state.mag_filter = SamplerState::Filter::Linear;
    if (SamplerCommon::AreBpTexMode0MipmapsEnabled(tm0))
      state.mipmap_filter = SamplerState::Filter::Linear;
    state.anisotropic_filtering = 1;
  }
  else
  {
    state.anisotropic_filtering = 0;
  }

  if (has_arbitrary_mips && SamplerCommon::AreBpTexMode0MipmapsEnabled(tm0))
  {
    // Apply a secondary bias calculated from the IR scale to pull inwards mipmaps
    // that have arbitrary contents, eg. are used for fog effects where the
    // distance they kick in at is important to preserve at any resolution.
    // Correct this with the upscaling factor of custom textures.
    s64 lod_offset = std::log2(g_renderer->GetEFBScale() / custom_tex_scale) * 256.f;
    state.lod_bias = MathUtil::Clamp<s64>(state.lod_bias + lod_offset, -32768, 32767);

    // Anisotropic also pushes mips farther away so it cannot be used either
    state.anisotropic_filtering = 0;
  }

  g_renderer->SetSamplerState(index, state);
}

void TextureCacheBase::BindTextures()
{
  for (u32 i = 0; i < bound_textures.size(); i++)
  {
    const TCacheEntry* tentry = bound_textures[i];
    if (IsValidBindPoint(i) && tentry)
    {
      g_renderer->SetTexture(i, tentry->texture.get());
      PixelShaderManager::SetTexDims(i, tentry->native_width, tentry->native_height);

      const float custom_tex_scale = tentry->GetWidth() / float(tentry->native_width);
      SetSamplerState(i, custom_tex_scale, tentry->is_custom_tex, tentry->has_arbitrary_mips);
    }
  }
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

TextureCacheBase::TCacheEntry* TextureCacheBase::Load(const u32 stage)
{
  // if this stage was not invalidated by changes to texture registers, keep the current texture
  if (IsValidBindPoint(stage) && bound_textures[stage])
  {
    return bound_textures[stage];
  }

  const FourTexUnits& tex = bpmem.tex[stage >> 2];
  const u32 id = stage & 3;
  const u32 address = (tex.texImage3[id].image_base /* & 0x1FFFFF*/) << 5;
  u32 width = tex.texImage0[id].width + 1;
  u32 height = tex.texImage0[id].height + 1;
  const TextureFormat texformat = static_cast<TextureFormat>(tex.texImage0[id].format);
  const u32 tlutaddr = tex.texTlut[id].tmem_offset << 9;
  const TLUTFormat tlutfmt = static_cast<TLUTFormat>(tex.texTlut[id].tlut_format);
  const bool use_mipmaps = SamplerCommon::AreBpTexMode0MipmapsEnabled(tex.texMode0[id]);
  u32 tex_levels = use_mipmaps ? ((tex.texMode1[id].max_lod + 0xf) / 0x10 + 1) : 1;
  const bool from_tmem = tex.texImage1[id].image_type != 0;
  const u32 tmem_address_even = from_tmem ? tex.texImage1[id].tmem_even * TMEM_LINE_SIZE : 0;
  const u32 tmem_address_odd = from_tmem ? tex.texImage2[id].tmem_odd * TMEM_LINE_SIZE : 0;

  auto entry = GetTexture(address, width, height, texformat,
                          g_ActiveConfig.iSafeTextureCache_ColorSamples, tlutaddr, tlutfmt,
                          use_mipmaps, tex_levels, from_tmem, tmem_address_even, tmem_address_odd);

  if (!entry)
    return nullptr;

  entry->frameCount = FRAMECOUNT_INVALID;
  bound_textures[stage] = entry;

  GFX_DEBUGGER_PAUSE_AT(NEXT_TEXTURE_CHANGE, true);

  // We need to keep track of invalided textures until they have actually been replaced or
  // re-loaded
  valid_bind_points.set(stage);

  return entry;
}

TextureCacheBase::TCacheEntry*
TextureCacheBase::GetTexture(u32 address, u32 width, u32 height, const TextureFormat texformat,
                             const int textureCacheSafetyColorSampleSize, u32 tlutaddr,
                             TLUTFormat tlutfmt, bool use_mipmaps, u32 tex_levels, bool from_tmem,
                             u32 tmem_address_even, u32 tmem_address_odd)
{
  // TexelSizeInNibbles(format) * width * height / 16;
  const unsigned int bsw = TexDecoder_GetBlockWidthInTexels(texformat);
  const unsigned int bsh = TexDecoder_GetBlockHeightInTexels(texformat);

  unsigned int expandedWidth = Common::AlignUp(width, bsw);
  unsigned int expandedHeight = Common::AlignUp(height, bsh);
  const unsigned int nativeW = width;
  const unsigned int nativeH = height;

  // Hash assigned to texcache entry (also used to generate filenames used for texture dumping and
  // custom texture lookup)
  u64 base_hash = TEXHASH_INVALID;
  u64 full_hash = TEXHASH_INVALID;

  TextureAndTLUTFormat full_format(texformat, tlutfmt);

  const bool isPaletteTexture = IsColorIndexed(texformat);

  // Reject invalid tlut format.
  if (isPaletteTexture && !IsValidTLUTFormat(tlutfmt))
    return nullptr;

  const u32 texture_size =
      TexDecoder_GetTextureSizeInBytes(expandedWidth, expandedHeight, texformat);
  u32 bytes_per_block = (bsw * bsh * TexDecoder_GetTexelSizeInNibbles(texformat)) / 2;
  u32 additional_mips_size = 0;  // not including level 0, which is texture_size

  // GPUs don't like when the specified mipmap count would require more than one 1x1-sized LOD in
  // the mipmap chain
  // e.g. 64x64 with 7 LODs would have the mipmap chain 64x64,32x32,16x16,8x8,4x4,2x2,1x1,0x0, so we
  // limit the mipmap count to 6 there
  tex_levels = std::min<u32>(IntLog2(std::max(width, height)) + 1, tex_levels);

  for (u32 level = 1; level != tex_levels; ++level)
  {
    // We still need to calculate the original size of the mips
    const u32 expanded_mip_width = Common::AlignUp(CalculateLevelSize(width, level), bsw);
    const u32 expanded_mip_height = Common::AlignUp(CalculateLevelSize(height, level), bsh);

    additional_mips_size +=
        TexDecoder_GetTextureSizeInBytes(expanded_mip_width, expanded_mip_height, texformat);
  }

  // TODO: the texture cache lookup is based on address, but a texture from tmem has no reason
  //       to have a unique and valid address. This could result in a regular texture and a tmem
  //       texture aliasing onto the same texture cache entry.
  const u8* src_data;
  if (from_tmem)
    src_data = &texMem[tmem_address_even];
  else
    src_data = Memory::GetPointer(address);

  if (!src_data)
  {
    ERROR_LOG(VIDEO, "Trying to use an invalid texture address 0x%8x", address);
    return nullptr;
  }

  // If we are recording a FifoLog, keep track of what memory we read. FifoRecorder does
  // its own memory modification tracking independent of the texture hashing below.
  if (g_bRecordFifoData && !from_tmem)
    FifoRecorder::GetInstance().UseMemory(address, texture_size + additional_mips_size,
                                          MemoryUpdate::TEXTURE_MAP);

  // TODO: This doesn't hash GB tiles for preloaded RGBA8 textures (instead, it's hashing more data
  // from the low tmem bank than it should)
  base_hash = Common::GetHash64(src_data, texture_size, textureCacheSafetyColorSampleSize);
  u32 palette_size = 0;
  if (isPaletteTexture)
  {
    palette_size = TexDecoder_GetPaletteSize(texformat);
    full_hash = base_hash ^ Common::GetHash64(&texMem[tlutaddr], palette_size,
                                              textureCacheSafetyColorSampleSize);
  }
  else
  {
    full_hash = base_hash;
  }

  // Search the texture cache for textures by address
  //
  // Find all texture cache entries for the current texture address, and decide whether to use one
  // of
  // them, or to create a new one
  //
  // In most cases, the fastest way is to use only one texture cache entry for the same address.
  // Usually,
  // when a texture changes, the old version of the texture is unlikely to be used again. If there
  // were
  // new cache entries created for normal texture updates, there would be a slowdown due to a huge
  // amount
  // of unused cache entries. Also thanks to texture pooling, overwriting an existing cache entry is
  // faster than creating a new one from scratch.
  //
  // Some games use the same address for different textures though. If the same cache entry was used
  // in
  // this case, it would be constantly overwritten, and effectively there wouldn't be any caching
  // for
  // those textures. Examples for this are Metroid Prime and Castlevania 3. Metroid Prime has
  // multiple
  // sets of fonts on each other stored in a single texture and uses the palette to make different
  // characters visible or invisible. In Castlevania 3 some textures are used for 2 different things
  // or
  // at least in 2 different ways(size 1024x1024 vs 1024x256).
  //
  // To determine whether to use multiple cache entries or a single entry, use the following
  // heuristic:
  // If the same texture address is used several times during the same frame, assume the address is
  // used
  // for different purposes and allow creating an additional cache entry. If there's at least one
  // entry
  // that hasn't been used for the same frame, then overwrite it, in order to keep the cache as
  // small as
  // possible. If the current texture is found in the cache, use that entry.
  //
  // For efb copies, the entry created in CopyRenderTargetToTexture always has to be used, or else
  // it was
  // done in vain.
  auto iter_range = textures_by_address.equal_range(address);
  TexAddrCache::iterator iter = iter_range.first;
  TexAddrCache::iterator oldest_entry = iter;
  int temp_frameCount = 0x7fffffff;
  TexAddrCache::iterator unconverted_copy = textures_by_address.end();

  while (iter != iter_range.second)
  {
    TCacheEntry* entry = iter->second;

    // Skip entries that are only left in our texture cache for the tmem cache emulation
    if (entry->tmem_only)
    {
      ++iter;
      continue;
    }

    // TODO: Some games (Rogue Squadron 3, Twin Snakes) seem to load a previously made XFB
    // copy as a regular texture. You can see this particularly well in RS3 whenever the
    // game freezes the image and fades it out to black on screen transitions, which fades
    // out a purple screen in XFB2Tex. Check for this here and convert them if necessary.

    // Do not load strided EFB copies, they are not meant to be used directly.
    // Also do not directly load EFB copies, which were partly overwritten.
    if (entry->IsEfbCopy() && entry->native_width == nativeW && entry->native_height == nativeH &&
        entry->memory_stride == entry->BytesPerRow() && !entry->may_have_overlapping_textures)
    {
      // EFB copies have slightly different rules as EFB copy formats have different
      // meanings from texture formats.
      if ((base_hash == entry->hash &&
           (!isPaletteTexture || g_Config.backend_info.bSupportsPaletteConversion)) ||
          IsPlayingBackFifologWithBrokenEFBCopies)
      {
        // TODO: We should check format/width/height/levels for EFB copies. Checking
        // format is complicated because EFB copy formats don't exactly match
        // texture formats. I'm not sure what effect checking width/height/levels
        // would have.
        if (!isPaletteTexture || !g_Config.backend_info.bSupportsPaletteConversion)
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
          entry->native_levels >= tex_levels && entry->native_width == nativeW &&
          entry->native_height == nativeH)
      {
        entry = DoPartialTextureUpdates(iter->second, &texMem[tlutaddr], tlutfmt);

        return entry;
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
        !entry->IsCopy() && !(isPaletteTexture && entry->base_hash == base_hash))
    {
      temp_frameCount = entry->frameCount;
      oldest_entry = iter;
    }
    ++iter;
  }

  if (unconverted_copy != textures_by_address.end())
  {
    TCacheEntry* decoded_entry =
        ApplyPaletteToEntry(unconverted_copy->second, &texMem[tlutaddr], tlutfmt);

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
      std::max(texture_size, palette_size) <= (u32)textureCacheSafetyColorSampleSize * 8)
  {
    auto hash_range = textures_by_hash.equal_range(full_hash);
    TexHashCache::iterator hash_iter = hash_range.first;
    while (hash_iter != hash_range.second)
    {
      TCacheEntry* entry = hash_iter->second;
      // All parameters, except the address, need to match here
      if (entry->format == full_format && entry->native_levels >= tex_levels &&
          entry->native_width == nativeW && entry->native_height == nativeH)
      {
        entry = DoPartialTextureUpdates(hash_iter->second, &texMem[tlutaddr], tlutfmt);

        return entry;
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

  std::shared_ptr<HiresTexture> hires_tex;
  if (g_ActiveConfig.bHiresTextures)
  {
    hires_tex = HiresTexture::Search(src_data, texture_size, &texMem[tlutaddr], palette_size, width,
                                     height, texformat, use_mipmaps);

    if (hires_tex)
    {
      const auto& level = hires_tex->m_levels[0];
      if (level.width != width || level.height != height)
      {
        width = level.width;
        height = level.height;
      }
      expandedWidth = level.width;
      expandedHeight = level.height;
    }
  }

  // how many levels the allocated texture shall have
  const u32 texLevels = hires_tex ? (u32)hires_tex->m_levels.size() : tex_levels;

  // We can decode on the GPU if it is a supported format and the flag is enabled.
  // Currently we don't decode RGBA8 textures from Tmem, as that would require copying from both
  // banks, and if we're doing an copy we may as well just do the whole thing on the CPU, since
  // there's no conversion between formats. In the future this could be extended with a separate
  // shader, however.
  bool decode_on_gpu = !hires_tex && g_ActiveConfig.UseGPUTextureDecoding() &&
                       g_texture_cache->SupportsGPUTextureDecode(texformat, tlutfmt) &&
                       !(from_tmem && texformat == TextureFormat::RGBA8);

  // create the entry/texture
  TextureConfig config;
  config.width = width;
  config.height = height;
  config.levels = texLevels;
  config.format = hires_tex ? hires_tex->GetFormat() : AbstractTextureFormat::RGBA8;

  ArbitraryMipmapDetector arbitrary_mip_detector;

  TCacheEntry* entry = AllocateCacheEntry(config);
  GFX_DEBUGGER_PAUSE_AT(NEXT_NEW_TEXTURE, true);

  if (!entry)
    return nullptr;

  const u8* tlut = &texMem[tlutaddr];
  if (hires_tex)
  {
    const auto& level = hires_tex->m_levels[0];
    entry->texture->Load(0, level.width, level.height, level.row_length, level.data.data(),
                         level.data.size());
  }

  // Initialized to null because only software loading uses this buffer
  u8* dst_buffer = nullptr;

  if (!hires_tex)
  {
    if (decode_on_gpu)
    {
      u32 row_stride = bytes_per_block * (expandedWidth / bsw);
      g_texture_cache->DecodeTextureOnGPU(entry, 0, src_data, texture_size, texformat, width,
                                          height, expandedWidth, expandedHeight, row_stride, tlut,
                                          tlutfmt);
    }
    else
    {
      size_t decoded_texture_size = expandedWidth * sizeof(u32) * expandedHeight;

      // Allocate memory for all levels at once
      size_t total_texture_size = decoded_texture_size;

      // For the downsample, we need 2 buffers; 1 is 1/4 of the original texture, the other 1/16
      size_t mip_downsample_buffer_size = decoded_texture_size * 5 / 16;

      size_t prev_level_size = decoded_texture_size;
      for (u32 i = 1; i < tex_levels; ++i)
      {
        prev_level_size /= 4;
        total_texture_size += prev_level_size;
      }

      // Add space for the downsampling at the end
      total_texture_size += mip_downsample_buffer_size;

      CheckTempSize(total_texture_size);
      dst_buffer = temp;
      if (!(texformat == TextureFormat::RGBA8 && from_tmem))
      {
        TexDecoder_Decode(dst_buffer, src_data, expandedWidth, expandedHeight, texformat, tlut,
                          tlutfmt);
      }
      else
      {
        u8* src_data_gb = &texMem[tmem_address_odd];
        TexDecoder_DecodeRGBA8FromTmem(dst_buffer, src_data, src_data_gb, expandedWidth,
                                       expandedHeight);
      }

      entry->texture->Load(0, width, height, expandedWidth, dst_buffer, decoded_texture_size);

      arbitrary_mip_detector.AddLevel(width, height, expandedWidth, dst_buffer);

      dst_buffer += decoded_texture_size;
    }
  }

  iter = textures_by_address.emplace(address, entry);
  if (textureCacheSafetyColorSampleSize == 0 ||
      std::max(texture_size, palette_size) <= (u32)textureCacheSafetyColorSampleSize * 8)
  {
    entry->textures_by_hash_iter = textures_by_hash.emplace(full_hash, entry);
  }

  entry->SetGeneralParameters(address, texture_size, full_format, false);
  entry->SetDimensions(nativeW, nativeH, tex_levels);
  entry->SetHashes(base_hash, full_hash);
  entry->is_custom_tex = hires_tex != nullptr;
  entry->memory_stride = entry->BytesPerRow();
  entry->SetNotCopy();

  std::string basename = "";
  if (g_ActiveConfig.bDumpTextures && !hires_tex)
  {
    basename = HiresTexture::GenBaseName(src_data, texture_size, &texMem[tlutaddr], palette_size,
                                         width, height, texformat, use_mipmaps, true);
  }

  if (hires_tex)
  {
    for (u32 level_index = 1; level_index != texLevels; ++level_index)
    {
      const auto& level = hires_tex->m_levels[level_index];
      entry->texture->Load(level_index, level.width, level.height, level.row_length,
                           level.data.data(), level.data.size());
    }
  }
  else
  {
    // load mips - TODO: Loading mipmaps from tmem is untested!
    src_data += texture_size;

    const u8* ptr_even = nullptr;
    const u8* ptr_odd = nullptr;
    if (from_tmem)
    {
      ptr_even = &texMem[tmem_address_even + texture_size];
      ptr_odd = &texMem[tmem_address_odd];
    }

    for (u32 level = 1; level != texLevels; ++level)
    {
      const u32 mip_width = CalculateLevelSize(width, level);
      const u32 mip_height = CalculateLevelSize(height, level);
      const u32 expanded_mip_width = Common::AlignUp(mip_width, bsw);
      const u32 expanded_mip_height = Common::AlignUp(mip_height, bsh);

      const u8*& mip_src_data = from_tmem ? ((level % 2) ? ptr_odd : ptr_even) : src_data;
      size_t mip_size =
          TexDecoder_GetTextureSizeInBytes(expanded_mip_width, expanded_mip_height, texformat);

      if (decode_on_gpu)
      {
        u32 row_stride = bytes_per_block * (expanded_mip_width / bsw);
        g_texture_cache->DecodeTextureOnGPU(entry, level, mip_src_data, mip_size, texformat,
                                            mip_width, mip_height, expanded_mip_width,
                                            expanded_mip_height, row_stride, tlut, tlutfmt);
      }
      else
      {
        // No need to call CheckTempSize here, as the whole buffer is preallocated at the beginning
        size_t decoded_mip_size = expanded_mip_width * sizeof(u32) * expanded_mip_height;
        TexDecoder_Decode(dst_buffer, mip_src_data, expanded_mip_width, expanded_mip_height,
                          texformat, tlut, tlutfmt);
        entry->texture->Load(level, mip_width, mip_height, expanded_mip_width, dst_buffer,
                             decoded_mip_size);

        arbitrary_mip_detector.AddLevel(mip_width, mip_height, expanded_mip_width, dst_buffer);

        dst_buffer += decoded_mip_size;
      }

      mip_src_data += mip_size;
    }
  }

  entry->has_arbitrary_mips = hires_tex ? hires_tex->HasArbitraryMipmaps() :
                                          arbitrary_mip_detector.HasArbitraryMipmaps(dst_buffer);

  if (g_ActiveConfig.bDumpTextures && !hires_tex)
  {
    for (u32 level = 0; level < texLevels; ++level)
    {
      DumpTexture(entry, basename, level, entry->has_arbitrary_mips);
    }
  }

  INCSTAT(stats.numTexturesUploaded);
  SETSTAT(stats.numTexturesAlive, textures_by_address.size());

  entry = DoPartialTextureUpdates(iter->second, &texMem[tlutaddr], tlutfmt);

  return entry;
}

TextureCacheBase::TCacheEntry*
TextureCacheBase::GetXFBTexture(u32 address, u32 width, u32 height, TextureFormat tex_format,
                                int texture_cache_safety_color_sample_size)
{
  auto tex_info = ComputeTextureInformation(address, width, height, tex_format,
                                            texture_cache_safety_color_sample_size, false, 0, 0, 0,
                                            TLUTFormat::IA8, 1);
  if (!tex_info)
    return nullptr;

  // Try a direct lookup by address/hash.
  const TextureLookupInformation tex_info_value = tex_info.value();
  TCacheEntry* entry = GetXFBFromCache(tex_info_value);
  if (entry)
    return entry;

  // At this point, the XFB wasn't found in cache. This means the address is most likely not
  // pointing at an xfb copy but instead an area of memory.  Let's attempt to stitch all entries in
  // this memory space together
  bool loaded_from_overlapping = true;
  entry = GetTextureFromOverlappingTextures(tex_info_value);
  if (!entry)
  {
    // At this point, the xfb address is truly "bogus" it likely is an area of memory defined by the
    // CPU, so load it from memory.
    entry = GetTextureFromMemory(tex_info_value);
    loaded_from_overlapping = false;
  }

  if (g_ActiveConfig.bDumpXFBTarget)
  {
    // While this isn't really an xfb copy, we can treat it as such
    // for dumping purposes
    static int xfb_count = 0;
    const std::string xfb_type = loaded_from_overlapping ? "combined" : "from_memory";
    entry->texture->Save(StringFromFormat("%sxfb_%s_%i.png",
                                          File::GetUserPath(D_DUMPTEXTURES_IDX).c_str(),
                                          xfb_type.c_str(), xfb_count++),
                         0);
  }

  return entry;
}

std::optional<TextureLookupInformation> TextureCacheBase::ComputeTextureInformation(
    u32 address, u32 width, u32 height, TextureFormat tex_format,
    int texture_cache_safety_color_sample_size, bool from_tmem, u32 tmem_address_even,
    u32 tmem_address_odd, u32 tlut_address, TLUTFormat tlut_format, u32 levels)
{
  TextureLookupInformation tex_info;

  tex_info.from_tmem = from_tmem;
  tex_info.tmem_address_even = tmem_address_even;
  tex_info.tmem_address_odd = tmem_address_odd;

  tex_info.address = address;

  if (from_tmem)
    tex_info.src_data = &texMem[tex_info.tmem_address_even];
  else
    tex_info.src_data = Memory::GetPointer(tex_info.address);

  if (tex_info.src_data == nullptr)
  {
    ERROR_LOG(VIDEO, "Trying to use an invalid texture address 0x%8x", tex_info.address);
    return {};
  }

  tex_info.texture_cache_safety_color_sample_size = texture_cache_safety_color_sample_size;

  // TexelSizeInNibbles(format) * width * height / 16;
  tex_info.block_width = TexDecoder_GetBlockWidthInTexels(tex_format);
  tex_info.block_height = TexDecoder_GetBlockHeightInTexels(tex_format);

  tex_info.bytes_per_block = (tex_info.block_width * tex_info.block_height *
                              TexDecoder_GetTexelSizeInNibbles(tex_format)) /
                             2;

  tex_info.expanded_width = Common::AlignUp(width, tex_info.block_width);
  tex_info.expanded_height = Common::AlignUp(height, tex_info.block_height);

  tex_info.total_bytes = TexDecoder_GetTextureSizeInBytes(tex_info.expanded_width,
                                                          tex_info.expanded_height, tex_format);

  tex_info.native_width = width;
  tex_info.native_height = height;
  tex_info.native_levels = levels;

  // GPUs don't like when the specified mipmap count would require more than one 1x1-sized LOD in
  // the mipmap chain
  // e.g. 64x64 with 7 LODs would have the mipmap chain 64x64,32x32,16x16,8x8,4x4,2x2,1x1,0x0, so we
  // limit the mipmap count to 6 there
  tex_info.computed_levels = std::min<u32>(
      IntLog2(std::max(tex_info.native_width, tex_info.native_height)) + 1, tex_info.native_levels);

  tex_info.full_format = TextureAndTLUTFormat(tex_format, tlut_format);
  tex_info.tlut_address = tlut_address;

  // TODO: This doesn't hash GB tiles for preloaded RGBA8 textures (instead, it's hashing more data
  // from the low tmem bank than it should)
  tex_info.base_hash = Common::GetHash64(tex_info.src_data, tex_info.total_bytes,
                                         tex_info.texture_cache_safety_color_sample_size);

  tex_info.is_palette_texture = IsColorIndexed(tex_format);

  if (tex_info.is_palette_texture)
  {
    tex_info.palette_size = TexDecoder_GetPaletteSize(tex_format);
    tex_info.full_hash = tex_info.base_hash ^
                         Common::GetHash64(&texMem[tex_info.tlut_address], tex_info.palette_size,
                                           tex_info.texture_cache_safety_color_sample_size);
  }
  else
  {
    tex_info.full_hash = tex_info.base_hash;
  }

  return tex_info;
}

TextureCacheBase::TCacheEntry*
TextureCacheBase::GetXFBFromCache(const TextureLookupInformation& tex_info)
{
  auto iter_range = textures_by_address.equal_range(tex_info.address);
  TexAddrCache::iterator iter = iter_range.first;

  while (iter != iter_range.second)
  {
    TCacheEntry* entry = iter->second;

    if ((entry->is_xfb_copy || entry->format.texfmt == TextureFormat::XFB) &&
        entry->native_width == tex_info.native_width &&
        entry->native_height == tex_info.native_height &&
        entry->memory_stride == entry->BytesPerRow() && !entry->may_have_overlapping_textures)
    {
      if (tex_info.base_hash == entry->hash && !entry->reference_changed)
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

  return nullptr;
}

TextureCacheBase::TCacheEntry*
TextureCacheBase::GetTextureFromOverlappingTextures(const TextureLookupInformation& tex_info)
{
  u32 numBlocksX = tex_info.native_width / tex_info.block_width;

  // XFBs created for the purpose of being a container for textures from memory
  // or as a container for overlapping textures, never need to be combined
  // with other textures
  TCacheEntry* stitched_entry =
      CreateNormalTexture(tex_info, FramebufferManagerBase::GetEFBLayers());
  stitched_entry->may_have_overlapping_textures = false;

  // It is possible that some of the overlapping textures overlap each other.
  // This behavior has been seen with XFB copies in Rogue Leader.
  // To get the correct result, we apply the texture updates in the order the textures were
  // originally loaded. This ensures that the parts of the texture that would have been overwritten
  // in memory on real hardware get overwritten the same way here too.
  // This should work, but it may be a better idea to keep track of partial XFB copy invalidations
  // instead, which would reduce the amount of copying work here.
  std::vector<TCacheEntry*> candidates;

  auto iter = FindOverlappingTextures(tex_info.address, tex_info.total_bytes);
  while (iter.first != iter.second)
  {
    TCacheEntry* entry = iter.first->second;
    if (entry->IsCopy() && !entry->tmem_only &&
        entry->OverlapsMemoryRange(tex_info.address, tex_info.total_bytes) &&
        entry->memory_stride == stitched_entry->memory_stride)
    {
      if (entry->hash == entry->CalculateHash())
      {
        candidates.emplace_back(entry);
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

  std::sort(candidates.begin(), candidates.end(),
            [](const TCacheEntry* a, const TCacheEntry* b) { return a->id < b->id; });

  bool updated_entry = false;
  for (TCacheEntry* entry : candidates)
  {
    if (tex_info.is_palette_texture)
    {
      TCacheEntry* decoded_entry =
          ApplyPaletteToEntry(entry, nullptr, tex_info.full_format.tlutfmt);
      if (decoded_entry)
      {
        // Link the efb copy with the partially updated texture, so we won't apply this partial
        // update again
        entry->CreateReference(stitched_entry);
        // Mark the texture update as used, as if it was loaded directly
        entry->frameCount = FRAMECOUNT_INVALID;
        entry = decoded_entry;
      }
      else
      {
        continue;
      }
    }

    s32 src_x, src_y, dst_x, dst_y;

    // Note for understanding the math:
    // Normal textures can't be strided, so the 2 missing cases with src_x > 0 don't exist
    if (entry->addr >= stitched_entry->addr)
    {
      s32 block_offset = (entry->addr - stitched_entry->addr) / tex_info.bytes_per_block;
      s32 block_x = block_offset % numBlocksX;
      s32 block_y = block_offset / numBlocksX;
      src_x = 0;
      src_y = 0;
      dst_x = block_x * tex_info.block_width;
      dst_y = block_y * tex_info.block_height;
    }
    else
    {
      s32 srcNumBlocksX = entry->native_width / tex_info.block_width;
      s32 block_offset = (stitched_entry->addr - entry->addr) / tex_info.bytes_per_block;
      s32 block_x = block_offset % srcNumBlocksX;
      s32 block_y = block_offset / srcNumBlocksX;
      src_x = block_x * tex_info.block_width;
      src_y = block_y * tex_info.block_height;
      dst_x = 0;
      dst_y = 0;
    }

    u32 copy_width = std::min(entry->native_width - src_x, stitched_entry->native_width - dst_x);
    u32 copy_height = std::min(entry->native_height - src_y, stitched_entry->native_height - dst_y);

    // If one of the textures is scaled, scale both with the current efb scaling factor
    if (stitched_entry->native_width != stitched_entry->GetWidth() ||
        stitched_entry->native_height != stitched_entry->GetHeight() ||
        entry->native_width != entry->GetWidth() || entry->native_height != entry->GetHeight())
    {
      ScaleTextureCacheEntryTo(stitched_entry,
                               g_renderer->EFBToScaledX(stitched_entry->native_width),
                               g_renderer->EFBToScaledY(stitched_entry->native_height));
      ScaleTextureCacheEntryTo(entry, g_renderer->EFBToScaledX(entry->native_width),
                               g_renderer->EFBToScaledY(entry->native_height));

      src_x = g_renderer->EFBToScaledX(src_x);
      src_y = g_renderer->EFBToScaledY(src_y);
      dst_x = g_renderer->EFBToScaledX(dst_x);
      dst_y = g_renderer->EFBToScaledY(dst_y);
      copy_width = g_renderer->EFBToScaledX(copy_width);
      copy_height = g_renderer->EFBToScaledY(copy_height);
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
    const u32 layers_to_copy = std::min(entry->GetNumLayers(), stitched_entry->GetNumLayers());
    for (u32 layer = 0; layer < layers_to_copy; layer++)
    {
      stitched_entry->texture->CopyRectangleFromTexture(entry->texture.get(), srcrect, layer, 0,
                                                        dstrect, layer, 0);
    }
    updated_entry = true;

    if (tex_info.is_palette_texture)
    {
      // Remove the temporary converted texture, it won't be used anywhere else
      // TODO: It would be nice to convert and copy in one step, but this code path isn't common
      InvalidateTexture(GetTexCacheIter(entry));
    }
    else
    {
      // Link the two textures together, so we won't apply this partial update again
      entry->CreateReference(stitched_entry);
      // Mark the texture update as used, as if it was loaded directly
      entry->frameCount = FRAMECOUNT_INVALID;
    }
  }

  if (!updated_entry)
  {
    // Kinda annoying that we have to throw away the texture we just created, but with the above
    // code requiring the TCacheEntry object exists, can't do much at the moment.
    InvalidateTexture(GetTexCacheIter(stitched_entry));
    return nullptr;
  }

  return stitched_entry;
}

TextureCacheBase::TCacheEntry*
TextureCacheBase::CreateNormalTexture(const TextureLookupInformation& tex_info, u32 layers)
{
  // create the entry/texture
  TextureConfig config;
  config.width = tex_info.native_width;
  config.height = tex_info.native_height;
  config.levels = tex_info.computed_levels;
  config.format = AbstractTextureFormat::RGBA8;
  config.rendertarget = true;
  config.layers = layers;

  TCacheEntry* entry = AllocateCacheEntry(config);
  GFX_DEBUGGER_PAUSE_AT(NEXT_NEW_TEXTURE, true);

  if (!entry)
    return nullptr;

  textures_by_address.emplace(tex_info.address, entry);
  if (tex_info.texture_cache_safety_color_sample_size == 0 ||
      std::max(tex_info.total_bytes, tex_info.palette_size) <=
          (u32)tex_info.texture_cache_safety_color_sample_size * 8)
  {
    entry->textures_by_hash_iter = textures_by_hash.emplace(tex_info.full_hash, entry);
  }

  entry->SetGeneralParameters(tex_info.address, tex_info.total_bytes, tex_info.full_format, false);
  entry->SetDimensions(tex_info.native_width, tex_info.native_height, tex_info.computed_levels);
  entry->SetHashes(tex_info.base_hash, tex_info.full_hash);
  entry->is_custom_tex = false;
  entry->memory_stride = entry->BytesPerRow();
  entry->SetNotCopy();

  INCSTAT(stats.numTexturesUploaded);
  SETSTAT(stats.numTexturesAlive, textures_by_address.size());

  return entry;
}

TextureCacheBase::TCacheEntry*
TextureCacheBase::GetTextureFromMemory(const TextureLookupInformation& tex_info)
{
  // We can decode on the GPU if it is a supported format and the flag is enabled.
  // Currently we don't decode RGBA8 textures from Tmem, as that would require copying from both
  // banks, and if we're doing an copy we may as well just do the whole thing on the CPU, since
  // there's no conversion between formats. In the future this could be extended with a separate
  // shader, however.
  bool decode_on_gpu = g_ActiveConfig.UseGPUTextureDecoding() &&
                       g_texture_cache->SupportsGPUTextureDecode(tex_info.full_format.texfmt,
                                                                 tex_info.full_format.tlutfmt) &&
                       !(tex_info.from_tmem && tex_info.full_format.texfmt == TextureFormat::RGBA8);

  // Since it's coming from RAM, it can only have one layer (no stereo).
  TCacheEntry* entry = CreateNormalTexture(tex_info, 1);
  entry->may_have_overlapping_textures = false;
  LoadTextureLevelZeroFromMemory(entry, tex_info, decode_on_gpu);
  return entry;
}

void TextureCacheBase::LoadTextureLevelZeroFromMemory(TCacheEntry* entry_to_update,
                                                      const TextureLookupInformation& tex_info,
                                                      bool decode_on_gpu)
{
  const u8* tlut = &texMem[tex_info.tlut_address];

  if (decode_on_gpu)
  {
    u32 row_stride = tex_info.bytes_per_block * (tex_info.expanded_width / tex_info.block_width);
    g_texture_cache->DecodeTextureOnGPU(
        entry_to_update, 0, tex_info.src_data, tex_info.total_bytes, tex_info.full_format.texfmt,
        tex_info.native_width, tex_info.native_height, tex_info.expanded_width,
        tex_info.expanded_height, row_stride, tlut, tex_info.full_format.tlutfmt);
  }
  else
  {
    size_t decoded_texture_size = tex_info.expanded_width * sizeof(u32) * tex_info.expanded_height;
    CheckTempSize(decoded_texture_size);
    if (!(tex_info.full_format.texfmt == TextureFormat::RGBA8 && tex_info.from_tmem))
    {
      TexDecoder_Decode(temp, tex_info.src_data, tex_info.expanded_width, tex_info.expanded_height,
                        tex_info.full_format.texfmt, tlut, tex_info.full_format.tlutfmt);
    }
    else
    {
      u8* src_data_gb = &texMem[tex_info.tmem_address_odd];
      TexDecoder_DecodeRGBA8FromTmem(temp, tex_info.src_data, src_data_gb, tex_info.expanded_width,
                                     tex_info.expanded_height);
    }

    entry_to_update->texture->Load(0, tex_info.native_width, tex_info.native_height,
                                   tex_info.expanded_width, temp, decoded_texture_size);
  }
}

TextureCacheBase::CopyFilterCoefficientArray TextureCacheBase::GetRAMCopyFilterCoefficients(
    const CopyFilterCoefficients::Values& coefficients) const
{
  // To simplify the backend, we precalculate the three coefficients in common. Coefficients 0, 1
  // are for the row above, 2, 3, 4 are for the current pixel, and 5, 6 are for the row below.
  return {{
      static_cast<float>(static_cast<u32>(coefficients[0]) + static_cast<u32>(coefficients[1])) /
          64.0f,
      static_cast<float>(static_cast<u32>(coefficients[2]) + static_cast<u32>(coefficients[3]) +
                         static_cast<u32>(coefficients[4])) /
          64.0f,
      static_cast<float>(static_cast<u32>(coefficients[5]) + static_cast<u32>(coefficients[6])) /
          64.0f,
  }};
}

TextureCacheBase::CopyFilterCoefficientArray TextureCacheBase::GetVRAMCopyFilterCoefficients(
    const CopyFilterCoefficients::Values& coefficients) const
{
  // If the user disables the copy filter, only apply it to the VRAM copy.
  // This way games which are sensitive to changes to the RAM copy of the XFB will be unaffected.
  CopyFilterCoefficientArray res = GetRAMCopyFilterCoefficients(coefficients);
  if (!g_ActiveConfig.bDisableCopyFilter)
    return res;

  // Disabling the copy filter in options should not ignore the values the game sets completely,
  // as some games use the filter coefficients to control the brightness of the screen. Instead,
  // add all coefficients to the middle sample, so the deflicker/vertical filter has no effect.
  res[1] += res[0] + res[2];
  res[0] = 0;
  res[2] = 0;
  return res;
}

bool TextureCacheBase::NeedsCopyFilterInShader(const CopyFilterCoefficientArray& coefficients) const
{
  // If the top/bottom coefficients are zero, no point sampling/blending from these rows.
  return coefficients[0] != 0 || coefficients[2] != 0;
}

void TextureCacheBase::CopyRenderTargetToTexture(
    u32 dstAddr, EFBCopyFormat dstFormat, u32 width, u32 height, u32 dstStride, bool is_depth_copy,
    const EFBRectangle& srcRect, bool isIntensity, bool scaleByHalf, float y_scale, float gamma,
    bool clamp_top, bool clamp_bottom, const CopyFilterCoefficients::Values& filter_coefficients)
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

  u8* dst = Memory::GetPointer(dstAddr);
  if (dst == nullptr)
  {
    ERROR_LOG(VIDEO, "Trying to copy from EFB to invalid address 0x%8x", dstAddr);
    return;
  }

  // tex_w and tex_h are the native size of the texture in the GC memory.
  // The size scaled_* represents the emulated texture. Those differ
  // because of upscaling and because of yscaling of XFB copies.
  // For the latter, we keep the EFB resolution for the virtual XFB blit.
  u32 tex_w = width;
  u32 tex_h = height;
  u32 scaled_tex_w = g_renderer->EFBToScaledX(srcRect.GetWidth());
  u32 scaled_tex_h = g_renderer->EFBToScaledY(srcRect.GetHeight());

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
    ERROR_LOG(VIDEO, "Memory stride too small (%i < %i)", dstStride, bytes_per_row);
    copy_to_vram = false;
  }

  TCacheEntry* entry = nullptr;
  if (copy_to_vram)
  {
    // create the texture
    TextureConfig config;
    config.rendertarget = true;
    config.width = scaled_tex_w;
    config.height = scaled_tex_h;
    config.layers = FramebufferManagerBase::GetEFBLayers();

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

      CopyEFBToCacheEntry(entry, is_depth_copy, srcRect, scaleByHalf, dstFormat, isIntensity, gamma,
                          clamp_top, clamp_bottom,
                          GetVRAMCopyFilterCoefficients(filter_coefficients));

      if (g_ActiveConfig.bDumpEFBTarget && !is_xfb_copy)
      {
        static int efb_count = 0;
        entry->texture->Save(StringFromFormat("%sefb_frame_%i.png",
                                              File::GetUserPath(D_DUMPTEXTURES_IDX).c_str(),
                                              efb_count++),
                             0);
      }

      if (g_ActiveConfig.bDumpXFBTarget && is_xfb_copy)
      {
        static int xfb_count = 0;
        entry->texture->Save(StringFromFormat("%sxfb_copy_%i.png",
                                              File::GetUserPath(D_DUMPTEXTURES_IDX).c_str(),
                                              xfb_count++),
                             0);
      }
    }
  }

  if (copy_to_ram)
  {
    CopyFilterCoefficientArray coefficients = GetRAMCopyFilterCoefficients(filter_coefficients);
    PEControl::PixelFormat srcFormat = bpmem.zcontrol.pixel_format;
    EFBCopyParams format(srcFormat, dstFormat, is_depth_copy, isIntensity,
                         NeedsCopyFilterInShader(coefficients));

    std::unique_ptr<AbstractStagingTexture> staging_texture = GetEFBCopyStagingTexture();
    if (staging_texture)
    {
      CopyEFB(staging_texture.get(), format, tex_w, bytes_per_row, num_blocks_y, dstStride, srcRect,
              scaleByHalf, y_scale, gamma, clamp_top, clamp_bottom, coefficients);

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
        entry->pending_efb_copy_invalidated = false;
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
      // Hack: Most games don't actually need the correct texture data in RAM
      //       and we can just keep a copy in VRAM. We zero the memory so we
      //       can check it hasn't changed before using our copy in VRAM.
      u8* ptr = dst;
      for (u32 i = 0; i < num_blocks_y; i++)
      {
        std::memset(ptr, 0, bytes_per_row);
        ptr += dstStride;
      }
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
    TCacheEntry* overlapping_entry = iter.first->second;

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
      if (overlapping_entry->textures_by_hash_iter != textures_by_hash.end())
      {
        textures_by_hash.erase(overlapping_entry->textures_by_hash_iter);
        overlapping_entry->textures_by_hash_iter = textures_by_hash.end();
      }
    }
    ++iter.first;
  }

  if (g_bRecordFifoData)
  {
    // Mark the memory behind this efb copy as dynamicly generated for the Fifo log
    u32 address = dstAddr;
    for (u32 i = 0; i < num_blocks_y; i++)
    {
      FifoRecorder::GetInstance().UseMemory(address, bytes_per_row, MemoryUpdate::TEXTURE_MAP,
                                            true);
      address += dstStride;
    }
  }

  // Even if the copy is deferred, still compute the hash. This way if the copy is used as a texture
  // in a subsequent draw before it is flushed, it will have the same hash.
  if (entry)
  {
    const u64 hash = entry->CalculateHash();
    entry->SetHashes(hash, hash);
    textures_by_address.emplace(dstAddr, entry);
  }
}

void TextureCacheBase::FlushEFBCopies()
{
  if (m_pending_efb_copies.empty())
    return;

  for (TCacheEntry* entry : m_pending_efb_copies)
    FlushEFBCopy(entry);
  m_pending_efb_copies.clear();
}

TextureConfig TextureCacheBase::GetEncodingTextureConfig()
{
  return TextureConfig(EFB_WIDTH * 4, 1024, 1, 1, 1, AbstractTextureFormat::BGRA8, true);
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
  // Copy from texture -> guest memory.
  u8* const dst = Memory::GetPointer(entry->addr);
  WriteEFBCopyToRAM(dst, entry->pending_efb_copy_width, entry->pending_efb_copy_height,
                    entry->memory_stride, std::move(entry->pending_efb_copy));

  // If the EFB copy was invalidated (e.g. the bloom case mentioned in InvalidateTexture), now is
  // the time to clean up the TCacheEntry. In which case, we don't need to compute the new hash of
  // the RAM copy. But we need to clean up the TCacheEntry, as InvalidateTexture doesn't free it.
  if (entry->pending_efb_copy_invalidated)
  {
    delete entry;
    return;
  }

  // Re-hash the texture now that the guest memory is populated.
  // This should be safe because we'll catch any writes before the game can modify it.
  const u64 hash = entry->CalculateHash();
  entry->SetHashes(hash, hash);

  // Check for any overlapping XFB copies which now need the hash recomputed.
  // See the comment above regarding Rogue Squadron 2.
  if (entry->is_xfb_copy)
  {
    const u32 covered_range = entry->pending_efb_copy_height * entry->memory_stride;
    auto range = FindOverlappingTextures(entry->addr, covered_range);
    for (auto iter = range.first; iter != range.second; ++iter)
    {
      TCacheEntry* overlapping_entry = iter->second;
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

  std::unique_ptr<AbstractStagingTexture> tex =
      g_renderer->CreateStagingTexture(StagingTextureType::Readback, GetEncodingTextureConfig());
  if (!tex)
    WARN_LOG(VIDEO, "Failed to create EFB copy staging texture");

  return tex;
}

void TextureCacheBase::ReleaseEFBCopyStagingTexture(std::unique_ptr<AbstractStagingTexture> tex)
{
  m_efb_copy_staging_texture_pool.push_back(std::move(tex));
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
// This comment is indented wrong because of the silly linter, btw.

#if defined(_M_X86) || defined(_M_X86_64)
  __m128i sixteenBytes = _mm_set1_epi16((s16)(u16)0xFE01);
#endif

  for (u32 i = 0; i < num_blocks_y; i++)
  {
    u32 size = bytes_per_row;
    u8* rowdst = dst;
#if defined(_M_X86) || defined(_M_X86_64)
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

TextureCacheBase::TCacheEntry* TextureCacheBase::AllocateCacheEntry(const TextureConfig& config)
{
  std::unique_ptr<AbstractTexture> texture = AllocateTexture(config);

  if (!texture)
  {
    return nullptr;
  }
  TCacheEntry* cacheEntry = new TCacheEntry(std::move(texture));
  cacheEntry->textures_by_hash_iter = textures_by_hash.end();
  cacheEntry->id = last_entry_id++;
  return cacheEntry;
}

std::unique_ptr<AbstractTexture> TextureCacheBase::AllocateTexture(const TextureConfig& config)
{
  TexPool::iterator iter = FindMatchingTextureFromPool(config);
  std::unique_ptr<AbstractTexture> entry;
  if (iter != texture_pool.end())
  {
    entry = std::move(iter->second.texture);
    texture_pool.erase(iter);
  }
  else
  {
    entry = g_renderer->CreateTexture(config);
    if (!entry)
      return nullptr;

    INCSTAT(stats.numTexturesCreated);
  }

  return entry;
}

TextureCacheBase::TexPool::iterator
TextureCacheBase::FindMatchingTextureFromPool(const TextureConfig& config)
{
  // Find a texture from the pool that does not have a frameCount of FRAMECOUNT_INVALID.
  // This prevents a texture from being used twice in a single frame with different data,
  // which potentially means that a driver has to maintain two copies of the texture anyway.
  // Render-target textures are fine through, as they have to be generated in a seperated pass.
  // As non-render-target textures are usually static, this should not matter much.
  auto range = texture_pool.equal_range(config);
  auto matching_iter = std::find_if(range.first, range.second, [](const auto& iter) {
    return iter.first.rendertarget || iter.second.frameCount != FRAMECOUNT_INVALID;
  });
  return matching_iter != range.second ? matching_iter : texture_pool.end();
}

TextureCacheBase::TexAddrCache::iterator
TextureCacheBase::GetTexCacheIter(TextureCacheBase::TCacheEntry* entry)
{
  auto iter_range = textures_by_address.equal_range(entry->addr);
  TexAddrCache::iterator iter = iter_range.first;
  while (iter != iter_range.second)
  {
    if (iter->second == entry)
    {
      return iter;
    }
    ++iter;
  }
  return textures_by_address.end();
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
  auto begin = textures_by_address.lower_bound(lower_addr);
  auto end = textures_by_address.upper_bound(addr + size_in_bytes);

  return std::make_pair(begin, end);
}

TextureCacheBase::TexAddrCache::iterator
TextureCacheBase::InvalidateTexture(TexAddrCache::iterator iter, bool discard_pending_efb_copy)
{
  if (iter == textures_by_address.end())
    return textures_by_address.end();

  TCacheEntry* entry = iter->second;

  if (entry->textures_by_hash_iter != textures_by_hash.end())
  {
    textures_by_hash.erase(entry->textures_by_hash_iter);
    entry->textures_by_hash_iter = textures_by_hash.end();
  }

  for (size_t i = 0; i < bound_textures.size(); ++i)
  {
    // If the entry is currently bound and not invalidated, keep it, but mark it as invalidated.
    // This way it can still be used via tmem cache emulation, but nothing else.
    // Spyro: A Hero's Tail is known for using such overwritten textures.
    if (bound_textures[i] == entry && IsValidBindPoint(static_cast<u32>(i)))
    {
      bound_textures[i]->tmem_only = true;
      return ++iter;
    }
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
      entry->pending_efb_copy_invalidated = true;
    }
  }

  auto config = entry->texture->GetConfig();
  texture_pool.emplace(config, TexPoolEntry(std::move(entry->texture)));

  // Don't delete if there's a pending EFB copy, as we need the TCacheEntry alive.
  if (!entry->pending_efb_copy)
    delete entry;

  return textures_by_address.erase(iter);
}

u32 TextureCacheBase::TCacheEntry::BytesPerRow() const
{
  const u32 blockW = TexDecoder_GetBlockWidthInTexels(format.texfmt);

  // Round up source height to multiple of block size
  const u32 actualWidth = Common::AlignUp(native_width, blockW);

  const u32 numBlocksX = actualWidth / blockW;

  // RGBA takes two cache lines per block; all others take one
  const u32 bytes_per_block = format == TextureFormat::RGBA8 ? 64 : 32;

  return numBlocksX * bytes_per_block;
}

u32 TextureCacheBase::TCacheEntry::NumBlocksY() const
{
  u32 blockH = TexDecoder_GetBlockHeightInTexels(format.texfmt);
  // Round up source height to multiple of block size
  u32 actualHeight = Common::AlignUp(native_height, blockH);

  return actualHeight / blockH;
}

void TextureCacheBase::TCacheEntry::SetXfbCopy(u32 stride)
{
  is_efb_copy = false;
  is_xfb_copy = true;
  memory_stride = stride;

  ASSERT_MSG(VIDEO, memory_stride >= BytesPerRow(), "Memory stride is too small");

  size_in_bytes = memory_stride * NumBlocksY();
}

void TextureCacheBase::TCacheEntry::SetEfbCopy(u32 stride)
{
  is_efb_copy = true;
  is_xfb_copy = false;
  memory_stride = stride;

  ASSERT_MSG(VIDEO, memory_stride >= BytesPerRow(), "Memory stride is too small");

  size_in_bytes = memory_stride * NumBlocksY();
}

void TextureCacheBase::TCacheEntry::SetNotCopy()
{
  is_xfb_copy = false;
  is_efb_copy = false;
}

int TextureCacheBase::TCacheEntry::HashSampleSize() const
{
  if (should_force_safe_hashing)
  {
    return 0;
  }

  return g_ActiveConfig.iSafeTextureCache_ColorSamples;
}

u64 TextureCacheBase::TCacheEntry::CalculateHash() const
{
  u8* ptr = Memory::GetPointer(addr);
  if (memory_stride == BytesPerRow())
  {
    return Common::GetHash64(ptr, size_in_bytes, HashSampleSize());
  }
  else
  {
    u32 blocks = NumBlocksY();
    u64 temp_hash = size_in_bytes;

    u32 samples_per_row = 0;
    if (HashSampleSize() != 0)
    {
      // Hash at least 4 samples per row to avoid hashing in a bad pattern, like just on the left
      // side of the efb copy
      samples_per_row = std::max(HashSampleSize() / blocks, 4u);
    }

    for (u32 i = 0; i < blocks; i++)
    {
      // Multiply by a prime number to mix the hash up a bit. This prevents identical blocks from
      // canceling each other out
      temp_hash = (temp_hash * 397) ^ Common::GetHash64(ptr, BytesPerRow(), samples_per_row);
      ptr += memory_stride;
    }
    return temp_hash;
  }
}
