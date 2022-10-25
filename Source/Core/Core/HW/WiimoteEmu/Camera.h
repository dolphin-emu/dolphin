// Copyright 2019 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "Core/HW/WiimoteEmu/Dynamics.h"
#include "Core/HW/WiimoteEmu/I2CBus.h"
#include "InputCommon/ControllerEmu/ControlGroup/Cursor.h"

namespace Common
{
class Matrix44;
}

namespace WiimoteEmu
{
using IRObject = Common::TVec2<u16>;

struct CameraPoint
{
  IRObject position;
  u8 size;

  // 0xFFFFs are interpreted as "not visible".
  constexpr CameraPoint() : position({0xffff, 0xffff}), size(0xff) {}
  constexpr CameraPoint(IRObject position_, u8 size_) : position(position_), size(size_) {}
  constexpr bool operator==(const CameraPoint& other) const
  {
    return this->position == other.position && this->size == other.size;
  }
  constexpr bool operator!=(const CameraPoint& other) const { return !(*this == other); }
};

// Four bytes for two objects. Filled with 0xFF if empty
struct IRBasic
{
  u8 x1;
  u8 y1;
  u8 x2hi : 2;
  u8 y2hi : 2;
  u8 x1hi : 2;
  u8 y1hi : 2;
  u8 x2;
  u8 y2;

  auto GetObject1() const { return IRObject(x1hi << 8 | x1, y1hi << 8 | y1); }
  auto GetObject2() const { return IRObject(x2hi << 8 | x2, y2hi << 8 | y2); }

  void SetObject1(const IRObject& obj)
  {
    x1 = obj.x;
    x1hi = obj.x >> 8;
    y1 = obj.y;
    y1hi = obj.y >> 8;
  }
  void SetObject2(const IRObject& obj)
  {
    x2 = obj.x;
    x2hi = obj.x >> 8;
    y2 = obj.y;
    y2hi = obj.y >> 8;
  }
};
static_assert(sizeof(IRBasic) == 5, "Wrong size");

// Three bytes for one object
struct IRExtended
{
  u8 x;
  u8 y;
  u8 size : 4;
  u8 xhi : 2;
  u8 yhi : 2;

  auto GetPosition() const { return IRObject(xhi << 8 | x, yhi << 8 | y); }
  void SetPosition(const IRObject& obj)
  {
    x = obj.x;
    xhi = obj.x >> 8;
    y = obj.y;
    yhi = obj.y >> 8;
  }
};
static_assert(sizeof(IRExtended) == 3, "Wrong size");

// Nine bytes for one object
// first 3 bytes are the same as extended
struct IRFull : IRExtended
{
  u8 xmin : 7;
  u8 : 1;
  u8 ymin : 7;
  u8 : 1;
  u8 xmax : 7;
  u8 : 1;
  u8 ymax : 7;
  u8 : 1;
  u8 zero;
  u8 intensity;
};
static_assert(sizeof(IRFull) == 9, "Wrong size");

class CameraLogic : public I2CSlave
{
public:
  // OEM sensor bar distance between LED clusters in meters.
  static constexpr float SENSOR_BAR_LED_SEPARATION = 0.2f;

  static constexpr int CAMERA_RES_X = 1024;
  static constexpr int CAMERA_RES_Y = 768;

  // Jordan: I calculate the FOV at 42 degrees horizontally and having a 4:3 aspect ratio.
  // This is 31.5 degrees vertically.
  static constexpr float CAMERA_AR = 4.f / 3;
  static constexpr float CAMERA_FOV_X = 42 * float(MathUtil::TAU) / 360;
  static constexpr float CAMERA_FOV_Y = CAMERA_FOV_X / CAMERA_AR;

  enum : u8
  {
    IR_MODE_BASIC = 1,
    IR_MODE_EXTENDED = 3,
    IR_MODE_FULL = 5,
  };

  // FYI: A real wiimote normally only returns 1 point for each LED cluster (2 total).
  // Sending all 4 points can actually cause some stuttering issues.
  static constexpr int NUM_POINTS = 2;

  // Range from 0-15. Small values (2-4) seem to be very typical.
  // This is reduced based on distance from sensor bar.
  static constexpr int MAX_POINT_SIZE = 15;

  void Reset();
  void DoState(PointerWrap& p);
  static std::array<CameraPoint, NUM_POINTS> GetCameraPoints(const Common::Matrix44& transform,
                                                             Common::Vec2 field_of_view);
  void Update(const std::array<CameraPoint, NUM_POINTS>& camera_points);
  void SetEnabled(bool is_enabled);

  static constexpr u8 I2C_ADDR = 0x58;
  static constexpr int CAMERA_DATA_BYTES = 36;

private:
  // TODO: some of this memory is write-only and should return error 7.
#pragma pack(push, 1)
  struct Register
  {
    // Contains sensitivity and other unknown data
    // TODO: Does disabling the camera peripheral reset the mode or sensitivity?
    std::array<u8, 9> sensitivity_block1;
    std::array<u8, 17> unk_0x09;

    // addr: 0x1a
    std::array<u8, 2> sensitivity_block2;
    std::array<u8, 20> unk_0x1c;

    // addr: 0x30
    u8 enable_object_tracking;
    std::array<u8, 2> unk_0x31;

    // addr: 0x33
    u8 mode;
    std::array<u8, 3> unk_0x34;

    // addr: 0x37
    std::array<u8, CAMERA_DATA_BYTES> camera_data;
    std::array<u8, 165> unk_0x5b;
  };
#pragma pack(pop)

  static_assert(0x100 == sizeof(Register));

public:
  // The real wiimote reads camera data from the i2c bus at offset 0x37:
  static const u8 REPORT_DATA_OFFSET = offsetof(Register, camera_data);

private:
  int BusRead(u8 slave_addr, u8 addr, int count, u8* data_out) override;
  int BusWrite(u8 slave_addr, u8 addr, int count, const u8* data_in) override;

  Register m_reg_data{};

  // When disabled the camera does not respond on the bus.
  // Change is triggered by wiimote report 0x13.
  bool m_is_enabled = false;
};
}  // namespace WiimoteEmu
