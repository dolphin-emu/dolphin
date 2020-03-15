// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/HW/WiimoteEmu/Camera.h"

#include <algorithm>
#include <cmath>

#include "Common/BitUtils.h"
#include "Common/ChunkFile.h"
#include "Common/MathUtil.h"
#include "Common/Matrix.h"

#include "Core/HW/WII_IPC.h"
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

void CameraLogic::Update(const Common::Matrix44& transform)
{
  // IR data is read from offset 0x37 on real hardware.
  auto& data = m_reg_data.camera_data;
  data.fill(0xff);

  constexpr u8 OBJECT_TRACKING_ENABLE = 0x08;

  // If Address 0x30 is not 0x08 the camera will return 0xFFs.
  // The Wii seems to write 0x01 here before changing modes/sensitivities.
  if (m_reg_data.enable_object_tracking != OBJECT_TRACKING_ENABLE)
    return;

  // If the sensor bar is off the camera will see no LEDs and return 0xFFs.
  if (!IOS::g_gpio_out[IOS::GPIO::SENSOR_BAR])
    return;

  using Common::Matrix33;
  using Common::Matrix44;
  using Common::Vec3;
  using Common::Vec4;

  constexpr auto CAMERA_FOV_Y = float(CAMERA_FOV_Y_DEG * MathUtil::TAU / 360);
  constexpr auto CAMERA_ASPECT_RATIO = float(CAMERA_FOV_X_DEG) / CAMERA_FOV_Y_DEG;

  // FYI: A real wiimote normally only returns 1 point for each LED cluster (2 total).
  // Sending all 4 points can actually cause some stuttering issues.
  constexpr int NUM_POINTS = 2;

  // Range from 0-15. Small values (2-4) seem to be very typical.
  // This is reduced based on distance from sensor bar.
  constexpr int MAX_POINT_SIZE = 15;

  // Sensor bar:
  // Distance in meters between LED clusters.
  constexpr float SENSOR_BAR_LED_SEPARATION = 0.2f;

  const std::array<Vec3, NUM_POINTS> leds{
      Vec3{-SENSOR_BAR_LED_SEPARATION / 2, 0, 0},
      Vec3{SENSOR_BAR_LED_SEPARATION / 2, 0, 0},
  };

  const auto camera_view = Matrix44::Perspective(CAMERA_FOV_Y, CAMERA_ASPECT_RATIO, 0.001f, 1000) *
                           Matrix44::FromMatrix33(Matrix33::RotateX(float(MathUtil::TAU / 4))) *
                           transform;

  struct CameraPoint
  {
    IRBasic::IRObject position;
    u8 size;
  };

  std::array<CameraPoint, leds.size()> camera_points;

  std::transform(leds.begin(), leds.end(), camera_points.begin(), [&](const Vec3& v) {
    const auto point = camera_view * Vec4(v, 1.0);

    // Check if LED is behind camera.
    if (point.z > 0)
    {
      // FYI: Casting down vs. rounding seems to produce more symmetrical output.
      const auto x = s32((1 - point.x / point.w) * CAMERA_RES_X / 2);
      const auto y = s32((1 - point.y / point.w) * CAMERA_RES_Y / 2);

      const auto point_size = std::lround(MAX_POINT_SIZE / point.w / 2);

      if (x >= 0 && y >= 0 && x < CAMERA_RES_X && y < CAMERA_RES_Y)
        return CameraPoint{{u16(x), u16(y)}, u8(point_size)};
    }

    // 0xFFFFs are interpreted as "not visible".
    return CameraPoint{{0xffff, 0xffff}, 0xff};
  });

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
