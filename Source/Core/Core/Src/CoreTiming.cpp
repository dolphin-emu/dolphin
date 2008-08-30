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

#include <vector>

#include "Thread.h"
#include "PowerPC/PowerPC.h"
#include "CoreTiming.h"
#include "StringUtil.h"

// TODO(ector): Replace new/delete in this file with a simple memory pool
// Don't expect a massive speedup though.

namespace CoreTiming
{

struct EventType
{
	TimedCallback callback;
	const char *name;
};

std::vector<EventType> event_types;

struct BaseEvent
{
	s64 time;
	u64 userdata;
	int type;
//	Event *next;
};

typedef LinkedListItem<BaseEvent> Event;

// STATE_TO_SAVE (how?)
Event *first;
Event *tsFirst;

int downcount, slicelength;
int maxSliceLength = 20000;

s64 globalTimer;
s64 idledCycles;

Common::CriticalSection externalEventSection;

void (*advanceCallback)(int cyclesExecuted);

int RegisterEvent(const char *name, TimedCallback callback)
{
	EventType type;
	type.name = name;
	type.callback = callback;
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
	downcount = maxSliceLength;
	slicelength = maxSliceLength;
	globalTimer = 0;
	idledCycles = 0;
}

void Shutdown()
{
	ClearPendingEvents();
	UnregisterAllEvents();
}

void DoState(PointerWrap &p)
{
	externalEventSection.Enter();
	p.Do(downcount);
	p.Do(slicelength);
	p.Do(globalTimer);
	p.Do(idledCycles);
	// OK, here we're gonna need to specialize depending on the mode.
	// Should do something generic to serialize linked lists.
	switch (p.GetMode()) {
	case PointerWrap::MODE_READ:
		{
		ClearPendingEvents();
		if (first)
			PanicAlert("Clear failed.");
		int more_events = 0;
		Event *prev = 0;
		while (true) {
			p.Do(more_events);
			if (!more_events)
				break;
			Event *ev = new Event;
			if (!prev)
				first = ev;
			else
				prev->next = ev;
			p.Do(ev->time);
			p.Do(ev->type);
			p.Do(ev->userdata);
			ev->next = 0;
			prev = ev;
			ev = ev->next;
		}
		}
		break;
	case PointerWrap::MODE_MEASURE:
	case PointerWrap::MODE_WRITE:
		{
		Event *ev = first;
		int more_events = 1;
		while (ev) {
			p.Do(more_events);
			p.Do(ev->time);
			p.Do(ev->type);
			p.Do(ev->userdata);
			ev = ev->next;
		}
		more_events = 0;
		p.Do(more_events);
		break;
		}
	}
	externalEventSection.Leave();
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
	externalEventSection.Enter();
	Event *ne = new Event;
	ne->time = globalTimer + cyclesIntoFuture;
	ne->type = event_type;
	ne->next = tsFirst;
	ne->userdata = userdata;
	tsFirst = ne;
	externalEventSection.Leave();
}

void ClearPendingEvents()
{
	while (first)
	{
		Event *e = first->next;
		delete first;
		first = e;
	}
}

void AddEventToQueue(Event *ne)
{
	// Damn, this logic got complicated. Must be an easier way.
	if (!first)
	{
		first = ne;
		ne->next = 0;
	}
	else
	{
		Event *ptr = first;
		Event *prev = 0;
		if (ptr->time > ne->time)
		{
			ne->next = first;
			first = ne;
		}
		else
		{
			prev = first;
			ptr = first->next;

			while (ptr)
			{
				if (ptr->time <= ne->time)
				{
					prev = ptr;
					ptr = ptr->next;
				}
				else
					break;
			}

			//OK, ptr points to the item AFTER our new item. Let's insert
			ne->next = prev->next;
			prev->next = ne;
			// Done!
		}
	}
}

// This must be run ONLY from within the cpu thread
// cyclesIntoFuture may be VERY inaccurate if called from anything else
// than Advance 
void ScheduleEvent(int cyclesIntoFuture, int event_type, u64 userdata)
{
	Event *ne = new Event;
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
	while (e) {
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
	if (first->type == event_type)
	{
		Event *next = first->next;
		delete first;
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
			delete ptr;
			ptr = prev->next;
		}
		else
		{
			prev = ptr;
			ptr = ptr->next;
		}
	}
}

void SetMaximumSlice(int maximumSliceLength)
{
	maxSliceLength = maximumSliceLength;
}


void Advance()
{
	// Move events from async queue into main queue
	externalEventSection.Enter();
	while (tsFirst)
	{
		//MessageBox(0,"yay",0,0);
		Event *next = tsFirst->next;
		AddEventToQueue(tsFirst);
		tsFirst = next;
	}
	externalEventSection.Leave();

	// we are out of run, downcount = -3
	int cyclesExecuted = slicelength - downcount;
	//  sliceLength = downac

	globalTimer += cyclesExecuted;

	while (first)
	{
		if (first->time <= globalTimer)
		{
//			LOG(GEKKO, "[Scheduler] %s     (%lld, %lld) ", 
//				first->name ? first->name : "?", (u64)globalTimer, (u64)first->time);
			
			event_types[first->type].callback(first->userdata, (int)(globalTimer - first->time));
			Event *next = first->next;
			delete first;
			first = next;
		}
		else
		{
			break;
		}
	}
	if (!first) 
	{
		LOG(GEKKO, "WARNING - no events in queue. Setting downcount to 10000");
		// PanicAlert?
		downcount += 10000;
	}
	else
	{
		slicelength = (int)(first->time - globalTimer);
		if (slicelength > maxSliceLength)
			slicelength = maxSliceLength;
		downcount = slicelength;
	}
	if (advanceCallback)
		advanceCallback(cyclesExecuted);
}

void LogPendingEvents()
{
	Event *ptr = first;
	while (ptr)
	{
		LOG(GEKKO, "PENDING: Now: %lld Pending: %lld Type: %d", globalTimer, ptr->time, ptr->type);
		ptr = ptr->next;
	}
}

void Idle()
{
	LOG(GEKKO, "Idle");
	
	idledCycles += downcount;
	downcount = 0;
	
	Advance();
}

std::string GetScheduledEventsSummary()
{
	Event *ptr = first;
	std::string text = "Scheduled events\n";
	text.reserve(1000);
	while (ptr)
	{
		int t = ptr->type;
		if (t < 0 || t >= event_types.size())
			PanicAlert("Invalid event type %i", t);
		const char *name = event_types[ptr->type].name;
		if (!name)
			name = "[unknown]";
		text += StringFromFormat("%s : %i %08x%08x\n", event_types[ptr->type].name, ptr->time, ptr->userdata >> 32, ptr->userdata);
		ptr = ptr->next;
	}
	return text;
}

}; // end of namespace
