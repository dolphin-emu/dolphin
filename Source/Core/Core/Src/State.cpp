// Copyright (C) 2003-2009 Dolphin Project.

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
#include "Thread.h"
#include "CoreTiming.h"
#include "HW/HW.h"
#include "PowerPC/PowerPC.h"
#include "PowerPC/Jit64/Jit.h"

#include "PluginManager.h"

#include <string>

#include "minilzo.h"

///////////
// TODO: Investigate a memory leak on save/load state
///////////

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


static int ev_Save, ev_BufferSave;
static int ev_Load, ev_BufferLoad;

static std::string cur_filename, lastFilename;
static u8 **cur_buffer = NULL;

// Temporary undo state buffers
static u8 *undoLoad = NULL;

static bool const bCompressed = true;

static Common::Thread *saveThread = NULL;

enum
{
	version = 1,
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
	CPluginManager &pm = CPluginManager::GetInstance();
        pm.GetVideo()->DoState(p.GetPPtr(), p.GetMode());
        pm.GetDSP()->DoState(p.GetPPtr(), p.GetMode());
		pm.GetPad(0)->DoState(p.GetPPtr(), p.GetMode());
	PowerPC::DoState(p);
	HW::DoState(p);
	CoreTiming::DoState(p);
}

void LoadBufferStateCallback(u64 userdata, int cyclesLate)
{
	if(!cur_buffer || !*cur_buffer) {
		Core::DisplayMessage("State does not exist", 1000);
		return;
	}

	jit->ClearCache();

	u8 *ptr = *cur_buffer;
	PointerWrap p(&ptr, PointerWrap::MODE_READ);
	DoState(p);

	cur_buffer = NULL;

	Core::DisplayMessage("Loaded state", 2000);
}

void SaveBufferStateCallback(u64 userdata, int cyclesLate)
{
	size_t sz;

	if(!cur_buffer) {
		Core::DisplayMessage("Error saving state", 1000);
		return;
	}

	jit->ClearCache();

	u8 *ptr = NULL;

	PointerWrap p(&ptr, PointerWrap::MODE_MEASURE);
	DoState(p);
	sz = (size_t)ptr;

	*cur_buffer = new u8[sz];
	ptr = *cur_buffer;
	p.SetMode(PointerWrap::MODE_WRITE);
	DoState(p);

	cur_buffer = NULL;
}

THREAD_RETURN CompressAndDumpState(void *pArgs)
{
	saveStruct *saveArg = (saveStruct *)pArgs;
	u8 *buffer = saveArg->buffer;
	size_t sz = saveArg->size;
	lzo_uint out_len = 0;
	state_header header;
	std::string filename = cur_filename;

	delete saveArg;

	// Moving to last overwritten save-state
	if(File::Exists(cur_filename.c_str())) {
		if(File::Exists(FULL_STATESAVES_DIR "lastState.sav"))
			File::Delete(FULL_STATESAVES_DIR "lastState.sav");

		if(!File::Rename(cur_filename.c_str(), FULL_STATESAVES_DIR "lastState.sav"))
			Core::DisplayMessage("Failed to move previous state to state undo backup", 1000);
	}

	FILE *f = fopen(filename.c_str(), "wb");
	if(f == NULL) {
		Core::DisplayMessage("Could not save state", 2000);
		return 0;
	}

	// Setting up the header
	memcpy(header.gameID, Core::GetStartupParameter().GetUniqueID().c_str(), 6);
	header.sz = bCompressed ? sz : 0;

	fwrite(&header, sizeof(state_header), 1, f);

	if (bCompressed) {
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
		filename.c_str()).c_str(), 2000);

	return 0;
}

void SaveStateCallback(u64 userdata, int cyclesLate)
{
	// If already saving state, wait for it to finish
	if(saveThread)
	{
		delete saveThread;
		saveThread = NULL;
	}

	jit->ClearCache();
	u8 *ptr = 0;
	PointerWrap p(&ptr, PointerWrap::MODE_MEASURE);
	DoState(p);
	size_t sz = (size_t)ptr;
	u8 *buffer = new u8[sz];
	ptr = buffer;
	p.SetMode(PointerWrap::MODE_WRITE);
	DoState(p);

	saveStruct *saveData = new saveStruct;
	saveData->buffer = buffer;
	saveData->size = sz;

	Core::DisplayMessage("Saving State...", 1000);

	saveThread = new Common::Thread(CompressAndDumpState, saveData);
}

void LoadStateCallback(u64 userdata, int cyclesLate)
{
	lzo_uint cur_len;
	lzo_uint new_len;

	bool bCompressedState;

	// If saving state, wait for it to finish
	if(saveThread)
	{
		delete saveThread;
		saveThread = NULL;
	}

	// Save temp buffer for undo load state
	cur_buffer = &undoLoad;
	SaveBufferStateCallback(userdata, cyclesLate);

	FILE *f = fopen(cur_filename.c_str(), "rb");
	if (!f) {
		Core::DisplayMessage("State not found", 2000);
		return;
	}

	u8 *buffer = NULL;
	state_header header;
	size_t sz;

	fread(&header, sizeof(state_header), 1, f);
	
	if(memcmp(Core::GetStartupParameter().GetUniqueID().c_str(), header.gameID, 6)) 
	{
		char gameID[7] = {0};
		memcpy(gameID, header.gameID, 6);
		Core::DisplayMessage(StringFromFormat("State belongs to a different game (ID %s)",
			gameID), 2000);

		fclose(f);

		return;
	}

	sz = header.sz;
	bCompressedState = (sz != 0);

	if (bCompressedState) {
		Core::DisplayMessage("Decompressing State...", 500);

		if (lzo_init() != LZO_E_OK)
			PanicAlert("Internal LZO Error - lzo_init() failed");
		else {
			lzo_uint i = 0;
			buffer = new u8[sz];
			
			for (;;) {
				if (fread(&cur_len, 1, sizeof(int), f) == 0)
					break;
				fread(out, 1, cur_len, f);

				int res = lzo1x_decompress(out, cur_len, (buffer + i), &new_len, NULL);
				if(res != LZO_E_OK) {
					PanicAlert("Internal LZO Error - decompression failed (%d)\n"
						"Try loading the state again", res);
					fclose(f);
					return;
				}
		
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

	jit->ClearCache();

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
	ev_BufferLoad = CoreTiming::RegisterEvent("LoadBufferState", &LoadBufferStateCallback);
	ev_BufferSave = CoreTiming::RegisterEvent("SaveBufferState", &SaveBufferStateCallback);
}

void State_Shutdown()
{
	if(saveThread)
	{
		delete saveThread;
		saveThread = NULL;
	}

	if(undoLoad)
	{
		delete[] undoLoad;
		undoLoad = NULL;
	}
}

std::string MakeStateFilename(int state_number)
{
	return StringFromFormat(FULL_STATESAVES_DIR "%s.s%02i", Core::GetStartupParameter().GetUniqueID().c_str(), state_number);
}

void State_SaveAs(const std::string &filename)
{
	cur_filename = filename;
	lastFilename = filename;
	CoreTiming::ScheduleEvent_Threadsafe(0, ev_Save);
}

void State_Save(int slot)
{
	State_SaveAs(MakeStateFilename(slot));
}

void State_LoadAs(const std::string &filename)
{
	cur_filename = filename;
	CoreTiming::ScheduleEvent_Threadsafe(0, ev_Load);
}

void State_Load(int slot)
{
	State_LoadAs(MakeStateFilename(slot));
}

void State_LoadLastSaved()
{
	if(lastFilename.empty())
		Core::DisplayMessage("There is no last saved state", 2000);
	else
		State_LoadAs(lastFilename);
}

void State_LoadFromBuffer(u8 **buffer)
{
	cur_buffer = buffer;
	CoreTiming::ScheduleEvent_Threadsafe(0, ev_BufferLoad);
}

void State_SaveToBuffer(u8 **buffer)
{
	cur_buffer = buffer;
	CoreTiming::ScheduleEvent_Threadsafe(0, ev_BufferSave);
}

// Load the last state before loading the state
void State_UndoLoadState()
{
	State_LoadFromBuffer(&undoLoad);
}

// Load the state that the last save state overwritten on
void State_UndoSaveState()
{
	State_LoadAs(FULL_STATESAVES_DIR "lastState.sav");
}
