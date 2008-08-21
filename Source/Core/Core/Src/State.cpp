#include "Common.h"

#include "State.h"
#include "CoreTiming.h"
#include "HW/HW.h"
#include "PowerPC/PowerPC.h"

static int ev_Save;
static int ev_Load;

static std::string cur_filename;

void DoState(ChunkFile &f)
{
	f.Descend("DOLP");
	PowerPC::DoState(f);
	HW::DoState(f);
	f.Ascend();
}

void SaveStateCallback(u64 userdata, int cyclesLate)
{
	ChunkFile f(cur_filename.c_str(), ChunkFile::MODE_WRITE);
	DoState(f);
}

void LoadStateCallback(u64 userdata, int cyclesLate)
{
	ChunkFile f(cur_filename.c_str(), ChunkFile::MODE_READ);
	DoState(f);
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

void SaveState(const char *filename)
{
	cur_filename = filename;
	CoreTiming::ScheduleEvent_Threadsafe(0, ev_Save);
}

void LoadState(const char *filename)
{
	cur_filename = filename;
	CoreTiming::ScheduleEvent_Threadsafe(0, ev_Load);
}