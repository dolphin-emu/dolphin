// Copyright (C) 2003 Dolphin Project.

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
#ifdef JITTEST
#include "PowerPC/Jit64IL/Jit.h"
#else
#include "PowerPC/Jit64/Jit.h"
#endif

#include "PluginManager.h"

#include <string>

#include "minilzo.h"

// TODO: Move to namespace

// TODO: Investigate a memory leak on save/load state

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

static bool state_op_in_progress = false;

static int ev_Save, ev_BufferSave;
static int ev_Load, ev_BufferLoad;

static std::string cur_filename, lastFilename;
static u8 **cur_buffer = NULL;

// Temporary undo state buffers
static u8 *undoLoad = NULL;

static bool const bCompressed = true;

static Common::Thread *saveThread = NULL;


// Don't forget to increase this after doing changes on the savestate system 
#define STATE_VERSION 2


void DoState(PointerWrap &p)
{
	u32 cookie = 0xBAADBABE + STATE_VERSION;
	p.Do(cookie);
	if (cookie != 0xBAADBABE + STATE_VERSION)
	{
		//PanicAlert("Savestate version mismatch !\nSorry, you can't load states from other revisions.");
		p.SetMode(PointerWrap::MODE_MEASURE);
		return;
	}
	// Begin with video plugin, so that it gets a chance to clear it's caches and writeback modified things to RAM
	CPluginManager &pm = CPluginManager::GetInstance();
        pm.GetVideo()->DoState(p.GetPPtr(), p.GetMode());
        pm.GetDSP()->DoState(p.GetPPtr(), p.GetMode());
		pm.GetPad(0)->DoState(p.GetPPtr(), p.GetMode());
		if (Core::g_CoreStartupParameter.bWii)
			pm.GetWiimote(0)->DoState(p.GetPPtr(), p.GetMode());
	PowerPC::DoState(p);
	HW::DoState(p);
	CoreTiming::DoState(p);
#ifdef JIT_UNLIMITED_ICACHE	
	p.DoVoid(jit.GetBlockCache()->GetICache(), JIT_ICACHE_SIZE);
	p.DoVoid(jit.GetBlockCache()->GetICacheEx(), JIT_ICACHEEX_SIZE);
	p.DoVoid(jit.GetBlockCache()->GetICacheVMEM(), JIT_ICACHE_SIZE);
#endif
}

void LoadBufferStateCallback(u64 userdata, int cyclesLate)
{
	if (!cur_buffer || !*cur_buffer) {
		Core::DisplayMessage("State does not exist", 1000);
		return;
	}

	jit.ClearCache();

	u8 *ptr = *cur_buffer;
	PointerWrap p(&ptr, PointerWrap::MODE_READ);
	DoState(p);

	Core::DisplayMessage("Loaded state", 2000);
	state_op_in_progress = false;
}

void SaveBufferStateCallback(u64 userdata, int cyclesLate)
{
	size_t sz;

	if (!cur_buffer) {
		Core::DisplayMessage("Error saving state", 1000);
		return;
	}

	jit.ClearCache();

	u8 *ptr = NULL;

	PointerWrap p(&ptr, PointerWrap::MODE_MEASURE);
	DoState(p);
	sz = (size_t)ptr;

	if (*cur_buffer)
		delete [] (*cur_buffer);

	*cur_buffer = new u8[sz];
	ptr = *cur_buffer;
	p.SetMode(PointerWrap::MODE_WRITE);
	DoState(p);

	state_op_in_progress = false;
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
	if (File::Exists(cur_filename.c_str())) {
		if (File::Exists(FULL_STATESAVES_DIR "lastState.sav"))
			File::Delete(FULL_STATESAVES_DIR "lastState.sav");

		if (!File::Rename(cur_filename.c_str(), FULL_STATESAVES_DIR "lastState.sav"))
			Core::DisplayMessage("Failed to move previous state to state undo backup", 1000);
	}

	FILE *f = fopen(filename.c_str(), "wb");
	if (f == NULL) {
		Core::DisplayMessage("Could not save state", 2000);
		delete [] buffer;
		return 0;
	}

	// Setting up the header
	memcpy(header.gameID, Core::GetStartupParameter().GetUniqueID().c_str(), 6);
	header.sz = bCompressed ? sz : 0;

	fwrite(&header, sizeof(state_header), 1, f);
	if (bCompressed) {
		lzo_uint cur_len = 0;
		lzo_uint i = 0;

		for (;;) {
			if ((i + IN_LEN) >= sz)
				cur_len = sz - i;
			else
				cur_len = IN_LEN;

			if (lzo1x_1_compress((buffer + i), cur_len, out, &out_len, wrkmem) != LZO_E_OK)
				PanicAlert("Internal LZO Error - compression failed");

			// The size of the data to write is 'out_len'
			fwrite(&out_len, sizeof(int), 1, f);
			fwrite(out, out_len, 1, f);

			if (cur_len != IN_LEN)
				break;
			i += cur_len;
		}

	}
	else
	{
		fwrite(buffer, sz, 1, f);
	}

	fclose(f);

	delete [] buffer;

	Core::DisplayMessage(StringFromFormat("Saved State to %s",
		filename.c_str()).c_str(), 2000);

	state_op_in_progress = false;
	return 0;
}

void SaveStateCallback(u64 userdata, int cyclesLate)
{
	// If already saving state, wait for it to finish
	if (saveThread)
	{
		delete saveThread;
		saveThread = NULL;
	}

	jit.ClearCache();

	// Measure the size of the buffer.
	u8 *ptr = 0;
	PointerWrap p(&ptr, PointerWrap::MODE_MEASURE);
	DoState(p);
	size_t sz = (size_t)ptr;

	// Then actually do the write.
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
	bool bCompressedState;

	// If saving state, wait for it to finish
	if (saveThread)
	{
		delete saveThread;
		saveThread = NULL;
	}

	// Save temp buffer for undo load state
	cur_buffer = &undoLoad;
	SaveBufferStateCallback(userdata, cyclesLate);

	FILE *f = fopen(cur_filename.c_str(), "rb");
	if (!f)
	{
		Core::DisplayMessage("State not found", 2000);
		return;
	}

	u8 *buffer = NULL;
	state_header header;
	size_t sz;

	fread(&header, sizeof(state_header), 1, f);
	
	if (memcmp(Core::GetStartupParameter().GetUniqueID().c_str(), header.gameID, 6)) 
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
	if (bCompressedState)
	{
		Core::DisplayMessage("Decompressing State...", 500);

		lzo_uint i = 0;
		buffer = new u8[sz];
		if (!buffer) {
			PanicAlert("Error allocating buffer");
			return;
		}
		while (true)
		{
			lzo_uint cur_len = 0;
			lzo_uint new_len = 0;
			if (fread(&cur_len, 1, sizeof(int), f) == 0)
				break;
			if (feof(f))
				break;  // don't know if this happens.
			fread(out, 1, cur_len, f);
			int res = lzo1x_decompress(out, cur_len, (buffer + i), &new_len, NULL);
			if (res != LZO_E_OK)
			{
				// This doesn't seem to happen anymore.
				PanicAlert("Internal LZO Error - decompression failed (%d) (%d, %d) \n"
					"Try loading the state again", res, i, new_len);
				fclose(f);
				delete [] buffer;
				return;
			}
	
			// The size of the data to read to our buffer is 'new_len'
			i += new_len;
		}
	}
	else
	{
		fseek(f, 0, SEEK_END);
		sz = (int)(ftell(f) - sizeof(int));
		fseek(f, sizeof(int), SEEK_SET);
		buffer = new u8[sz];
		int x;
		if ((x = (int)fread(buffer, 1, sz, f)) != (int)sz)
			PanicAlert("wtf? %d %d", x, sz);
	}

	fclose(f);

	jit.ClearCache();

	u8 *ptr = buffer;
	PointerWrap p(&ptr, PointerWrap::MODE_READ);
	DoState(p);

	if (p.GetMode() == PointerWrap::MODE_READ)
		Core::DisplayMessage(StringFromFormat("Loaded state from %s", cur_filename.c_str()).c_str(), 2000);
	else
		Core::DisplayMessage("Unable to Load : Can't load state from other revisions !", 4000);

	delete [] buffer;
}

void State_Init()
{
	ev_Load = CoreTiming::RegisterEvent("LoadState", &LoadStateCallback);
	ev_Save = CoreTiming::RegisterEvent("SaveState", &SaveStateCallback);
	ev_BufferLoad = CoreTiming::RegisterEvent("LoadBufferState", &LoadBufferStateCallback);
	ev_BufferSave = CoreTiming::RegisterEvent("SaveBufferState", &SaveBufferStateCallback);

	if (lzo_init() != LZO_E_OK)
		PanicAlert("Internal LZO Error - lzo_init() failed");
}

void State_Shutdown()
{
	if (saveThread)
	{
		delete saveThread;
		saveThread = NULL;
	}

	if (undoLoad)
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
	if (state_op_in_progress)
		return;
	state_op_in_progress = true;
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
	if (state_op_in_progress)
		return;
	state_op_in_progress = true;
	cur_filename = filename;
	CoreTiming::ScheduleEvent_Threadsafe(0, ev_Load);
}

void State_Load(int slot)
{
	State_LoadAs(MakeStateFilename(slot));
}

void State_LoadLastSaved()
{
	if (lastFilename.empty())
		Core::DisplayMessage("There is no last saved state", 2000);
	else
		State_LoadAs(lastFilename);
}

void State_LoadFromBuffer(u8 **buffer)
{
	if (state_op_in_progress)
		return;
	state_op_in_progress = true;
	cur_buffer = buffer;
	CoreTiming::ScheduleEvent_Threadsafe(0, ev_BufferLoad);
}

void State_SaveToBuffer(u8 **buffer)
{
	if (state_op_in_progress)
		return;
	state_op_in_progress = true;
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

size_t State_GetSize()
{
	// Measure the size of the buffer.
	u8 *ptr = 0;
	PointerWrap p(&ptr, PointerWrap::MODE_MEASURE);
	DoState(p);
	return (size_t)ptr;
}
