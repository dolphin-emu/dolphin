
#ifndef _FIFO_QUEUE_H_
#define _FIFO_QUEUE_H_

// a simple lockless thread-safe,
// single reader, single writer queue

#include "Atomic.h"

namespace Common
{

template <typename T>
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
		return m_size;
	}

	bool Empty() const
	{
		//return (m_read_ptr == m_write_ptr);
		return (0 == m_size);
	}

	const T& Front() const
	{
		return *m_read_ptr->current;
	}

	void Push(const T& t)
	{
		// create the element, add it to the queue
		m_write_ptr->current = new T(t);
		// set the next pointer to a new element ptr
		// then advance the write pointer 
		m_write_ptr = m_write_ptr->next = new ElementPtr();
		Common::AtomicIncrement(m_size);
	}

	void Pop()
	{
		Common::AtomicDecrement(m_size);
		ElementPtr *const tmpptr = m_read_ptr;
		// advance the read pointer
		m_read_ptr = m_read_ptr->next;
		// set the next element to NULL to stop the recursive deletion
		tmpptr->next = NULL;
		delete tmpptr;	// this also deletes the element
	}

	bool Pop(T& t)
	{
		if (Empty())
			return false;

		t = Front();
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
	// stores a pointer to element
	// and a pointer to the next ElementPtr
	class ElementPtr
	{
	public:
		ElementPtr() : current(NULL), next(NULL) {}

		~ElementPtr()
		{
			if (current)
			{
				delete current;
				// recusion ftw
				if (next)
					delete next;
			}
		}

		T *volatile current;
		ElementPtr *volatile next;
	};

	ElementPtr *volatile m_write_ptr;
	ElementPtr *volatile m_read_ptr;
	volatile u32 m_size;
};

}

#endif
