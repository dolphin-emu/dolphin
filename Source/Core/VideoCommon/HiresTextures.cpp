// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoCommon/HiresTextures.h"

#include <algorithm>
#include <cinttypes>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>
#include <xxhash.h>

#include "Common/File.h"
#include "Common/FileSearch.h"
#include "Common/FileUtil.h"
#include "Common/Flag.h"
#include "Common/Hash.h"
#include "Common/Image.h"
#include "Common/Logging/Log.h"
#include "Common/MemoryUtil.h"
#include "Common/StringUtil.h"
#include "Common/Swap.h"
#include "Common/Thread.h"
#include "Common/Timer.h"
#include "Core/Config/GraphicsSettings.h"
#include "Core/ConfigManager.h"
#include "VideoCommon/OnScreenDisplay.h"
#include "VideoCommon/VideoConfig.h"

struct DiskTexture
{
  std::string path;
  bool has_arbitrary_mipmaps;
};

static std::unordered_map<std::string, DiskTexture> s_textureMap;
static std::unordered_map<std::string, std::shared_ptr<HiresTexture>> s_textureCache;
static std::mutex s_textureCacheMutex;
static Common::Flag s_textureCacheAbortLoading;

static std::thread s_prefetcher;

static const std::string s_format_prefix = "tex1_";

void HiresTexture::Init()
{
  Update();
}

void HiresTexture::Shutdown()
{
  if (s_prefetcher.joinable())
  {
    s_textureCacheAbortLoading.Set();
    s_prefetcher.join();
  }

  s_textureMap.clear();
  s_textureCache.clear();
}

void HiresTexture::Update()
{
  if (s_prefetcher.joinable())
  {
    s_textureCacheAbortLoading.Set();
    s_prefetcher.join();
  }

  if (!g_ActiveConfig.bHiresTextures)
  {
    s_textureMap.clear();
    s_textureCache.clear();
    return;
  }

  if (!g_ActiveConfig.bCacheHiresTextures)
  {
    s_textureCache.clear();
  }

  const std::string& game_id = SConfig::GetInstance().GetGameID();
  const std::string texture_directory = GetTextureDirectory(game_id);
  const std::vector<std::string> extensions{".png", ".dds"};

  const std::vector<std::string> texture_paths =
      Common::DoFileSearch({texture_directory}, extensions, /*recursive*/ true);

  const std::string code = game_id + "_";

  for (auto& path : texture_paths)
  {
    std::string filename;
    SplitPath(path, nullptr, &filename, nullptr);

    if (filename.substr(0, s_format_prefix.length()) == s_format_prefix)
    {
      const size_t arb_index = filename.rfind("_arb");
      const bool has_arbitrary_mipmaps = arb_index != std::string::npos;
      if (has_arbitrary_mipmaps)
        filename.erase(arb_index, 4);
      s_textureMap[filename] = {path, has_arbitrary_mipmaps};
    }
  }

  if (g_ActiveConfig.bCacheHiresTextures)
  {
    // remove cached but deleted textures
    auto iter = s_textureCache.begin();
    while (iter != s_textureCache.end())
    {
      if (s_textureMap.find(iter->first) == s_textureMap.end())
      {
        iter = s_textureCache.erase(iter);
      }
      else
      {
        iter++;
      }
    }

    s_textureCacheAbortLoading.Clear();
    s_prefetcher = std::thread(Prefetch);
  }
}

void HiresTexture::Prefetch()
{
  Common::SetCurrentThreadName("Prefetcher");

  size_t size_sum = 0;
  size_t sys_mem = Common::MemPhysical();
  size_t recommended_min_mem = 2 * size_t(1024 * 1024 * 1024);
  // keep 2GB memory for system stability if system RAM is 4GB+ - use half of memory in other cases
  size_t max_mem =
      (sys_mem / 2 < recommended_min_mem) ? (sys_mem / 2) : (sys_mem - recommended_min_mem);
  u32 starttime = Common::Timer::GetTimeMs();
  for (const auto& entry : s_textureMap)
  {
    const std::string& base_filename = entry.first;

    if (base_filename.find("_mip") == std::string::npos)
    {
      std::unique_lock<std::mutex> lk(s_textureCacheMutex);

      auto iter = s_textureCache.find(base_filename);
      if (iter == s_textureCache.end())
      {
        // unlock while loading a texture. This may result in a race condition where
        // we'll load a texture twice, but it reduces the stuttering a lot.
        lk.unlock();
        std::unique_ptr<HiresTexture> texture = Load(base_filename, 0, 0);
        lk.lock();
        if (texture)
        {
          std::shared_ptr<HiresTexture> ptr(std::move(texture));
          iter = s_textureCache.insert(iter, std::make_pair(base_filename, ptr));
        }
      }
      if (iter != s_textureCache.end())
      {
        for (const Level& l : iter->second->m_levels)
          size_sum += l.data.size();
      }
    }

    if (s_textureCacheAbortLoading.IsSet())
    {
      return;
    }

    if (size_sum > max_mem)
    {
      Config::SetCurrent(Config::GFX_HIRES_TEXTURES, false);

      OSD::AddMessage(
          StringFromFormat(
              "Custom Textures prefetching after %.1f MB aborted, not enough RAM available",
              size_sum / (1024.0 * 1024.0)),
          10000);
      return;
    }
  }
  u32 stoptime = Common::Timer::GetTimeMs();
  OSD::AddMessage(StringFromFormat("Custom Textures loaded, %.1f MB in %.1f s",
                                   size_sum / (1024.0 * 1024.0), (stoptime - starttime) / 1000.0),
                  10000);
}

std::string HiresTexture::GenBaseName(const u8* texture, size_t texture_size, const u8* tlut,
                                      size_t tlut_size, u32 width, u32 height, TextureFormat format,
                                      bool has_mipmaps, bool dump)
{
  if (!dump && s_textureMap.empty())
    return "";

  // checking for min/max on paletted textures
  u32 min = 0xffff;
  u32 max = 0;
  switch (tlut_size)
  {
  case 0:
    break;
  case 16 * 2:
    for (size_t i = 0; i < texture_size; i++)
    {
      const u32 low_nibble = texture[i] & 0xf;
      const u32 high_nibble = texture[i] >> 4;

      min = std::min({min, low_nibble, high_nibble});
      max = std::max({max, low_nibble, high_nibble});
    }
    break;
  case 256 * 2:
  {
    for (size_t i = 0; i < texture_size; i++)
    {
      const u32 texture_byte = texture[i];

      min = std::min(min, texture_byte);
      max = std::max(max, texture_byte);
    }
    break;
  }
  case 16384 * 2:
    for (size_t i = 0; i < texture_size; i += sizeof(u16))
    {
      const u32 texture_halfword = Common::swap16(texture[i]) & 0x3fff;

      min = std::min(min, texture_halfword);
      max = std::max(max, texture_halfword);
    }
    break;
  }
  if (tlut_size > 0)
  {
    tlut_size = 2 * (max + 1 - min);
    tlut += 2 * min;
  }

  u64 tex_hash = XXH64(texture, texture_size, 0);
  u64 tlut_hash = tlut_size ? XXH64(tlut, tlut_size, 0) : 0;

  std::string basename = s_format_prefix + StringFromFormat("%dx%d%s_%016" PRIx64, width, height,
                                                            has_mipmaps ? "_m" : "", tex_hash);
  std::string tlutname = tlut_size ? StringFromFormat("_%016" PRIx64, tlut_hash) : "";
  std::string formatname = StringFromFormat("_%d", static_cast<int>(format));
  std::string fullname = basename + tlutname + formatname;

  // try to match a wildcard template
  if (!dump && s_textureMap.find(basename + "_$" + formatname) != s_textureMap.end())
    return basename + "_$" + formatname;

  // else generate the complete texture
  if (dump || s_textureMap.find(fullname) != s_textureMap.end())
    return fullname;

  return "";
}

u32 HiresTexture::CalculateMipCount(u32 width, u32 height)
{
  u32 mip_width = width;
  u32 mip_height = height;
  u32 mip_count = 1;
  while (mip_width > 1 || mip_height > 1)
  {
    mip_width = std::max(mip_width / 2, 1u);
    mip_height = std::max(mip_height / 2, 1u);
    mip_count++;
  }

  return mip_count;
}

std::shared_ptr<HiresTexture> HiresTexture::Search(const u8* texture, size_t texture_size,
                                                   const u8* tlut, size_t tlut_size, u32 width,
                                                   u32 height, TextureFormat format,
                                                   bool has_mipmaps)
{
  std::string base_filename =
      GenBaseName(texture, texture_size, tlut, tlut_size, width, height, format, has_mipmaps);

  std::lock_guard<std::mutex> lk(s_textureCacheMutex);

  auto iter = s_textureCache.find(base_filename);
  if (iter != s_textureCache.end())
  {
    return iter->second;
  }

  std::shared_ptr<HiresTexture> ptr(Load(base_filename, width, height));

  if (ptr && g_ActiveConfig.bCacheHiresTextures)
  {
    s_textureCache[base_filename] = ptr;
  }

  return ptr;
}

std::unique_ptr<HiresTexture> HiresTexture::Load(const std::string& base_filename, u32 width,
                                                 u32 height)
{
  // We need to have a level 0 custom texture to even consider loading.
  auto filename_iter = s_textureMap.find(base_filename);
  if (filename_iter == s_textureMap.end())
    return nullptr;

  // Try to load level 0 (and any mipmaps) from a DDS file.
  // If this fails, it's fine, we'll just load level0 again using SOIL.
  // Can't use make_unique due to private constructor.
  std::unique_ptr<HiresTexture> ret = std::unique_ptr<HiresTexture>(new HiresTexture());
  const DiskTexture& first_mip_file = filename_iter->second;
  ret->m_has_arbitrary_mipmaps = first_mip_file.has_arbitrary_mipmaps;
  LoadDDSTexture(ret.get(), first_mip_file.path);

  // Load remaining mip levels, or from the start if it's not a DDS texture.
  for (u32 mip_level = static_cast<u32>(ret->m_levels.size());; mip_level++)
  {
    std::string filename = base_filename;
    if (mip_level != 0)
      filename += StringFromFormat("_mip%u", mip_level);

    filename_iter = s_textureMap.find(filename);
    if (filename_iter == s_textureMap.end())
      break;

    // Try loading DDS textures first, that way we maintain compression of DXT formats.
    // TODO: Reduce the number of open() calls here. We could use one fd.
    Level level;
    if (!LoadDDSTexture(level, filename_iter->second.path, mip_level))
    {
      File::IOFile file;
      file.Open(filename_iter->second.path, "rb");
      std::vector<u8> buffer(file.GetSize());
      file.ReadBytes(buffer.data(), file.GetSize());

      if (!LoadTexture(level, buffer))
      {
        ERROR_LOG(VIDEO, "Custom texture %s failed to load", filename.c_str());
        break;
      }
    }

    ret->m_levels.push_back(std::move(level));
  }

  // If we failed to load any mip levels, we can't use this texture at all.
  if (ret->m_levels.empty())
    return nullptr;

  // Verify that the aspect ratio of the texture hasn't changed, as this could have side-effects.
  const Level& first_mip = ret->m_levels[0];
  if (first_mip.width * height != first_mip.height * width)
  {
    ERROR_LOG(VIDEO,
              "Invalid custom texture size %ux%u for texture %s. The aspect differs "
              "from the native size %ux%u.",
              first_mip.width, first_mip.height, first_mip_file.path.c_str(), width, height);
  }

  // Same deal if the custom texture isn't a multiple of the native size.
  if (width != 0 && height != 0 && (first_mip.width % width || first_mip.height % height))
  {
    ERROR_LOG(VIDEO,
              "Invalid custom texture size %ux%u for texture %s. Please use an integer "
              "upscaling factor based on the native size %ux%u.",
              first_mip.width, first_mip.height, first_mip_file.path.c_str(), width, height);
  }

  // Verify that each mip level is the correct size (divide by 2 each time).
  u32 current_mip_width = first_mip.width;
  u32 current_mip_height = first_mip.height;
  for (u32 mip_level = 1; mip_level < static_cast<u32>(ret->m_levels.size()); mip_level++)
  {
    if (current_mip_width != 1 || current_mip_height != 1)
    {
      current_mip_width = std::max(current_mip_width / 2, 1u);
      current_mip_height = std::max(current_mip_height / 2, 1u);

      const Level& level = ret->m_levels[mip_level];
      if (current_mip_width == level.width && current_mip_height == level.height)
        continue;

      ERROR_LOG(VIDEO,
                "Invalid custom texture size %dx%d for texture %s. Mipmap level %u must be %dx%d.",
                level.width, level.height, first_mip_file.path.c_str(), mip_level,
                current_mip_width, current_mip_height);
    }
    else
    {
      // It is invalid to have more than a single 1x1 mipmap.
      ERROR_LOG(VIDEO, "Custom texture %s has too many 1x1 mipmaps. Skipping extra levels.",
                first_mip_file.path.c_str());
    }

    // Drop this mip level and any others after it.
    while (ret->m_levels.size() > mip_level)
      ret->m_levels.pop_back();
  }

  // All levels have to have the same format.
  if (std::any_of(ret->m_levels.begin(), ret->m_levels.end(),
                  [&ret](const Level& l) { return l.format != ret->m_levels[0].format; }))
  {
    ERROR_LOG(VIDEO, "Custom texture %s has inconsistent formats across mip levels.",
              first_mip_file.path.c_str());

    return nullptr;
  }

  return ret;
}

bool HiresTexture::LoadTexture(Level& level, const std::vector<u8>& buffer)
{
  if (!Common::LoadPNG(buffer, &level.data, &level.width, &level.height))
    return false;

  if (level.data.empty())
    return false;

  // Loaded PNG images are converted to RGBA.
  level.format = AbstractTextureFormat::RGBA8;
  level.row_length = level.width;
  return true;
}

std::string HiresTexture::GetTextureDirectory(const std::string& game_id)
{
  const std::string texture_directory = File::GetUserPath(D_HIRESTEXTURES_IDX) + game_id;

  // If there's no directory with the region-specific ID, look for a 3-character region-free one
  if (!File::Exists(texture_directory))
    return File::GetUserPath(D_HIRESTEXTURES_IDX) + game_id.substr(0, 3);

  return texture_directory;
}

HiresTexture::~HiresTexture()
{
}

AbstractTextureFormat HiresTexture::GetFormat() const
{
  return m_levels.at(0).format;
}

bool HiresTexture::HasArbitraryMipmaps() const
{
  return m_has_arbitrary_mipmaps;
}
