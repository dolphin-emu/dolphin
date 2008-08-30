// Copyright (C) 2003-2008 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

#include "Common.h"

#include "State.h"
#include "Core.h"
#include "StringUtil.h"
#include "CoreTiming.h"
#include "HW/HW.h"
#include "PowerPC/PowerPC.h"
#include "PowerPC/Jit64/JitCache.h"

#include "Plugins/Plugin_Video.h"
#include "Plugins/Plugin_DSP.h"

#include <string>

#include "zlib.h"

static int ev_Save;
static int ev_Load;

static std::string cur_filename;

enum {
	version = 1
};

void DoState(PointerWrap &p)
{
	u32 cookie = 0xBAADBABE + version;
	p.Do(cookie);
	if (cookie != 0xBAADBABE + version) {
		PanicAlert("Can't load states from other versions.");
		return;
	}
	// Begin with video plugin, so that it gets a chance to clear it's caches and writeback modified things to RAM
    PluginVideo::Video_DoState(p.GetPPtr(), p.GetMode());
    PluginDSP::DSP_DoState(p.GetPPtr(), p.GetMode());
	PowerPC::DoState(p);
	HW::DoState(p);
	CoreTiming::DoState(p);
}

void SaveStateCallback(u64 userdata, int cyclesLate)
{
	Jit64::ClearCache();
	u8 *ptr = 0;
	PointerWrap p(&ptr, PointerWrap::MODE_MEASURE);
	DoState(p);
	int sz = (int)(u64)ptr;
	u8 *buffer = new u8[sz];
	ptr = buffer;
	p.SetMode(PointerWrap::MODE_WRITE);
	DoState(p);
	FILE *f = fopen(cur_filename.c_str(), "wb");
	fwrite(buffer, sz, 1, f);
	fclose(f);

	delete [] buffer;

	Core::DisplayMessage(StringFromFormat("Saved State to %s", cur_filename.c_str()).c_str(), 2000);
}

void LoadStateCallback(u64 userdata, int cyclesLate)
{
	FILE *f = fopen(cur_filename.c_str(), "rb");
	if (!f) {
		Core::DisplayMessage("State not found", 2000);
		return;
	}
	Jit64::ClearCache();
	fseek(f, 0, SEEK_END);
	int sz = ftell(f);
	fseek(f, 0, SEEK_SET);
	u8 *buffer = new u8[sz];
	int x;
	if (x=fread(buffer, 1, sz, f) != sz)
		PanicAlert("wtf? %d %d", x, sz);
	fclose(f);

	u8 *ptr = buffer;
	PointerWrap p(&ptr, PointerWrap::MODE_READ);
	DoState(p);
	delete [] buffer;

	Core::DisplayMessage(StringFromFormat("Loaded state from %s", cur_filename.c_str()).c_str(), 2000);
}

void State_Init()
{
	ev_Load = CoreTiming::RegisterEvent("LoadState", &LoadStateCallback);
	ev_Save = CoreTiming::RegisterEvent("SaveState", &SaveStateCallback);
}

void State_Shutdown()
{
	// nothing to do, here for consistency.
}

std::string MakeStateFilename(int state_number) {
	return StringFromFormat("StateSaves/%s.s%02i", Core::GetStartupParameter().GetUniqueID().c_str(), state_number);
}

void State_Save(int slot)
{
	cur_filename = MakeStateFilename(slot);
	CoreTiming::ScheduleEvent_Threadsafe(0, ev_Save);
}

void State_Load(int slot)
{
	cur_filename = MakeStateFilename(slot);
	CoreTiming::ScheduleEvent_Threadsafe(0, ev_Load);
}
