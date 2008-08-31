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

static bool const bCompressed = false;

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
	static const int chunkSize = 16384;
	z_stream strm;
	unsigned char outbuf[chunkSize] = {0}, inbuf[chunkSize] = {0};

	FILE *f = fopen(cur_filename.c_str(), "wb");
	if(f == NULL) {
		Core::DisplayMessage("Could not save state", 2000);
		return;
	}

	Jit64::ClearCache();
	u8 *ptr = 0;
	PointerWrap p(&ptr, PointerWrap::MODE_MEASURE);
	DoState(p);
	int sz = (int)(u64)ptr;
	u8 *buffer = new u8[sz];
	ptr = buffer;
	p.SetMode(PointerWrap::MODE_WRITE);
	DoState(p);

	if(bCompressed)
		fwrite(&sz, sizeof(int), 1, f);
	else {
		int zero = 0;
		fwrite(&zero, sizeof(int), 1, f);
	}


	if(bCompressed) {
		int chunks = sz / chunkSize, leftovers = sz % chunkSize;

		strm.zalloc = Z_NULL;
		strm.zfree = Z_NULL;
		strm.opaque = Z_NULL;
		deflateInit(&strm, Z_BEST_SPEED);

		for(int i = 0; i < chunks; i++) {
			strm.avail_in = chunkSize;
			memcpy(inbuf, buffer + i * chunkSize, chunkSize);
			strm.next_in = inbuf;
			memcpy(outbuf, buffer + i * chunkSize, chunkSize);
			strm.avail_out = chunkSize;
			strm.next_out = outbuf;
			deflate(&strm, Z_NO_FLUSH);
			fwrite(outbuf, 1, chunkSize - strm.avail_out, f);
		}

		strm.avail_in = leftovers;
		memcpy(inbuf, buffer + chunks * chunkSize, leftovers);
		strm.next_in = inbuf;
		memcpy(outbuf, buffer + chunks * chunkSize, leftovers);
		strm.avail_out = leftovers;
		strm.next_out = outbuf;
		deflate(&strm, Z_NO_FLUSH);
		fwrite(outbuf, 1, leftovers - strm.avail_out, f);

		(void)deflateEnd(&strm);
	} else
		fwrite(buffer, sz, 1, f);

	fclose(f);

	delete [] buffer;

	Core::DisplayMessage(StringFromFormat("Saved State to %s", 
		                 cur_filename.c_str()).c_str(), 2000);
}

void LoadStateCallback(u64 userdata, int cyclesLate)
{
	static const int chunkSize = 16384;
	z_stream strm;
	unsigned char outbuf[chunkSize] = {0}, inbuf[chunkSize] = {0};

	bool bCompressedState;

	FILE *f = fopen(cur_filename.c_str(), "rb");
	if (!f) {
		Core::DisplayMessage("State not found", 2000);
		return;
	}
	
	Jit64::ClearCache();

	u8 *buffer = NULL;

	int sz;
	fread(&sz, sizeof(int), 1, f);

	bCompressedState = (sz != 0);

	if(bCompressedState) {
		buffer = new u8[sz];

		int ret;
		strm.zalloc = Z_NULL;
		strm.zfree = Z_NULL;
		strm.opaque = Z_NULL;
		strm.avail_in = 0;
		strm.next_in = Z_NULL;
		ret = inflateInit(&strm);

		int cnt = 0;
		do {
			strm.avail_in = uInt(fread(inbuf, 1, chunkSize, f));
			if (strm.avail_in == 0)
				break;
			strm.next_in = inbuf;

			do {
				strm.avail_out = chunkSize;
				strm.next_out = outbuf;
				ret = inflate(&strm, Z_NO_FLUSH);

				int have = chunkSize - strm.avail_out;
				
				memcpy(buffer + cnt, outbuf, have);
				cnt += have;
			} while (strm.avail_out == 0);
		} while (ret != Z_STREAM_END);

		(void)inflateEnd(&strm);
	} else {
		fseek(f, 0, SEEK_END);
		sz = ftell(f) - sizeof(int);
		fseek(f, sizeof(int), SEEK_SET);

		buffer = new u8[sz];

		int x;
		if (x=fread(buffer, 1, sz, f) != sz)
			PanicAlert("wtf? %d %d", x, sz);
	}

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
