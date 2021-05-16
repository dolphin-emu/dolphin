// Copyright 2021 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>

#include "Common/Matrix.h"

class PointerWrap;

namespace FreeLook
{
struct CameraConfig2D;
}

class CameraController2D
{
public:
  CameraController2D() = default;
  virtual ~CameraController2D() = default;
  CameraController2D(const CameraController2D&) = delete;
  CameraController2D& operator=(const CameraController2D&) = delete;
  CameraController2D(CameraController2D&&) = delete;
  CameraController2D& operator=(CameraController2D&&) = delete;

  virtual void UpdateConfig(const FreeLook::CameraConfig2D& config) = 0;

  virtual const Common::Vec2& GetPositionOffset() const = 0;
  virtual const Common::Vec2& GetStretchMultiplier() const = 0;

  virtual void DoState(PointerWrap& p) = 0;

  virtual bool IsDirty() const = 0;
  virtual void SetClean() = 0;

  virtual bool SupportsInput() const = 0;
};

class CameraController2DInput : public CameraController2D
{
public:
  const Common::Vec2& GetStretchMultiplier() const final override;

  virtual void MoveVertical(float amt) = 0;
  virtual void MoveHorizontal(float amt) = 0;

  virtual void Reset();

  void IncreaseStretchX(float amt);
  void IncreaseStretchY(float amt);

  void DoState(PointerWrap& p) override;

  bool IsDirty() const final override { return m_dirty; }
  void SetClean() final override { m_dirty = false; }

  bool SupportsInput() const final override { return true; }

  void ModifySpeed(float multiplier);
  void ResetSpeed();
  float GetSpeed() const;

  float GetStretchMultiplierSize() const;

protected:
  bool m_dirty = false;

private:
  static constexpr float DEFAULT_SPEED = 1.0f;
  static constexpr Common::Vec2 DEFAULT_STRETCH_MULTIPLIER = Common::Vec2{1, 1};
  Common::Vec2 m_stretch_multiplier = DEFAULT_STRETCH_MULTIPLIER;

  float m_speed = DEFAULT_SPEED;
};

class FreeLookCamera2D
{
public:
  FreeLookCamera2D();
  void UpdateConfig(const FreeLook::CameraConfig2D& config);

  const Common::Vec2& GetPositionOffset() const;
  const Common::Vec2& GetStretchMultiplier() const;

  CameraController2D* GetController() const;

  bool IsActive() const;

  void DoState(PointerWrap& p);

private:
  std::unique_ptr<CameraController2D> m_controller;
};

extern FreeLookCamera2D g_freelook_camera_2d;
