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

static float lastOCFactor;
int slicelength;
static int maxSliceLength = MAX_SLICE_LENGTH;

static s64 idledCycles;
static u32 fakeDecStartValue;
static u64 fakeDecStartTicks;

s64 globalTimer;
u64 fakeTBStartValue;
u64 fakeTBStartTicks;

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

static void EmptyTimedCallback(u64 userdata, int cyclesLate) {}

// Changing the CPU speed in Dolphin isn't actually done by changing the physical clock rate,
// but by changing the amount of work done in a particular amount of time. This tends to be more
// compatible because it stops the games from actually knowing directly that the clock rate has
// changed, and ensures that anything based on waiting a specific number of cycles still works.
//
// Technically it might be more accurate to call this changing the IPC instead of the CPU speed,
// but the effect is largely the same.
static int DowncountToCycles(int downcount)
{
	return (int)(downcount / lastOCFactor);
}

static int CyclesToDowncount(int cycles)
{
	return (int)(cycles * lastOCFactor);
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
	lastOCFactor = SConfig::GetInstance().m_OCEnable ? SConfig::GetInstance().m_OCFactor : 1.0f;
	PowerPC::ppcState.downcount = CyclesToDowncount(maxSliceLength);
	slicelength = maxSliceLength;
	globalTimer = 0;
	idledCycles = 0;

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
	p.Do(slicelength);
	p.Do(globalTimer);
	p.Do(idledCycles);
	p.Do(fakeDecStartValue);
	p.Do(fakeDecStartTicks);
	p.Do(fakeTBStartValue);
	p.Do(fakeTBStartTicks);
	p.Do(lastOCFactor);
	p.DoMarker("CoreTimingData");

	MoveEvents();

	p.DoLinkedList<BaseEvent, GetNewEvent, FreeEvent, EventDoState>(first);
	p.DoMarker("CoreTimingEvents");
}

u64 GetTicks()
{
	return (u64)globalTimer;
}

u64 GetIdleTicks()
{
	return (u64)idledCycles;
}

// This is to be called when outside threads, such as the graphics thread, wants to
// schedule things to be executed on the main thread.
void ScheduleEvent_Threadsafe(int cyclesIntoFuture, int event_type, u64 userdata)
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
	ne.time = globalTimer + cyclesIntoFuture;
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
void ScheduleEvent_AnyThread(int cyclesIntoFuture, int event_type, u64 userdata)
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
void ScheduleEvent(int cyclesIntoFuture, int event_type, u64 userdata)
{
	_assert_msg_(POWERPC, Core::IsCPUThread() || Core::GetState() == Core::CORE_PAUSE,
				 "ScheduleEvent from wrong thread");
	Event *ne = GetNewEvent();
	ne->userdata = userdata;
	ne->type = event_type;
	ne->time = globalTimer + cyclesIntoFuture;
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

void ForceExceptionCheck(int cycles)
{
	if (DowncountToCycles(PowerPC::ppcState.downcount) > cycles)
	{
		slicelength -= (DowncountToCycles(PowerPC::ppcState.downcount) - cycles); // Account for cycles already executed by adjusting the slicelength
		PowerPC::ppcState.downcount = CyclesToDowncount(cycles);
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
		if (first->time <= globalTimer)
		{
			Event* evt = first;
			first = first->next;
			event_types[evt->type].callback(evt->userdata, (int)(globalTimer - evt->time));
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

	int cyclesExecuted = slicelength - DowncountToCycles(PowerPC::ppcState.downcount);
	globalTimer += cyclesExecuted;
	lastOCFactor = SConfig::GetInstance().m_OCEnable ? SConfig::GetInstance().m_OCFactor : 1.0f;
	PowerPC::ppcState.downcount = CyclesToDowncount(slicelength);

	while (first && first->time <= globalTimer)
	{
		//LOG(POWERPC, "[Scheduler] %s     (%lld, %lld) ",
		//             event_types[first->type].name ? event_types[first->type].name : "?", (u64)globalTimer, (u64)first->time);
		Event* evt = first;
		first = first->next;
		event_types[evt->type].callback(evt->userdata, (int)(globalTimer - evt->time));
		FreeEvent(evt);
	}

	if (!first)
	{
		WARN_LOG(POWERPC, "WARNING - no events in queue. Setting downcount to 10000");
		PowerPC::ppcState.downcount += CyclesToDowncount(10000);
	}
	else
	{
		slicelength = (int)(first->time - globalTimer);
		if (slicelength > maxSliceLength)
			slicelength = maxSliceLength;
		PowerPC::ppcState.downcount = CyclesToDowncount(slicelength);
	}
}

void LogPendingEvents()
{
	Event *ptr = first;
	while (ptr)
	{
		INFO_LOG(POWERPC, "PENDING: Now: %" PRId64 " Pending: %" PRId64 " Type: %d", globalTimer, ptr->time, ptr->type);
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
		Fifo::Update(0);
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
	return fakeTBStartValue;
}

void SetFakeTBStartValue(u64 val)
{
	fakeTBStartValue = val;
}

u64 GetFakeTBStartTicks()
{
	return fakeTBStartTicks;
}

void SetFakeTBStartTicks(u64 val)
{
	fakeTBStartTicks = val;
}

}  // namespace

