
#ifndef _FIFO_QUEUE_H_
#define _FIFO_QUEUE_H_

// a simple lockless thread-safe,
// single reader, single writer queue

namespace Common
{

template <typename T>
class FifoQueue
{
public:
	FifoQueue()
	{
		 m_write_ptr = m_read_ptr = new ElementPtr();
	}

	~FifoQueue()
	{
		// this will empty out the whole queue
		delete m_read_ptr;
	}

	bool Empty() const	// true if the queue is empty
	{
		return (m_read_ptr == m_write_ptr);
	}

	void Push(const T& t)
	{
		// create the element, add it to the queue
		m_write_ptr->current = new T(t);
		// set the next pointer to a new element ptr
		// then advance the write pointer 
		m_write_ptr = m_write_ptr->next = new ElementPtr();
	}

	bool Pop(T& t)
	{
		// if write pointer points to the same element, queue is empty
		if (m_read_ptr == m_write_ptr)
			return false;

		// get the element from out of the queue
		t = *m_read_ptr->current;

		ElementPtr *const tmpptr = m_read_ptr;
		// advance the read pointer
		m_read_ptr = m_read_ptr->next;
		
		// set the next element to NULL to stop the recursive deletion
		tmpptr->next = NULL;
		delete tmpptr;	// this also deletes the element

		return true;
	}

	bool Peek(T& t)
	{
		// if write pointer points to the same element, queue is empty
		if (m_read_ptr == m_write_ptr)
			return false;

		// get the element from out of the queue
		t = *m_read_ptr->current;

		return true;
	}

private:
	// stores a pointer to element at front of queue
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
};

}

#endif
