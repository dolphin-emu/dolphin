// Copyright 2021 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoCommon/FreeLookCamera2D.h"

#include <algorithm>
#include <math.h>

#include <fmt/format.h>

#include "Common/MathUtil.h"

#include "Common/ChunkFile.h"
#include "Common/StringUtil.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/FreeLookConfig.h"

#include "VideoCommon/VideoCommon.h"

FreeLookCamera2D g_freelook_camera_2d;

template <>
struct fmt::formatter<TextureLayer>
{
  constexpr auto parse(format_parse_context& ctxt) { return ctxt.begin(); }

  template <typename FormatContext>
  auto format(const TextureLayer& layer, FormatContext& ctxt)
  {
    return fmt::format_to(
        ctxt.out(), fmt::format("{}:{}", layer.GetLayerName(), fmt::join(layer.GetNames(), ",")));
  }
};

namespace
{
class BasicMovementController final : public CameraController2DInput
{
  void MoveVertical(float amt) override
  {
    if (m_current_layer)
    {
      m_current_layer->GetPositionOffset().y += amt;
      m_dirty = true;
    }
  }

  void MoveHorizontal(float amt) override
  {
    if (m_current_layer)
    {
      m_current_layer->GetPositionOffset().x += amt;
      m_dirty = true;
    }
  }
};

bool wildcard_compare(const std::string& wildcard_texture, const std::string& full_texture)
{
  std::size_t wildcard_index = 0;
  for (const char& c : full_texture)
  {
    const char wildcard_char = wildcard_texture[wildcard_index];
    wildcard_index++;
    if (wildcard_char == '*')
    {
      break;
    }
    else if (wildcard_char != '?' && wildcard_char != c)
    {
      return false;
    }
  }

  return true;
}
}  // namespace

TextureLayer::TextureLayer(const std::string& layer_name, std::vector<std::string> texture_names)
    : m_layer_name(layer_name), m_texture_names_ordered(std::move(texture_names))
{
  for (const auto& name : m_texture_names_ordered)
  {
    auto iter = name.find_first_of("?*");
    if (iter == std::string::npos)
    {
      m_texture_names.insert(name);
    }
    else
    {
      m_texture_names_with_wildcards.emplace_back(name);
    }
  }
}

void TextureLayer::DoState(PointerWrap& p)
{
  p.Do(m_layer_name);
  p.Do(m_texture_names_ordered);
  p.Do(m_stretch_multiplier);
  p.Do(m_position_offset);

  if (p.GetMode() == PointerWrap::MODE_READ)
  {
    for (const auto& name : m_texture_names_ordered)
    {
      m_texture_names.insert(name);
    }
  }
}

void TextureLayer::Reset()
{
  m_stretch_multiplier = DEFAULT_STRETCH_MULTIPLIER;
  m_position_offset = Common::Vec2{};
}

bool TextureLayer::Matches(const std::vector<std::string>& names) const
{
  return std::any_of(names.begin(), names.end(), [this](const std::string& name) {
    if (m_texture_names.count(name) > 0)
      return true;

    for (const auto& wildcard_texture : m_texture_names_with_wildcards)
    {
      if (wildcard_compare(wildcard_texture, name))
        return true;
    }

    return false;
  });
}

const Common::Vec2& TextureLayer::GetPositionOffset() const
{
  return m_position_offset;
}

const Common::Vec2& TextureLayer::GetStretchMultiplier() const
{
  return m_stretch_multiplier;
}

Common::Vec2& TextureLayer::GetPositionOffset()
{
  return m_position_offset;
}

Common::Vec2& TextureLayer::GetStretchMultiplier()
{
  return m_stretch_multiplier;
}

void CameraController2D::UpdateConfig(const FreeLook::CameraConfig2D& config)
{
  const std::string old_layers_string = fmt::to_string(fmt::join(m_layers, ";"));
  if (config.layers != old_layers_string)
  {
    m_layers.clear();
    m_current_layer = nullptr;
    const std::vector<std::string> layers = SplitString(config.layers, ';');
    if (!layers.empty())
    {
      for (const std::string& layer : layers)
      {
        const std::vector<std::string> layer_details = SplitString(layer, ':');
        const std::vector<std::string> textures = SplitString(layer_details[1], ',');
        std::vector<std::string> textures_stripped;
        textures_stripped.reserve(textures.size());
        for (const std::string& texture : textures)
        {
          textures_stripped.push_back(std::string{StripSpaces(texture)});
        }
        m_layers.emplace_back(layer_details[0], std::move(textures_stripped));
      }
      m_current_layer = &m_layers[0];
    }
  }
}

const Common::Vec2&
CameraController2D::GetPositionOffset(const std::vector<std::string>& names) const
{
  for (const auto& layer : m_layers)
  {
    if (layer.Matches(names))
      return layer.GetPositionOffset();
  }

  static Common::Vec2 def_pos_offset;
  return def_pos_offset;
}

const Common::Vec2&
CameraController2D::GetStretchMultiplier(const std::vector<std::string>& names) const
{
  for (const auto& layer : m_layers)
  {
    if (layer.Matches(names))
      return layer.GetStretchMultiplier();
  }

  static Common::Vec2 def_stretch_multiplier = Common::Vec2{1, 1};
  return def_stretch_multiplier;
}

void CameraController2D::DoState(PointerWrap& p)
{
  p.DoEachElement(m_layers, [](PointerWrap& pw, TextureLayer& layer) { layer.DoState(pw); });
}

void CameraController2D::SetLayer(std::size_t layer)
{
  if (layer < LayerCount())
  {
    m_current_layer = &m_layers[layer];
    static constexpr int display_message_ms = 3000;
    Core::DisplayMessage(
        fmt::format("FreeLook 2d camera texture layer '{}' set", m_current_layer->GetLayerName()),
        display_message_ms);
  }
}

std::size_t CameraController2D::LayerCount() const
{
  return m_layers.size();
}

void CameraController2DInput::IncreaseStretchX(float amt)
{
  if (m_current_layer)
  {
    m_current_layer->GetStretchMultiplier().x += amt;
  }
}

void CameraController2DInput::IncreaseStretchY(float amt)
{
  if (m_current_layer)
  {
    m_current_layer->GetStretchMultiplier().y += amt;
  }
}

void CameraController2DInput::IncrementLayer()
{
  m_current_layer_index++;
  if (m_current_layer_index >= LayerCount())
    m_current_layer_index = 0;
  SetLayer(m_current_layer_index);
}

void CameraController2DInput::DecrementLayer()
{
  m_current_layer_index--;
  if (m_current_layer_index < 0)
    m_current_layer_index = LayerCount() - 1;
  SetLayer(m_current_layer_index);
}

float CameraController2DInput::GetStretchMultiplierSize() const
{
  return 1.0f;
}

void CameraController2DInput::Reset()
{
  if (m_current_layer)
  {
    m_current_layer->Reset();
  }
  m_speed = DEFAULT_SPEED;
  m_dirty = true;
}

void CameraController2DInput::ModifySpeed(float amt)
{
  m_speed += amt;
  m_speed = std::max(m_speed, 0.0f);
}

void CameraController2DInput::ResetSpeed()
{
  m_speed = DEFAULT_SPEED;
}

float CameraController2DInput::GetSpeed() const
{
  return m_speed;
}

void CameraController2DInput::DoState(PointerWrap& p)
{
  CameraController2D::DoState(p);
  p.Do(m_speed);
}

FreeLookCamera2D::FreeLookCamera2D()
{
  m_controller = std::make_unique<BasicMovementController>();
}

void FreeLookCamera2D::UpdateConfig(const FreeLook::CameraConfig2D& config)
{
  m_controller->UpdateConfig(config);
}

const Common::Vec2& FreeLookCamera2D::GetPositionOffset(const std::vector<std::string>& names) const
{
  return m_controller->GetPositionOffset(names);
}

const Common::Vec2&
FreeLookCamera2D::GetStretchMultiplier(const std::vector<std::string>& names) const
{
  return m_controller->GetStretchMultiplier(names);
}

CameraController2D* FreeLookCamera2D::GetController() const
{
  return m_controller.get();
}

bool FreeLookCamera2D::IsActive() const
{
  return FreeLook::GetActiveConfig().enabled_2d;
}

void FreeLookCamera2D::DoState(PointerWrap& p)
{
  m_controller->DoState(p);
}
