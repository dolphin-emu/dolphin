// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoCommon/FreeLookCamera.h"

#include <algorithm>
#include <math.h>

#include <fmt/format.h>

#include "Common/MathUtil.h"

#include "Common/ChunkFile.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"

#include "VideoCommon/VideoCommon.h"

FreeLookCamera g_freelook_camera;

namespace
{
std::string to_string(FreeLook::ControlType type)
{
  switch (type)
  {
  case FreeLook::ControlType::SixAxis:
    return "Six Axis";
  case FreeLook::ControlType::FPS:
    return "First Person";
  case FreeLook::ControlType::Orbital:
    return "Orbital";
  }

  return "";
}

class SixAxisController final : public CameraController
{
public:
  SixAxisController() = default;

  Common::Matrix44 GetView() override
  {
    return Common::Matrix44::FromQuaternion(m_rotate_quat) *
           Common::Matrix44::Translate(m_position + m_offset);
  }

  void MoveVertical(float amt) override
  {
    m_position += m_rotate_quat.Conjugate() * Common::Vec3{0, amt, 0};
  }

  void MoveHorizontal(float amt) override
  {
    m_position += m_rotate_quat.Conjugate() * Common::Vec3{amt, 0, 0};
  }

  void MoveForward(float amt) override
  {
    m_position += m_rotate_quat.Conjugate() * Common::Vec3{0, 0, amt};
  }

  void SetPositionOffset(const Common::Vec3& offset) override { m_offset = offset; }

  void Rotate(const Common::Vec3& amt) override { Rotate(Common::Quaternion::RotateXYZ(amt)); }

  void Rotate(const Common::Quaternion& quat) override
  {
    m_rotate_quat = (quat * m_rotate_quat).Normalized();
  }

  void Reset() override
  {
    m_rotate_quat = Common::Quaternion::Identity();
    m_position = Common::Vec3{};
  }

  void DoState(PointerWrap& p) override
  {
    p.Do(m_rotate_quat);
    p.Do(m_position);
    p.Do(m_offset);
  }

private:
  Common::Quaternion m_rotate_quat = Common::Quaternion::Identity();
  Common::Vec3 m_position = Common::Vec3{};
  Common::Vec3 m_offset = Common::Vec3{};
};

class FPSController final : public CameraController
{
public:
  Common::Matrix44 GetView() override
  {
    return Common::Matrix44::FromQuaternion(m_rotate_quat) *
           Common::Matrix44::Translate(m_position + m_offset);
  }

  void MoveVertical(float amt) override
  {
    const Common::Vec3 up = m_rotate_quat.Conjugate() * Common::Vec3{0, 1, 0};
    m_position += up * amt;
  }

  void MoveHorizontal(float amt) override
  {
    const Common::Vec3 right = m_rotate_quat.Conjugate() * Common::Vec3{1, 0, 0};
    m_position += right * amt;
  }

  void MoveForward(float amt) override
  {
    const Common::Vec3 forward = m_rotate_quat.Conjugate() * Common::Vec3{0, 0, 1};
    m_position += forward * amt;
  }

  void SetPositionOffset(const Common::Vec3& offset) override { m_offset = offset; }

  void Rotate(const Common::Vec3& amt) override
  {
    if (amt.Length() == 0)
      return;

    m_rotation += amt;

    using Common::Quaternion;
    m_rotate_quat =
        (Quaternion::RotateX(m_rotation.x) * Quaternion::RotateY(m_rotation.y)).Normalized();
  }

  void Rotate(const Common::Quaternion& quat) override
  {
    Rotate(Common::FromQuaternionToEuler(quat));
  }

  void Reset() override
  {
    m_position = Common::Vec3{};
    m_rotation = Common::Vec3{};
    m_rotate_quat = Common::Quaternion::Identity();
  }

  void DoState(PointerWrap& p) override
  {
    p.Do(m_rotation);
    p.Do(m_rotate_quat);
    p.Do(m_position);
    p.Do(m_offset);
  }

private:
  Common::Vec3 m_rotation = Common::Vec3{};
  Common::Quaternion m_rotate_quat = Common::Quaternion::Identity();
  Common::Vec3 m_position = Common::Vec3{};
  Common::Vec3 m_offset = Common::Vec3{};
};

class OrbitalController final : public CameraController
{
public:
  Common::Matrix44 GetView() override
  {
    return Common::Matrix44::Translate(Common::Vec3{0, 0, -m_distance}) *
           Common::Matrix44::FromQuaternion(m_rotate_quat) *
           Common::Matrix44::Translate(m_focal_point);
  }

  void MoveVertical(float) override {}

  void MoveHorizontal(float) override {}

  void MoveForward(float amt) override
  {
    m_distance += -1 * amt;
    m_distance = std::clamp(m_distance, 0.0f, m_distance);
  }

  void SetPositionOffset(const Common::Vec3& offset) override { m_focal_point = offset; }

  void Rotate(const Common::Vec3& amt) override
  {
    if (amt.Length() == 0)
      return;

    m_rotation += amt;

    using Common::Quaternion;
    m_rotate_quat =
        (Quaternion::RotateX(m_rotation.x) * Quaternion::RotateY(m_rotation.y)).Normalized();
  }

  void Rotate(const Common::Quaternion& quat) override
  {
    Rotate(Common::FromQuaternionToEuler(quat));
  }

  void Reset() override
  {
    m_rotation = Common::Vec3{};
    m_rotate_quat = Common::Quaternion::Identity();
    m_distance = 0;
    m_focal_point = Common::Vec3{};
  }

  void DoState(PointerWrap& p) override
  {
    p.Do(m_rotation);
    p.Do(m_rotate_quat);
    p.Do(m_distance);
    p.Do(m_focal_point);
  }

private:
  float m_distance = 0;
  Common::Vec3 m_rotation = Common::Vec3{};
  Common::Quaternion m_rotate_quat = Common::Quaternion::Identity();
  Common::Vec3 m_focal_point = Common::Vec3{};
};
}  // namespace

FreeLookCamera::FreeLookCamera()
{
  SetControlType(FreeLook::ControlType::SixAxis);
}

void FreeLookCamera::SetControlType(FreeLook::ControlType type)
{
  if (m_current_type && *m_current_type == type)
  {
    return;
  }

  if (type == FreeLook::ControlType::SixAxis)
  {
    m_camera_controller = std::make_unique<SixAxisController>();
  }
  else if (type == FreeLook::ControlType::Orbital)
  {
    m_camera_controller = std::make_unique<OrbitalController>();
  }
  else if (type == FreeLook::ControlType::FPS)
  {
    m_camera_controller = std::make_unique<FPSController>();
  }

  m_current_type = type;
}

Common::Matrix44 FreeLookCamera::GetView()
{
  return m_camera_controller->GetView();
}

Common::Vec2 FreeLookCamera::GetFieldOfView() const
{
  return Common::Vec2{m_fov_x, m_fov_y};
}

void FreeLookCamera::MoveVertical(float amt)
{
  m_camera_controller->MoveVertical(amt);
  m_dirty = true;
}

void FreeLookCamera::MoveHorizontal(float amt)
{
  m_camera_controller->MoveHorizontal(amt);
  m_dirty = true;
}

void FreeLookCamera::MoveForward(float amt)
{
  m_camera_controller->MoveForward(amt);
  m_dirty = true;
}

void FreeLookCamera::SetPositionOffset(const Common::Vec3& offset)
{
  m_camera_controller->SetPositionOffset(offset);
  m_dirty = true;
}

void FreeLookCamera::Rotate(const Common::Vec3& amt)
{
  m_camera_controller->Rotate(amt);
  m_dirty = true;
}

void FreeLookCamera::Rotate(const Common::Quaternion& amt)
{
  m_camera_controller->Rotate(amt);
  m_dirty = true;
}

void FreeLookCamera::IncreaseFovX(float fov)
{
  m_fov_x += fov;
  m_fov_x = std::clamp(m_fov_x, m_min_fov_multiplier, m_fov_x);
}

void FreeLookCamera::IncreaseFovY(float fov)
{
  m_fov_y += fov;
  m_fov_y = std::clamp(m_fov_y, m_min_fov_multiplier, m_fov_y);
}

float FreeLookCamera::GetFovStepSize() const
{
  return 1.5f;
}

void FreeLookCamera::Reset()
{
  m_camera_controller->Reset();
  m_fov_x = 1.0f;
  m_fov_y = 1.0f;
  m_dirty = true;
}

void FreeLookCamera::ModifySpeed(float amt)
{
  m_speed += amt;
  m_speed = std::clamp(m_speed, 0.0f, m_speed);
}

void FreeLookCamera::ResetSpeed()
{
  m_speed = 60.0f;
}

float FreeLookCamera::GetSpeed() const
{
  return m_speed;
}

void FreeLookCamera::DoState(PointerWrap& p)
{
  if (p.mode == PointerWrap::MODE_WRITE || p.mode == PointerWrap::MODE_MEASURE)
  {
    p.Do(m_speed);
    p.Do(m_current_type);
    p.Do(m_fov_x);
    p.Do(m_fov_y);
    if (m_camera_controller)
    {
      m_camera_controller->DoState(p);
    }
  }
  else
  {
    const auto old_type = m_current_type;
    p.Do(m_speed);
    p.Do(m_current_type);
    p.Do(m_fov_x);
    p.Do(m_fov_y);
    if (old_type == m_current_type)
    {
      m_camera_controller->DoState(p);
    }
    else if (p.GetMode() == PointerWrap::MODE_READ)
    {
      const std::string old_type_name = old_type ? to_string(*old_type) : "";
      const std::string loaded_type_name = m_current_type ? to_string(*m_current_type) : "";
      const std::string message =
          fmt::format("State needs same free look camera type. Settings value '{}', loaded value "
                      "'{}'.  Aborting load state",
                      old_type_name, loaded_type_name);
      Core::DisplayMessage(message, 5000);
      p.SetMode(PointerWrap::MODE_VERIFY);
    }
  }
}

bool FreeLookCamera::IsDirty() const
{
  return m_dirty;
}

void FreeLookCamera::SetClean()
{
  m_dirty = false;
}

bool FreeLookCamera::IsActive() const
{
  return FreeLook::GetActiveConfig().enabled;
}
