// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cinttypes>
#include <mutex>
#include <string>
#include <vector>

#include "Common/ChunkFile.h"
#include "Common/FifoQueue.h"
#include "Common/StringUtil.h"
#include "Common/Thread.h"

#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/CoreTiming.h"
#include "Core/PowerPC/PowerPC.h"

#include "VideoCommon/Fifo.h"
#include "VideoCommon/VideoBackendBase.h"

#define MAX_SLICE_LENGTH 20000

namespace CoreTiming
{

struct EventType
{
	TimedCallback callback;
	std::string name;
};

static std::vector<EventType> event_types;

struct BaseEvent
{
	s64 time;
	u64 userdata;
	int type;
};

typedef LinkedListItem<BaseEvent> Event;

// STATE_TO_SAVE
static Event *first;
static std::mutex tsWriteLock;
static Common::FifoQueue<BaseEvent, false> tsQueue;

// event pools
static Event *eventPool = nullptr;

static float s_lastOCFactor;
float g_lastOCFactor_inverted;
int g_slicelength;
static int maxslicelength = MAX_SLICE_LENGTH;

static s64 idledCycles;
static u32 fakeDecStartValue;
static u64 fakeDecStartTicks;

// Are we in a function that has been called from Advance()
static bool globalTimerIsSane;

s64 g_globalTimer;
u64 g_fakeTBStartValue;
u64 g_fakeTBStartTicks;

static int ev_lost;

static Event* GetNewEvent()
{
	if (!eventPool)
		return new Event;

	Event* ev = eventPool;
	eventPool = ev->next;
	return ev;
}

static void FreeEvent(Event* ev)
{
	ev->next = eventPool;
	eventPool = ev;
}

static void EmptyTimedCallback(u64 userdata, s64 cyclesLate) {}

// Changing the CPU speed in Dolphin isn't actually done by changing the physical clock rate,
// but by changing the amount of work done in a particular amount of time. This tends to be more
// compatible because it stops the games from actually knowing directly that the clock rate has
// changed, and ensures that anything based on waiting a specific number of cycles still works.
//
// Technically it might be more accurate to call this changing the IPC instead of the CPU speed,
// but the effect is largely the same.
static int DowncountToCycles(int downcount)
{
	return (int)(downcount * g_lastOCFactor_inverted);
}

static int CyclesToDowncount(int cycles)
{
	return (int)(cycles * s_lastOCFactor);
}

int RegisterEvent(const std::string& name, TimedCallback callback)
{
	EventType type;
	type.name = name;
	type.callback = callback;

	// check for existing type with same name.
	// we want event type names to remain unique so that we can use them for serialization.
	for (auto& event_type : event_types)
	{
		if (name == event_type.name)
		{
			WARN_LOG(POWERPC, "Discarded old event type \"%s\" because a new type with the same name was registered.", name.c_str());
			// we don't know if someone might be holding on to the type index,
			// so we gut the old event type instead of actually removing it.
			event_type.name = "_discarded_event";
			event_type.callback = &EmptyTimedCallback;
		}
	}

	event_types.push_back(type);
	return (int)event_types.size() - 1;
}

void UnregisterAllEvents()
{
	if (first)
		PanicAlert("Cannot unregister events with events pending");
	event_types.clear();
}

void Init()
{
	s_lastOCFactor = SConfig::GetInstance().m_OCEnable ? SConfig::GetInstance().m_OCFactor : 1.0f;
	g_lastOCFactor_inverted = 1.0f / s_lastOCFactor;
	PowerPC::ppcState.downcount = CyclesToDowncount(maxslicelength);
	g_slicelength = maxslicelength;
	g_globalTimer = 0;
	idledCycles = 0;
	globalTimerIsSane = true;

	ev_lost = RegisterEvent("_lost_event", &EmptyTimedCallback);
}

void Shutdown()
{
	std::lock_guard<std::mutex> lk(tsWriteLock);
	MoveEvents();
	ClearPendingEvents();
	UnregisterAllEvents();

	while (eventPool)
	{
		Event *ev = eventPool;
		eventPool = ev->next;
		delete ev;
	}
}

static void EventDoState(PointerWrap &p, BaseEvent* ev)
{
	p.Do(ev->time);

	// this is why we can't have (nice things) pointers as userdata
	p.Do(ev->userdata);

	// we can't savestate ev->type directly because events might not get registered in the same order (or at all) every time.
	// so, we savestate the event's type's name, and derive ev->type from that when loading.
	std::string name;
	if (p.GetMode() != PointerWrap::MODE_READ)
		name = event_types[ev->type].name;

	p.Do(name);
	if (p.GetMode() == PointerWrap::MODE_READ)
	{
		bool foundMatch = false;
		for (unsigned int i = 0; i < event_types.size(); ++i)
		{
			if (name == event_types[i].name)
			{
				ev->type = i;
				foundMatch = true;
				break;
			}
		}
		if (!foundMatch)
		{
			WARN_LOG(POWERPC, "Lost event from savestate because its type, \"%s\", has not been registered.", name.c_str());
			ev->type = ev_lost;
		}
	}
}

void DoState(PointerWrap &p)
{
	std::lock_guard<std::mutex> lk(tsWriteLock);
	p.Do(g_slicelength);
	p.Do(g_globalTimer);
	p.Do(idledCycles);
	p.Do(fakeDecStartValue);
	p.Do(fakeDecStartTicks);
	p.Do(g_fakeTBStartValue);
	p.Do(g_fakeTBStartTicks);
	p.Do(s_lastOCFactor);
	if (p.GetMode() == PointerWrap::MODE_READ)
		g_lastOCFactor_inverted = 1.0f / s_lastOCFactor;

	p.DoMarker("CoreTimingData");

	MoveEvents();

	p.DoLinkedList<BaseEvent, GetNewEvent, FreeEvent, EventDoState>(first);
	p.DoMarker("CoreTimingEvents");
}

// This should only be called from the CPU thread, if you are calling it any other thread, you are doing something evil
u64 GetTicks()
{
	u64 ticks = (u64)g_globalTimer;
	if (!globalTimerIsSane)
	{
		int downcount = DowncountToCycles(PowerPC::ppcState.downcount);
		ticks += g_slicelength - downcount;
	}
	return ticks;
}

u64 GetIdleTicks()
{
	return (u64)idledCycles;
}

// This is to be called when outside threads, such as the graphics thread, wants to
// schedule things to be executed on the main thread.
void ScheduleEvent_Threadsafe(s64 cyclesIntoFuture, int event_type, u64 userdata)
{
	_assert_msg_(POWERPC, !Core::IsCPUThread(), "ScheduleEvent_Threadsafe from wrong thread");
	if (Core::g_want_determinism)
	{
		ERROR_LOG(POWERPC, "Someone scheduled an off-thread \"%s\" event while netplay or movie play/record "
		                   "was active.  This is likely to cause a desync.",
		                   event_types[event_type].name.c_str());
	}
	std::lock_guard<std::mutex> lk(tsWriteLock);
	Event ne;
	ne.time = g_globalTimer + cyclesIntoFuture;
	ne.type = event_type;
	ne.userdata = userdata;
	tsQueue.Push(ne);
}

// Executes an event immediately, then returns.
void ScheduleEvent_Immediate(int event_type, u64 userdata)
{
	_assert_msg_(POWERPC, Core::IsCPUThread(), "ScheduleEvent_Immediate from wrong thread");
	event_types[event_type].callback(userdata, 0);
}

// Same as ScheduleEvent_Threadsafe(0, ...) EXCEPT if we are already on the CPU thread
// in which case this is the same as ScheduleEvent_Immediate.
void ScheduleEvent_Threadsafe_Immediate(int event_type, u64 userdata)
{
	if (Core::IsCPUThread())
	{
		event_types[event_type].callback(userdata, 0);
	}
	else
	{
		ScheduleEvent_Threadsafe(0, event_type, userdata);
	}
}

// To be used from any thread, including the CPU thread
void ScheduleEvent_AnyThread(s64 cyclesIntoFuture, int event_type, u64 userdata)
{
	if (Core::IsCPUThread())
		ScheduleEvent(cyclesIntoFuture, event_type, userdata);
	else
		ScheduleEvent_Threadsafe(cyclesIntoFuture, event_type, userdata);
}

void ClearPendingEvents()
{
	while (first)
	{
		Event *e = first->next;
		FreeEvent(first);
		first = e;
	}
}

static void AddEventToQueue(Event* ne)
{
	Event* prev = nullptr;
	Event** pNext = &first;
	for (;;)
	{
		Event*& next = *pNext;
		if (!next || ne->time < next->time)
		{
			ne->next = next;
			next = ne;
			break;
		}
		prev = next;
		pNext = &prev->next;
	}
}

// This must be run ONLY from within the CPU thread
// cyclesIntoFuture may be VERY inaccurate if called from anything else
// than Advance
void ScheduleEvent(s64 cyclesIntoFuture, int event_type, u64 userdata)
{
	_assert_msg_(POWERPC, Core::IsCPUThread() || Core::GetState() == Core::CORE_PAUSE,
				 "ScheduleEvent from wrong thread");

	Event *ne = GetNewEvent();
	ne->userdata = userdata;
	ne->type = event_type;
	ne->time = GetTicks() + cyclesIntoFuture;

	// If this event needs to be scheduled before the next advance(), force one early
	if (!globalTimerIsSane)
		ForceExceptionCheck(cyclesIntoFuture);


	AddEventToQueue(ne);
}

void RemoveEvent(int event_type)
{
	while (first && first->type == event_type)
	{
		Event* next = first->next;
		FreeEvent(first);
		first = next;
	}

	if (!first)
		return;

	Event *prev = first;
	Event *ptr = prev->next;
	while (ptr)
	{
		if (ptr->type == event_type)
		{
			prev->next = ptr->next;
			FreeEvent(ptr);
			ptr = prev->next;
		}
		else
		{
			prev = ptr;
			ptr = ptr->next;
		}
	}
}

void RemoveAllEvents(int event_type)
{
	MoveEvents();
	RemoveEvent(event_type);
}

void ForceExceptionCheck(s64 cycles)
{
	if (s64(DowncountToCycles(PowerPC::ppcState.downcount)) > cycles)
	{
		// downcount is always (much) smaller than MAX_INT so we can safely cast cycles to an int here.
		g_slicelength -= (DowncountToCycles(PowerPC::ppcState.downcount) - (int)cycles); // Account for cycles already executed by adjusting the g_slicelength
		PowerPC::ppcState.downcount = CyclesToDowncount((int)cycles);
	}
}


//This raise only the events required while the fifo is processing data
void ProcessFifoWaitEvents()
{
	MoveEvents();

	if (!first)
		return;

	while (first)
	{
		if (first->time <= g_globalTimer)
		{
			Event* evt = first;
			first = first->next;
			event_types[evt->type].callback(evt->userdata, (int)(g_globalTimer - evt->time));
			FreeEvent(evt);
		}
		else
		{
			break;
		}
	}
}

void MoveEvents()
{
	BaseEvent sevt;
	while (tsQueue.Pop(sevt))
	{
		Event *evt = GetNewEvent();
		evt->time = sevt.time;
		evt->userdata = sevt.userdata;
		evt->type = sevt.type;
		AddEventToQueue(evt);
	}
}

void Advance()
{
	MoveEvents();

	int cyclesExecuted = g_slicelength - DowncountToCycles(PowerPC::ppcState.downcount);
	g_globalTimer += cyclesExecuted;
	s_lastOCFactor = SConfig::GetInstance().m_OCEnable ? SConfig::GetInstance().m_OCFactor : 1.0f;
	g_lastOCFactor_inverted = 1.0f / s_lastOCFactor;
	PowerPC::ppcState.downcount = CyclesToDowncount(g_slicelength);

	globalTimerIsSane = true;

	while (first && first->time <= g_globalTimer)
	{
		//LOG(POWERPC, "[Scheduler] %s     (%lld, %lld) ",
		//             event_types[first->type].name ? event_types[first->type].name : "?", (u64)g_globalTimer, (u64)first->time);
		Event* evt = first;
		first = first->next;
		event_types[evt->type].callback(evt->userdata, (int)(g_globalTimer - evt->time));
		FreeEvent(evt);
	}

	globalTimerIsSane = false;

	if (!first)
	{
		WARN_LOG(POWERPC, "WARNING - no events in queue. Setting downcount to 10000");
		PowerPC::ppcState.downcount += CyclesToDowncount(10000);
	}
	else
	{
		g_slicelength = (int)(first->time - g_globalTimer);
		if (g_slicelength > maxslicelength)
			g_slicelength = maxslicelength;
		PowerPC::ppcState.downcount = CyclesToDowncount(g_slicelength);
	}

	// Check for any external exceptions.
	// It's important to do this after processing events otherwise any exceptions will be delayed until the next slice:
	//        Pokemon Box refuses to boot if the first exception from the audio DMA is received late
	PowerPC::CheckExternalExceptions();
}

void LogPendingEvents()
{
	Event *ptr = first;
	while (ptr)
	{
		INFO_LOG(POWERPC, "PENDING: Now: %" PRId64 " Pending: %" PRId64 " Type: %d", g_globalTimer, ptr->time, ptr->type);
		ptr = ptr->next;
	}
}

void Idle()
{
	//DEBUG_LOG(POWERPC, "Idle");

	if (SConfig::GetInstance().bSyncGPUOnSkipIdleHack)
	{
		//When the FIFO is processing data we must not advance because in this way
		//the VI will be desynchronized. So, We are waiting until the FIFO finish and
		//while we process only the events required by the FIFO.
		ProcessFifoWaitEvents();
		Fifo::FlushGpu();
	}

	idledCycles += DowncountToCycles(PowerPC::ppcState.downcount);
	PowerPC::ppcState.downcount = 0;
}

std::string GetScheduledEventsSummary()
{
	Event *ptr = first;
	std::string text = "Scheduled events\n";
	text.reserve(1000);
	while (ptr)
	{
		unsigned int t = ptr->type;
		if (t >= event_types.size())
			PanicAlertT("Invalid event type %i", t);

		const std::string& name = event_types[ptr->type].name;

		text += StringFromFormat("%s : %" PRIi64 " %016" PRIx64 "\n", name.c_str(), ptr->time, ptr->userdata);
		ptr = ptr->next;
	}
	return text;
}

u32 GetFakeDecStartValue()
{
	return fakeDecStartValue;
}

void SetFakeDecStartValue(u32 val)
{
	fakeDecStartValue = val;
}

u64 GetFakeDecStartTicks()
{
	return fakeDecStartTicks;
}

void SetFakeDecStartTicks(u64 val)
{
	fakeDecStartTicks = val;
}

u64 GetFakeTBStartValue()
{
	return g_fakeTBStartValue;
}

void SetFakeTBStartValue(u64 val)
{
	g_fakeTBStartValue = val;
}

u64 GetFakeTBStartTicks()
{
	return g_fakeTBStartTicks;
}

void SetFakeTBStartTicks(u64 val)
{
	g_fakeTBStartTicks = val;
}

}  // namespace

