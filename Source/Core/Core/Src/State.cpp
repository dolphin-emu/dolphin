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
#include "ConfigManager.h"
#include "StringUtil.h"
#include "Thread.h"
#include "CoreTiming.h"
#include "OnFrame.h"
#include "HW/Wiimote.h"
#include "HW/DSP.h"
#include "HW/HW.h"
#include "HW/CPU.h"
#include "PowerPC/JitCommon/JitBase.h"
#include "VideoBackendBase.h"

#include <lzo/lzo1x.h>

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

static volatile bool g_op_in_progress = false;

static int ev_FileSave, ev_BufferSave, ev_FileLoad, ev_BufferLoad, ev_FileVerify, ev_BufferVerify;

static std::string g_current_filename, g_last_filename;

// Temporary undo state buffer
static std::vector<u8> g_undo_load_buffer;
static std::vector<u8> g_current_buffer;

static std::thread g_save_thread;

// Don't forget to increase this after doing changes on the savestate system 
static const int VERSION = 4;

struct StateHeader
{
	u8 gameID[6];
	size_t size;
};

static bool g_use_compression = true;

void EnableCompression(bool compression)
{
	g_use_compression = compression;
}

void DoState(PointerWrap &p)
{
	u32 cookie = 0xBAADBABE + VERSION;
	p.Do(cookie);
	if (cookie != 0xBAADBABE + VERSION)
	{
		p.SetMode(PointerWrap::MODE_MEASURE);
		return;
	}
	// Begin with video backend, so that it gets a chance to clear it's caches and writeback modified things to RAM
	// Pause the video thread in multi-threaded mode
	g_video_backend->RunLoop(false);
	g_video_backend->DoState(p);

	if (Core::g_CoreStartupParameter.bWii)
		Wiimote::DoState(p.GetPPtr(), p.GetMode());

	PowerPC::DoState(p);
	HW::DoState(p);
	CoreTiming::DoState(p);

	// Resume the video thread
	g_video_backend->RunLoop(true);
}

void LoadBufferStateCallback(u64 userdata, int cyclesLate)
{
	u8* ptr = &g_current_buffer[0];
	PointerWrap p(&ptr, PointerWrap::MODE_READ);
	DoState(p);

	Core::DisplayMessage("Loaded state", 2000);

	g_op_in_progress = false;
}

void SaveBufferStateCallback(u64 userdata, int cyclesLate)
{
	u8* ptr = NULL;
	PointerWrap p(&ptr, PointerWrap::MODE_MEASURE);

	DoState(p);
	const size_t buffer_size = reinterpret_cast<size_t>(ptr);
	g_current_buffer.resize(buffer_size);

	ptr = &g_current_buffer[0];
	p.SetMode(PointerWrap::MODE_WRITE);
	DoState(p);

	g_op_in_progress = false;
}

void VerifyBufferStateCallback(u64 userdata, int cyclesLate)
{
	u8* ptr = &g_current_buffer[0];
	PointerWrap p(&ptr, PointerWrap::MODE_VERIFY);
	DoState(p);

	Core::DisplayMessage("Verified state", 2000);

	g_op_in_progress = false;
}

void CompressAndDumpState(const std::vector<u8>* save_arg)
{
	const u8* const buffer_data = &(*save_arg)[0];
	const size_t buffer_size = save_arg->size();

	// For easy debugging
	Common::SetCurrentThreadName("SaveState thread");

	// Moving to last overwritten save-state
	if (File::Exists(g_current_filename))
	{
		if (File::Exists(File::GetUserPath(D_STATESAVES_IDX) + "lastState.sav"))
			File::Delete((File::GetUserPath(D_STATESAVES_IDX) + "lastState.sav"));

		if (!File::Rename(g_current_filename, File::GetUserPath(D_STATESAVES_IDX) + "lastState.sav"))
			Core::DisplayMessage("Failed to move previous state to state undo backup", 1000);
	}

	File::IOFile f(g_current_filename, "wb");
	if (!f)
	{
		Core::DisplayMessage("Could not save state", 2000);
		return;
	}

	// Setting up the header
	StateHeader header;
	memcpy(header.gameID, SConfig::GetInstance().m_LocalCoreStartupParameter.GetUniqueID().c_str(), 6);
	header.size = g_use_compression ? buffer_size : 0;

	f.WriteArray(&header, 1);

	if (0 != header.size)	// non-zero header size means the state is compressed
	{
		lzo_uint i = 0;
		while (true)
		{
			lzo_uint cur_len = 0;
			lzo_uint out_len = 0;

			if ((i + IN_LEN) >= buffer_size)
				cur_len = buffer_size - i;
			else
				cur_len = IN_LEN;

			if (lzo1x_1_compress(buffer_data + i, cur_len, out, &out_len, wrkmem) != LZO_E_OK)
				PanicAlertT("Internal LZO Error - compression failed");

			// The size of the data to write is 'out_len'
			f.WriteArray(&out_len, 1);
			f.WriteBytes(out, out_len);

			if (cur_len != IN_LEN)
				break;

			i += cur_len;
		}
	}
	else	// uncompressed
	{
		f.WriteBytes(buffer_data, buffer_size);
	}

	Core::DisplayMessage(StringFromFormat("Saved State to %s",
		g_current_filename.c_str()).c_str(), 2000);

	g_op_in_progress = false;
}

void SaveFileStateCallback(u64 userdata, int cyclesLate)
{
	// Pause the core while we save the state
	CCPU::EnableStepping(true);

	// Wait for the other threaded sub-systems to stop too
	// TODO: this is ugly
	SLEEP(100);

	Flush();

	// Measure the size of the buffer.
	u8 *ptr = NULL;
	PointerWrap p(&ptr, PointerWrap::MODE_MEASURE);
	DoState(p);
	const size_t buffer_size = reinterpret_cast<size_t>(ptr);

	// Then actually do the write.
	g_current_buffer.resize(buffer_size);
	ptr = &g_current_buffer[0];
	p.SetMode(PointerWrap::MODE_WRITE);
	DoState(p);
	
	if ((Frame::IsRecordingInput() || Frame::IsPlayingInput()) && !Frame::IsRecordingInputFromSaveState())
		Frame::SaveRecording(StringFromFormat("%s.dtm", g_current_filename.c_str()).c_str());

	Core::DisplayMessage("Saving State...", 1000);

	g_save_thread = std::thread(CompressAndDumpState, &g_current_buffer);

	// Resume the core and disable stepping
	CCPU::EnableStepping(false);
}

void LoadFileStateData(std::string& filename, std::vector<u8>& ret_data)
{
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

	if (0 != header.size)	// non-zero size means the state is compressed
	{
		Core::DisplayMessage("Decompressing State...", 500);

		buffer.resize(header.size);

		lzo_uint i = 0;
		while (true)
		{
			lzo_uint cur_len = 0;  // number of bytes to read
			lzo_uint new_len = 0;  // number of bytes to write

			if (!f.ReadArray(&cur_len, 1))
				break;

			f.ReadBytes(out, cur_len);
			const int res = lzo1x_decompress(out, cur_len, &buffer[i], &new_len, NULL);
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
	else	// uncompressed
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

void LoadFileStateCallback(u64 userdata, int cyclesLate)
{
	// Stop the core while we load the state
	CCPU::EnableStepping(true);

	// Wait for the other threaded sub-systems to stop too
	// TODO: uglyyy
	SLEEP(100);

	Flush();

	// Save temp buffer for undo load state
	// TODO: this should be controlled by a user option,
	// because it slows down every savestate load to provide an often-unused feature.
	SaveBufferStateCallback(userdata, cyclesLate);
	g_undo_load_buffer.swap(g_current_buffer);

	std::vector<u8> buffer;
	LoadFileStateData(g_current_filename, buffer);

	if (!buffer.empty())
	{
		u8 *ptr = &buffer[0];
		PointerWrap p(&ptr, PointerWrap::MODE_READ);
		DoState(p);

		if (p.GetMode() == PointerWrap::MODE_READ)
			Core::DisplayMessage(StringFromFormat("Loaded state from %s", g_current_filename.c_str()).c_str(), 2000);
		else
			Core::DisplayMessage("Unable to Load : Can't load state from other revisions !", 4000);
	
		if (File::Exists(g_current_filename + ".dtm"))
			Frame::LoadInput((g_current_filename + ".dtm").c_str());
		else if (!Frame::IsRecordingInputFromSaveState())
			Frame::EndPlayInput(false);
	}

	g_op_in_progress = false;

	// resume dat core
	CCPU::EnableStepping(false);
}

void VerifyFileStateCallback(u64 userdata, int cyclesLate)
{
	Flush();

	std::vector<u8> buffer;
	LoadFileStateData(g_current_filename, buffer);

	if (!buffer.empty())
	{
		u8 *ptr = &buffer[0];
		PointerWrap p(&ptr, PointerWrap::MODE_VERIFY);
		DoState(p);

		if (p.GetMode() == PointerWrap::MODE_READ)
			Core::DisplayMessage(StringFromFormat("Verified state at %s", g_current_filename.c_str()).c_str(), 2000);
		else
			Core::DisplayMessage("Unable to Verify : Can't verify state from other revisions !", 4000);
	}
}

void Init()
{
	ev_FileLoad = CoreTiming::RegisterEvent("LoadState", &LoadFileStateCallback);
	ev_FileSave = CoreTiming::RegisterEvent("SaveState", &SaveFileStateCallback);
	ev_FileVerify = CoreTiming::RegisterEvent("VerifyState", &VerifyFileStateCallback);

	ev_BufferLoad = CoreTiming::RegisterEvent("LoadBufferState", &LoadBufferStateCallback);
	ev_BufferSave = CoreTiming::RegisterEvent("SaveBufferState", &SaveBufferStateCallback);
	ev_BufferVerify = CoreTiming::RegisterEvent("VerifyBufferState", &VerifyBufferStateCallback);

	if (lzo_init() != LZO_E_OK)
		PanicAlertT("Internal LZO Error - lzo_init() failed");
}

void Shutdown()
{
	Flush();

	g_current_buffer.swap(std::vector<u8>());
	g_undo_load_buffer.swap(std::vector<u8>());
}

static std::string MakeStateFilename(int number)
{
	return StringFromFormat("%s%s.s%02i", File::GetUserPath(D_STATESAVES_IDX).c_str(),
		SConfig::GetInstance().m_LocalCoreStartupParameter.GetUniqueID().c_str(), number);
}

void ScheduleFileEvent(const std::string &filename, int ev)
{
	if (g_op_in_progress)
		return;
	g_op_in_progress = true;

	g_current_filename = filename;
	CoreTiming::ScheduleEvent_Threadsafe_Immediate(ev);
}

void SaveAs(const std::string &filename)
{
	g_last_filename = filename;
	ScheduleFileEvent(filename, ev_FileSave);
}

void LoadAs(const std::string &filename)
{
	ScheduleFileEvent(filename, ev_FileLoad);
}

void VerifyAt(const std::string &filename)
{
	ScheduleFileEvent(filename, ev_FileVerify);
}

void Save(int slot)
{
	SaveAs(MakeStateFilename(slot));
}

void Load(int slot)
{
	LoadAs(MakeStateFilename(slot));
}

void Verify(int slot)
{
	VerifyAt(MakeStateFilename(slot));
}

void LoadLastSaved()
{
	if (g_last_filename.empty())
		Core::DisplayMessage("There is no last saved state", 2000);
	else
		LoadAs(g_last_filename);
}

void ScheduleBufferEvent(std::vector<u8>& buffer, int ev)
{
	if (g_op_in_progress)
		return;
	g_op_in_progress = true;

	g_current_buffer.swap(buffer);
	CoreTiming::ScheduleEvent_Threadsafe_Immediate(ev);
}

void SaveToBuffer(std::vector<u8>& buffer)
{
	ScheduleBufferEvent(buffer, ev_BufferSave);
}

void LoadFromBuffer(std::vector<u8>& buffer)
{
	ScheduleBufferEvent(buffer, ev_BufferLoad);
}

void VerifyBuffer(std::vector<u8>& buffer)
{
	ScheduleBufferEvent(buffer, ev_BufferVerify);
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
	if (!g_undo_load_buffer.empty())
		LoadFromBuffer(g_undo_load_buffer);
	else
		PanicAlert("There is nothing to undo!");
}

// Load the state that the last save state overwritten on
void UndoSaveState()
{
	LoadAs((File::GetUserPath(D_STATESAVES_IDX) + "lastState.sav").c_str());
}

} // namespace State
