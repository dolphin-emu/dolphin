// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoCommon/FreeLookCamera.h"

#include <algorithm>
#include <math.h>

#include <fmt/format.h>

#include "Common/MathUtil.h"

#include "Common/ChunkFile.h"
#include "Core/Config/GraphicsSettings.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "VideoCommon/VideoCommon.h"
#include "VideoCommon/VideoConfig.h"

FreeLookCamera g_freelook_camera;

namespace
{
std::string to_string(FreelookControlType type)
{
  switch (type)
  {
  case FreelookControlType::SixAxis:
    return "Six Axis";
  case FreelookControlType::FPS:
    return "First Person";
  case FreelookControlType::Orbital:
    return "Orbital";
  }

  return "";
}

class SixAxisController : public CameraController
{
public:
  SixAxisController() = default;

  Common::Matrix44 GetView() override { return m_mat; }

  void MoveVertical(float amt) override
  {
    m_mat = Common::Matrix44::Translate(Common::Vec3{0, amt, 0}) * m_mat;
  }

  void MoveHorizontal(float amt) override
  {
    m_mat = Common::Matrix44::Translate(Common::Vec3{amt, 0, 0}) * m_mat;
  }

  void Zoom(float amt) override
  {
    m_mat = Common::Matrix44::Translate(Common::Vec3{0, 0, amt}) * m_mat;
  }

  void Rotate(const Common::Vec3& amt) override
  {
    using Common::Matrix33;
    m_mat = Common::Matrix44::FromMatrix33(Matrix33::RotateX(amt.x) * Matrix33::RotateY(amt.y) *
                                           Matrix33::RotateZ(amt.z)) *
            m_mat;
  }

  void Reset() override { m_mat = Common::Matrix44::Identity(); }

  void DoState(PointerWrap& p) { p.Do(m_mat); }

private:
  Common::Matrix44 m_mat = Common::Matrix44::Identity();
};

constexpr double HalfPI = MathUtil::PI / 2;

class FPSController : public CameraController
{
public:
  Common::Matrix44 GetView() override
  {
    return m_rotate_mat * Common::Matrix44::Translate(m_position);
  }

  void MoveVertical(float amt) override
  {
    Common::Vec3 up{m_rotate_mat.data[4], m_rotate_mat.data[5], m_rotate_mat.data[6]};
    m_position += up * amt;
  }

  void MoveHorizontal(float amt) override
  {
    Common::Vec3 right{m_rotate_mat.data[0], m_rotate_mat.data[1], m_rotate_mat.data[2]};
    m_position += right * amt;
  }

  void Zoom(float amt) override
  {
    Common::Vec3 forward{m_rotate_mat.data[8], m_rotate_mat.data[9], m_rotate_mat.data[10]};
    m_position += forward * amt;
  }

  void Rotate(const Common::Vec3& amt) override
  {
    m_rotation += amt;

    using Common::Matrix33;
    using Common::Matrix44;
    m_rotate_mat =
        Matrix44::FromMatrix33(Matrix33::RotateX(m_rotation.x) * Matrix33::RotateY(m_rotation.y));
  }

  void Reset() override
  {
    m_position = Common::Vec3{};
    m_rotation = Common::Vec3{};
    m_rotate_mat = Common::Matrix44::Identity();
  }

  void DoState(PointerWrap& p)
  {
    p.Do(m_rotation);
    p.Do(m_rotate_mat);
    p.Do(m_position);
  }

private:
  Common::Vec3 m_rotation = Common::Vec3{};
  Common::Matrix44 m_rotate_mat = Common::Matrix44::Identity();
  Common::Vec3 m_position = Common::Vec3{};
};

class OrbitalController : public CameraController
{
public:
  Common::Matrix44 GetView() override
  {
    Common::Matrix44 result = Common::Matrix44::Identity();
    result *= Common::Matrix44::Translate(Common::Vec3{0, 0, -m_distance});
    result *= Common::Matrix44::FromMatrix33(Common::Matrix33::RotateX(m_rotation.x));
    result *= Common::Matrix44::FromMatrix33(Common::Matrix33::RotateY(m_rotation.y));

    return result;
  }

  void MoveVertical(float) override {}

  void MoveHorizontal(float) override {}

  void Zoom(float amt) override
  {
    m_distance += -1 * amt;
    m_distance = std::clamp(m_distance, 0.0f, m_distance);
  }

  void Rotate(const Common::Vec3& amt) override { m_rotation += amt; }

  void Reset() override
  {
    m_rotation = Common::Vec3{};
    m_distance = 0;
  }

  void DoState(PointerWrap& p)
  {
    p.Do(m_rotation);
    p.Do(m_distance);
  }

private:
  float m_distance = 0;
  Common::Vec3 m_rotation = Common::Vec3{};
};
}  // namespace

void FreeLookCamera::SetControlType(FreelookControlType type)
{
  if (m_current_type && *m_current_type == type)
  {
    return;
  }

  if (type == FreelookControlType::SixAxis)
  {
    m_camera_controller = std::make_unique<SixAxisController>();
  }
  else if (type == FreelookControlType::Orbital)
  {
    m_camera_controller = std::make_unique<OrbitalController>();
  }
  else
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

void FreeLookCamera::Zoom(float amt)
{
  m_camera_controller->Zoom(amt);
  m_dirty = true;
}

void FreeLookCamera::Rotate(const Common::Vec3& amt)
{
  m_camera_controller->Rotate(amt);
  m_dirty = true;
}

void FreeLookCamera::IncreaseFovX(float fov)
{
  m_fov_x += fov;
  m_fov_x = std::clamp(m_fov_x, m_fov_step_size, m_fov_x);
}

void FreeLookCamera::IncreaseFovY(float fov)
{
  m_fov_y += fov;
  m_fov_y = std::clamp(m_fov_y, m_fov_step_size, m_fov_y);
}

float FreeLookCamera::GetFovStepSize() const
{
  return m_fov_step_size;
}

void FreeLookCamera::Reset()
{
  m_camera_controller->Reset();
  m_fov_x = 1.0f;
  m_fov_y = 1.0f;
  m_dirty = true;
}

void FreeLookCamera::DoState(PointerWrap& p)
{
  if (p.mode == PointerWrap::MODE_WRITE || p.mode == PointerWrap::MODE_MEASURE)
  {
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
