// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <mutex>
#include <thread>
#include <lzo/lzo1x.h>

#include "Common/CommonTypes.h"
#include "Common/Event.h"
#include "Common/StringUtil.h"
#include "Common/Timer.h"

#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/CoreTiming.h"
#include "Core/Movie.h"
#include "Core/State.h"
#include "Core/HW/CPU.h"
#include "Core/HW/DSP.h"
#include "Core/HW/HW.h"
#include "Core/HW/Memmap.h"
#include "Core/HW/SystemTimers.h"
#include "Core/HW/VideoInterface.h"
#include "Core/HW/Wiimote.h"
#include "Core/PowerPC/JitCommon/JitBase.h"

#include "VideoCommon/VideoBackendBase.h"

namespace State
{

#if defined(__LZO_STRICT_16BIT)
static const u32 IN_LEN = 8 * 1024u;
#elif defined(LZO_ARCH_I086) && !defined(LZO_HAVE_MM_HUGE_ARRAY)
static const u32 IN_LEN = 60 * 1024u;
#else
static const u32 IN_LEN = 128 * 1024u;
#endif

static const u32 OUT_LEN = IN_LEN + (IN_LEN / 16) + 64 + 3;

static unsigned char __LZO_MMODEL out[OUT_LEN];

#define HEAP_ALLOC(var, size) \
	lzo_align_t __LZO_MMODEL var[((size) + (sizeof(lzo_align_t) - 1)) / sizeof(lzo_align_t)]

static HEAP_ALLOC(wrkmem, LZO1X_1_MEM_COMPRESS);

static std::string g_last_filename;

static CallbackFunc g_onAfterLoadCb = nullptr;

// Temporary undo state buffer
static std::vector<u8> g_undo_load_buffer;
static std::vector<u8> g_current_buffer;
static int g_loadDepth = 0;

static std::mutex g_cs_undo_load_buffer;
static std::mutex g_cs_current_buffer;
static Common::Event g_compressAndDumpStateSyncEvent;

static std::thread g_save_thread;

// Don't forget to increase this after doing changes on the savestate system
static const u32 STATE_VERSION = 35;

enum
{
	STATE_NONE = 0,
	STATE_SAVE = 1,
	STATE_LOAD = 2,
};

static bool g_use_compression = true;

void EnableCompression(bool compression)
{
	g_use_compression = compression;
}

static void DoState(PointerWrap &p)
{
	u32 version = STATE_VERSION;
	{
		static const u32 COOKIE_BASE = 0xBAADBABE;
		u32 cookie = version + COOKIE_BASE;
		p.Do(cookie);
		version = cookie - COOKIE_BASE;
	}

	if (version != STATE_VERSION)
	{
		// if the version doesn't match, fail.
		// this will trigger a message like "Can't load state from other revisions"
		// we could use the version numbers to maintain some level of backward compatibility, but currently don't.
		p.SetMode(PointerWrap::MODE_MEASURE);
		return;
	}

	p.DoMarker("Version");

	// Begin with video backend, so that it gets a chance to clear it's caches and writeback modified things to RAM
	g_video_backend->DoState(p);
	p.DoMarker("video_backend");

	if (SConfig::GetInstance().m_LocalCoreStartupParameter.bWii)
		Wiimote::DoState(p.GetPPtr(), p.GetMode());
	p.DoMarker("Wiimote");

	PowerPC::DoState(p);
	p.DoMarker("PowerPC");
	HW::DoState(p);
	p.DoMarker("HW");
	CoreTiming::DoState(p);
	p.DoMarker("CoreTiming");
	Movie::DoState(p);
	p.DoMarker("Movie");
}

void LoadFromBuffer(std::vector<u8>& buffer)
{
	bool wasUnpaused = Core::PauseAndLock(true);

	u8* ptr = &buffer[0];
	PointerWrap p(&ptr, PointerWrap::MODE_READ);
	DoState(p);

	Core::PauseAndLock(false, wasUnpaused);
}

void SaveToBuffer(std::vector<u8>& buffer)
{
	bool wasUnpaused = Core::PauseAndLock(true);

	u8* ptr = nullptr;
	PointerWrap p(&ptr, PointerWrap::MODE_MEASURE);

	DoState(p);
	const size_t buffer_size = reinterpret_cast<size_t>(ptr);
	buffer.resize(buffer_size);

	ptr = &buffer[0];
	p.SetMode(PointerWrap::MODE_WRITE);
	DoState(p);

	Core::PauseAndLock(false, wasUnpaused);
}

void VerifyBuffer(std::vector<u8>& buffer)
{
	bool wasUnpaused = Core::PauseAndLock(true);

	u8* ptr = &buffer[0];
	PointerWrap p(&ptr, PointerWrap::MODE_VERIFY);
	DoState(p);

	Core::PauseAndLock(false, wasUnpaused);
}

// return state number not in map
static int GetEmptySlot(std::map<double, int> m)
{
	for (int i = 1; i <= (int)NUM_STATES; i++)
	{
		bool found = false;
		for (auto& p : m)
		{
			if (p.second == i)
			{
				found = true;
				break;
			}
		}
		if (!found) return i;
	}
	return -1;
}

static std::string MakeStateFilename(int number);

// read state timestamps
static std::map<double, int> GetSavedStates()
{
	StateHeader header;
	std::map<double, int> m;
	for (int i = 1; i <= (int)NUM_STATES; i++)
	{
		std::string filename = MakeStateFilename(i);
		if (File::Exists(filename))
		{
			if (ReadHeader(filename, header))
			{
				double d = Common::Timer::GetDoubleTime() - header.time;
				// increase time until unique value is obtained
				while (m.find(d) != m.end()) d += .001;
				m.insert(std::pair<double,int>(d, i));
			}
		}
	}
	return m;
}

struct CompressAndDumpState_args
{
	std::vector<u8>* buffer_vector;
	std::mutex* buffer_mutex;
	std::string filename;
	bool wait;
};

static void CompressAndDumpState(CompressAndDumpState_args save_args)
{
	std::lock_guard<std::mutex> lk(*save_args.buffer_mutex);
	if (!save_args.wait)
		g_compressAndDumpStateSyncEvent.Set();

	const u8* const buffer_data = &(*(save_args.buffer_vector))[0];
	const size_t buffer_size = (save_args.buffer_vector)->size();
	std::string& filename = save_args.filename;

	// For easy debugging
	Common::SetCurrentThreadName("SaveState thread");

	// Moving to last overwritten save-state
	if (File::Exists(filename))
	{
		if (File::Exists(File::GetUserPath(D_STATESAVES_IDX) + "lastState.sav"))
			File::Delete((File::GetUserPath(D_STATESAVES_IDX) + "lastState.sav"));
		if (File::Exists(File::GetUserPath(D_STATESAVES_IDX) + "lastState.sav.dtm"))
			File::Delete((File::GetUserPath(D_STATESAVES_IDX) + "lastState.sav.dtm"));

		if (!File::Rename(filename, File::GetUserPath(D_STATESAVES_IDX) + "lastState.sav"))
			Core::DisplayMessage("Failed to move previous state to state undo backup", 1000);
		else
			File::Rename(filename + ".dtm", File::GetUserPath(D_STATESAVES_IDX) + "lastState.sav.dtm");
	}

	if ((Movie::IsMovieActive()) && !Movie::IsJustStartingRecordingInputFromSaveState())
		Movie::SaveRecording(filename + ".dtm");
	else if (!Movie::IsMovieActive())
		File::Delete(filename + ".dtm");

	File::IOFile f(filename, "wb");
	if (!f)
	{
		Core::DisplayMessage("Could not save state", 2000);
		g_compressAndDumpStateSyncEvent.Set();
		return;
	}

	// Setting up the header
	StateHeader header;
	memcpy(header.gameID, SConfig::GetInstance().m_LocalCoreStartupParameter.GetUniqueID().c_str(), 6);
	header.size = g_use_compression ? (u32)buffer_size : 0;
	header.time = Common::Timer::GetDoubleTime();

	f.WriteArray(&header, 1);

	if (header.size != 0) // non-zero header size means the state is compressed
	{
		lzo_uint i = 0;
		while (true)
		{
			lzo_uint32 cur_len = 0;
			lzo_uint out_len = 0;

			if ((i + IN_LEN) >= buffer_size)
			{
				cur_len = (lzo_uint32)(buffer_size - i);
			}
			else
			{
				cur_len = IN_LEN;
			}

			if (lzo1x_1_compress(buffer_data + i, cur_len, out, &out_len, wrkmem) != LZO_E_OK)
				PanicAlertT("Internal LZO Error - compression failed");

			// The size of the data to write is 'out_len'
			f.WriteArray((lzo_uint32*)&out_len, 1);
			f.WriteBytes(out, out_len);

			if (cur_len != IN_LEN)
				break;

			i += cur_len;
		}
	}
	else // uncompressed
	{
		f.WriteBytes(buffer_data, buffer_size);
	}

	Core::DisplayMessage(StringFromFormat("Saved State to %s", filename.c_str()), 2000);
	g_compressAndDumpStateSyncEvent.Set();
}

void SaveAs(const std::string& filename, bool wait)
{
	// Pause the core while we save the state
	bool wasUnpaused = Core::PauseAndLock(true);

	// Measure the size of the buffer.
	u8 *ptr = nullptr;
	PointerWrap p(&ptr, PointerWrap::MODE_MEASURE);
	DoState(p);
	const size_t buffer_size = reinterpret_cast<size_t>(ptr);

	// Then actually do the write.
	{
		std::lock_guard<std::mutex> lk(g_cs_current_buffer);
		g_current_buffer.resize(buffer_size);
		ptr = &g_current_buffer[0];
		p.SetMode(PointerWrap::MODE_WRITE);
		DoState(p);
	}

	if (p.GetMode() == PointerWrap::MODE_WRITE)
	{
		Core::DisplayMessage("Saving State...", 1000);

		CompressAndDumpState_args save_args;
		save_args.buffer_vector = &g_current_buffer;
		save_args.buffer_mutex = &g_cs_current_buffer;
		save_args.filename = filename;
		save_args.wait = wait;

		Flush();
		g_save_thread = std::thread(CompressAndDumpState, save_args);
		g_compressAndDumpStateSyncEvent.Wait();

		g_last_filename = filename;
	}
	else
	{
		// someone aborted the save by changing the mode?
		Core::DisplayMessage("Unable to Save : Internal DoState Error", 4000);
	}

	// Resume the core and disable stepping
	Core::PauseAndLock(false, wasUnpaused);
}

bool ReadHeader(const std::string& filename, StateHeader& header)
{
	Flush();
	File::IOFile f(filename, "rb");
	if (!f)
	{
		Core::DisplayMessage("State not found", 2000);
		return false;
	}

	f.ReadArray(&header, 1);
	return true;
}

static void LoadFileStateData(const std::string& filename, std::vector<u8>& ret_data)
{
	Flush();
	File::IOFile f(filename, "rb");
	if (!f)
	{
		Core::DisplayMessage("State not found", 2000);
		return;
	}

	StateHeader header;
	f.ReadArray(&header, 1);

	if (memcmp(SConfig::GetInstance().m_LocalCoreStartupParameter.GetUniqueID().c_str(), header.gameID, 6))
	{
		Core::DisplayMessage(StringFromFormat("State belongs to a different game (ID %.*s)",
			6, header.gameID), 2000);
		return;
	}

	std::vector<u8> buffer;

	if (header.size != 0) // non-zero size means the state is compressed
	{
		Core::DisplayMessage("Decompressing State...", 500);

		buffer.resize(header.size);

		lzo_uint i = 0;
		while (true)
		{
			lzo_uint32 cur_len = 0;  // number of bytes to read
			lzo_uint new_len = 0;  // number of bytes to write

			if (!f.ReadArray(&cur_len, 1))
				break;

			f.ReadBytes(out, cur_len);
			const int res = lzo1x_decompress(out, cur_len, &buffer[i], &new_len, nullptr);
			if (res != LZO_E_OK)
			{
				// This doesn't seem to happen anymore.
				PanicAlertT("Internal LZO Error - decompression failed (%d) (%li, %li) \n"
					"Try loading the state again", res, i, new_len);
				return;
			}

			i += new_len;
		}
	}
	else // uncompressed
	{
		const size_t size = (size_t)(f.GetSize() - sizeof(StateHeader));
		buffer.resize(size);

		if (!f.ReadBytes(&buffer[0], size))
		{
			PanicAlert("wtf? reading bytes: %i", (int)size);
			return;
		}
	}

	// all good
	ret_data.swap(buffer);
}

void LoadAs(const std::string& filename)
{
	if (!Core::IsRunning())
		return;

	// Stop the core while we load the state
	bool wasUnpaused = Core::PauseAndLock(true);

	g_loadDepth++;

	// Save temp buffer for undo load state
	if (!Movie::IsJustStartingRecordingInputFromSaveState())
	{
		std::lock_guard<std::mutex> lk(g_cs_undo_load_buffer);
		SaveToBuffer(g_undo_load_buffer);
		if (Movie::IsMovieActive())
			Movie::SaveRecording(File::GetUserPath(D_STATESAVES_IDX) + "undo.dtm");
		else if (File::Exists(File::GetUserPath(D_STATESAVES_IDX) +"undo.dtm"))
			File::Delete(File::GetUserPath(D_STATESAVES_IDX) + "undo.dtm");
	}

	bool loaded = false;
	bool loadedSuccessfully = false;

	// brackets here are so buffer gets freed ASAP
	{
		std::vector<u8> buffer;
		LoadFileStateData(filename, buffer);

		if (!buffer.empty())
		{
			u8 *ptr = &buffer[0];
			PointerWrap p(&ptr, PointerWrap::MODE_READ);
			DoState(p);
			loaded = true;
			loadedSuccessfully = (p.GetMode() == PointerWrap::MODE_READ);
		}
	}

	if (loaded)
	{
		if (loadedSuccessfully)
		{
			Core::DisplayMessage(StringFromFormat("Loaded state from %s", filename.c_str()), 2000);
			if (File::Exists(filename + ".dtm"))
				Movie::LoadInput(filename + ".dtm");
			else if (!Movie::IsJustStartingRecordingInputFromSaveState() && !Movie::IsJustStartingPlayingInputFromSaveState())
				Movie::EndPlayInput(false);
		}
		else
		{
			// failed to load
			Core::DisplayMessage("Unable to Load : Can't load state from other revisions !", 4000);

			// since we could be in an inconsistent state now (and might crash or whatever), undo.
			if (g_loadDepth < 2)
				UndoLoadState();
		}
	}

	if (g_onAfterLoadCb)
		g_onAfterLoadCb();

	g_loadDepth--;

	// resume dat core
	Core::PauseAndLock(false, wasUnpaused);
}

void SetOnAfterLoadCallback(CallbackFunc callback)
{
	g_onAfterLoadCb = callback;
}

void VerifyAt(const std::string& filename)
{
	bool wasUnpaused = Core::PauseAndLock(true);

	std::vector<u8> buffer;
	LoadFileStateData(filename, buffer);

	if (!buffer.empty())
	{
		u8 *ptr = &buffer[0];
		PointerWrap p(&ptr, PointerWrap::MODE_VERIFY);
		DoState(p);

		if (p.GetMode() == PointerWrap::MODE_VERIFY)
			Core::DisplayMessage(StringFromFormat("Verified state at %s", filename.c_str()), 2000);
		else
			Core::DisplayMessage("Unable to Verify : Can't verify state from other revisions !", 4000);
	}

	Core::PauseAndLock(false, wasUnpaused);
}


void Init()
{
	if (lzo_init() != LZO_E_OK)
		PanicAlertT("Internal LZO Error - lzo_init() failed");
}

void Shutdown()
{
	Flush();

	// swapping with an empty vector, rather than clear()ing
	// this gives a better guarantee to free the allocated memory right NOW (as opposed to, actually, never)
	{
		std::lock_guard<std::mutex> lk(g_cs_current_buffer);
		std::vector<u8>().swap(g_current_buffer);
	}

	{
		std::lock_guard<std::mutex> lk(g_cs_undo_load_buffer);
		std::vector<u8>().swap(g_undo_load_buffer);
	}
}

static std::string MakeStateFilename(int number)
{
	return StringFromFormat("%s%s.s%02i", File::GetUserPath(D_STATESAVES_IDX).c_str(),
		SConfig::GetInstance().m_LocalCoreStartupParameter.GetUniqueID().c_str(), number);
}

void Save(int slot, bool wait)
{
	SaveAs(MakeStateFilename(slot), wait);
}

void Load(int slot)
{
	LoadAs(MakeStateFilename(slot));
}

void Verify(int slot)
{
	VerifyAt(MakeStateFilename(slot));
}

void LoadLastSaved(int i)
{
	std::map<double, int> savedStates = GetSavedStates();

	if (i > (int)savedStates.size())
		Core::DisplayMessage("State doesn't exist", 2000);
	else
	{
		std::map<double, int>::iterator it = savedStates.begin();
		std::advance(it, i-1);
		Load(it->second);
	}
}

// must wait for state to be written because it must know if all slots are taken
void SaveFirstSaved()
{
	std::map<double, int> savedStates = GetSavedStates();

	// save to an empty slot
	if (savedStates.size() < NUM_STATES)
		Save(GetEmptySlot(savedStates), true);
	// overwrite the oldest state
	else
	{
		std::map<double, int>::iterator it = savedStates.begin();
		std::advance(it, savedStates.size()-1);
		Save(it->second, true);
	}
}

void Flush()
{
	// If already saving state, wait for it to finish
	if (g_save_thread.joinable())
		g_save_thread.join();
}

// Load the last state before loading the state
void UndoLoadState()
{
	std::lock_guard<std::mutex> lk(g_cs_undo_load_buffer);
	if (!g_undo_load_buffer.empty())
	{
		if (File::Exists(File::GetUserPath(D_STATESAVES_IDX) + "undo.dtm") || (!Movie::IsMovieActive()))
		{
			LoadFromBuffer(g_undo_load_buffer);
			if (Movie::IsMovieActive())
				Movie::LoadInput(File::GetUserPath(D_STATESAVES_IDX) + "undo.dtm");
		}
		else
		{
			PanicAlert("No undo.dtm found, aborting undo load state to prevent movie desyncs");
		}
	}
	else
	{
		PanicAlert("There is nothing to undo!");
	}
}

// Load the state that the last save state overwritten on
void UndoSaveState()
{
	LoadAs(File::GetUserPath(D_STATESAVES_IDX) + "lastState.sav");
}

} // namespace State
