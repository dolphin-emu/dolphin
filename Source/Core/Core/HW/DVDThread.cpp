// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cinttypes>

#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "Common/MsgHandler.h"

#include "Core/CoreTiming.h"
#include "Core/HW/DVDInterface.h"
#include "Core/HW/DVDThread.h"

#include "DiscIO/Volume.h"

namespace DVDThread
{

static void FinishRead(u64 userdata, int cyclesLate);
static int s_finish_read;

static u64 s_dvd_offset;
static u32 s_output_address;
static u32 s_length;
static bool s_decrypt;

// Used to notify emulated software after executing command.
// Pointers don't work with savestates, so CoreTiming events are used instead
static int s_callback_event_type;

void Start()
{
	s_finish_read = CoreTiming::RegisterEvent("FinishReadDVDThread", FinishRead);
}

void Stop()
{

}

void DoState(PointerWrap &p)
{
	p.Do(s_dvd_offset);
	p.Do(s_output_address);
	p.Do(s_length);
	p.Do(s_decrypt);
	p.Do(s_callback_event_type);
}

void StartRead(u64 dvd_offset, u32 output_address, u32 length, bool decrypt,
               int callback_event_type, int ticks_until_completion)
{
	s_dvd_offset = dvd_offset;
	s_output_address = output_address;
	s_length = length;
	s_decrypt = decrypt;
	s_callback_event_type = callback_event_type;
	CoreTiming::ScheduleEvent(ticks_until_completion, s_finish_read);
}

static void FinishRead(u64 userdata, int cyclesLate)
{
	// Here is the actual disc reading
	if (!DVDInterface::DVDRead(s_dvd_offset, s_output_address, s_length, s_decrypt))
	{
		PanicAlertT("The disc could not be read (at 0x%" PRIx64 " - 0x%" PRIx64 ").",
		            s_dvd_offset, s_dvd_offset + s_length);
	}

	// Notify the emulated software that the command has been executed
	CoreTiming::ScheduleEvent_Immediate(s_callback_event_type, DVDInterface::INT_TCINT);
}

}
