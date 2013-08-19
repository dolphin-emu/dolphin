
#ifndef _FIFO_QUEUE_H_
#define _FIFO_QUEUE_H_

// a simple lockless thread-safe,
// single reader, single writer queue

#include "Atomic.h"

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
		return !m_read_ptr->next;
	}

	T& Front() const
	{
		return m_read_ptr->current;
	}

	template <typename Arg>
	void Push(Arg&& t)
	{
		// create the element, add it to the queue
		m_write_ptr->current = std::move(t);
		// set the next pointer to a new element ptr
		// then advance the write pointer 
		m_write_ptr = m_write_ptr->next = new ElementPtr();
		if (NeedSize)
			Common::AtomicIncrement(m_size);
	}

	void Pop()
	{
		if (NeedSize)
			Common::AtomicDecrement(m_size);
		ElementPtr *tmpptr = m_read_ptr;
		// advance the read pointer
		m_read_ptr = tmpptr->next;
		// set the next element to NULL to stop the recursive deletion
		tmpptr->next = NULL;
		delete tmpptr;	// this also deletes the element
	}

	bool Pop(T& t)
	{
		if (Empty())
			return false;

		t = std::move(Front());
		Pop();

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
	class ElementPtr;

public:
	class iterator
	{
	public:
		iterator() {}
		bool operator==(iterator other) { return other.m_pp == m_pp; }
		bool operator!=(iterator other) { return !(*this == other); }
		T *operator->() { return &**this; }
		T& operator*() { return (*m_pp)->current; }
		void operator++() { m_pp = &(*m_pp)->next; }
	protected:
		iterator(ElementPtr *volatile *pp) : m_pp(pp) {}
		ElementPtr *volatile *m_pp;
		friend class FifoQueue<T, NeedSize>;
	};

	iterator begin()
	{
		return iterator(&m_read_ptr);
	}

	iterator end()
	{
		return iterator(&m_write_ptr->next);
	}

	iterator erase(iterator itr)
	{
		ElementPtr *elp = *itr.m_pp;
		*itr.m_pp = elp->next;
		delete elp;
		return itr;
	}

private:
	// stores a pointer to element
	// and a pointer to the next ElementPtr
	class ElementPtr
	{
	public:
		ElementPtr() : next(NULL) {}

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

#endif
