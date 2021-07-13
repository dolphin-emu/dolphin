// Copyright 2021 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoCommon/FreeLookCamera2D.h"

#include <algorithm>
#include <math.h>

#include "Common/MathUtil.h"

#include "Common/ChunkFile.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/FreeLookConfig.h"

#include "VideoCommon/VideoCommon.h"

FreeLookCamera2D g_freelook_camera_2d;

namespace
{
class BasicMovementController final : public CameraController2DInput
{
  void MoveVertical(float amt) override
  {
    m_position_offset.y += amt;
    m_dirty = true;
  }

  void MoveHorizontal(float amt) override
  {
    m_position_offset.x += amt;
    m_dirty = true;
  }

  const Common::Vec2& GetPositionOffset() const override { return m_position_offset; }

  void DoState(PointerWrap& p) override
  {
    CameraController2DInput::DoState(p);
    p.Do(m_position_offset);
  }

  void Reset() override
  {
    CameraController2DInput::Reset();
    m_position_offset = Common::Vec2{};
  }

private:
  Common::Vec2 m_position_offset;
};
}  // namespace

void CameraController2DInput::IncreaseStretchX(float amt)
{
  m_stretch_multiplier.x += amt;
}

void CameraController2DInput::IncreaseStretchY(float amt)
{
  m_stretch_multiplier.y += amt;
}

float CameraController2DInput::GetStretchMultiplierSize() const
{
  return 1.0f;
}

const Common::Vec2& CameraController2DInput::GetStretchMultiplier() const
{
  return m_stretch_multiplier;
}

void CameraController2DInput::Reset()
{
  m_stretch_multiplier = DEFAULT_STRETCH_MULTIPLIER;
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
  p.Do(m_speed);
  p.Do(m_stretch_multiplier);
}

FreeLookCamera2D::FreeLookCamera2D()
{
  m_controller = std::make_unique<BasicMovementController>();
}

const Common::Vec2& FreeLookCamera2D::GetPositionOffset() const
{
  return m_controller->GetPositionOffset();
}

const Common::Vec2& FreeLookCamera2D::GetStretchMultiplier() const
{
  return m_controller->GetStretchMultiplier();
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
