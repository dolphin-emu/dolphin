// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

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
  virtual Common::Vec2 GetFieldOfView() const = 0;

  virtual void DoState(PointerWrap& p) = 0;

  virtual bool IsDirty() const = 0;
  virtual void SetClean() = 0;

  virtual bool SupportsInput() const = 0;

  virtual void UpdateConfig(const FreeLook::CameraConfig& config) = 0;
};

class CameraControllerInput : public CameraController
{
public:
  Common::Vec2 GetFieldOfView() const final override;

  void DoState(PointerWrap& p) override;

  bool IsDirty() const final override { return m_dirty; }
  void SetClean() final override { m_dirty = false; }

  bool SupportsInput() const final override { return true; }

  void UpdateConfig(const FreeLook::CameraConfig&) final override {}

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
  float m_fov_x = 1.0f;
  float m_fov_y = 1.0f;

  float m_min_fov_multiplier = 0.025f;
  float m_speed = 60.0f;
  bool m_dirty = false;
};

class CameraControllerGeneric : public CameraController
{
public:
  virtual void Update() = 0;

  bool SupportsInput() const final override { return false; }
};

class FreeLookCamera
{
public:
  FreeLookCamera();
  void UpdateConfig(const FreeLook::CameraConfig& config);
  Common::Matrix44 GetView() const;
  Common::Vec2 GetFieldOfView() const;

  void DoState(PointerWrap& p);

  bool IsActive() const;

  CameraController* GetController() const;

private:
  void SetControlType(FreeLook::ControlType type);
  bool m_dirty = false;
  std::optional<FreeLook::ControlType> m_current_type;
  std::unique_ptr<CameraController> m_camera_controller;
};

extern FreeLookCamera g_freelook_camera;
