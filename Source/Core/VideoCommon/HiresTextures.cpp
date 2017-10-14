// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoCommon/HiresTextures.h"

#include <SOIL/SOIL.h>
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

static std::unordered_map<std::string, std::string> s_textureMap;
static std::unordered_map<std::string, std::shared_ptr<HiresTexture>> s_textureCache;
static std::mutex s_textureCacheMutex;
static std::mutex s_textureCacheAquireMutex;  // for high priority access
static Common::Flag s_textureCacheAbortLoading;
static bool s_check_native_format;
static bool s_check_new_format;

static std::thread s_prefetcher;

static const std::string s_format_prefix = "tex1_";

HiresTexture::Level::Level() : data(nullptr, SOIL_free_image_data)
{
}

void HiresTexture::Init()
{
  s_check_native_format = false;
  s_check_new_format = false;

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
  std::vector<std::string> extensions{
      ".png", ".bmp", ".tga", ".dds",
      ".jpg"  // Why not? Could be useful for large photo-like textures
  };

  std::vector<std::string> filenames =
      Common::DoFileSearch({texture_directory}, extensions, /*recursive*/ true);

  const std::string code = game_id + "_";

  for (auto& rFilename : filenames)
  {
    std::string FileName;
    SplitPath(rFilename, nullptr, &FileName, nullptr);

    if (FileName.substr(0, code.length()) == code)
    {
      s_textureMap[FileName] = rFilename;
      s_check_native_format = true;
    }

    if (FileName.substr(0, s_format_prefix.length()) == s_format_prefix)
    {
      s_textureMap[FileName] = rFilename;
      s_check_new_format = true;
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
      {
        // try to get this mutex first, so the video thread is allow to get the real mutex faster
        std::unique_lock<std::mutex> lk(s_textureCacheAquireMutex);
      }
      std::unique_lock<std::mutex> lk(s_textureCacheMutex);

      auto iter = s_textureCache.find(base_filename);
      if (iter == s_textureCache.end())
      {
        // unlock while loading a texture. This may result in a race condition where we'll load a
        // texture twice,
        // but it reduces the stuttering a lot. Notice: The loading library _must_ be thread safe
        // now.
        // But bad luck, SOIL isn't, so TODO: remove SOIL usage here and use libpng directly
        // Also TODO: remove s_textureCacheAquireMutex afterwards. It won't be needed as the main
        // mutex will be locked rarely
        // lk.unlock();
        std::unique_ptr<HiresTexture> texture = Load(base_filename, 0, 0);
        // lk.lock();
        if (texture)
        {
          std::shared_ptr<HiresTexture> ptr(std::move(texture));
          iter = s_textureCache.insert(iter, std::make_pair(base_filename, ptr));
        }
      }
      if (iter != s_textureCache.end())
      {
        for (const Level& l : iter->second->m_levels)
        {
          size_sum += l.data_size;
        }
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
  std::string name = "";
  bool convert = false;
  if (!dump && s_check_native_format)
  {
    // try to load the old format first
    u64 tex_hash = GetHashHiresTexture(texture, (int)texture_size,
                                       g_ActiveConfig.iSafeTextureCache_ColorSamples);
    u64 tlut_hash = tlut_size ? GetHashHiresTexture(tlut, (int)tlut_size,
                                                    g_ActiveConfig.iSafeTextureCache_ColorSamples) :
                                0;
    name = StringFromFormat("%s_%08x_%i", SConfig::GetInstance().GetGameID().c_str(),
                            (u32)(tex_hash ^ tlut_hash), (u16)format);
    if (s_textureMap.find(name) != s_textureMap.end())
    {
      if (g_ActiveConfig.bConvertHiresTextures)
        convert = true;
      else
        return name;
    }
  }

  if (dump || s_check_new_format || convert)
  {
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
        min = std::min<u32>(min, texture[i] & 0xf);
        min = std::min<u32>(min, texture[i] >> 4);
        max = std::max<u32>(max, texture[i] & 0xf);
        max = std::max<u32>(max, texture[i] >> 4);
      }
      break;
    case 256 * 2:
      for (size_t i = 0; i < texture_size; i++)
      {
        min = std::min<u32>(min, texture[i]);
        max = std::max<u32>(max, texture[i]);
      }
      break;
    case 16384 * 2:
      for (size_t i = 0; i < texture_size / 2; i++)
      {
        min = std::min<u32>(min, Common::swap16(((u16*)texture)[i]) & 0x3fff);
        max = std::max<u32>(max, Common::swap16(((u16*)texture)[i]) & 0x3fff);
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
    std::string formatname = StringFromFormat("_%d", format);
    std::string fullname = basename + tlutname + formatname;

    for (int level = 0; level < 10 && convert; level++)
    {
      std::string oldname = name;
      if (level)
        oldname += StringFromFormat("_mip%d", level);

      // skip not existing levels
      if (s_textureMap.find(oldname) == s_textureMap.end())
        continue;

      for (int i = 0;; i++)
      {
        // for hash collisions, padd with an integer
        std::string newname = fullname;
        if (level)
          newname += StringFromFormat("_mip%d", level);
        if (i)
          newname += StringFromFormat(".%d", i);

        // new texture
        if (s_textureMap.find(newname) == s_textureMap.end())
        {
          std::string src = s_textureMap[oldname];
          size_t postfix = src.find_last_of('.');
          std::string dst = src.substr(0, postfix - oldname.length()) + newname +
                            src.substr(postfix, src.length() - postfix);
          if (File::Rename(src, dst))
          {
            s_textureMap.erase(oldname);
            s_textureMap[newname] = dst;
            s_check_new_format = true;
            OSD::AddMessage(StringFromFormat("Rename custom texture %s to %s", oldname.c_str(),
                                             newname.c_str()),
                            5000);
          }
          else
          {
            ERROR_LOG(VIDEO, "rename failed");
          }
          break;
        }
        else
        {
          // dst fail already exist, compare content
          std::string a, b;
          File::ReadFileToString(s_textureMap[oldname], a);
          File::ReadFileToString(s_textureMap[newname], b);

          if (a == b && a != "")
          {
            // equal, so remove
            if (File::Delete(s_textureMap[oldname]))
            {
              s_textureMap.erase(oldname);
              OSD::AddMessage(
                  StringFromFormat("Delete double old custom texture %s", oldname.c_str()), 5000);
            }
            else
            {
              ERROR_LOG(VIDEO, "delete failed");
            }
            break;
          }

          // else continue in this loop with the next higher padding variable
        }
      }
    }

    // try to match a wildcard template
    if (!dump && s_textureMap.find(basename + "_*" + formatname) != s_textureMap.end())
      return basename + "_*" + formatname;

    // else generate the complete texture
    if (dump || s_textureMap.find(fullname) != s_textureMap.end())
      return fullname;
  }

  return name;
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

  std::lock_guard<std::mutex> lk2(s_textureCacheAquireMutex);
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
  const std::string& first_mip_filename = filename_iter->second;
  LoadDDSTexture(ret.get(), first_mip_filename);

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
    if (!LoadDDSTexture(level, filename_iter->second))
    {
      File::IOFile file;
      file.Open(filename_iter->second, "rb");
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
    ERROR_LOG(VIDEO, "Invalid custom texture size %ux%u for texture %s. The aspect differs "
                     "from the native size %ux%u.",
              first_mip.width, first_mip.height, first_mip_filename.c_str(), width, height);
  }

  // Same deal if the custom texture isn't a multiple of the native size.
  if (width != 0 && height != 0 && (first_mip.width % width || first_mip.height % height))
  {
    ERROR_LOG(VIDEO, "Invalid custom texture size %ux%u for texture %s. Please use an integer "
                     "upscaling factor based on the native size %ux%u.",
              first_mip.width, first_mip.height, first_mip_filename.c_str(), width, height);
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
                level.width, level.height, first_mip_filename.c_str(), mip_level, current_mip_width,
                current_mip_height);
    }
    else
    {
      // It is invalid to have more than a single 1x1 mipmap.
      ERROR_LOG(VIDEO, "Custom texture %s has too many 1x1 mipmaps. Skipping extra levels.",
                first_mip_filename.c_str());
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
              first_mip_filename.c_str());

    return nullptr;
  }

  return ret;
}

bool HiresTexture::LoadTexture(Level& level, const std::vector<u8>& buffer)
{
  int channels;
  int width;
  int height;

  u8* data = SOIL_load_image_from_memory(buffer.data(), static_cast<int>(buffer.size()), &width,
                                         &height, &channels, SOIL_LOAD_RGBA);
  if (!data)
    return false;

  // Images loaded by SOIL are converted to RGBA.
  level.width = static_cast<u32>(width);
  level.height = static_cast<u32>(height);
  level.format = AbstractTextureFormat::RGBA8;
  level.data = ImageDataPointer(data, SOIL_free_image_data);
  level.row_length = level.width;
  level.data_size = static_cast<size_t>(level.row_length) * 4 * level.height;
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
