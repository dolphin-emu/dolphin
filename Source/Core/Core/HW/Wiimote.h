// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/Common.h"
#include "Common/CommonTypes.h"

class InputConfig;
class PointerWrap;

namespace ControllerEmu
{
class ControlGroup;
}

namespace WiimoteEmu
{
enum class WiimoteGroup;
enum class NunchukGroup;
enum class ClassicGroup;
enum class GuitarGroup;
enum class DrumsGroup;
enum class TurntableGroup;
}

enum
{
  WIIMOTE_CHAN_0 = 0,
  WIIMOTE_CHAN_1,
  WIIMOTE_CHAN_2,
  WIIMOTE_CHAN_3,
  WIIMOTE_BALANCE_BOARD,
  MAX_WIIMOTES = WIIMOTE_BALANCE_BOARD,
  MAX_BBMOTES = 5,
};

#define WIIMOTE_INI_NAME "WiimoteNew"

enum
{
  WIIMOTE_SRC_NONE = 0,
  WIIMOTE_SRC_EMU = 1,
  WIIMOTE_SRC_REAL = 2,
  WIIMOTE_SRC_HYBRID = 3,  // emu + real
};

extern unsigned int g_wiimote_sources[MAX_BBMOTES];

namespace Wiimote
{
enum class InitializeMode
{
  DO_WAIT_FOR_WIIMOTES,
  DO_NOT_WAIT_FOR_WIIMOTES,
};

void Shutdown();
void Initialize(InitializeMode init_mode);
void Connect(unsigned int index, bool connect);
void ResetAllWiimotes();
void LoadConfig();
void Resume();
void Pause();

unsigned int GetAttached();
void DoState(PointerWrap& p);
InputConfig* GetConfig();
ControllerEmu::ControlGroup* GetWiimoteGroup(int number, WiimoteEmu::WiimoteGroup group);
ControllerEmu::ControlGroup* GetNunchukGroup(int number, WiimoteEmu::NunchukGroup group);
ControllerEmu::ControlGroup* GetClassicGroup(int number, WiimoteEmu::ClassicGroup group);
ControllerEmu::ControlGroup* GetGuitarGroup(int number, WiimoteEmu::GuitarGroup group);
ControllerEmu::ControlGroup* GetDrumsGroup(int number, WiimoteEmu::DrumsGroup group);
ControllerEmu::ControlGroup* GetTurntableGroup(int number, WiimoteEmu::TurntableGroup group);

void ControlChannel(int number, u16 channel_id, const void* data, u32 size);
void InterruptChannel(int number, u16 channel_id, const void* data, u32 size);
bool ButtonPressed(int number);
void Update(int number, bool connected);
bool NetPlay_GetButtonPress(int wiimote, bool pressed);
}

namespace WiimoteReal
{
void Initialize(::Wiimote::InitializeMode init_mode);
void Stop();
void Shutdown();
void Resume();
void Pause();
void Refresh();

void LoadSettings();
}
