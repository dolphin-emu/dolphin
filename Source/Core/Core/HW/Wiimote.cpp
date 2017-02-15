// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/HW/Wiimote.h"

#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"

#include "Core/HW/WiimoteEmu/WiimoteEmu.h"
#include "Core/HW/WiimoteReal/WiimoteReal.h"
#include "Core/Movie.h"

#include "InputCommon/ControllerEmu/ControlGroup/ControlGroup.h"
#include "InputCommon/ControllerInterface/ControllerInterface.h"
#include "InputCommon/InputConfig.h"

namespace Wiimote
{
static InputConfig s_config(WIIMOTE_INI_NAME, _trans("Wii Remote"), "Wiimote");

InputConfig* GetConfig()
{
  return &s_config;
}

ControllerEmu::ControlGroup* GetWiimoteGroup(int number, WiimoteEmu::WiimoteGroup group)
{
  return static_cast<WiimoteEmu::Wiimote*>(s_config.GetController(number))->GetWiimoteGroup(group);
}

ControllerEmu::ControlGroup* GetNunchukGroup(int number, WiimoteEmu::NunchukGroup group)
{
  return static_cast<WiimoteEmu::Wiimote*>(s_config.GetController(number))->GetNunchukGroup(group);
}

ControllerEmu::ControlGroup* GetClassicGroup(int number, WiimoteEmu::ClassicGroup group)
{
  return static_cast<WiimoteEmu::Wiimote*>(s_config.GetController(number))->GetClassicGroup(group);
}

ControllerEmu::ControlGroup* GetGuitarGroup(int number, WiimoteEmu::GuitarGroup group)
{
  return static_cast<WiimoteEmu::Wiimote*>(s_config.GetController(number))->GetGuitarGroup(group);
}

ControllerEmu::ControlGroup* GetDrumsGroup(int number, WiimoteEmu::DrumsGroup group)
{
  return static_cast<WiimoteEmu::Wiimote*>(s_config.GetController(number))->GetDrumsGroup(group);
}

ControllerEmu::ControlGroup* GetTurntableGroup(int number, WiimoteEmu::TurntableGroup group)
{
  return static_cast<WiimoteEmu::Wiimote*>(s_config.GetController(number))
      ->GetTurntableGroup(group);
}

void Shutdown()
{
  s_config.ClearControllers();

  WiimoteReal::Stop();
}

void Initialize(InitializeMode init_mode)
{
  if (s_config.ControllersNeedToBeCreated())
  {
    for (unsigned int i = WIIMOTE_CHAN_0; i < MAX_BBMOTES; ++i)
      s_config.CreateController<WiimoteEmu::Wiimote>(i);
  }

  g_controller_interface.RegisterHotplugCallback(LoadConfig);

  s_config.LoadConfig(false);

  WiimoteReal::Initialize(init_mode);

  // Reload Wiimotes with our settings
  if (Movie::IsMovieActive())
    Movie::ChangeWiiPads();
}

void ResetAllWiimotes()
{
  for (int i = WIIMOTE_CHAN_0; i < MAX_BBMOTES; ++i)
    static_cast<WiimoteEmu::Wiimote*>(s_config.GetController(i))->Reset();
}

void LoadConfig()
{
  s_config.LoadConfig(false);
}

void Resume()
{
  WiimoteReal::Resume();
}

void Pause()
{
  WiimoteReal::Pause();
}

// An L2CAP packet is passed from the Core to the Wiimote on the HID CONTROL channel.
void ControlChannel(int number, u16 channel_id, const void* data, u32 size)
{
  if (WIIMOTE_SRC_HYBRID & g_wiimote_sources[number])
    static_cast<WiimoteEmu::Wiimote*>(s_config.GetController(number))
        ->ControlChannel(channel_id, data, size);
}

// An L2CAP packet is passed from the Core to the Wiimote on the HID INTERRUPT channel.
void InterruptChannel(int number, u16 channel_id, const void* data, u32 size)
{
  if (WIIMOTE_SRC_HYBRID & g_wiimote_sources[number])
    static_cast<WiimoteEmu::Wiimote*>(s_config.GetController(number))
        ->InterruptChannel(channel_id, data, size);
}

// This function is called periodically by the Core to update Wiimote state.
void Update(int number, bool connected)
{
  if (connected)
  {
    if (WIIMOTE_SRC_EMU & g_wiimote_sources[number])
      static_cast<WiimoteEmu::Wiimote*>(s_config.GetController(number))->Update();
    else
      WiimoteReal::Update(number);
  }
  else
  {
    if (WIIMOTE_SRC_EMU & g_wiimote_sources[number])
      static_cast<WiimoteEmu::Wiimote*>(s_config.GetController(number))->ConnectOnInput();

    if (WIIMOTE_SRC_REAL & g_wiimote_sources[number])
      WiimoteReal::ConnectOnInput(number);
  }
}

// Get a mask of attached the pads (eg: controller 1 & 4 -> 0x9)
unsigned int GetAttached()
{
  unsigned int attached = 0;
  for (unsigned int i = 0; i < MAX_BBMOTES; ++i)
    if (g_wiimote_sources[i])
      attached |= (1 << i);
  return attached;
}

// Save/Load state
void DoState(PointerWrap& p)
{
  for (int i = 0; i < MAX_BBMOTES; ++i)
    static_cast<WiimoteEmu::Wiimote*>(s_config.GetController(i))->DoState(p);
}

// Notifies the plugin of a change in emulation state
void EmuStateChange(EMUSTATE_CHANGE newState)
{
  WiimoteReal::StateChange(newState);
}
}
