// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/HW/WiimoteEmu/Camera.h"

#include "Common/BitUtils.h"
#include "Common/ChunkFile.h"
#include "Core/HW/WiimoteCommon/WiimoteReport.h"
#include "Core/HW/WiimoteEmu/MatrixMath.h"

namespace WiimoteEmu
{
void CameraLogic::Reset()
{
  reg_data = {};
}

void CameraLogic::DoState(PointerWrap& p)
{
  p.Do(reg_data);
}

int CameraLogic::BusRead(u8 slave_addr, u8 addr, int count, u8* data_out)
{
  if (I2C_ADDR != slave_addr)
    return 0;

  return RawRead(&reg_data, addr, count, data_out);
}

int CameraLogic::BusWrite(u8 slave_addr, u8 addr, int count, const u8* data_in)
{
  if (I2C_ADDR != slave_addr)
    return 0;

  return RawWrite(&reg_data, addr, count, data_in);
}

void CameraLogic::Update(const ControllerEmu::Cursor::StateData& cursor,
                         const NormalizedAccelData& accel, bool sensor_bar_on_top)
{
  double nsin, ncos;

  // Ugly code to figure out the wiimote's current angle.
  // TODO: Kill this.
  double ax = accel.x;
  double az = accel.z;
  const double len = sqrt(ax * ax + az * az);

  if (len)
  {
    ax /= len;
    az /= len;  // normalizing the vector
    nsin = ax;
    ncos = az;
  }
  else
  {
    nsin = 0;
    ncos = 1;
  }

  const double ir_sin = nsin;
  const double ir_cos = ncos;

  static constexpr int camWidth = 1024;
  static constexpr int camHeight = 768;
  static constexpr double bndleft = 0.78820266;
  static constexpr double bndright = -0.78820266;
  static constexpr double dist1 = 100.0 / camWidth;  // this seems the optimal distance for zelda
  static constexpr double dist2 = 1.2 * dist1;

  constexpr int NUM_POINTS = 4;

  std::array<Vertex, NUM_POINTS> v;

  for (auto& vtx : v)
  {
    vtx.x = cursor.x * (bndright - bndleft) / 2 + (bndleft + bndright) / 2;

    static constexpr double bndup = -0.315447;
    static constexpr double bnddown = 0.85;

    if (sensor_bar_on_top)
      vtx.y = cursor.y * (bndup - bnddown) / 2 + (bndup + bnddown) / 2;
    else
      vtx.y = cursor.y * (bndup - bnddown) / 2 - (bndup + bnddown) / 2;

    vtx.z = 0;
  }

  v[0].x -= (cursor.z * 0.5 + 1) * dist1;
  v[1].x += (cursor.z * 0.5 + 1) * dist1;
  v[2].x -= (cursor.z * 0.5 + 1) * dist2;
  v[3].x += (cursor.z * 0.5 + 1) * dist2;

#define printmatrix(m)                                                                             \
  PanicAlert("%f %f %f %f\n%f %f %f %f\n%f %f %f %f\n%f %f %f %f\n", m[0][0], m[0][1], m[0][2],    \
             m[0][3], m[1][0], m[1][1], m[1][2], m[1][3], m[2][0], m[2][1], m[2][2], m[2][3],      \
             m[3][0], m[3][1], m[3][2], m[3][3])
  Matrix rot, tot;
  static Matrix scale;
  MatrixScale(scale, 1, camWidth / camHeight, 1);
  MatrixRotationByZ(rot, ir_sin, ir_cos);
  MatrixMultiply(tot, scale, rot);

  u16 x[NUM_POINTS], y[NUM_POINTS];
  memset(x, 0xFF, sizeof(x));
  memset(y, 0xFF, sizeof(y));

  for (std::size_t i = 0; i < v.size(); i++)
  {
    MatrixTransformVertex(tot, v[i]);

    if ((v[i].x < -1) || (v[i].x > 1) || (v[i].y < -1) || (v[i].y > 1))
      continue;

    x[i] = static_cast<u16>(lround((v[i].x + 1) / 2 * (camWidth - 1)));
    y[i] = static_cast<u16>(lround((v[i].y + 1) / 2 * (camHeight - 1)));

    if (x[i] >= camWidth || y[i] >= camHeight)
    {
      x[i] = -1;
      y[i] = -1;
    }
  }

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
      for (unsigned int i = 0; i < 2; ++i)
      {
        IRBasic irdata = {};

        irdata.x1 = static_cast<u8>(x[i * 2]);
        irdata.x1hi = x[i * 2] >> 8;
        irdata.y1 = static_cast<u8>(y[i * 2]);
        irdata.y1hi = y[i * 2] >> 8;

        irdata.x2 = static_cast<u8>(x[i * 2 + 1]);
        irdata.x2hi = x[i * 2 + 1] >> 8;
        irdata.y2 = static_cast<u8>(y[i * 2 + 1]);
        irdata.y2hi = y[i * 2 + 1] >> 8;

        Common::BitCastPtr<IRBasic>(data + i * sizeof(IRBasic)) = irdata;
      }
      break;
    case IR_MODE_EXTENDED:
      for (unsigned int i = 0; i < 4; ++i)
      {
        if (x[i] < camWidth)
        {
          IRExtended irdata = {};

          irdata.x = static_cast<u8>(x[i]);
          irdata.xhi = x[i] >> 8;

          irdata.y = static_cast<u8>(y[i]);
          irdata.yhi = y[i] >> 8;

          irdata.size = 10;

          Common::BitCastPtr<IRExtended>(data + i * sizeof(IRExtended)) = irdata;
        }
      }
      break;
    case IR_MODE_FULL:
      for (unsigned int i = 0; i < 4; ++i)
      {
        if (x[i] < camWidth)
        {
          IRFull irdata = {};

          irdata.x = static_cast<u8>(x[i]);
          irdata.xhi = x[i] >> 8;

          irdata.y = static_cast<u8>(y[i]);
          irdata.yhi = y[i] >> 8;

          irdata.size = 10;

          // TODO: implement these sensibly:
          // TODO: do high bits of x/y min/max need to be set to zero?
          irdata.xmin = 0;
          irdata.ymin = 0;
          irdata.xmax = 0;
          irdata.ymax = 0;
          irdata.zero = 0;
          irdata.intensity = 0;

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

}  // namespace WiimoteEmu
