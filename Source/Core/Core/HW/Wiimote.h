// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <atomic>

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
enum class UDrawTabletGroup;
enum class DrawsomeTabletGroup;
enum class TaTaConGroup;
}  // namespace WiimoteEmu

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
};

extern std::array<std::atomic<u32>, MAX_BBMOTES> g_wiimote_sources;

namespace Wiimote
{
enum class InitializeMode
{
  DO_WAIT_FOR_WIIMOTES,
  DO_NOT_WAIT_FOR_WIIMOTES,
};

// The Real Wii Remote sends report every ~5ms (200 Hz).
constexpr int UPDATE_FREQ = 200;

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
ControllerEmu::ControlGroup* GetUDrawTabletGroup(int number, WiimoteEmu::UDrawTabletGroup group);
ControllerEmu::ControlGroup* GetDrawsomeTabletGroup(int number,
                                                    WiimoteEmu::DrawsomeTabletGroup group);
ControllerEmu::ControlGroup* GetTaTaConGroup(int number, WiimoteEmu::TaTaConGroup group);

void ControlChannel(int number, u16 channel_id, const void* data, u32 size);
void InterruptChannel(int number, u16 channel_id, const void* data, u32 size);
bool ButtonPressed(int number);
void Update(int number, bool connected);
bool NetPlay_GetButtonPress(int wiimote, bool pressed);
}  // namespace Wiimote

namespace WiimoteReal
{
void Initialize(::Wiimote::InitializeMode init_mode);
void Stop();
void Shutdown();
void Resume();
void Pause();
void Refresh();

void LoadSettings();
}  // namespace WiimoteReal
