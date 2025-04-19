// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/Host.h"
#include "Core/HW/Memmap.h"
#include "Core/IOS/USB/Emulated/MotionCamera.h"
#include "Core/System.h"

namespace IOS::HLE::USB
{

CameraBase::~CameraBase()
{
  if (m_image_data)
  {
    free(m_image_data);
    m_image_data = nullptr;
  }
}

void CameraBase::CreateSample(const u16 width, const u16 height)
{
  NOTICE_LOG_FMT(IOS_USB, "CameraBase::CreateSample width={}, height={}", width, height);
  u32 new_size = width * height * 2;
  if (m_image_size != new_size)
  {
    m_image_size = new_size;
    if (m_image_data)
    {
      free(m_image_data);
    }
    m_image_data = (u8*) calloc(1, m_image_size);
  }

  for (int line = 0 ; line < height; line++) {
    for (int col = 0; col < width; col++) {
      u8 *pos = m_image_data + 2 * (width * line + col);

      u8 r = col  * 255 / width;
      u8 g = col  * 255 / width;
      u8 b = line * 255 / height;

      u8 y = (( 66 * r + 129 * g +  25 * b + 128) / 256) +  16;
      u8 u = ((-38 * r -  74 * g + 112 * b + 128) / 256) + 128;
      u8 v = ((112 * r -  94 * g -  18 * b + 128) / 256) + 128;

      pos[0] = y;
      pos[1] = (col % 2 == 0) ? u : v;
    }
  }
}

void CameraBase::SetData(const u8* data, u32 length)
{
  if (length > m_image_size)
  {
    NOTICE_LOG_FMT(IOS_USB, "CameraBase::SetData length({}) > m_image_size({})", length, m_image_size);
    return;
  }
  m_image_size = length;
  memcpy(m_image_data, data, length);
  //ERROR_LOG_FMT(IOS_USB, "SetData length={:x}", length);
  //static bool done = false;
  //if (!done)
  //{
  //  FILE* f = fopen("yugioh_dualscanner.raw", "wb");
  //  if (!f)
  //  {
  //    ERROR_LOG_FMT(IOS_USB, "yugioh_dualscanner null");
  //    return;
  //  }
  //  fwrite(m_image_data, m_image_size, 1, f);
  //  fclose(f);
  //  done = true;
  //}
}

void CameraBase::GetData(const u8* data, u32 length)
{
  if (length > m_image_size)
  {
    NOTICE_LOG_FMT(IOS_USB, "CameraBase::GetData length({}) > m_image_size({})", length, m_image_size);
    return;
  }
  memcpy((void*)data, m_image_data, length);
}

std::string CameraBase::getUVCVideoStreamingControl(u8 value)
{
  std::string names[] = { "VS_CONTROL_UNDEFINED", "VS_PROBE", "VS_COMMIT" };
  if (value <= VS_COMMIT)
    return names[value];
  return "Unknown";
}

std::string CameraBase::getUVCRequest(u8 value)
{
  if (value == SET_CUR)
    return "SET_CUR";
  std::string names[] = { "GET_CUR", "GET_MIN", "GET_MAX", "GET_RES", "GET_LEN", "GET_INF", "GET_DEF" };
  if (GET_CUR <= value && value <= GET_DEF)
    return names[value - GET_CUR];
  return "Unknown";
}

std::string CameraBase::getUVCTerminalControl(u8 value)
{
  std::string names[] = { "CONTROL_UNDEFINED", "SCANNING_MODE", "AE_MODE", "AE_PRIORITY", "EXPOSURE_TIME_ABSOLUTE",
    "EXPOSURE_TIME_RELATIVE", "FOCUS_ABSOLUTE", "FOCUS_RELATIVE", "FOCUS_AUTO", "IRIS_ABSOLUTE", "IRIS_RELATIVE",
    "ZOOM_ABSOLUTE", "ZOOM_RELATIVE", "PANTILT_ABSOLUTE", "PANTILT_RELATIVE", "ROLL_ABSOLUTE", "ROLL_RELATIVE",
    "PRIVACY" };
  if (value <= CT_PRIVACY)
    return names[value];
  return "Unknown";
}

std::string CameraBase::getUVCProcessingUnitControl(u8 value)
{
  std::string names[] = { "CONTROL_UNDEFINED", "BACKLIGHT_COMPENSATION", "BRIGHTNESS", "CONTRAST", "GAIN",
    "POWER_LINE_FREQUENCY", "HUE", "SATURATION", "SHARPNESS", "GAMMA", "WHITE_BALANCE_TEMPERATURE",
    "WHITE_BALANCE_TEMPERATURE_AUTO", "WHITE_BALANCE_COMPONENT", "WHITE_BALANCE_COMPONENT_AUTO", "DIGITAL_MULTIPLIER",
    "DIGITAL_MULTIPLIER_LIMIT", "HUE_AUTO", "ANALOG_VIDEO_STANDARD", "ANALOG_LOCK_STATUS" };
  if (value <= PU_ANALOG_LOCK_STATUS)
    return names[value];
  return "Unknown";
}
}  // namespace IOS::HLE::USB
