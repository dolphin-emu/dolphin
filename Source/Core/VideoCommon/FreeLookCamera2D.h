// Copyright 2021 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#include "Common/Matrix.h"
#include "VideoCommon/TextureInfo.h"

class PointerWrap;

namespace FreeLook
{
struct CameraConfig2D;
}

class TextureLayer
{
public:
  TextureLayer() = default;
  explicit TextureLayer(const std::string& layer_name, std::vector<std::string> texture_names);
  void DoState(PointerWrap& p);

  void Reset();

  bool Matches(const std::vector<std::string>& names) const;

  const Common::Vec2& GetPositionOffset() const;
  const Common::Vec2& GetStretchMultiplier() const;

  Common::Vec2& GetPositionOffset();
  Common::Vec2& GetStretchMultiplier();

  const std::vector<std::string>& GetNames() const { return m_texture_names_ordered; }
  const std::string& GetLayerName() const { return m_layer_name; }

private:
  static constexpr Common::Vec2 DEFAULT_STRETCH_MULTIPLIER = Common::Vec2{1, 1};
  Common::Vec2 m_stretch_multiplier = DEFAULT_STRETCH_MULTIPLIER;
  Common::Vec2 m_position_offset;

  std::string m_layer_name;
  std::unordered_set<std::string> m_texture_names;
  std::vector<std::string> m_texture_names_ordered;

  std::vector<std::string> m_texture_names_with_wildcards;
};

class CameraController2D
{
public:
  CameraController2D() = default;
  virtual ~CameraController2D() = default;
  CameraController2D(const CameraController2D&) = delete;
  CameraController2D& operator=(const CameraController2D&) = delete;
  CameraController2D(CameraController2D&&) = delete;
  CameraController2D& operator=(CameraController2D&&) = delete;

  void UpdateConfig(const FreeLook::CameraConfig2D& config);

  const Common::Vec2& GetPositionOffset(const std::vector<std::string>& names) const;
  const Common::Vec2& GetStretchMultiplier(const std::vector<std::string>& names) const;

  virtual void DoState(PointerWrap& p);

  virtual bool IsDirty() const = 0;
  virtual void SetClean() = 0;

  virtual bool SupportsInput() const = 0;

private:
  std::vector<TextureLayer> m_layers;

protected:
  void SetLayer(std::size_t layer);
  std::size_t LayerCount() const;

  TextureLayer* m_current_layer;
};

class CameraController2DInput : public CameraController2D
{
public:
  virtual void MoveVertical(float amt) = 0;
  virtual void MoveHorizontal(float amt) = 0;

  void Reset();

  void IncreaseStretchX(float amt);
  void IncreaseStretchY(float amt);

  void IncrementLayer();
  void DecrementLayer();

  void DoState(PointerWrap& p) override;

  bool IsDirty() const final override { return true; }
  void SetClean() final override { m_dirty = false; }

  bool SupportsInput() const final override { return true; }

  void ModifySpeed(float multiplier);
  void ResetSpeed();
  float GetSpeed() const;

  float GetStretchMultiplierSize() const;

protected:
  bool m_dirty = false;

private:
  std::size_t m_current_layer_index;
  static constexpr float DEFAULT_SPEED = 1.0f;

  float m_speed = DEFAULT_SPEED;
};

class FreeLookCamera2D
{
public:
  FreeLookCamera2D();
  void UpdateConfig(const FreeLook::CameraConfig2D& config);

  const Common::Vec2& GetPositionOffset(const std::vector<std::string>& names) const;
  const Common::Vec2& GetStretchMultiplier(const std::vector<std::string>& names) const;

  CameraController2D* GetController() const;

  bool IsActive() const;

  void DoState(PointerWrap& p);

private:
  std::unique_ptr<CameraController2D> m_controller;
};

extern FreeLookCamera2D g_freelook_camera_2d;
