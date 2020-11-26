// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Controller.h"

#include "Core/HW/GCPad.h"
#include "Core/HW/Wiimote.h"

namespace API
{

GCPadStatus GCManip::Get(int controller_id)
{
  auto iter = m_overrides.find(controller_id);
  if (iter != m_overrides.end())
    return iter->second.pad_status;
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

WiimoteCommon::ButtonData WiiManip::Get(int controller_id)
{
  auto iter = m_overrides.find(controller_id);
  if (iter != m_overrides.end())
    return iter->second.button_data;
  return Wiimote::GetButtonData(controller_id);
}

void WiiManip::Set(WiimoteCommon::ButtonData button_data, int controller_id, ClearOn clear_on)
{
  m_overrides[controller_id] = {button_data, clear_on, /* used: */ false};
}

void WiiManip::PerformInputManip(WiimoteCommon::DataReportBuilder& rpt, int controller_id, int ext,
                                 const WiimoteEmu::EncryptionKey& key)
{
  auto iter = m_overrides.find(controller_id);
  if (iter == m_overrides.end())
  {
    return;
  }
  WiiInputOverride& input_override = iter->second;

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

GCManip& GetGCManip()
{
  static GCManip manip(API::GetEventHub());
  return manip;
}

WiiManip& GetWiiManip()
{
  static WiiManip manip(API::GetEventHub());
  return manip;
}

}  // namespace API
