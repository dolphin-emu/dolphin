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

#include "minilzo.h"


#if defined(__LZO_STRICT_16BIT)
#define IN_LEN      (8*1024u)
#elif defined(LZO_ARCH_I086) && !defined(LZO_HAVE_MM_HUGE_ARRAY)
#define IN_LEN      (60*1024u)
#else
#define IN_LEN      (128*1024ul)
#endif
#define OUT_LEN     (IN_LEN + IN_LEN / 16 + 64 + 3)

static unsigned char __LZO_MMODEL out [ OUT_LEN ];

#define HEAP_ALLOC(var,size) \
	lzo_align_t __LZO_MMODEL var [ ((size) + (sizeof(lzo_align_t) - 1)) / sizeof(lzo_align_t) ]

static HEAP_ALLOC(wrkmem,LZO1X_1_MEM_COMPRESS);


static int ev_Save;
static int ev_Load;

static std::string cur_filename;

static bool const bCompressed = true;

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
	lzo_uint out_len = 0;

	FILE *f = fopen(cur_filename.c_str(), "wb");
	if(f == NULL) {
		Core::DisplayMessage("Could not save state", 2000);
		return;
	}

	Jit64::ClearCache();
	u8 *ptr = 0;
	PointerWrap p(&ptr, PointerWrap::MODE_MEASURE);
	DoState(p);
	size_t sz = (size_t)ptr;
	u8 *buffer = new u8[sz];
	ptr = buffer;
	p.SetMode(PointerWrap::MODE_WRITE);
	DoState(p);

	if(bCompressed) {
		fwrite(&sz, sizeof(int), 1, f);
	} else {
		int zero = 0;
		fwrite(&zero, sizeof(int), 1, f);
	}


	if(bCompressed) {
		if (lzo_init() != LZO_E_OK)
			PanicAlert("Internal LZO Error - lzo_init() failed");
		else {
			lzo_uint cur_len;
			lzo_uint i = 0;

			for(;;) {
				if((i + IN_LEN) >= sz)
					cur_len = sz - i;
				else
					cur_len = IN_LEN;

				if(lzo1x_1_compress((buffer + i), cur_len, out, &out_len, wrkmem) != LZO_E_OK)
					PanicAlert("Internal LZO Error - compression failed");

				// The size of the data to write is 'out_len'
				fwrite(&out_len, sizeof(int), 1, f);
				fwrite(out, out_len, 1, f);

				if(cur_len != IN_LEN)
					break;
				i += cur_len;
			}
		}
	} else
		fwrite(buffer, sz, 1, f);

	fclose(f);

	delete [] buffer;

	Core::DisplayMessage(StringFromFormat("Saved State to %s", 
		                 cur_filename.c_str()).c_str(), 2000);
}

void LoadStateCallback(u64 userdata, int cyclesLate)
{
	lzo_uint cur_len;
	lzo_uint new_len;

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
		if (lzo_init() != LZO_E_OK)
			PanicAlert("Internal LZO Error - lzo_init() failed");
		else {
			lzo_uint i = 0;
			buffer = new u8[sz];
			
			for(;;) {
				if(fread(&cur_len, 1, sizeof(int), f) == 0)
					break;
				fread(out, 1, cur_len, f);

				int res = lzo1x_decompress(out, cur_len, (buffer + i), &new_len, NULL);
				if(res != LZO_E_OK)
					PanicAlert("Internal LZO Error - decompression failed (%d)", res);
		
				// The size of the data to read to our buffer is 'new_len'
				i += new_len;
			}
		}
	} else {
		fseek(f, 0, SEEK_END);
		sz = (int)(ftell(f) - sizeof(int));
		fseek(f, sizeof(int), SEEK_SET);

		buffer = new u8[sz];

		int x;
		if ((x = (int)fread(buffer, 1, sz, f)) != sz)
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
