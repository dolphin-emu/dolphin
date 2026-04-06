// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/Assets/TextureAssetUtils.h"

#include <algorithm>

#include "Common/FileUtil.h"
#include "Common/Logging/Log.h"
#include "Common/StringUtil.h"

namespace VideoCommon
{
namespace
{
// Loads additional mip levels into the texture structure until _mip<N> texture is not found
bool LoadMips(const std::filesystem::path& asset_path, CustomTextureData::ArraySlice* data)
{
  if (!data) [[unlikely]]
    return false;

  std::string path;
  std::string filename;
  std::string extension;
  SplitPath(PathToString(asset_path), &path, &filename, &extension);

  std::string extension_lower = extension;
  Common::ToLower(&extension_lower);

  // Load additional mip levels
  for (u32 mip_level = static_cast<u32>(data->m_levels.size());; mip_level++)
  {
    const auto mip_level_filename = filename + fmt::format("_mip{}", mip_level);

    const auto full_path = path + mip_level_filename + extension;
    if (!File::Exists(full_path))
      return true;

    VideoCommon::CustomTextureData::ArraySlice::Level level;
    if (extension_lower == ".dds")
    {
      if (!LoadDDSTexture(&level, full_path, mip_level))
      {
        ERROR_LOG_FMT(VIDEO, "Custom mipmap '{}' failed to load", mip_level_filename);
        return false;
      }
    }
    else if (extension_lower == ".png")
    {
      if (!LoadPNGTexture(&level, full_path))
      {
        ERROR_LOG_FMT(VIDEO, "Custom mipmap '{}' failed to load", mip_level_filename);
        return false;
      }
    }
    else
    {
      ERROR_LOG_FMT(VIDEO, "Custom mipmap '{}' has unsupported extension", mip_level_filename);
      return false;
    }

    data->m_levels.push_back(std::move(level));
  }

  return true;
}
}  // namespace
bool LoadTextureDataFromFile(const CustomAssetLibrary::AssetID& asset_id,
                             const std::filesystem::path& asset_path, AbstractTextureType type,
                             CustomTextureData* data)
{
  auto ext = PathToString(asset_path.extension());
  Common::ToLower(&ext);
  if (ext == ".dds")
  {
    if (!LoadDDSTexture(data, PathToString(asset_path)))
    {
      ERROR_LOG_FMT(VIDEO, "Asset '{}' error - could not load dds texture!", asset_id);
      return false;
    }

    if (data->m_slices.empty()) [[unlikely]]
      data->m_slices.emplace_back();

    if (!LoadMips(asset_path, data->m_slices.data()))
      return false;

    return true;
  }

  if (ext == ".png")
  {
    // PNG could support more complicated texture types in the future
    // but for now just error
    if (type != AbstractTextureType::Texture_2D)
    {
      ERROR_LOG_FMT(VIDEO, "Asset '{}' error - PNG is not supported for texture type '{}'!",
                    asset_id, type);
      return {};
    }

    // If we have no slices, create one
    if (data->m_slices.empty())
      data->m_slices.emplace_back();

    auto& slice = data->m_slices[0];
    // If we have no levels, create one to pass into LoadPNGTexture
    if (slice.m_levels.empty())
      slice.m_levels.emplace_back();

    if (!LoadPNGTexture(slice.m_levels.data(), PathToString(asset_path)))
    {
      ERROR_LOG_FMT(VIDEO, "Asset '{}' error - could not load png texture!", asset_id);
      return false;
    }

    if (!LoadMips(asset_path, &slice))
      return false;

    return true;
  }

  ERROR_LOG_FMT(VIDEO, "Asset '{}' error - extension '{}' unknown!", asset_id, ext);
  return false;
}

bool ValidateTextureData(const CustomAssetLibrary::AssetID& asset_id, const CustomTextureData& data,
                         u32 native_width, u32 native_height)
{
  if (data.m_slices.empty())
  {
    ERROR_LOG_FMT(VIDEO,
                  "Texture data can't be validated for asset '{}' because no data was available.",
                  asset_id);
    return false;
  }

  if (data.m_slices.size() > 1)
  {
    ERROR_LOG_FMT(
        VIDEO,
        "Texture data can't be validated for asset '{}' because it has more slices than expected.",
        asset_id);
    return false;
  }

  const auto& slice = data.m_slices[0];
  if (slice.m_levels.empty())
  {
    ERROR_LOG_FMT(
        VIDEO,
        "Texture data can't be validated for asset '{}' because first slice has no data available.",
        asset_id);
    return false;
  }

  // Verify that the aspect ratio of the texture hasn't changed, as this could have
  // side-effects.
  const CustomTextureData::ArraySlice::Level& first_mip = slice.m_levels[0];
  if (first_mip.width * native_height != first_mip.height * native_width)
  {
    // Note: this feels like this should return an error but
    // for legacy reasons this is only a notice that something *could*
    // go wrong
    WARN_LOG_FMT(VIDEO,
                 "Invalid texture data size {}x{} for asset '{}'. The aspect differs "
                 "from the native size {}x{}.",
                 first_mip.width, first_mip.height, asset_id, native_width, native_height);
  }

  // Same deal if the custom texture isn't a multiple of the native size.
  if (native_width != 0 && native_height != 0 &&
      (first_mip.width % native_width || first_mip.height % native_height))
  {
    // Note: this feels like this should return an error but
    // for legacy reasons this is only a notice that something *could*
    // go wrong
    WARN_LOG_FMT(VIDEO,
                 "Invalid texture data size {}x{} for asset '{}'. Please use an integer "
                 "upscaling factor based on the native size {}x{}.",
                 first_mip.width, first_mip.height, asset_id, native_width, native_height);
  }

  return true;
}

bool PurgeInvalidMipsFromTextureData(const CustomAssetLibrary::AssetID& asset_id,
                                     CustomTextureData* data)
{
  for (std::size_t slice_index = 0; slice_index < data->m_slices.size(); slice_index++)
  {
    auto& slice = data->m_slices[slice_index];
    const auto& first_mip = slice.m_levels[0];

    // Verify that each mip level is the correct size (divide by 2 each time).
    u32 current_mip_width = first_mip.width;
    u32 current_mip_height = first_mip.height;
    for (u32 mip_level = 1; mip_level < static_cast<u32>(slice.m_levels.size()); mip_level++)
    {
      if (current_mip_width != 1 || current_mip_height != 1)
      {
        current_mip_width = std::max(current_mip_width / 2, 1u);
        current_mip_height = std::max(current_mip_height / 2, 1u);

        const VideoCommon::CustomTextureData::ArraySlice::Level& level = slice.m_levels[mip_level];
        if (current_mip_width == level.width && current_mip_height == level.height)
          continue;

        ERROR_LOG_FMT(VIDEO,
                      "Invalid custom game texture size {}x{} for texture asset {}. Slice {} with "
                      "mipmap level {} "
                      "must be {}x{}.",
                      level.width, level.height, asset_id, slice_index, mip_level,
                      current_mip_width, current_mip_height);
      }
      else
      {
        // It is invalid to have more than a single 1x1 mipmap.
        ERROR_LOG_FMT(VIDEO,
                      "Custom game texture {} has too many 1x1 mipmaps for slice {}. Skipping "
                      "extra levels.",
                      asset_id, slice_index);
      }

      // Drop this mip level and any others after it.
      while (slice.m_levels.size() > mip_level)
        slice.m_levels.pop_back();
    }

    // All levels have to have the same format.
    if (std::ranges::any_of(slice.m_levels,
                            [&first_mip](const auto& l) { return l.format != first_mip.format; }))
    {
      ERROR_LOG_FMT(
          VIDEO, "Custom game texture {} has inconsistent formats across mip levels for slice {}.",
          asset_id, slice_index);

      return false;
    }
  }

  return true;
}
}  // namespace VideoCommon
