// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Controller.h"

#include "Core/Config/MainSettings.h"
#include "Core/HW/GCPad.h"
#include "Core/HW/SI/SI_Device.h"
#include "Core/HW/Wiimote.h"
#include "Core/HW/WiimoteEmu/Camera.h"
#include "InputCommon/GCAdapter.h"

namespace API
{

GCPadStatus GCManip::Get(int controller_id)
{
  auto iter = m_overrides.find(controller_id);
  if (iter != m_overrides.end())
    return iter->second.pad_status;
  if (Config::Get(Config::GetInfoForSIDevice(controller_id)) == SerialInterface::SIDEVICE_WIIU_ADAPTER)
    return GCAdapter::Input(controller_id);
  else
    return Pad::GetStatus(controller_id);
}

void GCManip::Set(GCPadStatus pad_status, int controller_id, ClearOn clear_on)
{
  m_overrides[controller_id] = {pad_status, clear_on, /* used: */ false};
}

void GCManip::PerformInputManip(GCPadStatus* pad_status, int controller_id)
{
  auto iter = m_overrides.find(controller_id);
  if (iter == m_overrides.end())
  {
    return;
  }
  GCInputOverride& input_override = iter->second;
  *pad_status = input_override.pad_status;
  if (input_override.clear_on == ClearOn::NextPoll)
  {
    m_overrides.erase(controller_id);
    return;
  }
  input_override.used = true;
}

WiimoteCommon::ButtonData WiiButtonsManip::Get(int controller_id)
{
  auto iter = m_overrides.find(controller_id);
  if (iter != m_overrides.end())
    return iter->second.button_data;
  return Wiimote::GetButtonData(controller_id);
}

void WiiButtonsManip::Set(WiimoteCommon::ButtonData button_data, int controller_id,
                                 ClearOn clear_on)
{
  m_overrides[controller_id] = {button_data, clear_on, /* used: */ false};
}

void WiiButtonsManip::PerformInputManip(WiimoteCommon::DataReportBuilder& rpt, int controller_id)
{
  if (!rpt.HasCore())
  {
    return;
  }
  auto iter = m_overrides.find(controller_id);
  if (iter == m_overrides.end())
  {
    return;
  }
  WiiInputButtonsOverride& input_override = iter->second;

  WiimoteCommon::DataReportBuilder::CoreData core;
  rpt.GetCoreData(&core);
  core.hex = input_override.button_data.hex;
  rpt.SetCoreData(core);
  if (input_override.clear_on == ClearOn::NextPoll)
  {
    m_overrides.erase(controller_id);
    return;
  }
  input_override.used = true;
}

void WiiIRManip::Set(IRCameraTransform ircamera_transform, int controller_id, ClearOn clear_on)
{
  m_overrides[controller_id] = {ircamera_transform, clear_on, /* used: */ false};
}

void WiiIRManip::PerformInputManip(WiimoteCommon::DataReportBuilder& rpt, int controller_id)
{
  if (!rpt.HasIR())
  {
    return;
  }
  const auto iter = m_overrides.find(controller_id);
  if (iter == m_overrides.end())
  {
    return;
  }
  const WiiInputIROverride& input_override = iter->second;

  u8* const ir_data = rpt.GetIRDataPtr();
  
  using WiimoteEmu::CameraLogic;
  u8 mode = CameraLogic::IR_MODE_BASIC;
  if (rpt.GetIRDataSize() == sizeof(WiimoteEmu::IRExtended) * 4)
    mode = CameraLogic::IR_MODE_EXTENDED;
  else if (rpt.GetIRDataSize() == sizeof(WiimoteEmu::IRFull) * 2)
    mode = CameraLogic::IR_MODE_FULL;

  using namespace Common;
  const auto face_forward = Matrix33::RotateX(static_cast<float>(MathUtil::TAU) / -4);
  const auto ir_transform = input_override.ircamera_transform;
  const auto transform =
    Matrix44::FromMatrix33(face_forward) * 
    Matrix44::FromQuaternion(Quaternion::RotateXYZ(ir_transform.pitch_yaw_roll)) *
    Matrix44::Translate(ir_transform.position);
  const Vec2 fov = {CameraLogic::CAMERA_FOV_X, CameraLogic::CAMERA_FOV_Y};
  CameraLogic::WriteIRDataForTransform(ir_data, mode, fov, transform);
}

GCManip& GetGCManip()
{
  static GCManip manip(GetEventHub());
  return manip;
}

WiiButtonsManip& GetWiiButtonsManip()
{
  static WiiButtonsManip manip(GetEventHub());
  return manip;
}

WiiIRManip& GetWiiIRManip()
{
  static WiiIRManip manip(GetEventHub());
  return manip;
}

}  // namespace API
