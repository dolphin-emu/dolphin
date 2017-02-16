// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

// Razer Hydra Wiimote Translation Layer

#pragma once

#include "Core/HW/WiimoteEmu/HydraTLayer.h"
#include "Core/HW/GCPadEmu.h"
#include "Core/HW/WiimoteEmu/Attachment/Nunchuk.h"
#include "InputCommon/GCPadStatus.h"
#include "InputCommon/ControllerInterface/Sixense/RazerHydra.h"
#include "VideoCommon/VR.h"

namespace HydraTLayer
{
float vr_gc_dpad_speed = 0, vr_gc_leftstick_speed = 0, vr_gc_rightstick_speed = 0;
float vr_cc_dpad_speed = 0, vr_wm_dpad_speed = 0, vr_wm_leftstick_speed = 0,
      vr_wm_rightstick_speed = 0, vr_ir_speed = 0;

void GetButtons(int index, bool sideways, bool has_extension, wm_buttons* butt,
                bool* cycle_extension)
{
  u32 mask;
  if (RazerHydra::getButtons(index, sideways, has_extension, &mask, cycle_extension))
  {
    butt->hex |= (mask & UDPWM_BA) ? WiimoteEmu::Wiimote::BUTTON_A : 0;
    butt->hex |= (mask & UDPWM_BB) ? WiimoteEmu::Wiimote::BUTTON_B : 0;
    butt->hex |= (mask & UDPWM_B1) ? WiimoteEmu::Wiimote::BUTTON_ONE : 0;
    butt->hex |= (mask & UDPWM_B2) ? WiimoteEmu::Wiimote::BUTTON_TWO : 0;
    butt->hex |= (mask & UDPWM_BP) ? WiimoteEmu::Wiimote::BUTTON_PLUS : 0;
    butt->hex |= (mask & UDPWM_BM) ? WiimoteEmu::Wiimote::BUTTON_MINUS : 0;
    butt->hex |= (mask & UDPWM_BH) ? WiimoteEmu::Wiimote::BUTTON_HOME : 0;
    butt->hex |= (mask & UDPWM_BU) ? WiimoteEmu::Wiimote::PAD_UP : 0;
    butt->hex |= (mask & UDPWM_BD) ? WiimoteEmu::Wiimote::PAD_DOWN : 0;
    butt->hex |= (mask & UDPWM_BL) ? WiimoteEmu::Wiimote::PAD_LEFT : 0;
    butt->hex |= (mask & UDPWM_BR) ? WiimoteEmu::Wiimote::PAD_RIGHT : 0;
  }
  if (index == 0)
  {
    vr_wm_dpad_speed =
        (butt->hex & (WiimoteEmu::Wiimote::PAD_UP | WiimoteEmu::Wiimote::PAD_DOWN |
                      WiimoteEmu::Wiimote::PAD_LEFT | WiimoteEmu::Wiimote::PAD_RIGHT)) ?
            1.0f :
            0.0f;
    ;
  }
}

void GetAcceleration(int index, bool sideways, bool has_extension,
                     WiimoteEmu::AccelData* const data)
{
  float x, y, z;
  if (RazerHydra::getAccel(index, sideways, has_extension, &x, &y, &z))
  {
    // If we're IR pointing at the "screen", then we need to check we aren't pitching up or down
    // more than 60 degrees
    // because the Wii doesn't return IR results when pitching up or down 60 degrees or more.
    // We need to force it to return IR results by pretending it is only pitched 59 degrees.
    if (g_vr_has_ir && g_vr_ir_x > -1 && g_vr_ir_x < 1 && g_vr_ir_y > -1 && g_vr_ir_y < 1)
    {
      float pitch = RADIANS_TO_DEGREES(asin(-y));
      float roll = atan2(z, x);
      if (fabs(pitch) > 60)
      {
        pitch = 50 * SignOf(pitch);
        y = sin(DEGREES_TO_RADIANS(pitch));
        float c = cos(DEGREES_TO_RADIANS(pitch));
        x = sin(roll) * c;
        z = cos(roll) * c;
      }
    }
    data->x = x;
    data->y = y;
    data->z = z;
  }
}

void GetIR(int index, double* x, double* y, double* z)
{
  RazerHydra::getIR(index, x, y, z);
  if (index == 0)
  {
    HydraTLayer::vr_ir_speed =
        (float)((int)(fabs(*x) > 0.5 && fabs(*x) <= 1) + (int)(fabs(*y) > 0.5 && fabs(*y) <= 1));
  }
}

void GetNunchukAcceleration(int index, WiimoteEmu::AccelData* const data)
{
  float gx, gy, gz;
  if (RazerHydra::getNunchuckAccel(index, &gx, &gy, &gz))
  {
    data->x = gx;
    data->y = gy;
    data->z = gz;
  }
}

void GetNunchuk(int index, u8* jx, u8* jy, wm_nc_core* butt)
{
  float x, y;
  u8 mask;
  if (RazerHydra::getNunchuk(index, &x, &y, &mask))
  {
    butt->hex |= (mask & UDPWM_NC) ? WiimoteEmu::Nunchuk::BUTTON_C : 0;
    butt->hex |= (mask & UDPWM_NZ) ? WiimoteEmu::Nunchuk::BUTTON_Z : 0;
    *jx = u8(0x80 + x * 127);
    *jy = u8(0x80 + y * 127);
  }
  if (index == 0)
  {
    vr_wm_leftstick_speed = fabs((*jx - 0x80) / 127.0f) + fabs((*jy - 0x80) / 127.0f);
  }
}

void GetClassic(int index, wm_classic_extension* ccdata)
{
  float lx, ly, rx, ry, l, r;
  u32 mask;
  if (RazerHydra::getClassic(index, &lx, &ly, &rx, &ry, &l, &r, &mask))
  {
    ccdata->bt.hex |= (mask & CC_B_A) ? WiimoteEmu::Classic::BUTTON_A : 0;
    ccdata->bt.hex |= (mask & CC_B_B) ? WiimoteEmu::Classic::BUTTON_B : 0;
    ccdata->bt.hex |= (mask & CC_B_X) ? WiimoteEmu::Classic::BUTTON_X : 0;
    ccdata->bt.hex |= (mask & CC_B_Y) ? WiimoteEmu::Classic::BUTTON_Y : 0;
    ccdata->bt.hex |= (mask & CC_B_PLUS) ? WiimoteEmu::Classic::BUTTON_PLUS : 0;
    ccdata->bt.hex |= (mask & CC_B_MINUS) ? WiimoteEmu::Classic::BUTTON_MINUS : 0;
    ccdata->bt.hex |= (mask & CC_B_HOME) ? WiimoteEmu::Classic::BUTTON_HOME : 0;
    ccdata->bt.hex |= (mask & CC_B_L) ? WiimoteEmu::Classic::TRIGGER_L : 0;
    ccdata->bt.hex |= (mask & CC_B_R) ? WiimoteEmu::Classic::TRIGGER_R : 0;
    ccdata->bt.hex |= (mask & CC_B_ZL) ? WiimoteEmu::Classic::BUTTON_ZL : 0;
    ccdata->bt.hex |= (mask & CC_B_ZR) ? WiimoteEmu::Classic::BUTTON_ZR : 0;
    ccdata->bt.hex |= (mask & CC_B_UP) ? WiimoteEmu::Classic::PAD_UP : 0;
    ccdata->bt.hex |= (mask & CC_B_DOWN) ? WiimoteEmu::Classic::PAD_DOWN : 0;
    ccdata->bt.hex |= (mask & CC_B_LEFT) ? WiimoteEmu::Classic::PAD_LEFT : 0;
    ccdata->bt.hex |= (mask & CC_B_RIGHT) ? WiimoteEmu::Classic::PAD_RIGHT : 0;
    ccdata->regular_data.lx = (u8)(31.5f + lx * 31.5f);
    ccdata->regular_data.ly = (u8)(31.5f + ly * 31.5f);
    u8 trigger = (u8)(l * 31);
    ccdata->lt1 = trigger;
    ccdata->lt2 = trigger >> 3;
    u8 x = u8(15.5f + rx * 15.5f);
    u8 y = u8(15.5f + ry * 15.5f);
    ccdata->rx1 = x;
    ccdata->rx2 = x >> 1;
    ccdata->rx3 = x >> 3;
    ccdata->ry = y;
    trigger = (u8)(r * 31);
    ccdata->rt = trigger;
  }
  if (index == 0)
  {
    vr_cc_dpad_speed =
        (ccdata->bt.hex & (WiimoteEmu::Classic::PAD_UP | WiimoteEmu::Classic::PAD_DOWN |
                           WiimoteEmu::Classic::PAD_LEFT | WiimoteEmu::Classic::PAD_RIGHT)) ?
            1.0f :
            0.0f;
    vr_wm_leftstick_speed = fabs((ccdata->regular_data.lx - 31.5f) / 31.5f) +
                            fabs((ccdata->regular_data.ly - 31.5f) / 31.5f);
    vr_wm_rightstick_speed =
        fabs((ccdata->rx1 + (ccdata->rx2 << 1) + (ccdata->rx3 << 3) - 15.5f) / 15.5f) +
        fabs((ccdata->ry - 15.5f) / 15.5f);
  }
}

void GetGameCube(int index, u16* butt, u8* stick_x, u8* stick_y, u8* substick_x, u8* substick_y,
                 u8* ltrigger, u8* rtrigger)
{
  float lx, ly, rx, ry, l, r;
  u32 mask;
  if (RazerHydra::getGameCube(index, &lx, &ly, &rx, &ry, &l, &r, &mask))
  {
    *butt |= (mask & GC_B_A) ? PAD_BUTTON_A : 0;
    *butt |= (mask & GC_B_B) ? PAD_BUTTON_B : 0;
    *butt |= (mask & GC_B_X) ? PAD_BUTTON_X : 0;
    *butt |= (mask & GC_B_Y) ? PAD_BUTTON_Y : 0;
    *butt |= (mask & GC_B_START) ? PAD_BUTTON_START : 0;
    *butt |= (mask & GC_B_L) ? PAD_TRIGGER_L : 0;
    *butt |= (mask & GC_B_R) ? PAD_TRIGGER_R : 0;
    *butt |= (mask & GC_B_Z) ? PAD_TRIGGER_Z : 0;
    *butt |= (mask & GC_B_UP) ? PAD_BUTTON_UP : 0;
    *butt |= (mask & GC_B_DOWN) ? PAD_BUTTON_DOWN : 0;
    *butt |= (mask & GC_B_LEFT) ? PAD_BUTTON_LEFT : 0;
    *butt |= (mask & GC_B_RIGHT) ? PAD_BUTTON_RIGHT : 0;
    *stick_x = (u8)(0x80 + lx * 127);
    *stick_y = (u8)(0x80 + ly * 127);
    *substick_x = (u8)(0x80 + rx * 127);
    *substick_y = (u8)(0x80 + ry * 127);
    *ltrigger = (u8)(l * 255);
    *rtrigger = (u8)(r * 255);
  }
  if (index == 0)
  {
    vr_gc_dpad_speed =
        (*butt & (PAD_BUTTON_UP | PAD_BUTTON_DOWN | PAD_BUTTON_LEFT | PAD_BUTTON_RIGHT)) ? 1.0f :
                                                                                           0.0f;
    vr_gc_leftstick_speed = fabs((*stick_x - 0x80) / 127.0f) + fabs((*stick_y - 0x80) / 127.0f);
    vr_gc_rightstick_speed =
        fabs((*substick_x - 0x80) / 127.0f) + fabs((*substick_y - 0x80) / 127.0f);
  }
}
}