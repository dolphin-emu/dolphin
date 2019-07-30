// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

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
  enum : u8
  {
    IR_MODE_BASIC = 1,
    IR_MODE_EXTENDED = 3,
    IR_MODE_FULL = 5,
  };

  void Reset();
  void DoState(PointerWrap& p);
  void Update(const Common::Matrix44& transform);
  void SetEnabled(bool is_enabled);

  static constexpr u8 I2C_ADDR = 0x58;
  static constexpr int CAMERA_DATA_BYTES = 36;

private:
  // TODO: some of this memory is write-only and should return error 7.
#pragma pack(push, 1)
  struct Register
  {
    // Contains sensitivity and other unknown data
    // TODO: Do the IR and Camera enabling reports write to the i2c bus?
    // TODO: Does disabling the camera peripheral reset the mode or sensitivity?
    // TODO: Break out this "data" array into some known members
    u8 data[0x33];
    u8 mode;
    u8 unk[3];
    // addr: 0x37
    u8 camera_data[CAMERA_DATA_BYTES];
    u8 unk2[165];
  };
#pragma pack(pop)

  static_assert(0x100 == sizeof(Register));

public:
  // The real wiimote reads camera data from the i2c bus at offset 0x37:
  static const u8 REPORT_DATA_OFFSET = offsetof(Register, camera_data);

private:
  int BusRead(u8 slave_addr, u8 addr, int count, u8* data_out) override;
  int BusWrite(u8 slave_addr, u8 addr, int count, const u8* data_in) override;

  Register reg_data;

  // When disabled the camera does not respond on the bus.
  // Change is triggered by wiimote report 0x13.
  bool m_is_enabled;
};
}  // namespace WiimoteEmu
