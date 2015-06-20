// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

// This is the library to communicate with Dolphin.

#include <cstdint>

#ifdef _WIN32

#include <tchar.h>

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t  s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

#else

#ifndef GEKKO

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t  s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

#endif
// For using Windows lock code
#define TCHAR char
#define LONG int

#endif // _WIN32


enum PadError
{
	PAD_ERR_NONE = 0,
	PAD_ERR_NO_CONTROLLER = -1,
	PAD_ERR_NOT_READY = -2,
	PAD_ERR_TRANSFER = -3,
};

enum
{
	PAD_USE_ORIGIN = 0x0080,
};

enum PadButton
{
	PAD_BUTTON_LEFT = 0x0001,
	PAD_BUTTON_RIGHT = 0x0002,
	PAD_BUTTON_DOWN = 0x0004,
	PAD_BUTTON_UP = 0x0008,
	PAD_TRIGGER_Z = 0x0010,
	PAD_TRIGGER_R = 0x0020,
	PAD_TRIGGER_L = 0x0040,
	PAD_BUTTON_A = 0x0100,
	PAD_BUTTON_B = 0x0200,
	PAD_BUTTON_X = 0x0400,
	PAD_BUTTON_Y = 0x0800,
	PAD_BUTTON_START = 0x1000,
};

struct GCPadStatus
{
	u16 button;                 // Or-ed PAD_BUTTON_* and PAD_TRIGGER_* bits
	u8  stickX;                 // 0 <= stickX       <= 255
	u8  stickY;                 // 0 <= stickY       <= 255
	u8  substickX;              // 0 <= substickX    <= 255
	u8  substickY;              // 0 <= substickY    <= 255
	u8  triggerLeft;            // 0 <= triggerLeft  <= 255
	u8  triggerRight;           // 0 <= triggerRight <= 255
	u8  analogA;                // 0 <= analogA      <= 255
	u8  analogB;                // 0 <= analogB      <= 255
	s8  err;                    // one of PAD_ERR_* number

	static const u8 MAIN_STICK_CENTER_X = 0x80;
	static const u8 MAIN_STICK_CENTER_Y = 0x80;
	static const u8 MAIN_STICK_RADIUS = 0x7f;
	static const u8 C_STICK_CENTER_X = 0x80;
	static const u8 C_STICK_CENTER_Y = 0x80;
	static const u8 C_STICK_RADIUS = 0x7f;
};

#define PIPE_NAME "\\\\.\\pipe\\DolphinIPC"
#define EMPTY_CONTROL GCPadStatus()

namespace Dolphin
{
	bool   Init();
	void   SetFrameCallback(void(*)());
	int    StartLoop();

	u8     Read8(u32);
	u16    Read16(u32);
	u32    Read32(u32);
	u64    Read64(u32);

	void   Write8(u32, u8);
	void   Write16(u32, u16);
	void   Write32(u32, u32);
	void   Write64(u32, u64);

	void   QueueInput(struct GCPadStatus);
	u64*   InterpretInput(struct GCPadStatus);
};
