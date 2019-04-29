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

#include "Core/HW/WiimoteCommon/WiimoteReport.h"

namespace WiimoteEmu
{
void CameraLogic::Reset()
{
  reg_data = {};

  m_is_enabled = false;
}

void CameraLogic::DoState(PointerWrap& p)
{
  p.Do(reg_data);

  // FYI: m_is_enabled is handled elsewhere.
}

int CameraLogic::BusRead(u8 slave_addr, u8 addr, int count, u8* data_out)
{
  if (I2C_ADDR != slave_addr)
    return 0;

  if (!m_is_enabled)
    return 0;

  return RawRead(&reg_data, addr, count, data_out);
}

int CameraLogic::BusWrite(u8 slave_addr, u8 addr, int count, const u8* data_in)
{
  if (I2C_ADDR != slave_addr)
    return 0;

  if (!m_is_enabled)
    return 0;

  return RawWrite(&reg_data, addr, count, data_in);
}

void CameraLogic::Update(const Common::Matrix44& transform)
{
  using Common::Matrix33;
  using Common::Matrix44;
  using Common::Vec3;
  using Common::Vec4;

  constexpr int CAMERA_WIDTH = 1024;
  constexpr int CAMERA_HEIGHT = 768;

  // Wiibrew claims the camera FOV is about 33 deg by 23 deg.
  // Unconfirmed but it seems to work well enough.
  constexpr int CAMERA_FOV_X_DEG = 33;
  constexpr int CAMERA_FOV_Y_DEG = 23;

  constexpr auto CAMERA_FOV_Y = float(CAMERA_FOV_Y_DEG * MathUtil::TAU / 360);
  constexpr auto CAMERA_ASPECT_RATIO = float(CAMERA_FOV_X_DEG) / CAMERA_FOV_Y_DEG;

  // FYI: A real wiimote normally only returns 1 point for each LED cluster (2 total).
  // Sending all 4 points can actually cause some stuttering issues.
  constexpr int NUM_POINTS = 2;

  // Range from 0-15. Small values (2-4) seem to be very typical.
  // This is reduced based on distance from sensor bar.
  constexpr int MAX_POINT_SIZE = 15;

  // Sensor bar:
  // Values are optimized for default settings in "Super Mario Galaxy 2"
  // This seems to be acceptable for a good number of games.
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
    u16 x;
    u16 y;
    u8 size;
  };

  // 0xFFFFs are interpreted as "not visible".
  constexpr CameraPoint INVISIBLE_POINT{0xffff, 0xffff, 0xff};

  std::array<CameraPoint, leds.size()> camera_points;

  std::transform(leds.begin(), leds.end(), camera_points.begin(), [&](const Vec3& v) {
    const auto point = camera_view * Vec4(v, 1.0);

    if (point.z > 0)
    {
      // FYI: Casting down vs. rounding seems to produce more symmetrical output.
      const auto x = s32((1 - point.x / point.w) * CAMERA_WIDTH / 2);
      const auto y = s32((1 - point.y / point.w) * CAMERA_HEIGHT / 2);

      const auto point_size = std::lround(MAX_POINT_SIZE / point.w / 2);

      if (x >= 0 && y >= 0 && x < CAMERA_WIDTH && y < CAMERA_HEIGHT)
        return CameraPoint{u16(x), u16(y), u8(point_size)};
    }

    return INVISIBLE_POINT;
  });

  // IR data is read from offset 0x37 on real hardware
  auto& data = reg_data.camera_data;
  // A maximum of 36 bytes:
  std::fill(std::begin(data), std::end(data), 0xff);

  // Fill report with valid data when full handshake was done
  // TODO: kill magic number:
  if (reg_data.data[0x30])
  {
    switch (reg_data.mode)
    {
    case IR_MODE_BASIC:
      for (std::size_t i = 0; i != camera_points.size() / 2; ++i)
      {
        IRBasic irdata = {};

        const auto& p1 = camera_points[i * 2];
        irdata.x1 = p1.x;
        irdata.x1hi = p1.x >> 8;
        irdata.y1 = p1.y;
        irdata.y1hi = p1.y >> 8;

        const auto& p2 = camera_points[i * 2 + 1];
        irdata.x2 = p2.x;
        irdata.x2hi = p2.x >> 8;
        irdata.y2 = p2.y;
        irdata.y2hi = p2.y >> 8;

        Common::BitCastPtr<IRBasic>(data + i * sizeof(IRBasic)) = irdata;
      }
      break;
    case IR_MODE_EXTENDED:
      for (std::size_t i = 0; i != camera_points.size(); ++i)
      {
        const auto& p = camera_points[i];
        if (p.x < CAMERA_WIDTH)
        {
          IRExtended irdata = {};

          // TODO: Move this logic into IRExtended class?
          irdata.x = p.x;
          irdata.xhi = p.x >> 8;

          irdata.y = p.y;
          irdata.yhi = p.y >> 8;

          irdata.size = p.size;

          Common::BitCastPtr<IRExtended>(data + i * sizeof(IRExtended)) = irdata;
        }
      }
      break;
    case IR_MODE_FULL:
      for (std::size_t i = 0; i != camera_points.size(); ++i)
      {
        const auto& p = camera_points[i];
        if (p.x < CAMERA_WIDTH)
        {
          IRFull irdata = {};

          irdata.x = p.x;
          irdata.xhi = p.x >> 8;

          irdata.y = p.y;
          irdata.yhi = p.y >> 8;

          irdata.size = p.size;

          // TODO: does size need to be scaled up?
          // E.g. does size 15 cover the entire sensor range?

          irdata.xmin = std::max(p.x - p.size, 0);
          irdata.ymin = std::max(p.y - p.size, 0);
          irdata.xmax = std::min(p.x + p.size, CAMERA_WIDTH);
          irdata.ymax = std::min(p.y + p.size, CAMERA_HEIGHT);

          // TODO: Is this maybe MSbs of the "intensity" value?
          irdata.zero = 0;

          constexpr int SUBPIXEL_RESOLUTION = 8;
          constexpr long MAX_INTENSITY = 0xff;

          // This is apparently the number of pixels the point takes up at 128x96 resolution.
          // We simulate a circle that shrinks at sensor edges.
          const auto intensity =
              std::lround((irdata.xmax - irdata.xmin) * (irdata.ymax - irdata.ymin) /
                          SUBPIXEL_RESOLUTION / SUBPIXEL_RESOLUTION * MathUtil::TAU / 8);

          irdata.intensity = u8(std::min(MAX_INTENSITY, intensity));

          Common::BitCastPtr<IRFull>(data + i * sizeof(IRFull)) = irdata;
        }
      }
      break;
    default:
      // This seems to be fairly common, 0xff data is sent in this case:
      // WARN_LOG(WIIMOTE, "Game is requesting IR data before setting IR mode.");
      break;
    }
  }
}

void CameraLogic::SetEnabled(bool is_enabled)
{
  m_is_enabled = is_enabled;
}

}  // namespace WiimoteEmu
