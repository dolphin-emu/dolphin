// Copyright 2020 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/FreeLookCamera.h"

#include <algorithm>
#include <math.h>

#include <fmt/format.h>

#include "Common/MathUtil.h"

#include "Common/ChunkFile.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"

#include "VideoCommon/VideoCommon.h"

#include <iostream>

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
  case FreeLook::ControlType::RioCam:
    return "Rio Cam";
  }
  return "";
}

class RioCamController final : public CameraControllerInput
{
public:
  Common::Matrix44 GetView() const override
  {
    if (use_mat) { return m_mat; }
    return Common::Matrix44::FromQuaternion(m_rotate_quat) *
           Common::Matrix44::Translate(m_position);
  }

  void MoveVertical(float amt) override
  {
    if (amt > 0){ // Dugout
      use_mat = true;
      m_mat = Common::Matrix44::FromArray({0.545977f,-0.253666f,-0.798475f,-16.638653f,
                                           -0.061070f,0.938476f,-0.339901f,-1.409028f,
                                           0.835570f,0.234341f,0.496894f,-26.413183f,
                                           0.000000f,0.000000f,0.000000f,1.000000f});

    }
    else { //LF Pole
      use_mat = true;
      m_mat = Common::Matrix44::FromArray({-0.542877f,-0.241105f,-0.804458f,-26.235294f,
                                           0.000098f,0.957884f,-0.287154f,-1.449248f,
                                           0.839813f,-0.155968f,-0.519989f,-83.894585f,
                                           0.000000f,0.000000f,0.000000f,1.000000f});
    }
  }

  void MoveHorizontal(float amt) override
  {
    if (amt > 0){ //Directly behind HP
      use_mat = false;
      m_rotation.x = 0.357953f;
      m_rotation.y = 0.000000f;
      m_rotation.z = 0.000000f;

      m_position.x = 0.000000f;
      m_position.y = -4.889825f;
      m_position.z = 0.288412f;
    }
    else { //RF Grandstand
      use_mat = true;
      m_mat = Common::Matrix44::FromArray({-0.762376f,0.180257f,0.621523f,24.537916f,
                                           -0.111071f,0.909723f,-0.400084f,-5.326676f,
                                           -0.637532f,-0.374047f,-0.673530f,-115.002136f,
                                           0.000000f,0.000000f,0.000000f,1.000000f});
    }

    using Common::Quaternion;
    m_rotate_quat =
        (Quaternion::RotateX(m_rotation.x) * Quaternion::RotateY(m_rotation.y)).Normalized();
  }

  void MoveForward(float amt) override
  {
    use_mat = false;
    if (amt > 0){ // Over the pitcher shoulder
      m_rotation.x = -0.333371f;
      m_rotation.y =  3.147738f;
      m_rotation.z = -0.000247f;
      m_position.x =  2.287449f;
      m_position.y =  7.688197f;
      m_position.z = 37.479488f;
    }
    else{ // High above the plate, looking down
      m_rotation.x = -0.081675f;
      m_rotation.y = 0.000000f;
      m_rotation.z = 0.000000f;

      m_position.x = 0.000000f;
      m_position.y = 6.358200f;
      m_position.z = -19.586224f;
    }
    using Common::Quaternion;
    m_rotate_quat =
        (Quaternion::RotateX(m_rotation.x) * Quaternion::RotateY(m_rotation.y)).Normalized();
  }

  void Rotate(const Common::Vec3& amt) override
  {
    if (amt.Length() == 0)
      return;
    
    use_mat = true;
    m_mat = Common::Matrix44::FromArray({-0.518640f,0.215621f,0.827358f,28.380379f,
                                         -0.149931f,0.929747f,-0.336291f,-12.713995f,
                                         -0.841743f,-0.298461f,-0.449876f,-82.801498f,
                                         0.000000f,0.000000f,0.000000f,1.000000f});
  }

  void Rotate(const Common::Quaternion& quat) override
  {
    Rotate(Common::FromQuaternionToEuler(quat));
  }

  void Reset() override
  {
    use_mat = false;
    CameraControllerInput::Reset();
    m_position = Common::Vec3{};
    m_rotation = Common::Vec3{};
    m_rotate_quat = Common::Quaternion::Identity();
  }

  void DoState(PointerWrap& p) override
  {
    CameraControllerInput::DoState(p);
    p.Do(m_rotation);
    p.Do(m_rotate_quat);
    p.Do(m_position);
  }

private:
  Common::Vec3 m_rotation = Common::Vec3{};
  Common::Quaternion m_rotate_quat = Common::Quaternion::Identity();
  Common::Vec3 m_position = Common::Vec3{};

  Common::Vec3 m_rotation_print = Common::Vec3{};
  Common::Vec3 m_position_print = Common::Vec3{};

  //Matrix used to get certain views
  Common::Matrix44 m_mat = Common::Matrix44::Identity();
  bool use_mat = false;
};

//=============================================================================================

class SixAxisController final : public CameraControllerInput
{
public:
  SixAxisController() = default;

  Common::Matrix44 GetView() const override { return m_mat; }

  void MoveVertical(float amt) override
  {
    m_mat = Common::Matrix44::Translate(Common::Vec3{0, amt, 0}) * m_mat;

    //std::cout << "POSITION: X=" << std::to_string(m_position.x) << " Y=" << std::to_string(m_position.y) << " Z=" << std::to_string(m_position.z) << std::endl; 
  }

  void MoveHorizontal(float amt) override
  {
    m_mat = Common::Matrix44::Translate(Common::Vec3{amt, 0, 0}) * m_mat;


    //std::cout << "POSITION: X=" << std::to_string(m_position.x) << " Y=" << std::to_string(m_position.y) << " Z=" << std::to_string(m_position.z) << std::endl; 
  }

  void MoveForward(float amt) override
  {
    m_mat = Common::Matrix44::Translate(Common::Vec3{0, 0, amt}) * m_mat;

    //std::cout << "POSITION: X=" << std::to_string(m_position.x) << " Y=" << std::to_string(m_position.y) << " Z=" << std::to_string(m_position.z) << std::endl; 
  }

  void Rotate(const Common::Vec3& amt) override { Rotate(Common::Quaternion::RotateXYZ(amt)); }

  void Rotate(const Common::Quaternion& quat) override
  {
    Common::Matrix44 m_mat_prev = m_mat;
    m_mat = Common::Matrix44::FromQuaternion(quat) * m_mat;

    if (!(m_mat == m_mat_prev)){
      for (int x = 0; x < 16; ++x){
        if (x == 0) {
          std::cout << " Matrix Data = {";
        }
        std::cout << std::to_string(m_mat.data[x]) << ",";
        if (x == 15) {
          std::cout << " }" << std::endl;
        }
      }
    }
  }

  void Reset() override
  {
    CameraControllerInput::Reset();
    m_mat = Common::Matrix44::Identity();
  }

  void DoState(PointerWrap& p) override
  {
    CameraControllerInput::DoState(p);
    p.Do(m_mat);
  }

private:
  Common::Matrix44 m_mat = Common::Matrix44::Identity();
};

class FPSController final : public CameraControllerInput
{
public:
  Common::Matrix44 GetView() const override
  {
    return Common::Matrix44::FromQuaternion(m_rotate_quat) *
           Common::Matrix44::Translate(m_position);
  }

  void MoveVertical(float amt) override
  {
    const Common::Vec3 up = m_rotate_quat.Conjugate() * Common::Vec3{0, 1, 0};
    m_position += up * amt;

    if ((m_rotation.x != m_rotation_print.x) || (m_position.x != m_position_print.x)){
      std::cout << "Position: " << std::endl; 
      std::cout << "    m_position.x = " << std::to_string(m_position.x) << ";" << std::endl;
      std::cout << "    m_position.y = " << std::to_string(m_position.y) << ";" << std::endl;
      std::cout << "    m_position.z = " << std::to_string(m_position.z) << ";" << std::endl; 

      std::cout << "Rotation: " << std::endl; 
      std::cout << "    m_rotation.x = " << std::to_string(m_rotation.x) << ";" << std::endl;
      std::cout << "    m_rotation.y = " << std::to_string(m_rotation.y) << ";" << std::endl;
      std::cout << "    m_rotation.z = " << std::to_string(m_rotation.z) << ";" << std::endl;
      std::cout << std::endl;

      m_rotation_print.x = m_rotation.x;
      m_rotation_print.y = m_rotation.y;
      m_rotation_print.z = m_rotation.z;

      m_position_print.x = m_position.x;
      m_position_print.y = m_position.y;
      m_position_print.z = m_position.z;
    }
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
    CameraControllerInput::Reset();
    m_position = Common::Vec3{};
    m_rotation = Common::Vec3{};
    m_rotate_quat = Common::Quaternion::Identity();
  }

  void DoState(PointerWrap& p) override
  {
    CameraControllerInput::DoState(p);
    p.Do(m_rotation);
    p.Do(m_rotate_quat);
    p.Do(m_position);
  }

private:
  Common::Vec3 m_rotation = Common::Vec3{};
  Common::Quaternion m_rotate_quat = Common::Quaternion::Identity();
  Common::Vec3 m_position = Common::Vec3{};

  Common::Vec3 m_rotation_print = Common::Vec3{};
  Common::Vec3 m_position_print = Common::Vec3{};
};

class OrbitalController final : public CameraControllerInput
{
public:
  Common::Matrix44 GetView() const override
  {
    return Common::Matrix44::Translate(Common::Vec3{0, 0, -m_distance}) *
           Common::Matrix44::FromQuaternion(m_rotate_quat);
  }

  void MoveVertical(float) override {}

  void MoveHorizontal(float) override {}

  void MoveForward(float amt) override
  {
    m_distance += -1 * amt;
    m_distance = std::max(m_distance, MIN_DISTANCE);
  }

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
    CameraControllerInput::Reset();
    m_rotation = Common::Vec3{};
    m_rotate_quat = Common::Quaternion::Identity();
    m_distance = MIN_DISTANCE;
  }

  void DoState(PointerWrap& p) override
  {
    CameraControllerInput::DoState(p);
    p.Do(m_rotation);
    p.Do(m_rotate_quat);
    p.Do(m_distance);
  }

private:
  static constexpr float MIN_DISTANCE = 0.0f;
  float m_distance = MIN_DISTANCE;
  Common::Vec3 m_rotation = Common::Vec3{};
  Common::Quaternion m_rotate_quat = Common::Quaternion::Identity();
};
}  // namespace

Common::Vec2 CameraControllerInput::GetFieldOfViewMultiplier() const
{
  return Common::Vec2{m_fov_x_multiplier, m_fov_y_multiplier};
}

void CameraControllerInput::DoState(PointerWrap& p)
{
  p.Do(m_speed);
  p.Do(m_fov_x_multiplier);
  p.Do(m_fov_y_multiplier);
}

void CameraControllerInput::IncreaseFovX(float fov)
{
  m_fov_x_multiplier += fov;
  m_fov_x_multiplier = std::max(m_fov_x_multiplier, MIN_FOV_MULTIPLIER);
}

void CameraControllerInput::IncreaseFovY(float fov)
{
  m_fov_y_multiplier += fov;
  m_fov_y_multiplier = std::max(m_fov_y_multiplier, MIN_FOV_MULTIPLIER);
}

float CameraControllerInput::GetFovStepSize() const
{
  return 1.5f;
}

void CameraControllerInput::Reset()
{
  m_fov_x_multiplier = DEFAULT_FOV_MULTIPLIER;
  m_fov_y_multiplier = DEFAULT_FOV_MULTIPLIER;
  m_dirty = true;
}

void CameraControllerInput::ModifySpeed(float amt)
{
  m_speed += amt;
  m_speed = std::max(m_speed, 0.0f);
}

void CameraControllerInput::ResetSpeed()
{
  m_speed = DEFAULT_SPEED;
}

float CameraControllerInput::GetSpeed() const
{
  return m_speed;
}

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
    std::cout << " FPS selected" << std::endl;
  }
  else if (type == FreeLook::ControlType::RioCam)
  {
    m_camera_controller = std::make_unique<RioCamController>();
    std::cout << " RioCam selected" << std::endl;
  }

  m_current_type = type;
}

Common::Matrix44 FreeLookCamera::GetView() const
{
  return m_camera_controller->GetView();
}

Common::Vec2 FreeLookCamera::GetFieldOfViewMultiplier() const
{
  return m_camera_controller->GetFieldOfViewMultiplier();
}

void FreeLookCamera::DoState(PointerWrap& p)
{
  if (p.mode == PointerWrap::MODE_WRITE || p.mode == PointerWrap::MODE_MEASURE)
  {
    p.Do(m_current_type);
    if (m_camera_controller)
    {
      m_camera_controller->DoState(p);
    }
  }
  else
  {
    const auto old_type = m_current_type;
    p.Do(m_current_type);
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

bool FreeLookCamera::IsActive() const
{
  return FreeLook::GetActiveConfig().enabled;
}

CameraController* FreeLookCamera::GetController() const
{
  return m_camera_controller.get();
}
