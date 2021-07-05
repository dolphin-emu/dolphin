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

  virtual Common::Matrix44 GetView() = 0;

  virtual void MoveVertical(float amt) = 0;
  virtual void MoveHorizontal(float amt) = 0;

  virtual void MoveForward(float amt) = 0;

  virtual void Rotate(const Common::Vec3& amt) = 0;
  virtual void Rotate(const Common::Quaternion& quat) = 0;

  virtual void Reset() = 0;

  virtual void DoState(PointerWrap& p) = 0;
};

class FreeLookCamera
{
public:
  FreeLookCamera();
  void SetControlType(FreeLook::ControlType type);
  Common::Matrix44 GetView();
  Common::Vec2 GetFieldOfView() const;

  void MoveVertical(float amt);
  void MoveHorizontal(float amt);
  void MoveForward(float amt);

  void Rotate(const Common::Vec3& amt);
  void Rotate(const Common::Quaternion& amt);

  void IncreaseFovX(float fov);
  void IncreaseFovY(float fov);
  float GetFovStepSize() const;

  void Reset();

  void DoState(PointerWrap& p);

  bool IsDirty() const;
  void SetClean();

  void ModifySpeed(float multiplier);
  void ResetSpeed();
  float GetSpeed() const;

  bool IsActive() const;

private:
  bool m_dirty = false;
  float m_fov_x = 1.0f;
  float m_fov_y = 1.0f;
  std::optional<FreeLook::ControlType> m_current_type;
  std::unique_ptr<CameraController> m_camera_controller;

  float m_min_fov_multiplier = 0.025f;
  float m_speed = 60.0f;
};

extern FreeLookCamera g_freelook_camera;
