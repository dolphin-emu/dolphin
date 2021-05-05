// Copyright 2021 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>

#include "Common/CommonTypes.h"

constexpr u16 FREE_LOOK_PROTO_VERSION = 1;

#pragma pack(push, 1)

struct PacketHeader
{
  u16 version;
  u8 message_count;
  u32 payload_size;  // not including header
  u8 msg_type;
};

struct MessageTransformByQuaternionPosition
{
  static const u8 msg_type = 0;
  double qx;
  double qy;
  double qz;
  double qw;

  double posx;
  double posy;
  double posz;
};

struct MessageTransformByEulerPosition
{
  static const u8 msg_type = 1;
  double pitch;
  double yaw;
  double roll;

  double posx;
  double posy;
  double posz;
};

struct MessageTransformByMatrix
{
  static const u8 msg_type = 2;
  std::array<double, 16> data;
};

struct MessageFieldOfView
{
  static const u8 msg_type = 3;
  double horizontal_multiplier;
  double vertical_multiplier;
};

#pragma pack(pop)
