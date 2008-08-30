#include "Common.h"

#include "State.h"
#include "CoreTiming.h"
#include "HW/HW.h"
#include "PowerPC/PowerPC.h"

#include "Plugins/Plugin_Video.h"

#include <string>

static int ev_Save;
static int ev_Load;

static std::string cur_filename;

void DoState(PointerWrap &p)
{
	PowerPC::DoState(p);
	HW::DoState(p);
    PluginVideo::Video_DoState(p.GetPPtr(), p.GetMode());
}

void SaveStateCallback(u64 userdata, int cyclesLate)
{
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
}

void LoadStateCallback(u64 userdata, int cyclesLate)
{
	// ChunkFile f(cur_filename.c_str(), ChunkFile::MODE_READ);
	FILE *f = fopen(cur_filename.c_str(), "r");
	fseek(f, 0, SEEK_END);
	int sz = ftell(f);
	fseek(f, 0, SEEK_SET);
	u8 *buffer = new u8[sz];
	fread(buffer, sz, 1, f);
	fclose(f);

	u8 *ptr;
	PointerWrap p(&ptr, PointerWrap::MODE_READ);
	DoState(p);
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

void State_Save(const char *filename)
{
	cur_filename = filename;
	CoreTiming::ScheduleEvent_Threadsafe(0, ev_Save);
}

void State_Load(const char *filename)
{
	cur_filename = filename;
	CoreTiming::ScheduleEvent_Threadsafe(0, ev_Load);
}
