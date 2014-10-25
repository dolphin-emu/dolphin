// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <cinttypes>
#include <string>
#include <vector>

#include "Common/ChunkFile.h"
#include "Common/FifoQueue.h"
#include "Common/StringUtil.h"
#include "Common/Thread.h"

#include "Core/Core.h"
#include "Core/CoreTiming.h"
#include "Core/PowerPC/PowerPC.h"

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

int slicelength;
static int maxSliceLength = MAX_SLICE_LENGTH;

static s64 idledCycles;
static u32 fakeDecStartValue;
static u64 fakeDecStartTicks;

s64 globalTimer;
u64 fakeTBStartValue;
u64 fakeTBStartTicks;

static int ev_lost;


static void (*advanceCallback)(int cyclesExecuted) = nullptr;

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
		PanicAlertT("Cannot unregister events with events pending");
	event_types.clear();
}

void Init()
{
	PowerPC::ppcState.downcount = maxSliceLength;
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
	std::lock_guard<std::mutex> lk(tsWriteLock);
	Event ne;
	ne.time = globalTimer + cyclesIntoFuture;
	ne.type = event_type;
	ne.userdata = userdata;
	tsQueue.Push(ne);
}

// Same as ScheduleEvent_Threadsafe(0, ...) EXCEPT if we are already on the CPU thread
// in which case the event will get handled immediately, before returning.
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
	Event *ne = GetNewEvent();
	ne->userdata = userdata;
	ne->type = event_type;
	ne->time = globalTimer + cyclesIntoFuture;
	AddEventToQueue(ne);
}

void RegisterAdvanceCallback(void (*callback)(int cyclesExecuted))
{
	advanceCallback = callback;
}

bool IsScheduled(int event_type)
{
	if (!first)
		return false;
	Event *e = first;
	while (e)
	{
		if (e->type == event_type)
			return true;
		e = e->next;
	}
	return false;
}

void RemoveEvent(int event_type)
{
	if (!first)
		return;

	while (first)
	{
		if (first->type == event_type)
		{
			Event *next = first->next;
			FreeEvent(first);
			first = next;
		}
		else
		{
			break;
		}
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

void SetMaximumSlice(int maximumSliceLength)
{
	maxSliceLength = maximumSliceLength;
}

void ForceExceptionCheck(int cycles)
{
	if (PowerPC::ppcState.downcount > cycles)
	{
		slicelength -= (PowerPC::ppcState.downcount - cycles); // Account for cycles already executed by adjusting the slicelength
		PowerPC::ppcState.downcount = cycles;
	}
}

void ResetSliceLength()
{
	maxSliceLength = MAX_SLICE_LENGTH;
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

	int cyclesExecuted = slicelength - PowerPC::ppcState.downcount;
	globalTimer += cyclesExecuted;
	PowerPC::ppcState.downcount = slicelength;

	while (first)
	{
		if (first->time <= globalTimer)
		{
			//LOG(POWERPC, "[Scheduler] %s     (%lld, %lld) ",
			//             event_types[first->type].name ? event_types[first->type].name : "?", (u64)globalTimer, (u64)first->time);
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

	if (!first)
	{
		WARN_LOG(POWERPC, "WARNING - no events in queue. Setting downcount to 10000");
		PowerPC::ppcState.downcount += 10000;
	}
	else
	{
		slicelength = (int)(first->time - globalTimer);
		if (slicelength > maxSliceLength)
			slicelength = maxSliceLength;
		PowerPC::ppcState.downcount = slicelength;
	}

	if (advanceCallback)
		advanceCallback(cyclesExecuted);
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

	//When the FIFO is processing data we must not advance because in this way
	//the VI will be desynchronized. So, We are waiting until the FIFO finish and
	//while we process only the events required by the FIFO.
	while (g_video_backend->Video_IsPossibleWaitingSetDrawDone())
	{
		ProcessFifoWaitEvents();
		Common::YieldCPU();
	}

	idledCycles += PowerPC::ppcState.downcount;
	PowerPC::ppcState.downcount = 0;

	Advance();
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

