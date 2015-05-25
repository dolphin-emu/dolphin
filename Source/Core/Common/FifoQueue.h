// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

// a simple lockless thread-safe,
// single reader, single writer queue

#include <algorithm>
#include <cstddef>

#include "Common/Atomic.h"
#include "Common/CommonTypes.h"

namespace Common
{

template <typename T, bool NeedSize = true>
class FifoQueue
{
public:
	FifoQueue() : m_size(0)
	{
		 m_write_ptr = m_read_ptr = new ElementPtr();
	}

	~FifoQueue()
	{
		// this will empty out the whole queue
		delete m_read_ptr;
	}

	u32 Size() const
	{
		static_assert(NeedSize, "using Size() on FifoQueue without NeedSize");
		return m_size;
	}

	bool Empty() const
	{
		return !AtomicLoad(m_read_ptr->next);
	}

	T& Front() const
	{
		AtomicLoadAcquire(m_read_ptr->next);
		return m_read_ptr->current;
	}

	template <typename Arg>
	void Push(Arg&& t)
	{
		// create the element, add it to the queue
		m_write_ptr->current = std::forward<Arg>(t);
		// set the next pointer to a new element ptr
		// then advance the write pointer
		ElementPtr* new_ptr = new ElementPtr();
		AtomicStoreRelease(m_write_ptr->next, new_ptr);
		m_write_ptr = new_ptr;
		if (NeedSize)
			Common::AtomicIncrement(m_size);
	}

	void Pop()
	{
		if (NeedSize)
			Common::AtomicDecrement(m_size);
		ElementPtr *tmpptr = m_read_ptr;
		// advance the read pointer
		m_read_ptr = AtomicLoad(tmpptr->next);
		// set the next element to nullptr to stop the recursive deletion
		tmpptr->next = nullptr;
		delete tmpptr; // this also deletes the element
	}

	bool Pop(T& t)
	{
		if (Empty())
			return false;

		if (NeedSize)
			Common::AtomicDecrement(m_size);

		ElementPtr *tmpptr = m_read_ptr;
		m_read_ptr = AtomicLoadAcquire(tmpptr->next);
		t = std::move(tmpptr->current);
		tmpptr->next = nullptr;
		delete tmpptr;
		return true;
	}

	// not thread-safe
	void Clear()
	{
		m_size = 0;
		delete m_read_ptr;
		m_write_ptr = m_read_ptr = new ElementPtr();
	}

private:
	// stores a pointer to element
	// and a pointer to the next ElementPtr
	class ElementPtr
	{
	public:
		ElementPtr() : next(nullptr) {}

		~ElementPtr()
		{
			if (next)
				delete next;
		}

		T current;
		ElementPtr *volatile next;
	};

	ElementPtr *m_write_ptr;
	ElementPtr *m_read_ptr;
	volatile u32 m_size;
};

}
