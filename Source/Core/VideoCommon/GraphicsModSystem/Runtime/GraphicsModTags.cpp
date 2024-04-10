// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/GraphicsModSystem/Runtime/GraphicsModTags.h"

#include <algorithm>
#include <array>

using ConfigTag = GraphicsModSystem::Config::GraphicsModTag;

namespace
{
std::array<ConfigTag, 3> draw_call_tag_config = {
    ConfigTag{.m_name = "Bloom",
              .m_description =
                  "Post processing effect that blurs out the brightest areas of the screen",
              .m_color = Common::Vec3{0.66f, 0.63f, 0.16f}},
    ConfigTag{.m_name = "Depth of Field",
              .m_description = "Post processing effect that blurs distant objects",
              .m_color = Common::Vec3{0.8f, 0, 0}},
    ConfigTag{.m_name = "User Interface",
              .m_description = "",
              .m_color = Common::Vec3{0.01f, 0.04f, 0.99f}}};

constexpr std::array<GraphicsModSystem::DrawCallTag, 3> draw_call_tag_enum = {
    GraphicsModSystem::DrawCallTag::Bloom, GraphicsModSystem::DrawCallTag::DepthOfField,
    GraphicsModSystem::DrawCallTag::UserInterface};

std::array<ConfigTag, 2> texture_tag_config = {
    ConfigTag{.m_name = "Ignore",
              .m_description = "This texture will not contribute to the material hash",
              .m_color = Common::Vec3{0.66f, 0.63f, 0.16f}},
    ConfigTag{.m_name = "Arbitrary Mipmaps",
              .m_description =
                  "This texture has mipmaps that aren't just lower resolution of the first mip",
              .m_color = Common::Vec3{0.8f, 0, 0}}};

constexpr std::array<GraphicsModSystem::TextureTag, 2> texture_tag_enum = {
    GraphicsModSystem::TextureTag::Ignore, GraphicsModSystem::TextureTag::ArbitraryMipMap};

}  // namespace

namespace GraphicsModSystem::Runtime
{
std::span<const ConfigTag> GetDrawCallTagConfig()
{
  return draw_call_tag_config;
}

std::span<const DrawCallTag> GetDrawCallTags()
{
  return draw_call_tag_enum;
}

std::span<const ConfigTag> GetTextureTagConfig()
{
  return texture_tag_config;
}

std::span<const TextureTag> GetTextureTags()
{
  return texture_tag_enum;
}

std::vector<std::string> GetNamesFromDrawCallTags(GraphicsModSystem::DrawCallTags drawcall_tags)
{
  std::vector<std::string> result;
  for (std::size_t i = 0; i < draw_call_tag_config.size(); i++)
  {
    const auto& string_name = draw_call_tag_config[i].m_name;
    const auto& enum_value = draw_call_tag_enum[i];
    if (drawcall_tags[enum_value])
      result.push_back(string_name);
  }
  return result;
}

GraphicsModSystem::DrawCallTags GetDrawCallTagsFromNames(std::span<const std::string> names)
{
  GraphicsModSystem::DrawCallTags result;
  for (std::size_t i = 0; i < draw_call_tag_config.size(); i++)
  {
    const auto& string_name = draw_call_tag_config[i].m_name;
    const auto& enum_value = draw_call_tag_enum[i];
    if (std::ranges::find(names, string_name) != names.end())
      result[enum_value] = true;
  }
  return result;
}

std::vector<std::string> GetNamesFromTextureTags(GraphicsModSystem::TextureTags texture_tags)
{
  std::vector<std::string> result;
  for (std::size_t i = 0; i < texture_tag_config.size(); i++)
  {
    const auto& string_name = texture_tag_config[i].m_name;
    const auto& enum_value = texture_tag_enum[i];
    if (texture_tags[enum_value])
      result.push_back(string_name);
  }
  return result;
}

GraphicsModSystem::TextureTags GetTextureTagsFromNames(std::span<const std::string> names)
{
  GraphicsModSystem::TextureTags result;
  for (std::size_t i = 0; i < texture_tag_config.size(); i++)
  {
    const auto& string_name = texture_tag_config[i].m_name;
    const auto& enum_value = texture_tag_enum[i];
    if (std::ranges::find(names, string_name) != names.end())
      result[enum_value] = true;
  }
  return result;
}
}  // namespace GraphicsModSystem::Runtime
