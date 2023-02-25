// Copyright 2020 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>
#include <optional>

#include "Common/Matrix.h"
#include "Core/FreeLookConfig.h"

class PointerWrap;

class CameraController
{
public:
  CameraController() = default;
  virtual ~CameraController() = default;

  CameraController(const CameraController&) = delete;
  CameraController& operator=(const CameraController&) = delete;

  CameraController(CameraController&&) = delete;
  CameraController& operator=(CameraController&&) = delete;

  virtual Common::Matrix44 GetView() const = 0;
  virtual Common::Vec2 GetFieldOfViewMultiplier() const = 0;

  virtual void DoState(PointerWrap& p) = 0;

  virtual bool IsDirty() const = 0;
  virtual void SetClean() = 0;

  virtual bool SupportsInput() const = 0;
};

class CameraControllerInput : public CameraController
{
public:
  Common::Vec2 GetFieldOfViewMultiplier() const final override;

  void DoState(PointerWrap& p) override;

  bool IsDirty() const final override { return m_dirty; }
  void SetClean() final override { m_dirty = false; }

  bool SupportsInput() const final override { return true; }

  virtual void MoveVertical(float amt) = 0;
  virtual void MoveHorizontal(float amt) = 0;

  virtual void MoveForward(float amt) = 0;

  virtual void Rotate(const Common::Vec3& amt) = 0;
  virtual void Rotate(const Common::Quaternion& quat) = 0;

  virtual void Reset() = 0;

  void IncreaseFovX(float fov);
  void IncreaseFovY(float fov);
  float GetFovStepSize() const;

  void ModifySpeed(float multiplier);
  void ResetSpeed();
  float GetSpeed() const;

private:
  static constexpr float MIN_FOV_MULTIPLIER = 0.025f;
  static constexpr float DEFAULT_SPEED = 60.0f;
  static constexpr float DEFAULT_FOV_MULTIPLIER = 1.0f;

  float m_fov_x_multiplier = DEFAULT_FOV_MULTIPLIER;
  float m_fov_y_multiplier = DEFAULT_FOV_MULTIPLIER;
  float m_speed = DEFAULT_SPEED;
  bool m_dirty = false;
};

class FreeLookCamera
{
public:
  FreeLookCamera();
  void SetControlType(FreeLook::ControlType type);
  Common::Matrix44 GetView() const;
  Common::Vec2 GetFieldOfViewMultiplier() const;

  void DoState(PointerWrap& p);

  bool IsActive() const;

  CameraController* GetController() const;

private:
  std::optional<FreeLook::ControlType> m_current_type;
  std::unique_ptr<CameraController> m_camera_controller;
};

extern FreeLookCamera g_freelook_camera;
