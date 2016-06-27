// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonTypes.h"

enum
{
  HYDRA_BUTTON_START = 0x001,
  HYDRA_BUTTON_UNKNOWN1 = 0x002,
  HYDRA_BUTTON_UNKNOWN2 = 0x004,
  HYDRA_BUTTON_3 = 0x008,
  HYDRA_BUTTON_4 = 0x010,
  HYDRA_BUTTON_1 = 0x020,
  HYDRA_BUTTON_2 = 0x040,
  HYDRA_BUTTON_BUMPER = 0x080,
  HYDRA_BUTTON_STICK = 0x100,
};

typedef struct
{
  float position[3];
  float rotation_matrix[3][3];
  float stick_x, stick_y, trigger;
  u32 buttons;
  u8 sequence_number;
  float quaternion[4];
  u16 version_firmware, version_hardware, type_of_packet, magnetic_freq;
  s32 enabled, controller_index;
  u8 docked, hand, hemisphere_tracking;
} THydraController;

typedef struct
{
  THydraController c[4];
} TAllHydraControllers;

typedef struct
{
  float p[3], v[3], a[3], t;
  float jx, jy, jcx, jcy;
  u32 pressed, released;
} THydraControllerState;

#ifdef _WIN32
typedef int(__cdecl* PHydra_Init)(void);
typedef int(__cdecl* PHydra_Exit)(void);
typedef int(__cdecl* PHydra_GetAllNewestData)(TAllHydraControllers* all_controllers);

void InitSixenseLib();
bool HydraUpdate();

extern bool g_sixense_initialized;
extern THydraControllerState g_hydra_state[2];
extern TAllHydraControllers g_hydra;

extern PHydra_Exit Hydra_Exit;
extern PHydra_GetAllNewestData Hydra_GetAllNewestData;

#else
// Sixense does have an SDK for other platforms, but someone else will have to implement that.
// For now, here are dummy functions in case any other platform includes this file.
typedef int (*PHydra_Init)();
typedef int (*PHydra_Exit)();
typedef int (*PHydra_GetAllNewestData)(TAllHydraControllers* all_controllers);

void InitSixenseLib();
bool g_sixense_initialized = false;
PHydra_Exit Hydra_Exit = nullptr;
PHydra_GetAllNewestData Hydra_GetAllNewestData = nullptr;

THydraControllerState g_hydra_state[2];
TAllHydraControllers g_hydra;

// dummy function stubs:
inline void InitSixenseLib()
{
}

inline bool HydraUpdate()
{
  return false;
}

#endif
