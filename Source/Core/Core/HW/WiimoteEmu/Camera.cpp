// Copyright 2019 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HW/WiimoteEmu/Camera.h"

#include <algorithm>
#include <cmath>

#include "Common/BitUtils.h"
#include "Common/ChunkFile.h"
#include "Common/MathUtil.h"
#include "Common/Matrix.h"

#include "Core/HW/WiimoteCommon/WiimoteReport.h"

namespace WiimoteEmu
{
void CameraLogic::Reset()
{
  m_reg_data = {};

  m_is_enabled = false;
}

void CameraLogic::DoState(PointerWrap& p)
{
  p.Do(m_reg_data);

  // FYI: m_is_enabled is handled elsewhere.
}

int CameraLogic::BusRead(u8 slave_addr, u8 addr, int count, u8* data_out)
{
  if (I2C_ADDR != slave_addr)
    return 0;

  if (!m_is_enabled)
    return 0;

  return RawRead(&m_reg_data, addr, count, data_out);
}

int CameraLogic::BusWrite(u8 slave_addr, u8 addr, int count, const u8* data_in)
{
  if (I2C_ADDR != slave_addr)
    return 0;

  if (!m_is_enabled)
    return 0;

  return RawWrite(&m_reg_data, addr, count, data_in);
}

std::array<CameraPoint, CameraLogic::NUM_POINTS>
CameraLogic::GetCameraPoints(const Common::Matrix44& transform, Common::Vec2 field_of_view)
{
  using Common::Matrix33;
  using Common::Matrix44;
  using Common::Vec3;
  using Common::Vec4;

  const std::array<Vec3, 2> leds{
      Vec3{-SENSOR_BAR_LED_SEPARATION / 2, 0, 0},
      Vec3{SENSOR_BAR_LED_SEPARATION / 2, 0, 0},
  };

  const auto camera_view =
      Matrix44::Perspective(field_of_view.y, field_of_view.x / field_of_view.y, 0.001f, 1000) *
      Matrix44::FromMatrix33(Matrix33::RotateX(float(MathUtil::TAU / 4))) * transform;

  std::array<CameraPoint, CameraLogic::NUM_POINTS> camera_points;

  std::ranges::transform(leds, camera_points.begin(), [&](const Vec3& v) {
    const auto point = camera_view * Vec4(v, 1.0);

    // Check if LED is behind camera.
    if (point.z < 0)
      return CameraPoint();

    // FYI: truncating vs. rounding seems to produce more symmetrical cursor positioning.
    const auto x = s32((1 - point.x / point.w) * CAMERA_RES_X / 2);
    const auto y = s32((1 - point.y / point.w) * CAMERA_RES_Y / 2);

    // Check if LED is outside of view.
    if (x < 0 || y < 0 || x >= CAMERA_RES_X || y >= CAMERA_RES_Y)
      return CameraPoint();

    // Curve fit from data using an official WM+ and sensor bar at middle "sensitivity".
    // Point sizes at minimum and maximum sensitivity differ by around 0.5 (not implemented).
    const auto point_size = 2.37f * std::pow(point.z, -0.778f);

    const auto clamped_point_size = std::clamp<s32>(std::lround(point_size), 1, MAX_POINT_SIZE);

    return CameraPoint({u16(x), u16(y)}, u8(clamped_point_size));
  });

  return camera_points;
}

void CameraLogic::Update(const std::array<CameraPoint, NUM_POINTS>& camera_points)
{
  // IR data is read from offset 0x37 on real hardware.
  auto& data = m_reg_data.camera_data;
  data.fill(0xff);

  constexpr u8 OBJECT_TRACKING_ENABLE = 0x08;

  // If Address 0x30 is not 0x08 the camera will return 0xFFs.
  // The Wii seems to write 0x01 here before changing modes/sensitivities.
  if (m_reg_data.enable_object_tracking != OBJECT_TRACKING_ENABLE)
    return;

  switch (m_reg_data.mode)
  {
  case IR_MODE_BASIC:
    for (std::size_t i = 0; i != camera_points.size() / 2; ++i)
    {
      IRBasic irdata = {};

      irdata.SetObject1(camera_points[i * 2].position);
      irdata.SetObject2(camera_points[i * 2 + 1].position);

      Common::BitCastPtr<IRBasic>(&data[i * sizeof(IRBasic)]) = irdata;
    }
    break;
  case IR_MODE_EXTENDED:
    for (std::size_t i = 0; i != camera_points.size(); ++i)
    {
      const auto& p = camera_points[i];
      if (p.position.x < CAMERA_RES_X)
      {
        IRExtended irdata = {};

        irdata.SetPosition(p.position);
        irdata.size = p.size;

        Common::BitCastPtr<IRExtended>(&data[i * sizeof(IRExtended)]) = irdata;
      }
    }
    break;
  case IR_MODE_FULL:
    for (std::size_t i = 0; i != camera_points.size(); ++i)
    {
      const auto& p = camera_points[i];
      if (p.position.x < CAMERA_RES_X)
      {
        IRFull irdata = {};

        irdata.SetPosition(p.position);
        irdata.size = p.size;

        // TODO: does size need to be scaled up?
        // E.g. does size 15 cover the entire sensor range?

        irdata.xmin = std::max(p.position.x - p.size, 0);
        irdata.ymin = std::max(p.position.y - p.size, 0);
        irdata.xmax = std::min(p.position.x + p.size, CAMERA_RES_X);
        irdata.ymax = std::min(p.position.y + p.size, CAMERA_RES_Y);

        constexpr int SUBPIXEL_RESOLUTION = 8;
        constexpr long MAX_INTENSITY = 0xff;

        // This is apparently the number of pixels the point takes up at 128x96 resolution.
        // We simulate a circle that shrinks at sensor edges.
        const auto intensity =
            std::lround((irdata.xmax - irdata.xmin) * (irdata.ymax - irdata.ymin) /
                        SUBPIXEL_RESOLUTION / SUBPIXEL_RESOLUTION * MathUtil::TAU / 8);

        irdata.intensity = u8(std::min(MAX_INTENSITY, intensity));

        Common::BitCastPtr<IRFull>(&data[i * sizeof(IRFull)]) = irdata;
      }
    }
    break;
  default:
    // This seems to be fairly common, 0xff data is sent in this case:
    // WARN_LOG(WIIMOTE, "Game is requesting IR data before setting IR mode.");
    break;
  }
}

void CameraLogic::SetEnabled(bool is_enabled)
{
  m_is_enabled = is_enabled;
}

}  // namespace WiimoteEmu
